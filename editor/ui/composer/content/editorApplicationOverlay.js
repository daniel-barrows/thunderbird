/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
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

/* Implementations of nsIControllerCommand for composer commands */

function initEditorContextMenuItems(aEvent)
{
  var shouldShowEditPage = !gContextMenu.onImage && !gContextMenu.onLink && !gContextMenu.onTextInput && !gContextMenu.inDirList;
  gContextMenu.showItem( "context-editpage", shouldShowEditPage );

  var shouldShowEditLink = gContextMenu.onSaveableLink; 
  gContextMenu.showItem( "context-editlink", shouldShowEditLink );

  // Hide the applications separator if there's no add-on apps present. 
  gContextMenu.showItem("context-sep-apps", gContextMenu.shouldShowSeparator("context-sep-apps"));
}
  
function initEditorContextMenuListener(aEvent)
{
  var popup = document.getElementById("contentAreaContextMenu");
  if (popup)
    popup.addEventListener("popupshowing", initEditorContextMenuItems, false);
}

addEventListener("load", initEditorContextMenuListener, false);

function editDocument(aDocument)      
{
  if (!aDocument)
    aDocument = window.content.document;

  editPage(aDocument.URL); 
}

function editPageOrFrame()
{
  var focusedWindow = document.commandDispatcher.focusedWindow;

  // if the uri is a specific frame, grab it, else use the frameset uri 
  // and let Composer handle error if necessary
  editPage(getContentFrameURI(focusedWindow));
}

function getContentFrameURI(aFocusedWindow)
{
  var contentFrame = isContentFrame(aFocusedWindow) ?
                       aFocusedWindow : window.content;
  return contentFrame.location.href;
}

// Any non-editor window wanting to create an editor with a URL
//   should use this instead of "window.openDialog..."
//  We must always find an existing window with requested URL
function editPage(url, aFileType)
{
  // aFileType is optional and needs to default to html.
  aFileType = aFileType || "html";

  // Always strip off "view-source:" and #anchors
  url = url.replace(/^view-source:/, "").replace(/#.*/, "");

  // if the current window is a browser window, then extract the current charset menu setting from the current 
  // document and use it to initialize the new composer window...

  var wintype = document.documentElement.getAttribute('windowtype');
  var charsetArg;

  if (wintype == "navigator:browser" && content.document)
    charsetArg = "charset=" + content.document.characterSet;

  try {
    var uri = createURI(url, null, null);

    var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
    var windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
    var enumerator = windowManagerInterface.getEnumerator("composer:" + aFileType);
    var emptyWindow;
    while ( enumerator.hasMoreElements() )
    {
      var win = enumerator.getNext();
      if ( win && win.IsWebComposer())
      {
        if (CheckOpenWindowForURIMatch(uri, win))
        {
          // We found an editor with our url
          win.focus();
          return;
        }
        else if (!emptyWindow && win.PageIsEmptyAndUntouched())
        {
          emptyWindow = win;
        }
      }
    }

    if (emptyWindow)
    {
      // we have an empty window we can use
      if (aFileType == "html" && emptyWindow.IsInHTMLSourceMode())
        emptyWindow.SetEditMode(emptyWindow.PreviousNonSourceDisplayMode);
      emptyWindow.EditorLoadUrl(url);
      emptyWindow.focus();
      emptyWindow.SetSaveAndPublishUI(url);
      return;
    }

    // Create new Composer / Text Editor window.
    if (aFileType == "text" && ("EditorNewPlaintext" in window))
      EditorNewPlaintext(url, charsetArg);
    else
      NewEditorWindow(url, charsetArg);

  } catch(e) {}
}

function createURI(urlstring)
{
  try {
    var ioserv = Components.classes["@mozilla.org/network/io-service;1"]
               .getService(Components.interfaces.nsIIOService);
    return ioserv.newURI(urlstring, null, null);
  } catch (e) {}

  return null;
}

function CheckOpenWindowForURIMatch(uri, win)
{
  try {
    var contentWindow = win.content;
    var contentDoc = contentWindow.document;
    var htmlDoc = contentDoc.QueryInterface(Components.interfaces.nsIDOMHTMLDocument);
    var winuri = createURI(htmlDoc.URL);
    return winuri.equals(uri);
  } catch (e) {}
  
  return false;
}

function toEditor()
{
  if (!CycleWindow("composer:html"))
    NewEditorWindow();
}

function NewEditorWindow(aUrl, aCharsetArg)
{
  window.openDialog("chrome://editor/content",
                    "_blank",
                    "chrome,all,dialog=no",
                    aUrl || "about:blank",
                    aCharsetArg);
}

function NewEditorFromTemplate()
{
  // XXX not implemented
}

function NewEditorFromDraft()
{
  // XXX not implemented
}
