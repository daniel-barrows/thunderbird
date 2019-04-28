/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["GlodaABIndexer", "GlodaABAttrs"];

const {GlodaCollectionManager} = ChromeUtils.import("resource:///modules/gloda/collection.js");
const {GlodaDatastore} = ChromeUtils.import("resource:///modules/gloda/datastore.js");
const {Gloda} = ChromeUtils.import("resource:///modules/gloda/gloda.js");
const {GlodaIndexer, IndexingJob} = ChromeUtils.import("resource:///modules/gloda/indexer.js");
const {Log4Moz} = ChromeUtils.import("resource:///modules/gloda/log4moz.js");
const {FreeTagNoun} = ChromeUtils.import("resource:///modules/gloda/noun_freetag.js");
const {GlodaUtils} = ChromeUtils.import("resource:///modules/gloda/utils.js");
const {MailServices} = ChromeUtils.import("resource:///modules/MailServices.jsm");


var GlodaABIndexer = {
  _log: null,

  name: "index_ab",
  enable() {
    if (this._log == null)
      this._log =  Log4Moz.repository.getLogger("gloda.index_ab");

    MailServices.ab.addAddressBookListener(this,
                                           Ci.nsIAbListener.itemAdded |
                                           Ci.nsIAbListener.itemChanged |
                                           Ci.nsIAbListener.directoryItemRemoved);
  },

  disable() {
    MailServices.ab.removeAddressBookListener(this);
  },

  // it's a getter so we can reference 'this'
  get workers() {
    return [
      ["ab-card", {
         worker: this._worker_index_card,
       }],
    ];
  },

  * _worker_index_card(aJob, aCallbackHandle) {
    let card = aJob.id;

    if (card.primaryEmail) {
      // load the identity
      let query = Gloda.newQuery(Gloda.NOUN_IDENTITY);
      query.kind("email");
      // we currently normalize all e-mail addresses to be lowercase
      query.value(card.primaryEmail.toLowerCase());
      let identityCollection = query.getCollection(aCallbackHandle);
      yield Gloda.kWorkAsync;

      if (identityCollection.items.length) {
        let identity = identityCollection.items[0];
        // force the identity to know it has an associated ab card.
        identity._hasAddressBookCard = true;

        this._log.debug("Found identity, processing card.");
        yield aCallbackHandle.pushAndGo(
          Gloda.grokNounItem(identity.contact, {card}, false, false,
                             aCallbackHandle));
        this._log.debug("Done processing card.");
      }
    }

    yield GlodaIndexer.kWorkDone;
  },

  initialSweep() {
  },

  /* ------ nsIAbListener ------ */
  /**
   * When an address book card is added, update the cached GlodaIdentity
   *  object's cached idea of whether the identity has an ab card.
   */
  onItemAdded(aParentDir, aItem) {
    if (!(aItem instanceof Ci.nsIAbCard))
      return;

    this._log.debug("Received Card Add Notification");
    let identity = GlodaCollectionManager.cacheLookupOneByUniqueValue(
      Gloda.NOUN_IDENTITY, "email@" + aItem.primaryEmail.toLowerCase());
    if (identity)
      identity._hasAddressBookCard = true;
  },
  /**
   * When an address book card is added, update the cached GlodaIdentity
   *  object's cached idea of whether the identity has an ab card.
   */
  onItemRemoved(aParentDir, aItem) {
    if (!(aItem instanceof Ci.nsIAbCard))
      return;

    this._log.debug("Received Card Removal Notification");
    let identity = GlodaCollectionManager.cacheLookupOneByUniqueValue(
      Gloda.NOUN_IDENTITY, "email@" + aItem.primaryEmail.toLowerCase());
    if (identity)
      identity._hasAddressBookCard = false;
  },
  onItemPropertyChanged(aItem, aProperty, aOldValue, aNewValue) {
    if (aProperty == null && aItem instanceof Ci.nsIAbCard) {
      this._log.debug("Received Card Change Notification");

      let card = aItem; // instanceof already QueryInterface'd for us.
      let job = new IndexingJob("ab-card", card);
      GlodaIndexer.indexJob(job);
    }
  },
};
GlodaIndexer.registerIndexer(GlodaABIndexer);

var GlodaABAttrs = {
  providerName: "gloda.ab_attr",
  _log: null,

  init() {
    this._log =  Log4Moz.repository.getLogger("gloda.abattrs");

    try {
      this.defineAttributes();
    } catch (ex) {
      this._log.error("Error in init: " + ex);
      throw ex;
    }
  },

  defineAttributes() {
    /* ***** Contacts ***** */
    this._attrIdentityContact = Gloda.defineAttribute({
      provider: this,
      extensionName: Gloda.BUILT_IN,
      attributeType: Gloda.kAttrDerived,
      attributeName: "identities",
      singular: false,
      special: Gloda.kSpecialColumnChildren,
      // specialColumnName: "contactID",
      storageAttributeName: "_identities",
      subjectNouns: [Gloda.NOUN_CONTACT],
      objectNoun: Gloda.NOUN_IDENTITY,
      }); // tested-by: test_attributes_fundamental
    this._attrContactName = Gloda.defineAttribute({
      provider: this,
      extensionName: Gloda.BUILT_IN,
      attributeType: Gloda.kAttrFundamental,
      attributeName: "name",
      singular: true,
      special: Gloda.kSpecialString,
      specialColumnName: "name",
      subjectNouns: [Gloda.NOUN_CONTACT],
      objectNoun: Gloda.NOUN_STRING,
      canQuery: true,
      }); // tested-by: test_attributes_fundamental
    this._attrContactPopularity = Gloda.defineAttribute({
      provider: this,
      extensionName: Gloda.BUILT_IN,
      attributeType: Gloda.kAttrDerived,
      attributeName: "popularity",
      singular: true,
      special: Gloda.kSpecialColumn,
      specialColumnName: "popularity",
      subjectNouns: [Gloda.NOUN_CONTACT],
      objectNoun: Gloda.NOUN_NUMBER,
      canQuery: true,
      }); // not-tested
    this._attrContactFrecency = Gloda.defineAttribute({
      provider: this,
      extensionName: Gloda.BUILT_IN,
      attributeType: Gloda.kAttrDerived,
      attributeName: "frecency",
      singular: true,
      special: Gloda.kSpecialColumn,
      specialColumnName: "frecency",
      subjectNouns: [Gloda.NOUN_CONTACT],
      objectNoun: Gloda.NOUN_NUMBER,
      canQuery: true,
      }); // not-tested

    /* ***** Identities ***** */
    this._attrIdentityContact = Gloda.defineAttribute({
      provider: this,
      extensionName: Gloda.BUILT_IN,
      attributeType: Gloda.kAttrDerived,
      attributeName: "contact",
      singular: true,
      special: Gloda.kSpecialColumnParent,
      specialColumnName: "contactID", // the column in the db
      idStorageAttributeName: "_contactID",
      valueStorageAttributeName: "_contact",
      subjectNouns: [Gloda.NOUN_IDENTITY],
      objectNoun: Gloda.NOUN_CONTACT,
      canQuery: true,
      }); // tested-by: test_attributes_fundamental
    this._attrIdentityKind = Gloda.defineAttribute({
      provider: this,
      extensionName: Gloda.BUILT_IN,
      attributeType: Gloda.kAttrFundamental,
      attributeName: "kind",
      singular: true,
      special: Gloda.kSpecialString,
      specialColumnName: "kind",
      subjectNouns: [Gloda.NOUN_IDENTITY],
      objectNoun: Gloda.NOUN_STRING,
      canQuery: true,
      }); // tested-by: test_attributes_fundamental
    this._attrIdentityValue = Gloda.defineAttribute({
      provider: this,
      extensionName: Gloda.BUILT_IN,
      attributeType: Gloda.kAttrFundamental,
      attributeName: "value",
      singular: true,
      special: Gloda.kSpecialString,
      specialColumnName: "value",
      subjectNouns: [Gloda.NOUN_IDENTITY],
      objectNoun: Gloda.NOUN_STRING,
      canQuery: true,
      }); // tested-by: test_attributes_fundamental

    /* ***** Contact Meta ***** */
    // Freeform tags; not explicit like thunderbird's fundamental tags.
    //  we differentiate for now because of fundamental implementation
    //  differences.
    this._attrFreeTag = Gloda.defineAttribute({
      provider: this,
      extensionName: Gloda.BUILT_IN,
      attributeType: Gloda.kAttrExplicit,
      attributeName: "freetag",
      bind: true,
      bindName: "freeTags",
      singular: false,
      subjectNouns: [Gloda.NOUN_CONTACT],
      objectNoun: Gloda.lookupNoun("freetag"),
      parameterNoun: null,
      canQuery: true,
      }); // not-tested
    // we need to find any existing bound freetag attributes, and use them to
    //  populate to FreeTagNoun's understanding
    if ("parameterBindings" in this._attrFreeTag) {
      for (let freeTagName in this._attrFreeTag.parameterBindings) {
        this._log.debug("Telling FreeTagNoun about: " + freeTagName);
        FreeTagNoun.getFreeTag(freeTagName);
      }
    }
  },

  * process(aContact, aRawReps, aIsNew, aCallbackHandle) {
    let card = aRawReps.card;
    if (aContact.NOUN_ID != Gloda.NOUN_CONTACT) {
      this._log.warn("Somehow got a non-contact: " + aContact);
      return; // this will produce an exception; we like.
    }

    // update the name
    if (card.displayName && card.displayName != aContact.name)
      aContact.name = card.displayName;

    aContact.freeTags = [];

    let tags = null;
    try {
      tags = card.getProperty("Categories", null);
    } catch (ex) {
      this._log.error("Problem accessing property: " + ex);
    }
    if (tags) {
      for (let tagName of tags.split(",")) {
        tagName = tagName.trim();
        if (tagName) {
          aContact.freeTags.push(FreeTagNoun.getFreeTag(tagName));
        }
      }
    }

    yield Gloda.kWorkDone;
  },
};
