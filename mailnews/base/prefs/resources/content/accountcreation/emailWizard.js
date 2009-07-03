/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * David Ascher <davida@mozilla.com> and
 * Ben Bucksch <mozilla bucksch.org>
 * Portions created by the Initial Developer are Copyright (C) 2008-2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * This is the dialog opened by menu File | New account | Mail... .
 *
 * It gets the user's realname, email address and password,
 * and tries to automatically configure the account from that,
 * using various mechanisms. If all fails, the user can enter/edit
 * the config, then we create the account.
 *
 * Steps:
 * - User enters realname, email address and password
 * - check for config files on disk
 *   (shipping with Thunderbird, for enterprise deployments)
 * - (if fails) try to get the config file from the ISP via a
 *   fixed URL on the domain of the email address
 * - (if fails) try to get the config file from our own database
 *   at MoMo servers, maintained by the community
 * - (if fails) try to guess the config, by guessing hostnames,
 *    probing ports, checking config via server's CAPS line etc..
 * - verify the setup, by trying to login to the configured servers
 * - let user verify and maybe edit the server names and ports
 * - If user clicks OK, create the account
 */


// from http://xyfer.blogspot.com/2005/01/javascript-regexp-email-validator.html
var emailRE = /^[-_a-z0-9\'+*$^&%=~!?{}]+(?:\.[-_a-z0-9\'+*$^&%=~!?{}]+)*@(?:[-a-z0-9.]+\.[a-z]{2,6}|\d{1,3}(?:\.\d{1,3}){3})(?::\d+)?$/i;
var domainRE = /^((?:[\w-]+\.)*\w[\w-]{0,66})\.([a-z]{2,6}(?:\.[a-z]{2})?)$|(\[?(\d{1,3}\.){3}\d{1,3}\]?)$/i
const kHighestPort = 65535;

Cu.import("resource://app/modules/gloda/log4moz.js");

let gSmtpManager = Cc["@mozilla.org/messengercompose/smtp;1"]
                   .getService(Ci.nsISmtpService);
let gAccountMgr = Cc["@mozilla.org/messenger/account-manager;1"]
                  .getService(Ci.nsIMsgAccountManager);
let gEmailWizardLogger = Log4Moz.getConfiguredLogger("emailwizard");
let gStringsBundle;

// debugging aid
if (kDebug) {
  function getElementById(id)
  {
    let elt = document.getElementById(id);
    if (!elt)
    {
      gEmailWizardLogger.error("Can't find element ID: " + id);
      return null;
    }
    return elt;
  }
} else // production
  getElementById = document.getElementById;

function _hide(id)
{
  getElementById(id).hidden = true;
}
function _show(id)
{
  getElementById(id).hidden = false;
}
function EmailConfigWizard()
{
  this._init();
}
EmailConfigWizard.prototype =
{
  _init : function EmailConfigWizard__init()
  {
    gEmailWizardLogger.info("Initializing setup wizard");
    this._probeAbortable = null;
  },

  onLoad : function ()
  {
    this._email = "";
    this._realname = "";
    this._password = "";
    this._verifiedConfig = false;
    this._userChangedIncomingProtocol = false;
    this._userChangedIncomingPort = false;
    this._userChangedIncomingSocketType = false;
    this._userChangedOutgoingPort = false;
    this._userChangedOutgoingSocketType = false;

    this._incomingWarning = 'cleartext';
    this._outgoingWarning = 'cleartext';
    this._userPickedOutgoingServer = false;
    this._customFields = {}; // map of: field ID from config file {String} -> field value entered by user {String}

    if (window.arguments && window.arguments[0] &&
        window.arguments[0].msgWindow)
      this._parentMsgWindow = window.arguments[0].msgWindow;

    gStringsBundle = getElementById("strings");

    // Populate SMTP server dropdown with already configured SMTP servers from
    // other accounts.
    let gSmtpManager = Cc["@mozilla.org/messengercompose/smtp;1"]
                       .getService(Ci.nsISmtpService);
    this._smtpServers = gSmtpManager.smtpServers;
    var menupopup = getElementById("smtp_menupopup");
    while (this._smtpServers.hasMoreElements())
    {
      var server = this._smtpServers.getNext().QueryInterface(Ci.nsISmtpServer);
      var menuitem = document.createElement("menuitem");
      var label = server.displayname;
      if (server.key == gSmtpManager.defaultServer.key)
        label += " " + gStringsBundle.getString("defaultServerTag");

      menuitem.setAttribute("label", label);
      menuitem.setAttribute("value", server.key);
      menuitem.hostname = server.hostname;
      menupopup.appendChild(menuitem);
    }
    var menulist = getElementById("outgoing_server");
    menulist.addEventListener("command",
      function() { gEmailConfigWizard.userChangedOutgoing(); }, true);
  },

  /* When the next button is clicked we've moved from the initial account
   * information stage to using that information to configure account details.
   */
  onNext : function()
  {
    // change the inputs to a flat look
    this._accountInfoInputs(true);

    this._email = getElementById("email").value;
    this._realname = getElementById("realname").value;
    this._password = getElementById("password").value;

    this.showConfigDetails();

    let domain = this._email.split("@")[1];
    this.findConfig(domain, this._email);

    // swap out buttons
    _hide("next_button");
    _show("back_button");
    _show("stop_button");
    _hide("edit_button");
    _hide("go_button");
  },

  /* The back button can be clicked at anytime and should stop all probing of
   * account details.  This allows the person to return to their account
   * information details in case anything was incorrect.
   *
   * Any account details that were manually entered should be remembered even
   * when this back button is clicked
   */
  onBack : function()
  {
    this.hideConfigDetails();
    this.clearConfigDetails();

    // change the inputs back to regular
    this._accountInfoInputs(false);

    // swap buttons back
    _show("next_button");
    _hide("back_button");
    _hide("advanced_settings");
  },

  /* Helper method that enables or disabled all the account information inputs
   *
   * NOTE: The remember password input was skipped purposefully, people should
   *        continue to be able to change that setting even as account details
   *        are determined.
   */
  _accountInfoInputs : function(disabled)
  {
    let ids = ["realname","email","password"];
    for ( let i = 0; i < ids.length; i++ )
      getElementById(ids[i]).disabled = disabled;
  },

  userChangedOutgoing : function ()
  {
    this._outgoingState = 'done';
    this._outgoingWarning = '';

    if (this._probeAbortable)
      this._probeAbortable.cancel('outgoing');

    this._userPickedOutgoingServer = true;
    _hide('outgoing_protocol');
    _hide('outgoing_port');
    _hide('outgoing_security');

    if (this._incomingState == 'done')
      this.foundConfig(this.getUserConfig());
  },

  /* This does very little other than to check that a name was entered at all
   * Since this is such an insignificant test we should be using a very light
   * or even jovial warning.
   */
  validateRealname : function ()
  {
    if (getElementById('realname').value.length > 0)
      this.clearError('nameerror');
    else
      this.setError("nameerror", "name.error");
  },

  /*
   * This checks if the email address is at least possibly valid, meaning it
   * has an '@' before the last char.
   */
  emailAddrValidish : function ()
  {
    let emailAddr = getElementById('email').value;
    let atPos = emailAddr.lastIndexOf("@");
    return  atPos > 0 && atPos + 1 < emailAddr.length;
  },

  /*
   * onEmailInput and onRealnameInput are called onInput, and just handle
   * hiding/showing the next button based on whether there's a semi-reasonable
   * e-mail address and nonblank realname to start with.
   */
  onEmailInput : function()
  {
    if (getElementById('realname').value.length > 0 &&
        this.emailAddrValidish())
      _show("next_button");
    else
      _hide("next_button");
   },

  onRealnameInput : function()
  {
    if (getElementById('realname').value.length > 0 &&
        this.emailAddrValidish())
      _show("next_button");
    else
      _hide("next_button");
   },

  /* This check is done on blur and is only done as an informative warning
   * we don't want to block the person if they've entered and email that
   * doesn't conform to our regex
   */
  validateEmail : function ()
  {
    if (getElementById('email').value.length <= 0)
      return;

    if (emailRE.test(getElementById('email').value))
      this.clearError('emailerror');
    else
      this.setError("emailerror", "email.error");
  },

  // We use this to  prevent probing from forgetting the user's choice.
  setSecurity : function(eltId)
  {
    switch (eltId) {
      case 'incoming_security':
        this._userChangedIncomingSocketType = true;
        this._incomingState = "";
        break;
      case 'outgoing_security':
        this._userChangedOutgoingSocketType = true;
        this._outgoingState = "";
        break;
    }
  },

  setIncomingProtocol : function()
  {
    this._userChangedIncomingProtocol = true;
    this._incomingState = "";
  },

  setPort : function(eltId)
  {
    switch (eltId) {
      case 'incoming_port':
        this._userChangedIncomingPort = true;
        this._incomingState = "";
        break;
      case 'outgoing_port':
        this._userChangedOutgoingPort = true;
        this._outgoingState = "";
        break;
    }
  },
  
  /* If the user just tabbed through the password input without entering
   * anything, set the type back to text so we don't wind up showing the
   * emptytext as bullet characters.
   */
  onblurPassword : function ()
  {
    let passwordElt = getElementById("password");
    if (passwordElt.value.length < 1)
      passwordElt.type = "text";
  },

  findConfig : function(domain, email)
  {
    gEmailWizardLogger.info("findConfig()");
    this.startSpinner("searching_for_configs");
    this.setSpinnerStatus("check_preconfig");
    if (this._probeAbortable)
    {
      gEmailWizardLogger.info("aborting existing config search");
      this._probeAbortable.cancel();
    }
    var me = this;
    this._probeAbortable = 
      fetchConfigFromDisk(
        domain,
        function(config) // success
        {
          me.foundConfig(config);
          me.stopSpinner("finished_with_success");
          me.setSpinnerStatus("found_preconfig");
          me._probeAbortable = null;
        },
        function(e) // fetchConfigFromDisk failed
        {
          gEmailWizardLogger.info("fetchConfigFromDisk failed: " + e);
          me.startSpinner("searching_for_configs");
          me.setSpinnerStatus("checking_mozilla_config");
          me._probeAbortable = 
            fetchConfigFromDB(
              domain,
              function(config) // success
              {
                me.foundConfig(config);
                me.stopSpinner("finished_with_success");
                me.setSpinnerStatus("found_isp_config");
                me.showEditButton();
                me._probeAbortable = null;
              },
              function(e) // fetchConfigFromDB failed
              {
                gEmailWizardLogger.info("fetchConfigFromDB failed: " + e);
                me.setSpinnerStatus("probing_config");
                var initialConfig = new AccountConfig();
                me._prefillConfig(initialConfig);
                me.startSpinner("searching_for_configs")
                me.setSpinnerStatus("guessing_from_email");
                me._guessConfig(domain, initialConfig, 'both');
              });
          });
  },

  _guessConfig : function(domain, initialConfig, which)
  {
    let me = this;
    // guessConfig takes several callback functions, which we define inline.
    me._probeAbortable = guessConfig(domain,
          function(type, hostname, port, ssl, done, config) // progress
          {
            gEmailWizardLogger.info("progress callback host " + hostname +
                                    " port " +  port + " type " + type);
            if (type == "imap" || type == "pop3")
            {
              config.incoming.type = type;
              config.incoming.hostname = hostname;
              config.incoming.port = port;
              config.incoming.socketType = ssl;
              config.incoming._inprogress = !done; // XXX not nice to change the AccountConfig object
            }
            else if (type == "smtp" && !me._userPickedOutgoingServer)
            {
              config.outgoing.hostname = hostname;
              config.outgoing.port = port;
              config.outgoing.socketType = ssl;
              config.outgoing._inprogress = !done;
            }
            me.updateConfig(config);
          },
          function(config) // success
          {
            me.foundConfig(config);
            gEmailWizardLogger.info("in success, incomingState = " +
                                    me._incomingState + " outgoingState = " +
                                    me._outgoingState);
            if (me._incomingState == 'done' && me._outgoingState == 'done')
            {
              me.stopSpinner("finished_with_success");
              me.setSpinnerStatus("config_details_found");
              _hide("stop_button");
              _show("edit_button");
            }
            else if (me._incomingState == 'done' && me._outgoingState != 'probing')
            {
              me.stopSpinner("finished_with_success");
              me.setSpinnerStatus("incoming_found_specify_outgoing");
              me.editConfigDetails();
            }
            else if (me._outgoingState == 'done' && me._incomingState != 'probing')
            {
              me.stopSpinner("finished_with_success");
              me.setSpinnerStatus("outgoing_found_specify_incoming");
              me.editConfigDetails();
            }
            if (me._outgoingState != 'probing' &&
                me._incomingState != 'probing')
              me._probeAbortable = null;

          },
          function(e, config) // guessconfig failed
          {
            gEmailWizardLogger.info("guessConfig failed: " + e);
            me.updateConfig(config);
            me.stopSpinner("finished_with_errors");
            me.setSpinnerStatus("enter_missing_hostnames");
            me._probeAbortable = null;
            me.editConfigDetails();
          },
          function(e, config) // guessconfig failed for incoming
          {
            gEmailWizardLogger.info("guessConfig failed for incoming: " + e);
            me.setSpinnerStatus("incoming_failed_trying_outgoing");
            config.incoming.hostname = -1;
            me.updateConfig(config);
          },
          function(e, config) // guessconfig failed for outgoing
          {
            gEmailWizardLogger.info("guessConfig failed for outgoing: " + e);
            me.setSpinnerStatus("outgoing_failed_trying_incoming");
            if (!me._userPickedOutgoingServer)
              config.outgoing.hostname = -1;

            me.updateConfig(config);
          },
    initialConfig, which);
  },

  foundConfig : function(config)
  {
    gEmailWizardLogger.info("foundConfig()");
    assert(config instanceof AccountConfig, "BUG: Arg 'config' needs to be an AccountConfig object");

    if (!config)
      config = new AccountConfig();

    this._currentConfig = config;
    this._haveValidConfigForDomain = this._email.split("@")[1];;

    if (!this._realname || !this._email)
      return;

    return this._foundConfig2(config);
  },
  // Continuation of foundConfig2() after custom fields.
  _foundConfig2 : function(config)
  {
    this._currentConfigFilledIn = config.copy();
    _show("advanced_settings");
    replaceVariables(this._currentConfigFilledIn, this._realname, this._email,
                     this._password, this._customFields);

    this.updateConfig(this._currentConfigFilledIn);

    getElementById('create_button').disabled = false;
    getElementById('create_button').hidden = false;

  },

  /*
   * Returns either the nsISmtpServer.key for an existing account that matches
   * our discovered hostname + port + username, or false if no match is found.
   */
  keyForExistingOutgoingAccount : function()
  {
    var smtpServers = gSmtpManager.smtpServers;
    while (smtpServers.hasMoreElements())
    {
      let existingServer = smtpServers.getNext().QueryInterface(Components.interfaces.nsISmtpServer);
      if (existingServer.hostname == this._currentConfigFilledIn.outgoing.hostname &&
          existingServer.port == this._currentConfigFilledIn.outgoing.port &&
          existingServer.username == this._currentConfigFilledIn.outgoing.username)
        return existingServer.key;
    }
    return false;
  },

  checkIncomingAccountIsNew : function()
  {
    let incoming = this._currentConfigFilledIn.incoming;
    let isNew = gAccountMgr.findRealServer(incoming.username,
                                          incoming.hostname,
                                          sanitize.enum(incoming.type,
                                                        ["pop3", "imap", "nntp"]),
                                          incoming.port) == null;

    // if username does not have an '@', also check the e-mail
    // address form of the name.
    if (isNew && incoming.username.indexOf("@") < 0) {
      return gAccountMgr.findRealServer(this._email,
                                        incoming.hostname,
                                        sanitize.enum(incoming.type,
                                                      ["pop3", "imap", "nntp"]),
                                        incoming.port) == null;
    }
    return isNew;
  },

  toggleAcknowledgeIncoming : function()
  {
    this._incomingWarningAcknowledged =
      getElementById('acknowledge_incoming').checked;
    this.checkEnableIKnow();
  },

  toggleAcknowledgeOutgoing : function()
  {
    this._outgoingWarningAcknowledged =
      getElementById('acknowledge_outgoing').checked;
    this.checkEnableIKnow();
  },

  checkEnableIKnow: function()
  {
    if ((!this._incomingWarning || this._incomingWarningAcknowledged) &&
        (!this._outgoingWarning || this._outgoingWarningAcknowledged))
      document.getElementById('iknow').disabled = false;
    else
      document.getElementById('iknow').disabled = true;
  },

  onOK : function()
  {
    try {

      gEmailWizardLogger.info("OK/Create button clicked");

      // we can't check if the account already exists here, because
      // we created it to test the password already.
      if (this._outgoingWarning || this._incomingWarning)
      {
        _hide('mastervbox');
        _show('warningbox');

        let incomingwarningstring;
        let outgoingwarningstring;
        let incoming = this._currentConfigFilledIn.incoming;
        let incoming_settings = incoming.hostname + ':' + incoming.port +
                                ' (' + sslLabel(incoming.socketType) + ')';
        let outgoing = this._currentConfigFilledIn.outgoing;
        let outgoing_settings = outgoing.hostname + ':' + outgoing.port +
                                ' (' + sslLabel(outgoing.socketType) + ')';
        switch (this._incomingWarning)
        {
          case 'cleartext':
            incomingwarningstring = gStringsBundle.getString("cleartext_incoming");
            setText('warning_incoming', incomingwarningstring);
            _show('incoming_box');
            setText('incoming_settings', incoming_settings);
            _show('acknowledge_incoming');
            break;
          case 'selfsigned':
            incomingwarningstring = gStringsBundle.getString("selfsigned_incoming");
            setText('warning_incoming', incomingwarningstring);
            setText('incoming_settings', incoming_settings);
            _show('incoming_box');
            _show('acknowledge_incoming');
            break;
          case '':
            _hide('incoming_box');
            _hide('acknowledge_incoming');
        }
        switch (this._outgoingWarning)
        {
          case 'cleartext':
            outgoingwarningstring = gStringsBundle.getString("cleartext_outgoing");
            setText('warning_outgoing', outgoingwarningstring);
            setText('outgoing_settings', outgoing_settings);
            _show('outgoing_box');
            _show('acknowledge_outgoing');
            break;
          case 'selfsigned':
            outgoingwarningstring = gStringsBundle.getString("selfsigned_outgoing");
            setText('warning_outgoing', outgoingwarningstring);
            setText('outgoing_settings', outgoing_settings);
            _show('outgoing_box');
            _show('acknowledge_outgoing');
            break;
          case '':
            _hide('outgoing_box');
            _hide('acknowledge_outgoing');
        }
      }
      else
      {
        // no certificate or cleartext issues
        this.validateAndFinish();
      }

    } catch (ex) { alertPrompt(gStringsBundle.getString("error_creating_account"), ex); }
  },

  getMeOutOfHere : function()
  {
    _hide('warningbox');
    _show('mastervbox');
    getElementById('acknowledge_incoming').checked = false;
    getElementById('acknowledge_outgoing').checked = false;
  },

  validateAndFinish : function()
  {
    // if we're coming from the cert warning dialog
    _show('mastervbox');
    _hide('warningbox');

    if (!this.checkIncomingAccountIsNew())
    {
      alertPrompt(gStringsBundle.getString("error_creating_account"),
                  gStringsBundle.getString("incoming_server_exists"));
      return;
    }
    // No need to check if we aren't adding it.
    if (this._currentConfigFilledIn.outgoing.addThisServer)
    {
      let existingKey = this.keyForExistingOutgoingAccount();
      if (existingKey)
      {
        this._currentConfigFilledIn.outgoing.addThisServer = false;
        this._currentConfigFilledIn.outgoing.existingServerKey = existingKey;
      }
    }

    getElementById('create_button').disabled = true;
    var me = this;
    if (!this._verifiedConfig)
      this.verifyConfig(function() // success
                        {
                          me.finish();
                        },
                        function(e) // failure
                        {
                          getElementById('create_button').disabled = false;
                        });
    else
      this.finish();
  },

  verifyConfig : function(successCallback, errorCallback)
  {
    var me = this;
    this.startSpinner("checking_password");
    // logic function defined in verifyConfig.js
    verifyConfig(
      this._currentConfigFilledIn,
      // guess login config?
      this._currentConfigFilledIn.source != AccountConfig.kSourceXML,
      this._parentMsgWindow,
      function() // success
      {
        me._verifiedConfig = true;
        me.stopSpinner("password_ok");
        if (successCallback)
          successCallback();
      },
      function(e) // failed
      {
        me.stopSpinner("config_not_verified");
        me.setError('passworderror', 'user_pass_invalid');
        alertPrompt(gStringsBundle.getString("error_creating_account"),
                    gStringsBundle.getString("config_not_verified"));
        if (errorCallback)
          errorCallback(e);
      });
  },

  finish : function()
  {
    gEmailWizardLogger.info("creating account in backend");
    this._currentConfigFilledIn.rememberPassword =
      getElementById("remember_password").checked;
    createAccountInBackend(this._currentConfigFilledIn);
    window.close();
  },

  advancedSettings : function()
  {
    let config = this._currentConfigFilledIn ? this._currentConfigFilledIn.copy()
                                             : this.getUserConfig();
    // call this to set the password
    replaceVariables(config, this._realname, this._email,
                     this._password, this._customFields);

    gEmailWizardLogger.info("creating account in backend");
    config.rememberPassword =
      getElementById("remember_password").checked;
    var newAccount = createAccountInBackend(config);
    var windowManager =
      Components.classes['@mozilla.org/appshell/window-mediator;1']
                .getService(Components.interfaces.nsIWindowMediator);

    var existingAccountManager =
      windowManager.getMostRecentWindow("mailnews:accountmanager");

    if (existingAccountManager)
      existingAccountManager.focus();
    else
      window.openDialog("chrome://messenger/content/AccountManager.xul",
                        "AccountManager", "chrome,centerscreen,modal,titlebar",
                        { server: newAccount.incomingServer,
                          selectPage: 'am-server.xul' });
    window.close();
  },
  /**
   * Gets the values that the user edited in the right of the dialog.
   */
  getUserConfig : function()
  {
    var config = this._currentConfig;
    if (!config)
      config = new AccountConfig();

    config.source = AccountConfig.kSourceGuess;

    // Did the user select one of the already configured SMTP servers from the
    // drop-down list? If so, use it.
    if (this._userPickedOutgoingServer)
    {
      config.outgoing.addThisServer = false;
      config.outgoing.existingServerKey =
        getElementById("outgoing_server").selectedItem.value;
      config.outgoing.existingServerLabel =
        getElementById("outgoing_server").selectedItem.label;
    }
    else
    {
      config.outgoing.username = getElementById("username").value;
      config.outgoing.hostname = getElementById("outgoing_server").value;
      config.outgoing.port =
        sanitize.integerRange(getElementById("outgoing_port").value, 1,
                              kHighestPort);
      config.outgoing.socketType = parseInt(getElementById("outgoing_security").value);
    }
    config.incoming.username = getElementById("username").value;
    config.incoming.hostname = getElementById("incoming_server").value;
    config.incoming.port =
      sanitize.integerRange(getElementById("incoming_port").value, 1,
                            kHighestPort);
    config.incoming.type =
      getElementById("incoming_protocol").value == 1 ? "imap" : "pop3";
    // type is a string, "imap" or "pop3", protocol is a protocol type.
    config.incoming.protocol = sanitize.translate(config.incoming.type, { "imap" : 0, "pop3" : 1});
    config.incoming.socketType = parseInt(getElementById("incoming_security").value);

    return config;
  },

  _setIncomingStatus : function (state, details)
  {
    if (!details)
      details = "";

    switch (state)
    {
      case 'strong':
        this._incomingState = 'done';
        this._incomingWarning = '';
        // fall through
      case 'weak':
        this._incomingWarning = details;
        this._incomingWarningAcknowledged = false;
        break;
      default:
        this._incomingState = state;
    }
    // since we look for SSL/TLS first, if we get 'weak', we're
    // stuck with it, and might as well admit we're done.
    if (state == 'weak')
      this._incomingState = 'done';

    this._setIconAndTooltip('incoming_status', state, details);
  },

  _setOutgoingStatus: function (state, details)
  {
    if (!details)
      details = '';

    if (this._userPickedOutgoingServer)
      return;

    switch (state)
    {
      case 'strong':
        this._outgoingState = 'done';
        this._outgoingWarning = '';
        // fall through
      case 'weak':
        this._outgoingWarning = details;
        this._outgoingWarningAcknowledged = false;
        break
      default:
        this._outgoingState = state;
    }
    // since we look for SSL/TLS first, if we get 'weak', we're
    // stuck with it, and might as well admit we're done.
    if (state == 'weak')
      this._outgoingState = 'done';

    this._setIconAndTooltip('outgoing_status', state, details);
  },

  _setIconAndTooltip : function(id, state, details)
  {
    let icon = getElementById(id);
    icon.setAttribute("state", state);
    switch (state)
    {
      case "weak":
        icon.setAttribute("tooltip", "insecureserver-" + details);
        icon.setAttribute("popup", "insecureserver-" + details + "-panel");
        break;
      case "hidden":
        icon.removeAttribute("tooltip");
        icon.removeAttribute("popup");
        break;
      case "strong":
        icon.setAttribute("tooltip", "secureservertooltip");
        icon.setAttribute("popup", "secureserver-panel");
        break;
    }
  },

  showConfigDetails : function()
  {
    // show the config details area
    _show("settingsbox");
    // also show the create button because it's outside of the area
    _show("create_button");
  },

  hideConfigDetails : function()
  {
    _hide("settingsbox");
    _hide("create_button");
  },

  /* Clears out the config details information, this is really only meant to be
   * called from the (Back) button.
   * XXX This can destroy user input where it might not be necessary
   */
  clearConfigDetails : function()
  {
    for (let i = 0; i < this._configDetailTextInputs.length; i++ )
    {
      getElementById(this._configDetailTextInputs[i]).value = "";
    }

    this._setIncomingStatus('hidden');
    this._setOutgoingStatus('hidden');
  },

  /* Setting the config details form so it can be edited.  We also disable
   * (and hide) the create button during this time because we don't know what
   * might have changed.  The function called from the button that restarts
   * the config check should be enabling the config button as needed.
   */
  editConfigDetails : function() {
    this._disableConfigDetails(false);
    this._setIncomingStatus('hidden');
    this._setOutgoingStatus('hidden');
    getElementById('create_button').disabled = true;
    _hide("create_button");
    _hide("stop_button");
    _hide("edit_button");
    _show("go_button");
  },

  /* This _doesn't_ set create back to enabled, that needs to be done in a
   * config check.  Only showing the button.
   * XXX However it seems that a disabled button can still receive click
   *     events so this needs to be looked into further.
   */
  goWithConfigDetails : function() {
    this._disableConfigDetails(true);
    _show("create_button");
  },

  // IDs of <textbox> inputs that can have a .value attr set
  _configDetailTextInputs : ["username", "incoming_server",
                             "incoming_port", "incoming_protocol",
                             "outgoing_port"],

  // IDs of the <menulist> elements that don't accept a .value attr but can
  // be disabled
  _configDetailMenulists : ["incoming_security",
                            "outgoing_server", "outgoing_security"],

  /* Helper function to loop through all config details form elements and
   * enable or disable each
   */
  _disableConfigDetails : function(disabled)
  {
    let formElements =
      this._configDetailTextInputs.concat(this._configDetailMenulists);
    for (let i = 0; i < formElements.length; i++)
    {
      getElementById(formElements[i]).disabled = disabled;
    }
  },

  /**
   * Updates a (probed) config to the user,
   * in the config details area
   *
   * @param config {AccountConfig} The config to present to user
   */
  updateConfig : function(config)
  {
    _show("advanced_settings");
    this._currentConfig = config;
    // if we have a username, set it.
    if (config.incoming.username)
    {
      getElementById("username").value = config.incoming.username;
    }
    else
    {
      // XXX needs more thought
    }

    // incoming server
    if (config.incoming.hostname && config.incoming.hostname != -1)
    {
      /* -1 failure needs to be handled in the context of the failure
        * at this point all we know is there is no hostname, but not why
        * the error handling behaviour needs to be done above
        */
      getElementById("incoming_server").value = config.incoming.hostname;
      getElementById("incoming_port").value = config.incoming.port;
      getElementById("incoming_security").value = config.incoming.socketType;
      getElementById("incoming_protocol").value =
        sanitize.translate(config.incoming.type, { "imap" : 1, "pop3" : 2});

      if (config.incoming._inprogress)
      {
        this._setIncomingStatus('probing');
      }
      else
      {
        switch (config.incoming.socketType)
        {
          case 2: // SSL / TLS
          case 3: // STARTTLS
            if (config.incoming.badCert)
            {
              this._setIncomingStatus('weak', 'selfsigned');
              var params = { exceptionAdded : false };
              params.prefetchCert = true;
              params.location = config.incoming.targetSite;
              window.openDialog('chrome://pippki/content/exceptionDialog.xul',
                                '','chrome,centerscreen,modal', params);
              config.incoming.badCert = false;
            }
            else
            {
              this._setIncomingStatus('strong');
            }
            break;
          case 1: // plain
            this._setIncomingStatus('weak', 'cleartext');
            break;
          default:
            throw new NotReached("sslType " + config.incoming.socketType + " unknown");
        }
      }
    }

    // outgoing server
    if (config.outgoing.hostname && config.outgoing.hostname != -1)
    {
      /* -1 failure needs to be handled in the context of the failure
       * at this point all we know is there is no hostname, but not why.
       * The error handling behaviour needs to be done elsewhere
       */
      if (!gEmailConfigWizard._userPickedOutgoingServer)
      {
        getElementById("outgoing_server").value = config.outgoing.hostname;
        getElementById("outgoing_port").value = config.outgoing.port;
        getElementById("outgoing_security").value = config.outgoing.socketType;
        _show("outgoing_port");
        _show("outgoing_security");
        if (config.outgoing._inprogress) {
          this._setOutgoingStatus('probing');
        }
        else
        {
          switch (config.outgoing.socketType)
          {
            case 2: // SSL / TLS
            case 3: // STARTTLS
              if (config.outgoing.badCert)
                this._setOutgoingStatus('weak', 'selfsigned');
              else
                this._setOutgoingStatus('strong');
              break;
            case 1: // plain
              this._setOutgoingStatus('weak');
              break;
            default:
              throw new NotReached("sslType " + config.incoming.socketType + " unknown");
          }
        }
      }
      else
      {
        config.outgoing.addThisServer = false;
        config.outgoing.useGlobalPreferredServer = true;
      }
    }
  },

  /* (Edit) button click handler.  This turns the config details area into an
   * editable form and makes the (Go) button appear.  The edit button should
   * only be available after the config probing is completely finished,
   * replacing what was the (Stop) button.
   */
  onEdit : function()
  {
    this.editConfigDetails();
  },

  // short hand so I didn't have to insert the swap functions in various places
  showEditButton : function() { _show("edit_button"); _hide("stop_button"); },

  /* (Stop) button click handler.  This should stop short any probing or config
   * guessing progress and changing the config details area into manual edit
   * mode.  This button should only be available during probing, after which it
   * is replaced by the (Edit) button.
   */
  onStop : function()
  {
    if (!this._probeAbortable)
    {
      gEmailWizardLogger.info("onStop without a _probeAbortable to cancel");
    }
    else
    {
      this._probeAbortable.cancel();
      gEmailWizardLogger.info("onStop cancelled _probeAbortable");
    }
    this.stopSpinner('manually_edit_config');
    this.editConfigDetails();
  },

  /* (Go) button click handler.  Restarts the config guessing process after a
   * person possibly editing the fields.  The go button replaces either the
   * (Stop) or (Edit) button after they have been clicked.
   */
  onGo : function()
  {
    // swap out go for stop button
    _hide("go_button");
    // the stop is naturally not hidden so has no place in the code where it is
    // told to be shown
    _show("stop_button");
    this.goWithConfigDetails();
    var newConfig = this.getUserConfig();
    if (this._incomingState != "done") {
      newConfig.incoming._inprogress = true;
      gEmailConfigWizard._startProbingIncoming(newConfig);
    }
    if (!this._userPickedOutgoingServer && this._outgoingState != "done") {
      newConfig.outgoing._inprogress = true;
      gEmailConfigWizard._startProbingOutgoing(newConfig);
    }
  },

  onCancel : function()
  {
    // The window onclose handler will call onWizardShutdown for us.
    window.close();
  },

  // UI helper functions

  startSpinner: function(actionStrName)
  {
    this._showStatusTitle(false, actionStrName);
    gEmailWizardLogger.warn("spinner start\n");
  },

  setSpinnerStatus : function(actionStrName)
  {
    this._showStatus(actionStrName);
  },

  stopSpinner: function(actionStrName)
  {
    this._showStatusTitle(true, actionStrName);
    gEmailWizardLogger.warn("spinner stop\n");
  },

  // thought this would be needed other places, not likely though
  _hideSpinner : function(hidden)
  {
    getElementById("config_spinner").hidden = hidden;
  },

  _showStatusTitle: function(success, msgName)
  {
    let msg;
    try {
      msg = msgName ? gStringsBundle.getString(msgName) : "";
    } catch(ex) {
      gEmailWizardLogger.error("missing string for " + msgName);
    }

    this._hideSpinner(success);
    let title = getElementById("config_status_title");
    title.hidden = false;
    title.textContent = msg;
    gEmailWizardLogger.info("show status title " + (success ? "success" : "failure") +
                            ", msg: " + msg);
  },

  _showStatus: function(msgName)
  {
    let msg;
    try {
      msg = msgName ? gStringsBundle.getString(msgName) : "";
    } catch(ex) {
      gEmailWizardLogger.error("missing string for " + msgName);
    }

    let subtitle = getElementById("config_status_subtitle");
    subtitle.hidden = false;
    subtitle.textContent = msg;
    gEmailWizardLogger.info("show status subtitle: " + msg);
  },

  _prefillConfig: function(initialConfig)
  {
    var emailsplit = this._email.split("@");
    if (emailsplit.length != 2)
      throw new Exception(gStringsBundle.getString("email.error"));

    var emaillocal = sanitize.nonemptystring(emailsplit[0]);
    initialConfig.incoming.username = emaillocal;
    initialConfig.outgoing.username = emaillocal;
    initialConfig.outgoing.protocol = 'smtp';
    return initialConfig;
  },

  _startProbingIncoming : function(config)
  {
    gEmailWizardLogger.info("_startProbingIncoming: " + config.incoming.hostname +
                            " probe = " + this._probeAbortable);
    this.startSpinner("searching_for_configs");
    this.setSpinnerStatus(this._outgoingState == "probing" ?
                          "check_server_details" : "check_in_server_details");

    config.incoming._inprogress = true;
    // User entered hostname, we may want to probe port and protocol and socketType
    if (!this._userChangedIncomingProtocol)
      config.incoming.protocol = undefined;
    if (!this._userChangedIncomingPort)
      config.incoming.port = undefined;
    if (!this._userChangedIncomingSocketType)
      config.incoming.socketType = undefined;

    if (this._probeAbortable)
    {
      gEmailWizardLogger.info("restarting probe: " + config.incoming.hostname);
      this._probeAbortable.restart(config.incoming.hostname, config, "incoming",
                                   config.incoming.protocol, config.incoming.port,
                                   config.incoming.socketType);
    }
    else
    {
      this._guessConfig(config.incoming.hostname, config, "incoming");
    }
  },

  _startProbingOutgoing : function(config)
  {
    gEmailWizardLogger.info("_startProbingOutgoing: " + config.outgoing.hostname +
                            " probe = " + this._probeAbortable);
    this.startSpinner("searching_for_configs");
    this.setSpinnerStatus(this._incomingState == "probing" ?
                          "check_server_details" : "check_out_server_details");

    config.outgoing._inprogress = true;
    // User entered hostname, we want to probe port and protocol and socketType
    if (!this._userChangedOutgoingPort)
      config.outgoing.port = undefined;
    if (!this.userChangedOutgoingSocketType)
      config.outgoing.socketType = undefined;

    if (this._probeAbortable)
    {
      gEmailWizardLogger.info("restarting probe: " + config.outgoing.hostname);
      this._probeAbortable.restart(config.outgoing.hostname, config, "outgoing",
                                   "smtp", config.outgoing.port);
    }
    else
    {
      this._guessConfig(config.outgoing.hostname, config, "outgoing");
    }
  },

  clearError: function(which) {
    _hide(which);
    getElementById(which).textContent = "";
  },

  setError: function(which, msg_name) {
    try {
      _show(which);
      getElementById(which).textContent = gStringsBundle.getString(msg_name);
    }
    catch(ex) {
      alertPrompt("missing error string", msg_name);
    }
  },

  onKeyDown : function (key)
  {
    if (key == 27)
      this.onCancel();
    else if (key == 13 && !getElementById('create_button').disabled)
      this.onOK();
  },

  onWizardShutdown: function EmailConfigWizard_onWizardshutdown() {
    if (this._probeAbortable)
      this._probeAbortable.cancel();

    gEmailWizardLogger.info("Shutting down email config dialog");
  }
};

var gEmailConfigWizard = new EmailConfigWizard();
gEmailWizardLogger.info("email account setup dialog");

function sslLabel(val)
{
  switch (val)
  {
    case 1:
      return gStringsBundle.getString("no_encryption");
    case 2:
      return gStringsBundle.getString("ssltls");
    case 3:
      return gStringsBundle.getString("starttls");;
    default:
      throw new NotReached("Unknown SSL type");
  }
}


function setText(id, value)
{
  var element = document.getElementById(id);
  if (!element)
    return;

  if (element.localName == "textbox" || element.localName == "label")
    element.value = value;
  else if (element.localName == "description")
    element.textContent = value;
  else
    throw new NotReached("XUL element type not supported");
}


/**
* Called by dialog logic, if there are custom fields needed
* The dialog just shows all these descriptions, and a textfield next to each,
* and returns the values the user entered.
*
* @param inputFields {Array}   @see AccountConfig.inputFields
* @param defaults {Object}
* Associative array / map of
* field ID (from config file) -> default value in text field
* The default values can come from a previous invokation of the dialog.
* May be null.
*/
function CustomFieldsDialog(inputFields, defaults)
{
  if (!defaults)
    defaults = {};

  this._inputFields = inputFields;
  this._defaults = defaults;
}
CustomFieldsDialog.prototype =
{
  /**
   * Open dialog, unless the needed data is already in |defaults|.
   * @param successCallback, cancelCallback @see open()
   */
  openIfNeeded : function(successCallback, cancelCallback)
  {
    var needInput = false;
    for (var i = 0; i < this._inputFields.length; i++)
    {
      let fieldid = this._inputFields[i].varname;
      if (!this._defaults[fieldid])
        needInput = true;
    }
    if (!needInput)
    {
      successCallback(this._defaults);
      return;
    }

    this.open(successCallback, cancelCallback);
  },

  /**
   * @param successCallback {Function({Object})}
   * Will be called when the user entered all values and clicked OK.
   * The first parameter contains the values that the user entered,
   * in form Map of: field ID -> user value
   *
   * @param cancelCallback {Function()}
   * The user cancelled the dialog.
   */
  open : function(successCallback, cancelCallback)
  {
    this._successCallback = successCallback;
    this._cancelCallback = cancelCallback;

    var rows = getElementById("customfields-rows");

    // first, clear dialog from any possible previous use
    while (rows.hasChildNodes())
      rows.removeChild(rows.firstChild);

    for (var i = 0; i < this._inputFields.length; i++)
    {
      let fieldid = this._inputFields[i].varname;
      let displayName = this._inputFields[i].displayName;
      let exampleValue = this._inputFields[i].exampleValue;

      // only 5 fields allowed per spec, to cut down on dialog size and preserve sanity
      if (i >= 5)
        throw new Exception(gStringsBundle.getString("customfields_tooMany.error"));

      // Create UI widgets
      let row = document.createElement("row");
      let descr = document.createElement("description");
      let textfield = document.createElement("textbox");
      let exHBox = document.createElement("hbox");
      let exLabel = document.createElement("label");
      let example = document.createElement("label");
      exHBox.appendChild(exLabel);
      exHBox.appendChild(example);
      row.appendChild(descr);
      row.appendChild(textfield);
      row.appendChild(exHBox);
      rows.appendChild(row);
      descr.setAttribute("class", "customfield-label");
      textfield.setAttribute("class", "customfield-value");
      example.setAttribute("class", "customfield-example");
      // fieldid was already sanitized in readFrom XML.js as alphanumdash and uppercase
      textfield.id = "customfield-value-" + fieldid;
      exLabel.setAttribute("value", gStringsBundle.getString("customfields_example.label"));

      // Set labels and values
      descr.textContent = displayName;
      example.setAttribute("value", exampleValue);
      if (this._defaults && this._defaults[fieldid])
        textfield.setAttribute("value", this._defaults[fieldid]);
    }

    _hide("mastervbox");
    _show("customfields-box");
  },

  // UI button pressed
  onCancel : function()
  {
    _show("mastervbox");
    _hide("customfields-box");
    gCustomFieldsDialog = null;
    try {
      this._cancelCallback();
    } catch (e) {
      // XXX TODO FIXME
      alert(e.message); throw e;
    }
  },

  // UI button pressed
  onOK : function()
  {
    try {
      var result = {};
      for (var i = 0; i < this._inputFields.length; i++)
      {
        let fieldid = this._inputFields[i].varname;
        result[fieldid] = getElementById("customfield-value-" + fieldid).value;

        gEmailWizardLogger.info("User value for " + fieldid + " is " + result[fieldid]);
        if (!result[fieldid])
        {
          getElementById("customfields-error").textContent =
            gStringsBundle.getString("customfields_empty.error");
          return;
        }
      }

      _show("mastervbox");
      _hide("customfields-box");
      gCustomFieldsDialog = null;

      this._successCallback(result);

    } catch (e) { alert(e.message); throw e; } // TODO alertPrompt()
  }
}

var gCustomFieldsDialog = null;
