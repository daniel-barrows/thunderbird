/* ***** BEGIN LICENSE BLOCK *****
 *   Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is Thunderbird Global Database.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Messaging, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andrew Sutherland <asutherland@asutherland.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 * A list of first names for use by MessageGenerator to create deterministic,
 *  reversible names.  To keep things easily reversible, if you add names, make
 *  sure they have no spaces in them!
 */
const FIRST_NAMES = [
  "Andy", "Bob", "Chris", "David", "Emily", "Felix",
  "Gillian", "Helen", "Idina", "Johnny", "Kate", "Lilia",
  "Martin", "Neil", "Olof", "Pete", "Quinn", "Rasmus",
  "Sarah", "Troels", "Ulf", "Vince", "Will", "Xavier",
  "Yoko", "Zig"
  ];

/**
 * A list of last names for use by MessageGenerator to create deterministic,
 *  reversible names.  To keep things easily reversible, if you add names, make
 *  sure they have no spaces in them!
 */
const LAST_NAMES = [
  "Anway", "Bell", "Clarke", "Davol", "Ekberg", "Flowers",
  "Gilbert", "Hook", "Ivarsson", "Jones", "Kurtz", "Lowe",
  "Morris", "Nagel", "Orzabal", "Price", "Quinn", "Rolinski",
  "Stanley", "Tennant", "Ulvaeus", "Vannucci", "Wiggs", "Xavier",
  "Young", "Zig"
  ];

/**
 * A list of adjectives used to construct a deterministic, reversible subject
 *  by MessageGenerator.  To keep things easily reversible, if you add more,
 *  make sure they have no spaces in them!  Also, make sure your additions
 *  don't break the secret Monty Python reference!
 */
const SUBJECT_ADJECTIVES = [
  "Big", "Small", "Huge", "Tiny",
  "Red", "Green", "Blue", "My",
  "Happy", "Sad", "Grumpy", "Angry",
  "Awesome", "Fun", "Lame", "Funky",
  ];

/**
 * A list of nouns used to construct a deterministic, reversible subject
 *  by MessageGenerator.  To keep things easily reversible, if you add more,
 *  make sure they have no spaces in them!  Also, make sure your additions
 *  don't break the secret Monty Python reference!
 */
const SUBJECT_NOUNS = [
  "Meeting", "Party", "Shindig", "Wedding",
  "Document", "Report", "Spreadsheet", "Hovercraft",
  "Aardvark", "Giraffe", "Llama", "Velociraptor",
  "Laser", "Ray-Gun", "Pen", "Sword",
  ];

/**
 * A list of suffixes used to construct a deterministic, reversible subject
 *  by MessageGenerator.  These can (clearly) have spaces in them.  Make sure
 *  your additions don't break the secret Monty Python reference!
 */
const SUBJECT_SUFFIXES = [
  "Today", "Tomorrow", "Yesterday", "In a Fortnight",
  "Needs Attention", "Very Important", "Highest Priority", "Full Of Eels",
  "In The Lobby", "On Your Desk", "In Your Car", "Hiding Behind The Door",
  ];

/**
 * Base class for MIME Part representation.
 */
function SyntheticPart(aProperties) {
  if (aProperties) {
    if (aProperties.charset)
      this._charset = aProperties.charset;
    if (aProperties.format)
      this._format = aProperties.format;
    if (aProperties.filename)
      this._filename = aProperties.filename;
    if (aProperties.boundary)
      this._boundary = aProperties.boundary;
  }
}
SyntheticPart.prototype = {
  get contentTypeHeaderValue() {
    s = this._contentType;
    if (this._charset)
      s += '; charset=' + this._charset;
    if (this._format)
      s += '; format=' + this._format;
    if (this._filename)
      s += ';\r\n name="' + this._filename +'"'
    if (this._boundary)
      s += ';\r\n boundary="' + this._boundary + '"';
    return s;
  },
  get hasTransferEncoding() {
    return this._encoding;
  },
  get contentTransferEncodingHeaderValue() {
    return this._encoding;
  },
  get hasDisposition() {
    return this._filename;
  },
  get contentDispositionHeaderValue() {
    s = '';
    if (this._filename)
      s += 'attachment;\r\n filename="' + this._filename + '"';
    return s;
  },
};

/**
 * Leaf MIME part, defaulting to text/plain.
 */
function SyntheticPartLeaf(aBody, aProperties) {
  SyntheticPart.call(this, aProperties);
  this.body = aBody;
}
SyntheticPartLeaf.prototype = {
  __proto__: SyntheticPart.prototype,
  _contentType: 'text/plain',
  _charset: 'ISO-8859-1',
  _format: 'flowed',
  _encoding: '7bit',
  toMessageString: function() {
    return this.body;
  }
}

/**
 * Multipart (multipart/*) MIME part base class.
 */
function SyntheticPartMulti(aParts, aProperties) {
  SyntheticPart.call(this, aProperties);

  this._boundary = '--------------CHOPCHOP' + this.BOUNDARY_COUNTER;
  this.__proto__.BOUNDARY_COUNTER += 1;
  this.parts = (aParts != null) ? aParts : [];
}
SyntheticPartMulti.prototype = {
  __proto__: SyntheticPart.prototype,
  BOUNDARY_COUNTER: 0,
  toMessageString: function() {
    s = "This is a multi-part message in MIME format.\r\n";
    for (let [,part] in Iterator(this.parts)) {
      s += "--" + this._boundary + "\r\n";
      s += "Content-Type: " + part.contentTypeHeaderValue + '\r\n';
      if (part.hasTransferEncoding)
        s += 'Content-Transfer-Encoding: ' +
             part.contentTransferEncodingHeaderValue + '\r\n';
      if (part.hasDisposition)
        s += 'Content-Disposition: ' + part.contentDispositionHeaderValue +
             '\r\n';
      s += '\r\n';
      s += part.toMessageString() + '\r\n\r\n';
    }
    s += "--" + this._boundary + '--';
    return s;
  },
};

/**
 * Multipart mixed (multipart/mixed) MIME part.
 */
function SyntheticPartMultiMixed() {
  SyntheticPartMulti.apply(this, arguments);
}
SyntheticPartMultiMixed.prototype = {
  __proto__: SyntheticPartMulti.prototype,
  _contentType: 'multipart/mixed',
}

/**
 * A synthetic message, created by the MessageGenerator.  Captures both the
 *  ingredients that went into the synthetic message as well as the rfc822 form
 *  of the message.
 */
function SyntheticMessage(aHeaders, aBodyPart) {
  this.headers = aHeaders || {};
  this.bodyPart = aBodyPart || new SyntheticPartLeaf("");
}

SyntheticMessage.prototype = {
  /** @returns the Message-Id header value. */
  get messageId() { return this._messageId; },
  /**
   * Sets the Message-Id header value.
   *
   * @param aMessageId A unique string without the greater-than and less-than,
   *     we add those for you.
   */
  set messageId(aMessageId) {
    this._messageId = aMessageId;
    this.headers["Message-Id"] = "<" + aMessageId + ">";
  },

  /** @returns the message Date header value. */
  get date() { return this._date; },
  /**
   * Sets the Date header to the given javascript Date object.
   *
   * @param aDate The date you want the message to claim to be from.
   */
  set date(aDate) {
    this._date = aDate;
    let dateParts = aDate.toString().split(" ");
    this.headers["Date"] = dateParts[0] + ", " + dateParts[2] + " " +
                           dateParts[1] + " " + dateParts[3] + " " +
                           dateParts[4] + " " + dateParts[5].substring(3);
  },

  /** @returns the message subject */
  get subject() { return this._subject; },
  /**
   * Sets the message subject.
   *
   * @param aSubject A string sans newlines or other illegal characters.
   */
  set subject(aSubject) {
    this._subject = aSubject;
    this.headers["Subject"] = aSubject;
  },

  /**
   * Given a tuple containing [a display name, an e-mail address], returns a
   *  string suitable for use in a to/from/cc header line.
   *
   * @param aNameAndAddress A list with two elements.  The first should be the
   *     display name (sans wrapping quotes).  The second element should be the
   *     e-mail address (sans wrapping greater-than/less-than).
   */
  _formatMailFromNameAndAddress: function(aNameAndAddress) {
    return '"' + aNameAndAddress[0] + '" ' +
           '<' + aNameAndAddress[1] + '>';
  },

  /** @returns the name-and-address tuple used when setting the From header. */
  get from() { return this._from; },
  /**
   * Sets the From header using the given tuple containing [a display name,
   *  an e-mail address].
   *
   * @param aNameAndAddress A list with two elements.  The first should be the
   *     display name (sans wrapping quotes).  The second element should be the
   *     e-mail address (sans wrapping greater-than/less-than).
   */
  set from(aNameAndAddress) {
    this._from = aNameAndAddress;
    this.headers["From"] = this._formatMailFromNameAndAddress(aNameAndAddress);
  },

  /** @returns The display name part of the From header. */
  get fromName() { return this._from[0]; },
  /** @returns The e-mail address part of the From header (no display name). */
  get fromAddress() { return this._from[1]; },

  /**
   * For our header storage, we may need to pre-add commas, this does it.
   *
   * @param aList A list of strings that is mutated so that every string in the
   *     list except the last one has a comma appended to it.
   */
  _commaize: function(aList) {
    for (let i=0; i < aList.length - 1; i++)
      aList[i] = aList[i] + ",";
    return aList;
  },

  /**
   * @returns the comma-ized list of name-and-address tuples used to set the To
   *     header.
   */
  get to() { return this._to; },
  /**
   * Sets the To header using a list of tuples containing [a display name,
   *  an e-mail address].
   *
   * @param aNameAndAddress A list of name-and-address tuples.  Each tuple is a
   *     list with two elements.  The first should be the
   *     display name (sans wrapping quotes).  The second element should be the
   *     e-mail address (sans wrapping greater-than/less-than).
   */
  set to(aNameAndAddresses) {
    this._to = aNameAndAddresses;
    this.headers["To"] = this._commaize(
                           [this._formatMailFromNameAndAddress(nameAndAddr)
                            for each (nameAndAddr in aNameAndAddresses)]);
  },
  /** @returns The display name of the first intended recipient. */
  get toName() { return this._to[0][0]; },
  /** @returns The email address (no display name) of the first recipient. */
  get toAddress() { return this._to[0][1]; },

  /**
   * @returns The comma-ized list of name-and-address tuples used to set the Cc
   *     header.
   */
  get cc() { return this._cc; },
  /**
   * Sets the Cc header using a list of tuples containing [a display name,
   *  an e-mail address].
   *
   * @param aNameAndAddress A list of name-and-address tuples.  Each tuple is a
   *     list with two elements.  The first should be the
   *     display name (sans wrapping quotes).  The second element should be the
   *     e-mail address (sans wrapping greater-than/less-than).
   */
  set cc(aNameAndAddresses) {
    this._cc = aNameAndAddresses;
    this.headers["Cc"] = this._commaize(
                           [this._formatMailFromNameAndAddress(nameAndAddr)
                            for each (nameAndAddr in aNameAndAddresses)]);
  },

  get bodyPart() {
    return this._bodyPart;
  },
  set bodyPart(aBodyPart) {
    this._bodyPart = aBodyPart;
    this.headers["Content-Type"] = this._bodyPart.contentTypeHeaderValue;
  },

  /**
   * Normalizes header values, which may be strings or arrays of strings, into
   *  a suitable string suitable for appending to the header name/key.
   *
   * @returns a normalized string representation of the header value(s), which
   *     may include spanning multiple lines.
   */
  _formatHeaderValues: function(aHeaderValues) {
    // may not be an array
    if (!(aHeaderValues instanceof Array))
      return aHeaderValues;
    // it's an array!
    if (aHeaderValues.length == 1)
      return aHeaderValues[0];
    return aHeaderValues.join("\r\n\t");
  },

  /**
   * @returns a string uniquely identifying this message, at least as long as
   *     the messageId is set and unique.
   */
  toString: function() {
    return "msg:" + this._messageId;
  },

  /**
   * @returns this messages in rfc822 format, or something close enough.
   */
  toMessageString: function() {
    let lines = [headerKey + ": " + this._formatHeaderValues(headerValues)
                 for each ([headerKey, headerValues] in Iterator(this.headers))];

    return lines.join("\r\n") + "\r\n\r\n" + this.bodyPart.toMessageString() +
      "\r\n";
  },

  /**
   * @returns this message in rfc822 format in a string stream.
   */
  toStream: function () {
    let stream = Cc["@mozilla.org/io/string-input-stream;1"]
                   .createInstance(Ci.nsIStringInputStream);
    let str = this.toMessageString();
    stream.setData(str, str.length);
    return stream;
  },

  /**
   * Writes this message to an mbox stream.  This means adding a "From " line
   *  and making sure we've got a trailing newline.
   */
  writeToMboxStream: function (aStream) {
    let str = "From " + this._from[1] + "\r\n" + this.toMessageString() +
      "\r\n";
    aStream.write(str, str.length);
  }
}

/**
 * Write a list of messages in mbox format to a file
 *
 * @param aMessages The list of SyntheticMessages instances to write.
 * @param aBaseDir The base directory to start from.
 * @param ... Zero or more relative paths to join to the base directory.
 */
function writeMessagesToMbox (aMessages, aBaseDir) {
  try {
    let targetFile = Cc["@mozilla.org/file/local;1"]
                       .createInstance(Ci.nsILocalFile);
    targetFile.initWithFile(aBaseDir);
    for (let iArg = 2; iArg < arguments.length; iArg++)
      targetFile.appendRelativePath(arguments[iArg]);

    let ostream = Cc["@mozilla.org/network/file-output-stream;1"]
                    .createInstance(Ci.nsIFileOutputStream);
    ostream.init(targetFile, -1, -1, 0);

    for (let iMessage = 0; iMessage < aMessages.length; iMessage++) {
      aMessages[iMessage].writeToMboxStream(ostream);
    }

    ostream.close();
  }
  catch (ex) {
    do_throw(ex);
  }
}

/**
 * Provides mechanisms for creating vaguely interesting, but at least valid,
 *  SyntheticMessage instances.
 */
function MessageGenerator() {
  this._clock = new Date(2000, 1, 1);
  this._nextNameNumber = 0;
  this._nextSubjectNumber = 0;
  this._nextMessageIdNum = 0;
}

MessageGenerator.prototype = {
  /**
   * The maximum number of unique names makeName can produce.
   */
  MAX_VALID_NAMES: FIRST_NAMES.length * LAST_NAMES.length,
  /**
   * The maximum number of unique e-mail address makeMailAddress can produce.
   */
  MAX_VALID_MAIL_ADDRESSES: FIRST_NAMES.length * LAST_NAMES.length,
  /**
   * The maximum number of unique subjects makeSubject can produce.
   */
  MAX_VALID_SUBJECTS: SUBJECT_ADJECTIVES.length * SUBJECT_NOUNS.length *
                      SUBJECT_SUFFIXES,

  /**
   * Generate a consistently determined (and reversible) name from a unique
   *  value.  Currently up to 26*26 unique names can be generated, which
   *  should be sufficient for testing purposes, but if your code cares, check
   *  against MAX_VALID_NAMES.
   *
   * @param aNameNumber The 'number' of the name you want which must be less
   *     than MAX_VALID_NAMES.
   * @returns The unique name corresponding to the name number.
   */
  makeName: function(aNameNumber) {
    let iFirst = aNameNumber % FIRST_NAMES.length;
    let iLast = (iFirst + Math.floor(aNameNumber / FIRST_NAMES.length)) %
                LAST_NAMES.length;

    return FIRST_NAMES[iFirst] + " " + LAST_NAMES[iLast];
  },

  /**
   * Generate a consistently determined (and reversible) e-mail address from
   *  a unique value; intended to work in parallel with makeName.  Currently
   *  up to 26*26 unique addresses can be generated, but if your code cares,
   *  check against MAX_VALID_MAIL_ADDRESSES.
   *
   * @param aNameNumber The 'number' of the mail address you want which must be
   *     less than MAX_VALID_MAIL_ADDRESSES.
   * @returns The unique name corresponding to the name mail address.
   */
  makeMailAddress: function(aNameNumber) {
    let iFirst = aNameNumber % FIRST_NAMES.length;
    let iLast = (iFirst + Math.floor(aNameNumber / FIRST_NAMES.length)) %
                LAST_NAMES.length;

    return FIRST_NAMES[iFirst].toLowerCase() + "@" +
           LAST_NAMES[iLast].toLowerCase() + ".nul";
  },

  /**
   * Generate a pair of name and e-mail address.
   *
   * @param aNameNumber The optional 'number' of the name and mail address you
   *     want.  If you do not provide a value, we will increment an internal
   *     counter to ensure that a new name is allocated and that will not be
   *     re-used.  If you use our automatic number once, you must use it always,
   *     unless you don't mind or can ensure no collisions occur between our
   *     number allocation and your uses.  If provided, the number must be
   *     less than MAX_VALID_NAMES.
   * @return A list containing two elements.  The first is a name produced by
   *     a call to makeName, and the second an e-mail address produced by a
   *     call to makeMailAddress.  This representation is used by the
   *     SyntheticMessage class when dealing with names and addresses.
   */
  makeNameAndAddress: function(aNameNumber) {
    if (aNameNumber === undefined)
      aNameNumber = this._nextNameNumber++;
    return [this.makeName(aNameNumber), this.makeMailAddress(aNameNumber)];
  },

  /**
   * Generate and return multiple pairs of names and e-mail addresses.  The
   *  names are allocated using the automatic mechanism as documented on
   *  makeNameAndAddress.  You should accordingly not allocate / hard code name
   *  numbers on your own.
   *
   * @param aCount The number of people you want name and address tuples for.
   * @returns a list of aCount name-and-address tuples.
   */
  makeNamesAndAddresses: function(aCount) {
    let namesAndAddresses = [];
    for (let i=0; i < aCount; i++)
      namesAndAddresses.push(this.makeNameAndAddress());
    return namesAndAddresses;
  },

  /**
   * Generate a consistently determined (and reversible) subject from a unique
   *  value.  Up to MAX_VALID_SUBJECTS can be produced.
   *
   * @param aSubjectNumber The subject number you want generated, must be less
   *     than MAX_VALID_SUBJECTS.
   * @returns The subject corresponding to the given subject number.
   */
  makeSubject: function(aSubjectNumber) {
    if (aSubjectNumber === undefined)
      aSubjectNumber = this._nextSubjectNumber++;
    let iAdjective = aSubjectNumber % SUBJECT_ADJECTIVES.length;
    let iNoun = (iAdjective + Math.floor(aSubjectNumber /
                                         SUBJECT_ADJECTIVES.length)) %
                SUBJECT_NOUNS.length;
    let iSuffix = (iNoun + Math.floor(aSubjectNumber /
                   (SUBJECT_ADJECTIVES.length * SUBJECT_NOUNS.length))) %
                  SUBJECT_SUFFIXES.length;
    return SUBJECT_ADJECTIVES[iAdjective] + " " +
           SUBJECT_NOUNS[iNoun] + " " +
           SUBJECT_SUFFIXES[iSuffix];
  },

  /**
   * Fabricate a message-id suitable for the given synthetic message.  Although
   *  we don't use the message yet, in theory it would let us tailor the
   *  message id to the server that theoretically might be sending it.  Or some
   *  such.
   *
   * @param The synthetic message you would like us to make up a message-id for.
   *     We don't set the message-id on the message, that's up to you.
   * @returns a Message-id suitable for the given message.
   */
  makeMessageId: function(aSynthMessage) {
    let msgId = this._nextMessageIdNum + "@made.up";
    this._nextMessageIdNum++;
    return msgId;
  },

  /**
   * Generates a valid date which is after all previously issued dates by this
   *  method, ensuring an apparent ordering of time consistent with the order
   *  in which code is executed / messages are generated.
   * If you need a precise time ordering or precise times, make them up
   *  yourself.
   *
   * @returns A made-up time in JavaScript Date object form.
   */
  makeDate: function() {
    let date = this._clock;
    // advance time by an hour
    this._clock = new Date(date.valueOf() + 60 * 60 * 1000);
    return date;
  },

  /**
   * Create a SyntheticMessage.  All arguments are optional, but allow
   *  additional control.  With no arguments specified, a new name/address will
   *  be generated that has not been used before, and sent to a new name/address
   *  that has not been used before.
   *
   * @param aArgs An object with any of the following attributes provided:
   *     attachments: A list of dictionaries suitable for passing to
   *         syntheticPartLeaf, plus a 'body' attribute that has already been
   *         encoded.  Line chopping is on you FOR NOW.
   *     body: A dictionary suitable for passing to SyntheticPart plus a 'body'
   *         attribute that has already been encoded (if encoding is required).
   *         Line chopping is on you FOR NOW.
   *     callerData: A value to propagate to the callerData attribute on the
   *         resulting message.
   *     inReplyTo: the SyntheticMessage this message should be in reply-to.
   *         If that message was in reply to another message, we will
   *         appropriately compensate for that.
   *     replyAll: a boolean indicating whether this should be a reply-to-all or
   *         just to the author of the message.  (er, to-only, not cc.)
   *     subject: The subject to use; you are responsible for doing any encoding
   *         before passing it in.
   *     toCount: the number of people who the message should be to.
   * @returns a SyntheticMessage fashioned just to your liking.
   */
  makeMessage: function(aArgs) {
    aArgs = aArgs || {};
    let msg = new SyntheticMessage();

    if (aArgs.inReplyTo) {
      msg.parent = aArgs.inReplyTo;
      msg.parent.children.push(msg);

      let srcMsg = aArgs.inReplyTo;

      msg.subject = (srcMsg.subject.substring(0, 4) == "Re: ") ? srcMsg.subject
                    : ("Re: " + srcMsg.subject);
      if (aArgs.replyAll)
        msg.to = [srcMsg.from].concat(srcMsg.to.slice(1));
      else
        msg.to = [srcMsg.from];
      msg.from = srcMsg.to[0];

      // we want the <>'s.
      msg.headers["In-Reply-To"] = srcMsg.headers["Message-Id"];
      msg.headers["References"] = (srcMsg.headers["References"] || []).concat(
                                   [srcMsg.headers["Message-Id"]]);
    }
    else {
      msg.parent = null;

      msg.subject = aArgs.subject || this.makeSubject();
      msg.from = this.makeNameAndAddress();
      msg.to = this.makeNamesAndAddresses(aArgs.toCount || 1);
    }

    msg.children = [];
    msg.messageId = this.makeMessageId(msg);
    msg.date = this.makeDate();

    let bodyPart;
    if (aArgs.bodyPart)
      bodyPart = aArgs.bodyPart;
    else if (aArgs.body)
      bodyPart = new SyntheticPartLeaf(aArgs.body.body, aArgs.body);
    else
      bodyPart = new SyntheticPartLeaf("I am an e-mail.");

    // if it has any attachments, create a multipart/mixed to be the body and
    //  have it be the parent of the existing body and all the attachments
    if (aArgs.attachments) {
      let parts = [bodyPart];
      for each (let [,attachDesc] in Iterator(aArgs.attachments))
        parts.push(new SyntheticPartLeaf(attachDesc.body, attachDesc));
      bodyPart = new SyntheticPartMultiMixed(parts);
    }

    msg.bodyPart = bodyPart;

    msg.callerData = aArgs.callerData;

    return msg;
  }
}

/**
 * Repository of generative message scenarios.  Uses the magic bindMethods
 *  function below to allow you to reference methods/attributes without worrying
 *  about how those methods will get the right 'this' pointer if passed as
 *  simply a function argument to someone.  So if you do:
 *  foo = messageScenarioFactory.method, followed by foo(...), it will be
 *  equivalent to having simply called messageScenarioFactory.method(...).
 *  (Normally this would not be the case when using JavaScript.)
 *
 * @param aMessageGenerator The optional message generator we should use.
 *     If you don't pass one, we create our own.  You would want to pass one so
 *     that if you also create synthetic messages directly via the message
 *     generator then the two sources can avoid duplicate use of the same
 *     names/addresses/subjects/message-ids.
 */
function MessageScenarioFactory(aMessageGenerator) {
  if(!aMessageGenerator)
    aMessageGenerator = new MessageGenerator();
  this._msgGen = aMessageGenerator;
}

MessageScenarioFactory.prototype = {
  /** Create a chain of direct-reply messages of the given length. */
  directReply: function(aNumMessages) {
    aNumMessages = aNumMessages || 2;
    let messages = [this._msgGen.makeMessage()];
    for (let i = 1; i < aNumMessages; i++) {
      messages.push(this._msgGen.makeMessage({inReplyTo: messages[i-1]}));
    }
    return messages;
  },

  /** Two siblings (present), one parent (missing). */
  siblingsMissingParent: function() {
    let missingParent = this._msgGen.makeMessage();
    let msg1 = this._msgGen.makeMessage({inReplyTo: missingParent});
    let msg2 = this._msgGen.makeMessage({inReplyTo: missingParent});
    return [msg1, msg2];
  },

  /** Present parent, missing child, present grand-child. */
  missingIntermediary: function() {
    let msg1 = this._msgGen.makeMessage();
    let msg2 = this._msgGen.makeMessage({inReplyTo: msg1});
    let msg3 = this._msgGen.makeMessage({inReplyTo: msg2});
    return [msg1, msg3];
  },

  /**
   * The root message and all non-leaf nodes have aChildrenPerParent children,
   *  for a total of aHeight layers.  (If aHeight is 1, we have just the root;
   *  if aHeight is 2, the root and his aChildrePerParent children.)
   */
  fullPyramid: function(aChildrenPerParent, aHeight) {
    let msgGen = this._msgGen;
    let root = msgGen.makeMessage();
    let messages = [root];
    function helper(aParent, aRemDepth) {
      for (let iChild = 0; iChild < aChildrenPerParent; iChild++) {
        let child = msgGen.makeMessage({inReplyTo: aParent});
        messages.push(child);
        if (aRemDepth)
          helper(child, aRemDepth - 1);
      }
    }
    if (aHeight > 1)
      helper(root, aHeight - 2);
    return messages;
  }
}

/**
 * Decorate the given object's methods will python-style method binding.  We
 *  create a getter that returns a method that wraps the call, providing the
 *  actual method with the 'this' of the object that was 'this' when the getter
 *  was called.
 * Note that we don't follow the prototype chain; we only process the object you
 *  immediately pass to us.  This does not pose a problem for the 'this' magic
 *  because we are using a getter and 'this' in js always refers to the object
 *  in question (never any part of its prototype chain).  As such, you probably
 *  want to invoke us on your prototype object(s).
 *
 * @param The object on whom we want to perform magic binding.  This should
 *     probably be your prototype object.
 */
function bindMethods(aObj) {
  for (let [name, ubfunc] in Iterator(aObj)) {
    // the variable binding needs to get captured...
    let realFunc = ubfunc;
    let getterFunc = function() {
      // 'this' is magic and not from the enclosing scope.  we are assuming the
      //  getter will receive a valid 'this', and so
      let realThis = this;
      return function() { return realFunc.apply(realThis, arguments); };
    }
    delete aObj[name];
    aObj.__defineGetter__(name, getterFunc);
  }
}

bindMethods(MessageScenarioFactory.prototype);
