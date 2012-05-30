/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const nsIWindowDataSource = Components.interfaces.nsIWindowDataSource;

function toNavigator()
{
  if (!CycleWindow("navigator:browser"))
    OpenBrowserWindow();
}

function ExpirePassword()
{
  // Queries the HTTP Auth Manager and clears all sessions
  Components.classes['@mozilla.org/network/http-auth-manager;1']
            .getService(Components.interfaces.nsIHttpAuthManager)
            .clearAll();

  // Expires the master password
  Components.classes["@mozilla.org/security/pk11tokendb;1"]
            .createInstance(Components.interfaces.nsIPK11TokenDB)
            .getInternalKeyToken()
            .checkPassword("");
}

function toDownloadManager()
{
  //Ported extensions may only implement the Basic toolkit Interface
  //and not our progress dialogs.
  var dlUI = Components.classes["@mozilla.org/download-manager-ui;1"]
                       .getService(Components.interfaces.nsIDownloadManagerUI);
  if (dlUI instanceof Components.interfaces.nsISuiteDownloadManagerUI) {
    dlUI.showManager(window);
  } else {
    dlUI.show(window);
  }
}

function toDataManager(aView)
{
  switchToTabHavingURI("about:data", true, function(browser) {
    if (aView)
      browser.contentWindow.wrappedJSObject.gDataman.loadView(aView);
  });
}

function toEM(aView)
{
  switchToTabHavingURI("about:addons", true, function(browser) {
    if (aView)
      browser.contentWindow.wrappedJSObject.loadView(aView);
  });
}

function toBookmarksManager()
{
  toOpenWindowByType("bookmarks:manager",
                     "chrome://communicator/content/bookmarks/bookmarksManager.xul");
}

function toJavaScriptConsole()
{
    toOpenWindowByType("global:console", "chrome://global/content/console.xul");
}

function toOpenWindow( aWindow )
{
  try {
    // Try to focus the previously focused window e.g. message compose body
    aWindow.document.commandDispatcher.focusedWindow.focus();
  } catch (e) {
    // e.g. full-page plugin or non-XUL document; just raise the top window
    aWindow.focus();
  }
}

function toOpenWindowByType( inType, uri, features )
{
  // don't do several loads in parallel
  if (uri in window)
    return;

  var topWindow = Services.wm.getMostRecentWindow(inType);
  if ( topWindow )
    toOpenWindow( topWindow );
  else
  {
    // open the requested window, but block it until it's fully loaded
    function newWindowLoaded(event)
    {
      // make sure that this handler is called only once
      window.removeEventListener("unload", newWindowLoaded, false);
      window[uri].removeEventListener("load", newWindowLoaded, false);
      delete window[uri];
    }
    // remember the newly loading window until it's fully loaded
    // or until the current window passes away
    window[uri] = window.openDialog(uri, "", features || "all,dialog=no");
    window[uri].addEventListener("load", newWindowLoaded, false);
    window.addEventListener("unload", newWindowLoaded, false);
  }
}

function OpenBrowserWindow()
{
  if (document.documentElement.getAttribute("windowtype") ==
      "navigator:browser" && window.content && window.content.document)
  {
    // if and only if the current window is a browser window and
    // it has a document with a character set, then extract the
    // current charset menu setting from the current document
    // and use it to initialize the new browser window
    window.openDialog(getBrowserURL(), "_blank",
                      "chrome,all,dialog=no", null,
                      "charset=" + window.content.document.characterSet);
  } else if (Services.wm.getMostRecentWindow("navigator:browser")) {
    // if a browser window already exists then set startpage to null so
    // navigator.js can check pref for how new window should be opened
    window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", null);
  } else {
    // open the first browser window as if we were starting up
    var cmdLine = {
      handleFlagWithParam: function handleFlagWithParam(flag, caseSensitive) {
        return flag == "remote" ? "xfeDoCommand(openBrowser)" : null;
      },
      handleFlag: function handleFlag(flag, caseSensitive) {
        return false;
      },
      preventDefault: true
    };
    const clh_prefix = "@mozilla.org/commandlinehandler/general-startup;1";
    Components.classes[clh_prefix + "?type=browser"]
              .getService(Components.interfaces.nsICommandLineHandler)
              .handle(cmdLine);
  }
}

function CycleWindow( aType )
{
  var topWindowOfType = Services.wm.getMostRecentWindow(aType);
  var topWindow = Services.wm.getMostRecentWindow(null);

  if ( topWindowOfType == null )
    return null;

  if ( topWindowOfType != topWindow ) {
    toOpenWindow(topWindowOfType);
    return topWindowOfType;
  }

  var enumerator = Services.wm.getEnumerator(aType);
  var firstWindow = enumerator.getNext();
  var iWindow = firstWindow;
  while (iWindow != topWindow && enumerator.hasMoreElements())
    iWindow = enumerator.getNext();

  if (enumerator.hasMoreElements()) {
    iWindow = enumerator.getNext();
    toOpenWindow(iWindow);
    return iWindow;
  }

  if (firstWindow == topWindow) // Only one window
    return null;

  toOpenWindow(firstWindow);
  return firstWindow;
}

function ShowWindowFromResource( node )
{
	var windowManagerDS = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService(nsIWindowDataSource);
    
    var desiredWindow = null;
    var url = node.getAttribute('id');
	desiredWindow = windowManagerDS.getWindowForResource( url );
	if ( desiredWindow )
	{
		toOpenWindow(desiredWindow);
	}
}

function OpenTaskURL( inURL )
{
	
	window.open( inURL );
}

function ShowUpdateFromResource( node )
{
	var url = node.getAttribute('url');
        // hack until I get a new interface on xpiflash to do a 
        // look up on the name/url pair.
	OpenTaskURL( "http://www.mozilla.org/binaries.html");
}

function checkFocusedWindow()
{
  var windowManagerDS = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService(nsIWindowDataSource);

  var sep = document.getElementById("sep-window-list");
  // Using double parens to avoid warning
  while ((sep = sep.nextSibling)) {
    var url = sep.getAttribute('id');
    var win = windowManagerDS.getWindowForResource(url);
    if (win == window) {
      sep.setAttribute("checked", "true");
      break;
    }
  }
}

function toProfileManager()
{
  var promgrWin = Services.wm.getMostRecentWindow("mozilla:profileSelection");
  if (promgrWin) {
    promgrWin.focus();
  } else {
    var params = Components.classes["@mozilla.org/embedcomp/dialogparam;1"]
                 .createInstance(Components.interfaces.nsIDialogParamBlock);
  
    params.SetNumberStrings(1);
    params.SetString(0, "menu");
    window.openDialog("chrome://communicator/content/profile/profileSelection.xul",
                "",
                "centerscreen,chrome,titlebar",
                params);
  }
  // Here, we don't care about the result code
  // that was returned in the param block.
}

// This function is used by mac's platformCommunicatorOverlay
function ZoomCurrentWindow()
{
  if (window.windowState == STATE_NORMAL)
    window.maximize();
  else
    window.restore();
}
