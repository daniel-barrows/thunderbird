ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource:///modules/MailServices.jsm");
ChromeUtils.import("resource:///modules/IOUtils.js");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://testing-common/mailnews/mailTestUtils.js");
ChromeUtils.import("resource://testing-common/mailnews/localAccountUtils.js");

var CC = Components.Constructor;

// WebApps.jsm called by ProxyAutoConfig (PAC) requires a valid nsIXULAppInfo.
ChromeUtils.import("resource://testing-common/AppInfo.jsm");
updateAppInfo();

// Ensure the profile directory is set up
do_get_profile();

var gDEPTH = "../../../../";

// Import the required setup scripts.
load("../../../resources/abSetup.js");

// Import the smtp server scripts
ChromeUtils.import("resource://testing-common/mailnews/maild.js");
ChromeUtils.import("resource://testing-common/mailnews/smtpd.js");
ChromeUtils.import("resource://testing-common/mailnews/auth.js");

ChromeUtils.import("resource:///modules/MailServices.jsm");

var gDraftFolder;

// Setup the daemon and server
function setupServerDaemon(handler) {
  if (!handler)
    handler = function (d) { return new SMTP_RFC2821_handler(d); };
  var server = new nsMailServer(handler, new smtpDaemon());
  return server;
}

function getBasicSmtpServer(port=1, hostname="localhost") {
  let server = localAccountUtils.create_outgoing_server(port, "user",
    "password", hostname);

  // Override the default greeting so we get something predicitable
  // in the ELHO message
  Services.prefs.setCharPref("mail.smtpserver.default.hello_argument", "test");

  return server;
}

function getSmtpIdentity(senderName, smtpServer) {
  // Set up the identity
  let identity = MailServices.accounts.createIdentity();
  identity.email = senderName;
  identity.smtpServerKey = smtpServer.key;

  return identity;
}

var test;

function do_check_transaction(real, expected) {
  // real.them may have an extra QUIT on the end, where the stream is only
  // closed after we have a chance to process it and not them. We therefore
  // excise this from the list
  if (real.them[real.them.length-1] == "QUIT")
    real.them.pop();

  Assert.equal(real.them.join(","), expected.join(","));
  dump("Passed test " + test + "\n");
}

// This listener is designed just to call OnStopCopy() when its OnStopCopy
// function is called - the rest of the functions are unneeded for a lot of
// tests (but we can't use asyncCopyListener because we need the
// nsIMsgSendListener interface as well).
var copyListener = {
  // nsIMsgSendListener
  onStartSending: function (aMsgID, aMsgSize) {},
  onProgress: function (aMsgID, aProgress, aProgressMax) {},
  onStatus: function (aMsgID, aMsg) {},
  onStopSending: function (aMsgID, aStatus, aMsg, aReturnFile) {},
  onGetDraftFolderURI: function (aFolderURI) {},
  onSendNotPerformed: function (aMsgID, aStatus) {},

  // nsIMsgCopyServiceListener
  OnStartCopy: function () {},
  OnProgress: function (aProgress, aProgressMax) {},
  SetMessageKey: function (aKey) {},
  GetMessageId: function (aMessageId) {},
  OnStopCopy: function (aStatus) {
    OnStopCopy(aStatus);
  },

  // QueryInterface
  QueryInterface: ChromeUtils.generateQI(["nsIMsgSendListener",
                                          "nsIMsgCopyServiceListener"]),
};

var progressListener = {
  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP)
      this.resolve(mailTestUtils.firstMsgHdr(gDraftFolder));
  },

  onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress,
    aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) {},
  onLocationChange: function(aWebProgress, aRequest, aLocation, aFlags) {},
  onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage) {},
  onSecurityChange: function(aWebProgress, aRequest, state) {},
  onContentBlockingEvent: function(aWebProgress, aRequest, aEvent) {},

  QueryInterface: ChromeUtils.generateQI(["nsIWebProgressListener",
                                          "nsISupportsWeakReference"]),
};

function createMessage(aAttachment) {
  let fields = Cc["@mozilla.org/messengercompose/composefields;1"]
                 .createInstance(Ci.nsIMsgCompFields);
  let attachments = [];
  if (aAttachment) {
    let attachment = Cc["@mozilla.org/messengercompose/attachment;1"]
                       .createInstance(Ci.nsIMsgAttachment);
    if (aAttachment instanceof Ci.nsIFile) {
      attachment.url = "file://" + aAttachment.path;
      attachment.contentType = 'text/plain';
      attachment.name = aAttachment.leafName;
    } else {
      attachment.url = "data:,sometext";
      attachment.name = aAttachment;
    }
    attachments = [attachment];
  }
  return richCreateMessage(fields, attachments);
}

function richCreateMessage(fields, attachments=[], identity=null,
                           account=null) {
  let params = Cc["@mozilla.org/messengercompose/composeparams;1"]
                 .createInstance(Ci.nsIMsgComposeParams);
  params.composeFields = fields;

  let msgCompose = MailServices.compose.initCompose(params);
  if (identity === null)
    identity = getSmtpIdentity(null, getBasicSmtpServer());

  let rootFolder = localAccountUtils.rootFolder;
  gDraftFolder = null;
  // Make sure the drafts folder is empty
  try {
    gDraftFolder = rootFolder.getChildNamed("Drafts");
  } catch (e) {
    // we don't have to remove the folder because it doen't exist yet
    gDraftFolder = rootFolder.createLocalSubfolder("Drafts");
  }
  // Clear all messages
  let array = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
  let enumerator = gDraftFolder.msgDatabase.EnumerateMessages();
  while (enumerator.hasMoreElements())
    array.appendElement(enumerator.getNext());
  if (array.length)
    gDraftFolder.deleteMessages(array, null, true, false, null, false);

  // Set attachment
  fields.removeAttachments();
  for (let attachment of attachments)
    fields.addAttachment(attachment);

  let progress = Cc["@mozilla.org/messenger/progress;1"]
                   .createInstance(Ci.nsIMsgProgress);
  let promise = new Promise((resolve, reject) => {
    progressListener.resolve = resolve;
    progressListener.reject = reject;
  });
  progress.registerListener(progressListener);
  msgCompose.SendMsg(Ci.nsIMsgSend.nsMsgSaveAsDraft, identity,
                     account ? account.key : "", null,
                     progress);
  return promise;
}

function getAttachmentFromContent(aContent) {
  function getBoundaryStringFromContent(aContent) {
    let found = aContent.match(/Content-Type: multipart\/mixed;\s+boundary="(.*?)"/);
    Assert.notEqual(found, null);
    Assert.equal(found.length, 2);

    return found[1];
  };

  let boundary = getBoundaryStringFromContent(aContent);
  let regex = new RegExp("\\r\\n\\r\\n--" + boundary + "\\r\\n" +
                         "([\\s\\S]*?)\\r\\n" +
                         "--" + boundary + "--", "m");
  let attachments = aContent.match(regex);
  Assert.notEqual(attachments, null);
  Assert.equal(attachments.length, 2);
  return attachments[1];
}

registerCleanupFunction(function() {
  load(gDEPTH + "mailnews/resources/mailShutdown.js");
});
