/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Note: this file is included in aboutDialog.xul and preferences/advanced.xul
// if MOZ_UPDATER is defined.

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/DownloadUtils.jsm");

const PREF_APP_UPDATE_CANCELATIONS_OSX = "app.update.cancelations.osx";
const PREF_APP_UPDATE_ELEVATE_NEVER    = "app.update.elevate.never";

var gAppUpdater;

function onUnload(aEvent) {
  if (gAppUpdater.isChecking)
    gAppUpdater.checker.stopCurrentCheck();
  // Safe to call even when there isn't a download in progress.
  gAppUpdater.removeDownloadListener();
  gAppUpdater = null;
}


function appUpdater()
{
  this.updateDeck = document.getElementById("updateDeck");

  // Hide the update deck when there is already an update window open to avoid
  // syncing issues between them.
  if (Services.wm.getMostRecentWindow("Update:Wizard")) {
    this.updateDeck.hidden = true;
    return;
  }

  XPCOMUtils.defineLazyServiceGetter(this, "aus",
                                     "@mozilla.org/updates/update-service;1",
                                     "nsIApplicationUpdateService");
  XPCOMUtils.defineLazyServiceGetter(this, "checker",
                                     "@mozilla.org/updates/update-checker;1",
                                     "nsIUpdateChecker");
  XPCOMUtils.defineLazyServiceGetter(this, "um",
                                     "@mozilla.org/updates/update-manager;1",
                                     "nsIUpdateManager");

  this.bundle = Services.strings.
                createBundle("chrome://messenger/locale/messenger.properties");

  let manualURL = Services.urlFormatter.formatURLPref("app.update.url.manual");
  document.getElementById("manualLink")
          .setAttribute("onclick", 'openURL("' + manualURL + '");');
  document.getElementById("failedLink")
          .setAttribute("onclick", 'openURL("' + manualURL + '");');

  if (this.updateDisabledByPolicy) {
    this.selectPanel("policyDisabled");
    return;
  }

  if (this.isPending || this.isApplied) {
    this.selectPanel("apply");
    return;
  }

  if (this.aus.isOtherInstanceHandlingUpdates) {
    this.selectPanel("otherInstanceHandlingUpdates");
    return;
  }

  if (this.isDownloading) {
    this.startDownload();
    // selectPanel("downloading") is called from setupDownloadingUI().
    return;
  }

  // That leaves the options
  // "Check for updates, but let me choose whether to install them", and
  // "Automatically install updates".
  // In both cases, we check for updates without asking.
  // In the "let me choose" case, we ask before downloading though, in onCheckComplete.
  this.checkForUpdates();
}

appUpdater.prototype =
{
  // true when there is an update check in progress.
  isChecking: false,

  // true when there is an update already staged / ready to be applied.
  get isPending() {
    if (this.update) {
      return this.update.state == "pending" ||
             this.update.state == "pending-service" ||
             this.update.state == "pending-elevate";
    }
    return this.um.activeUpdate &&
           (this.um.activeUpdate.state == "pending" ||
            this.um.activeUpdate.state == "pending-service" ||
            this.um.activeUpdate.state == "pending-elevate");
  },

  // true when there is an update already installed in the background.
  get isApplied() {
    if (this.update) {
      return this.update.state == "applied" ||
             this.update.state == "applied-service";
    }
    return this.um.activeUpdate &&
           (this.um.activeUpdate.state == "applied" ||
            this.um.activeUpdate.state == "applied-service");
  },

  // true when there is an update download in progress.
  get isDownloading() {
    if (this.update)
      return this.update.state == "downloading";
    return this.um.activeUpdate &&
           this.um.activeUpdate.state == "downloading";
  },

  // true when updating has been disabled by enterprise policy. Not yet used.
  get updateDisabledByPolicy() {
    return Services.policies &&
           !Services.policies.isAllowed("appUpdate");
  },

  // true when updating in background is enabled.
  get backgroundUpdateEnabled() {
    return !this.updateDisabledByPolicy &&
           gAppUpdater.aus.canStageUpdates;
  },

  // true when updating is automatic.
  get updateAuto() {
    try {
      return Services.prefs.getBoolPref("app.update.auto");
    }
    catch (e) { }
    return true; // Thunderbird default is true
  },

  /**
   * Sets the panel of the updateDeck.
   *
   * @param  aChildID
   *         The id of the deck's child to select, e.g. "apply".
   */
  selectPanel: function(aChildID) {
    let panel = document.getElementById(aChildID);

    let button = panel.querySelector("button");
    if (button) {
      if (aChildID == "downloadAndInstall") {
        let updateVersion = gAppUpdater.update.displayVersion;
        // Include the build ID if this is an "a#" (nightly) build
        if (/a\d+$/.test(updateVersion)) {
          let buildID = gAppUpdater.update.buildID;
          let year = buildID.slice(0, 4);
          let month = buildID.slice(4, 6);
          let day = buildID.slice(6, 8);
          updateVersion += ` (${year}-${month}-${day})`;
        }
        button.label = this.bundle.formatStringFromName("update.downloadAndInstallButton.label", [updateVersion], 1);
        button.accessKey = this.bundle.GetStringFromName("update.downloadAndInstallButton.accesskey");
      }
      this.updateDeck.selectedPanel = panel;
      if (!document.commandDispatcher.focusedElement || // don't steal the focus
          document.commandDispatcher.focusedElement.localName == "button") // except from the other buttons
        button.focus();

    } else {
      this.updateDeck.selectedPanel = panel;
    }
  },

  /**
   * Check for updates
   */
  checkForUpdates: function() {
    // Clear prefs that could prevent a user from discovering available updates.
    if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CANCELATIONS_OSX)) {
      Services.prefs.clearUserPref(PREF_APP_UPDATE_CANCELATIONS_OSX);
    }
    if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_ELEVATE_NEVER)) {
      Services.prefs.clearUserPref(PREF_APP_UPDATE_ELEVATE_NEVER);
    }
    this.selectPanel("checkingForUpdates");
    this.isChecking = true;
    this.checker.checkForUpdates(this.updateCheckListener, true);
    // after checking, onCheckComplete() is called
  },

  /**
   * Handles oncommand for the "Restart to Update" button
   * which is presented after the download has been downloaded.
   */
  buttonRestartAfterDownload: function() {
    if (!this.isPending && !this.isApplied) {
      return;
    }

    // Notify all windows that an application quit has been requested.
    let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].
                     createInstance(Ci.nsISupportsPRBool);
    Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");

    // Something aborted the quit process.
    if (cancelQuit.data) {
      return;
    }

    let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].
                     getService(Ci.nsIAppStartup);

    // If already in safe mode restart in safe mode (bug 327119)
    if (Services.appinfo.inSafeMode) {
      appStartup.restartInSafeMode(Ci.nsIAppStartup.eAttemptQuit);
      return;
    }

    appStartup.quit(Ci.nsIAppStartup.eAttemptQuit |
                    Ci.nsIAppStartup.eRestart);
  },

  /**
   * Implements nsIUpdateCheckListener. The methods implemented by
   * nsIUpdateCheckListener are in a different scope from nsIIncrementalDownload
   * to make it clear which are used by each interface.
   */
  updateCheckListener: {
    /**
     * See nsIUpdateService.idl
     */
    onCheckComplete: function(aRequest, aUpdates, aUpdateCount) {
      gAppUpdater.isChecking = false;
      gAppUpdater.update = gAppUpdater.aus.
                           selectUpdate(aUpdates, aUpdates.length);
      if (!gAppUpdater.update) {
        gAppUpdater.selectPanel("noUpdatesFound");
        return;
      }

      if (gAppUpdater.update.unsupported) {
        let unsupportedLink = document.getElementById("unsupportedLink");
        if (gAppUpdater.update.detailsURL)
          unsupportedLink.href = gAppUpdater.update.detailsURL;
        else
          unsupportedLink.setAttribute("hidden", true);

        gAppUpdater.selectPanel("unsupportedSystem");
        return;
      }

      if (!gAppUpdater.aus.canApplyUpdates) {
        gAppUpdater.selectPanel("manualUpdate");
        return;
      }

      if (gAppUpdater.updateAuto) // automatically download and install
        gAppUpdater.startDownload();
      else // ask
        gAppUpdater.selectPanel("downloadAndInstall");
    },

    /**
     * See nsIUpdateService.idl
     */
    onError: function(aRequest, aUpdate) {
      // Errors in the update check are treated as no updates found. If the
      // update check fails repeatedly without a success the user will be
      // notified with the normal app update user interface so this is safe.
      gAppUpdater.isChecking = false;
      gAppUpdater.selectPanel("noUpdatesFound");
      return;
    },

    /**
     * See nsISupports.idl
     */
    QueryInterface: ChromeUtils.generateQI(["nsIUpdateCheckListener"]),
  },

  /**
   * Starts the download of an update mar.
   */
  startDownload: function() {
    if (!this.update)
      this.update = this.um.activeUpdate;
    this.update.QueryInterface(Ci.nsIWritablePropertyBag);
    this.update.setProperty("foregroundDownload", "true");

    this.aus.pauseDownload();
    let state = this.aus.downloadUpdate(this.update, false);
    if (state == "failed") {
      this.selectPanel("downloadFailed");
      return;
    }

    this.setupDownloadingUI();
  },

  /**
   * Switches to the UI responsible for tracking the download.
   */
  setupDownloadingUI: function() {
    this.downloadStatus = document.getElementById("downloadStatus");
    this.downloadStatus.value =
      DownloadUtils.getTransferTotal(0, this.update.selectedPatch.size);
    this.selectPanel("downloading");
    this.aus.addDownloadListener(this);
  },

  removeDownloadListener: function() {
    if (this.aus) {
      this.aus.removeDownloadListener(this);
    }
  },

  /**
   * See nsIRequestObserver.idl
   */
  onStartRequest: function(aRequest, aContext) {
  },

  /**
   * See nsIRequestObserver.idl
   */
  onStopRequest: function(aRequest, aContext, aStatusCode) {
    switch (aStatusCode) {
    case Cr.NS_ERROR_UNEXPECTED:
      if (this.update.selectedPatch.state == "download-failed" &&
          (this.update.isCompleteUpdate || this.update.patchCount != 2)) {
        // Verification error of complete patch, informational text is held in
        // the update object.
        this.removeDownloadListener();
        this.selectPanel("downloadFailed");
        break;
      }
      // Verification failed for a partial patch, complete patch is now
      // downloading so return early and do NOT remove the download listener!
      break;
    case Cr.NS_BINDING_ABORTED:
      // Do not remove UI listener since the user may resume downloading again.
      break;
    case Cr.NS_OK:
      this.removeDownloadListener();
      if (this.backgroundUpdateEnabled) {
        this.selectPanel("applying");
        let update = this.um.activeUpdate;
        let self = this;
        Services.obs.addObserver(function selectPanelOnUpdate(aSubject, aTopic, aData) {
          // Update the UI when the background updater is finished
          let status = aData;
          if (status == "applied" || status == "applied-service" ||
              status == "pending" || status == "pending-service" ||
              status == "pending-elevate") {
            // If the update is successfully applied, or if the updater has
            // fallen back to non-staged updates, show the "Restart to Update"
            // button.
            self.selectPanel("apply");
          } else if (status == "failed") {
            // Background update has failed, let's show the UI responsible for
            // prompting the user to update manually.
            self.selectPanel("downloadFailed");
          } else if (status == "downloading") {
            // We've fallen back to downloading the full update because the
            // partial update failed to get staged in the background.
            // Therefore we need to keep our observer.
            self.setupDownloadingUI();
            return;
          }
          Services.obs.removeObserver(selectPanelOnUpdate, "update-staged");
        }, "update-staged");
      } else {
        this.selectPanel("apply");
      }
      break;
    default:
      this.removeDownloadListener();
      this.selectPanel("downloadFailed");
      break;
    }
  },

  /**
   * See nsIProgressEventSink.idl
   */
  onStatus: function(aRequest, aContext, aStatus, aStatusArg) {
  },

  /**
   * See nsIProgressEventSink.idl
   */
  onProgress: function(aRequest, aContext, aProgress, aProgressMax) {
    this.downloadStatus.value =
      DownloadUtils.getTransferTotal(aProgress, aProgressMax);
  },

  /**
   * See nsISupports.idl
   */
  QueryInterface: ChromeUtils.generateQI(["nsIProgressEventSink",
                                          "nsIRequestObserver"]),
};
