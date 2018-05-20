/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Load DownloadUtils module for convertByteUnits
ChromeUtils.import("resource://gre/modules/DownloadUtils.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

var gAdvancedPane = {
  mPane: null,
  mInitialized: false,
  mShellServiceWorking: false,
  mBundle: null,

  _loadInContent: Services.prefs.getBoolPref("mail.preferences.inContent"),

  init: function ()
  {
    this.mPane = document.getElementById("paneAdvanced");
    this.updateCompactOptions();
    this.mBundle = document.getElementById("bundlePreferences");
    this.formatLocaleSetLabels();

    if (!(("arguments" in window) && window.arguments[1]))
    {
      // If no tab was specified, select the last used tab.
      let preference = document.getElementById("mail.preferences.advanced.selectedTabIndex");
      if (preference.value)
        document.getElementById("advancedPrefs").selectedIndex = preference.value;
    }
    if (AppConstants.MOZ_UPDATER)
      this.updateReadPrefs();

    // Default store type initialization.
    let storeTypeElement = document.getElementById("storeTypeMenulist");
    // set the menuitem to match the account
    let defaultStoreID = Services.prefs.getCharPref("mail.serverDefaultStoreContractID");
    let targetItem = storeTypeElement.getElementsByAttribute("value", defaultStoreID);
    storeTypeElement.selectedItem = targetItem[0];

    if (AppConstants.MOZ_CRASHREPORTER)
      this.initSubmitCrashes();
    this.initTelemetry();
    this.updateActualCacheSize();

    // Search integration -- check whether we should hide or disable integration
    let hideSearchUI = false;
    let disableSearchUI = false;
    ChromeUtils.import("resource:///modules/SearchIntegration.js");
    if (SearchIntegration)
    {
      if (SearchIntegration.osVersionTooLow)
        hideSearchUI = true;
      else if (SearchIntegration.osComponentsNotRunning)
        disableSearchUI = true;
    }
    else
    {
      hideSearchUI = true;
    }

    if (hideSearchUI)
    {
      document.getElementById("searchIntegrationContainer").hidden = true;
    }
    else if (disableSearchUI)
    {
      let searchCheckbox = document.getElementById("searchIntegration");
      searchCheckbox.checked = false;
      document.getElementById("searchintegration.enable").disabled = true;
    }

    // If the shell service is not working, disable the "Check now" button
    // and "perform check at startup" checkbox.
    try {
      let shellSvc = Cc["@mozilla.org/mail/shell-service;1"]
                       .getService(Ci.nsIShellService);
      this.mShellServiceWorking = true;
    } catch (ex) {
      // The elements may not exist if HAVE_SHELL_SERVICE is off.
      if (document.getElementById("alwaysCheckDefault")) {
        document.getElementById("alwaysCheckDefault").disabled = true;
        document.getElementById("alwaysCheckDefault").checked = false;
      }
      if (document.getElementById("checkDefaultButton"))
        document.getElementById("checkDefaultButton").disabled = true;
      this.mShellServiceWorking = false;
    }

    if (AppConstants.MOZ_UPDATER) {
      let distroId = Services.prefs.getCharPref("distribution.id" , "");
      if (distroId) {
        let distroVersion = Services.prefs.getCharPref("distribution.version");

        let distroIdField = document.getElementById("distributionId");
        distroIdField.value = distroId + " - " + distroVersion;
        distroIdField.style.display = "block";

        let distroAbout = Services.prefs.getStringPref("distribution.about", "");
        if (distroAbout) {
          let distroField = document.getElementById("distribution");
          distroField.value = distroAbout;
          distroField.style.display = "block";
        }
      }

      let version = AppConstants.MOZ_APP_VERSION_DISPLAY;

      // Include the build ID and display warning if this is an "a#" (nightly) build
      if (/a\d+$/.test(version)) {
        let buildID = Services.appinfo.appBuildID;
        let year = buildID.slice(0, 4);
        let month = buildID.slice(4, 6);
        let day = buildID.slice(6, 8);
        version += ` (${year}-${month}-${day})`;
      }

      // Append "(32-bit)" or "(64-bit)" build architecture to the version number:
      let bundle = Services.strings.createBundle("chrome://messenger/locale/messenger.properties");
      let archResource = Services.appinfo.is64Bit
                         ? "aboutDialog.architecture.sixtyFourBit"
                         : "aboutDialog.architecture.thirtyTwoBit";
      let arch = bundle.GetStringFromName(archResource);
      version += ` (${arch})`;

      document.getElementById("version").textContent = version;

      if (!AppConstants.NIGHTLY_BUILD) {
        // Show a release notes link if we have a URL.
        let relNotesLink = document.getElementById("releasenotes");
        let relNotesPrefType = Services.prefs.getPrefType("app.releaseNotesURL");
        if (relNotesPrefType != Services.prefs.PREF_INVALID) {
          let relNotesURL = Services.urlFormatter.formatURLPref("app.releaseNotesURL");
          if (relNotesURL != "about:blank") {
            relNotesLink.href = relNotesURL;
            relNotesLink.hidden = false;
          }
        }
      }

      gAppUpdater = new appUpdater();
    }

    this.mInitialized = true;
  },

  tabSelectionChanged: function ()
  {
    if (this.mInitialized)
    {
      document.getElementById("mail.preferences.advanced.selectedTabIndex")
              .valueFromPreferences = document.getElementById("advancedPrefs").selectedIndex;
    }
  },

  /**
   * Checks whether Thunderbird is currently registered with the operating
   * system as the default app for mail, rss and news.  If Thunderbird is not
   * currently the default app, the user is given the option of making it the
   * default for each type; otherwise, the user is informed that Thunderbird is
   * already the default.
   */
  checkDefaultNow: function (aAppType)
  {
    if (!this.mShellServiceWorking)
      return;

    // otherwise, bring up the default client dialog
    if (this._loadInContent) {
      gSubDialog.open("chrome://messenger/content/systemIntegrationDialog.xul",
                      "resizable=no", "calledFromPrefs");
    } else {
      window.openDialog("chrome://messenger/content/systemIntegrationDialog.xul",
                        "SystemIntegration",
                        "modal,centerscreen,chrome,resizable=no", "calledFromPrefs");
    }
  },

  showConfigEdit: function()
  {
    if (this._loadInContent) {
      gSubDialog.open("chrome://global/content/config.xul");
    } else {
      document.documentElement.openWindow("Preferences:ConfigManager",
                                          "chrome://global/content/config.xul",
                                          "", null);
    }
  },

  /**
   * Set the default store contract ID.
   */
  updateDefaultStore: function(storeID)
  {
    Services.prefs.setCharPref("mail.serverDefaultStoreContractID", storeID);
  },

  // NETWORK TAB

  /*
   * Preferences:
   *
   * browser.cache.disk.capacity
   * - the size of the browser cache in KB
   */

  // Retrieves the amount of space currently used by disk cache
  updateActualCacheSize: function()
  {
    let actualSizeLabel = document.getElementById("actualDiskCacheSize");
    let prefStrBundle = document.getElementById("bundlePreferences");

    // Needs to root the observer since cache service keeps only a weak reference.
    this.observer = {
      onNetworkCacheDiskConsumption: function(consumption) {
        let size = DownloadUtils.convertByteUnits(consumption);
        // The XBL binding for the string bundle may have been destroyed if
        // the page was closed before this callback was executed.
        if (!prefStrBundle.getFormattedString) {
          return;
        }
        actualSizeLabel.value = prefStrBundle.getFormattedString("actualDiskCacheSize", size);
      },

      QueryInterface: ChromeUtils.generateQI([
        Ci.nsICacheStorageConsumptionObserver,
        Ci.nsISupportsWeakReference
      ])
    };

    actualSizeLabel.value = prefStrBundle.getString("actualDiskCacheSizeCalculated");

    try {
      let cacheService =
        Cc["@mozilla.org/netwerk/cache-storage-service;1"]
          .getService(Ci.nsICacheStorageService);
      cacheService.asyncGetDiskConsumption(this.observer);
    } catch (e) {}
  },

  updateCacheSizeUI: function (smartSizeEnabled)
  {
    document.getElementById("useCacheBefore").disabled = smartSizeEnabled;
    document.getElementById("cacheSize").disabled = smartSizeEnabled;
    document.getElementById("useCacheAfter").disabled = smartSizeEnabled;
  },

  readSmartSizeEnabled: function ()
  {
    // The smart_size.enabled preference element is inverted="true", so its
    // value is the opposite of the actual pref value
    var disabled = document.getElementById("browser.cache.disk.smart_size.enabled").value;
    this.updateCacheSizeUI(!disabled);
  },

  /**
   * Converts the cache size from units of KB to units of MB and returns that
   * value.
   */
  readCacheSize: function ()
  {
    var preference = document.getElementById("browser.cache.disk.capacity");
    return preference.value / 1024;
  },

  /**
   * Converts the cache size as specified in UI (in MB) to KB and returns that
   * value.
   */
  writeCacheSize: function ()
  {
    var cacheSize = document.getElementById("cacheSize");
    var intValue = parseInt(cacheSize.value, 10);
    return isNaN(intValue) ? 0 : intValue * 1024;
  },

  /**
   * Clears the cache.
   */
  clearCache: function ()
  {
    try {
      let cache = Cc["@mozilla.org/netwerk/cache-storage-service;1"]
                    .getService(Ci.nsICacheStorageService);
      cache.clear();
    } catch (ex) {}
    this.updateActualCacheSize();
  },

  updateButtons: function (aButtonID, aPreferenceID)
  {
    var button = document.getElementById(aButtonID);
    var preference = document.getElementById(aPreferenceID);
    // This is actually before the value changes, so the value is not as you expect.
    button.disabled = preference.value == true;
    return undefined;
  },

/**
 * Selects the item of the radiogroup based on the pref values and locked
 * states.
 *
 * UI state matrix for update preference conditions
 *
 * UI Components:                              Preferences
 * Radiogroup                                  i   = app.update.enabled
 *                                             ii  = app.update.auto
 *
 * Disabled states:
 * Element           pref  value  locked  disabled
 * radiogroup        i     t/f    f       false
 *                   i     t/f    *t*     *true*
 *                   ii    t/f    f       false
 *                   ii    t/f    *t*     *true*
 */
updateReadPrefs: function ()
{
  var enabledPref = document.getElementById("app.update.enabled");
  var autoPref = document.getElementById("app.update.auto");
  var radiogroup = document.getElementById("updateRadioGroup");

  if (!enabledPref.value)   // Don't care for autoPref.value in this case.
    radiogroup.value="manual"     // 3. Never check for updates.
  else if (autoPref.value)  // enabledPref.value && autoPref.value
    radiogroup.value="auto";      // 1. Automatically install updates
  else                      // enabledPref.value && !autoPref.value
    radiogroup.value="checkOnly"; // 2. Check, but let me choose

  var canCheck = Cc["@mozilla.org/updates/update-service;1"].
                   getService(Ci.nsIApplicationUpdateService).
                   canCheckForUpdates;

  // canCheck is false if the enabledPref is false and locked,
  // or the binary platform or OS version is not known.
  // A locked pref is sufficient to disable the radiogroup.
  radiogroup.disabled = !canCheck || enabledPref.locked || autoPref.locked;

  if (AppConstants.MOZ_MAINTENANCE_SERVICE) {
    // Check to see if the maintenance service is installed.
    // If it is don't show the preference at all.
    let installed;
    try {
      let wrk = Cc["@mozilla.org/windows-registry-key;1"]
                  .createInstance(Ci.nsIWindowsRegKey);
      wrk.open(wrk.ROOT_KEY_LOCAL_MACHINE,
               "SOFTWARE\\Mozilla\\MaintenanceService",
               wrk.ACCESS_READ | wrk.WOW64_64);
      installed = wrk.readIntValue("Installed");
      wrk.close();
    } catch(e) { }
    if (installed != 1) {
      document.getElementById("useService").hidden = true;
    }
  }
},

/**
 * Sets the pref values based on the selected item of the radiogroup.
 */
updateWritePrefs: function ()
{
  var enabledPref = document.getElementById("app.update.enabled");
  var autoPref = document.getElementById("app.update.auto");
  var radiogroup = document.getElementById("updateRadioGroup");
  switch (radiogroup.value) {
    case "auto":      // 1. Automatically install updates
      enabledPref.value = true;
      autoPref.value = true;
      break;
    case "checkOnly": // 2. Check, but but let me choose
      enabledPref.value = true;
      autoPref.value = false;
      break;
    case "manual":    // 3. Never check for updates.
      enabledPref.value = false;
      autoPref.value = false;
  }
},

  showUpdates: function ()
  {
    if (this._loadInContent) {
      gSubDialog.open("chrome://mozapps/content/update/history.xul");
    } else {
      var prompter = Cc["@mozilla.org/updates/update-prompt;1"]
                       .createInstance(Ci.nsIUpdatePrompt);
      prompter.showUpdateHistory(window);
    }
  },

  updateCompactOptions: function(aCompactEnabled)
  {
    document.getElementById("offlineCompactFolderMin").disabled =
      !document.getElementById("offlineCompactFolder").checked ||
      document.getElementById("mail.purge_threshhold_mb").locked;
  },

  updateSubmitCrashReports: function(aChecked)
  {
    Cc["@mozilla.org/toolkit/crash-reporter;1"]
      .getService(Ci.nsICrashReporter)
      .submitReports = aChecked;
  },
  /**
   * Display the return receipts configuration dialog.
   */
  showReturnReceipts: function()
  {
    if (this._loadInContent) {
      gSubDialog.open("chrome://messenger/content/preferences/receipts.xul",
                      "resizable=no");
    } else {
      document.documentElement
              .openSubDialog("chrome://messenger/content/preferences/receipts.xul",
                             "", null);
    }
  },

  /**
   * Display the the connection settings dialog.
   */
  showConnections: function ()
  {
    if (this._loadInContent) {
      gSubDialog.open("chrome://messenger/content/preferences/connection.xul",
                      "resizable=no");
    } else {
      document.documentElement
              .openSubDialog("chrome://messenger/content/preferences/connection.xul",
                             "", null);
    }
  },

  /**
   * Display the the offline settings dialog.
   */
  showOffline: function()
  {
    if (this._loadInContent) {
      gSubDialog.open("chrome://messenger/content/preferences/offline.xul",
                      "resizable=no");
    } else {
      document.documentElement
              .openSubDialog("chrome://messenger/content/preferences/offline.xul",
                             "", null);
    }
  },

  /**
   * Display the user's certificates and associated options.
   */
  showCertificates: function ()
  {
    if (this._loadInContent) {
      gSubDialog.open("chrome://pippki/content/certManager.xul");
    } else {
      document.documentElement.openWindow("mozilla:certmanager",
                                          "chrome://pippki/content/certManager.xul",
                                          "", null);
    }
  },

  /**
   * security.OCSP.enabled is an integer value for legacy reasons.
   * A value of 1 means OCSP is enabled. Any other value means it is disabled.
   */
  readEnableOCSP: function ()
  {
    var preference = document.getElementById("security.OCSP.enabled");
    // This is the case if the preference is the default value.
    if (preference.value === undefined) {
      return true;
    }
    return preference.value == 1;
  },

  /**
   * See documentation for readEnableOCSP.
   */
  writeEnableOCSP: function ()
  {
    var checkbox = document.getElementById("enableOCSP");
    return checkbox.checked ? 1 : 0;
  },

  /**
   * Display a dialog from which the user can manage his security devices.
   */
  showSecurityDevices: function ()
  {
    if (this._loadInContent) {
      gSubDialog.open("chrome://pippki/content/device_manager.xul");
    } else {
      document.documentElement.openWindow("mozilla:devicemanager",
                                          "chrome://pippki/content/device_manager.xul",
                                          "", null);
    }
  },

  /**
   * When the user toggles the layers.acceleration.disabled pref,
   * sync its new value to the gfx.direct2d.disabled pref too.
   */
  updateHardwareAcceleration: function(aVal)
  {
    if (AppConstants.platforms == "win")
      Services.prefs.setBoolPref("gfx.direct2d.disabled", !aVal);
  },

  // DATA CHOICES TAB

  /**
   * Open a text link.
   */
  openTextLink: function (evt) {
    // Opening links behind a modal dialog is poor form. Work around flawed
    // text-link handling by opening in browser if we'd instead get a content
    // tab behind the modal options dialog.
    if (Services.prefs.getBoolPref("browser.preferences.instantApply")) {
      return true; // Yes, open the link in a content tab.
    }
    var url = evt.target.getAttribute("href");
    var messenger = Cc["@mozilla.org/messenger;1"]
      .createInstance(Ci.nsIMessenger);
    messenger.launchExternalURL(url);
    evt.preventDefault();
    return false;
  },

  /**
   * Set up or hide the Learn More links for various data collection options
   */
  _setupLearnMoreLink: function (pref, element) {
    // set up the Learn More link with the correct URL
    let url = Services.prefs.getCharPref(pref);
    let el = document.getElementById(element);

    if (url) {
      el.setAttribute("href", url);
    } else {
      el.setAttribute("hidden", "true");
    }
  },

  initSubmitCrashes: function ()
  {
    var checkbox = document.getElementById("submitCrashesBox");
    try {
      var cr = Cc["@mozilla.org/toolkit/crash-reporter;1"].
               getService(Ci.nsICrashReporter);
      checkbox.checked = cr.submitReports;
    } catch (e) {
      checkbox.style.display = "none";
    }
    this._setupLearnMoreLink("toolkit.crashreporter.infoURL", "crashReporterLearnMore");
  },

  updateSubmitCrashes: function ()
  {
    var checkbox = document.getElementById("submitCrashesBox");
    try {
      var cr = Cc["@mozilla.org/toolkit/crash-reporter;1"].
               getService(Ci.nsICrashReporter);
      cr.submitReports = checkbox.checked;
    } catch (e) { }
  },


  /**
   * The preference/checkbox is configured in XUL.
   *
   * In all cases, set up the Learn More link sanely
   */
  initTelemetry: function ()
  {
    if (AppConstants.MOZ_TELEMETRY_REPORTING)
      this._setupLearnMoreLink("toolkit.telemetry.infoURL", "telemetryLearnMore");
  },

  formatLocaleSetLabels: function() {
    const localeService =
      Cc["@mozilla.org/intl/localeservice;1"]
        .getService(Ci.mozILocaleService);
    const osprefs =
      Cc["@mozilla.org/intl/ospreferences;1"]
        .getService(Ci.mozIOSPreferences);
    let appLocale = localeService.getAppLocalesAsBCP47()[0];
    let rsLocale = osprefs.getRegionalPrefsLocales()[0];
    let names = Services.intl.getLocaleDisplayNames(undefined, [appLocale, rsLocale]);
    let appLocaleRadio = document.getElementById("appLocale");
    let rsLocaleRadio = document.getElementById("rsLocale");
    let appLocaleLabel = this.mBundle.getFormattedString("appLocale.label",
                                                         [names[0]]);
    let rsLocaleLabel = this.mBundle.getFormattedString("rsLocale.label",
                                                        [names[1]]);
    appLocaleRadio.setAttribute("label", appLocaleLabel);
    rsLocaleRadio.setAttribute("label", rsLocaleLabel);
    appLocaleRadio.accessKey = this.mBundle.getString("appLocale.accesskey");
    rsLocaleRadio.accessKey = this.mBundle.getString("rsLocale.accesskey");
  },
};
