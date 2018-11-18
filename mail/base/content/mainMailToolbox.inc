# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

<toolbox id="mail-toolbox"
         class="mail-toolbox"
         mode="full"
         defaultmode="full"
#ifdef XP_MACOSX
         iconsize="small"
         defaulticonsize="small"
#endif
         labelalign="end"
         defaultlabelalign="end">

  <toolbarpalette id="MailToolbarPalette">
    <toolbarbutton id="button-getmsg"
                   type="menu-button"
                   class="toolbarbutton-1"
                   label="&getMsgButton1.label;"
                   tooltiptext="&getMsgButton.tooltip;"
                   oncommand="MsgGetMessagesForAccount();"
                   observes="button_getNewMessages">
      <menupopup id="button-getMsgPopup"
                 onpopupshowing="getMsgToolbarMenu_init();"
                 oncommand="MsgGetMessagesForAccount(event.target._folder); event.stopPropagation();"
                 type="folder"
                 expandFolders="false"
                 mode="getMail">
        <menuitem id="button-getAllNewMsg"
                  label="&getAllNewMsgCmd.label;"
                  accesskey="&getAllNewMsgCmd.accesskey;"
                  command="cmd_getMsgsForAuthAccounts"/>
        <menuseparator id="button-getAllNewMsgSeparator"/>
      </menupopup>
    </toolbarbutton>
    <toolbarbutton id="button-newmsg"
                   class="toolbarbutton-1"
                   label="&newMsgButton.label;"
                   tooltiptext="&newMsgButton.tooltip;"
                   command="cmd_newMessage"/>
    <toolbarbutton id="button-reply"
                   class="toolbarbutton-1"
                   label="&replyButton.label;"
                   tooltiptext="&replyButton.tooltip;"
                   observes="button_reply"
                   command="cmd_reply"/>
    <toolbarbutton id="button-replyall"
                   class="toolbarbutton-1"
                   label="&replyAllButton.label;"
                   tooltiptext="&replyAllButton.tooltip;"
                   observes="button_replyall"
                   command="cmd_replyall"/>
    <toolbarbutton id="button-replylist"
                   class="toolbarbutton-1"
                   label="&replyListButton.label;"
                   tooltiptext="&replyListButton.tooltip;"
                   observes="button_replylist"
                   command="cmd_replylist"/>
    <toolbarbutton id="button-forward"
                   type="menu-button"
                   class="toolbarbutton-1"
                   label="&forwardButton.label;"
                   tooltiptext="&forwardButton.tooltip;"
                   observes="button_forward"
                   command="cmd_forward">
      <menupopup id="button-ForwardPopup">
        <menuitem id="button-ForwardAsInlineMenu"
                  label="&buttonMenuForwardAsInline.label;"
                  tooltiptext="&forwardAsInline.tooltip;"
                  command="cmd_forwardInline"/>
        <menuitem id="button-ForwardAsAttachmentMenu"
                  label="&buttonMenuForwardAsAttachment.label;"
                  tooltiptext="&forwardAsAttachment.tooltip;"
                  command="cmd_forwardAttachment"/>
      </menupopup>
    </toolbarbutton>
    <toolbarbutton id="button-file"
                   type="menu"
                   class="toolbarbutton-1"
                   label="&fileButton.label;"
                   tooltiptext="&fileButton.tooltip;"
                   observes="button_file"
                   oncommand="MsgMoveMessage(event.target._folder)">
      <menupopup id="button-filePopup"
                 type="folder"
                 mode="filing"
                 showRecent="true"
                 showFileHereLabel="true"
                 recentLabel="&moveCopyMsgRecentMenu.label;"
                 recentAccessKey="&moveCopyMsgRecentMenu.accesskey;"/>
    </toolbarbutton>
    <toolbarbutton id="button-archive"
                   class="toolbarbutton-1"
                   label="&archiveButton.label;"
                   tooltiptext="&archiveButton.tooltip;"
                   observes="button_archive"
                   oncommand="MsgArchiveSelectedMessages(event)"/>
    <toolbarbutton id="button-showconversation"
                   class="toolbarbutton-1"
                   label="&openConversationButton.label;"
                   tooltiptext="&openConversationButton.tooltip;"
                   observes="button_showconversation"
                   oncommand="gConversationOpener.openConversationForMessages(gFolderDisplay.selectedMessages)"/>
    <toolbarbutton id="button-goback"
                   class="toolbarbutton-1"
                   type="menu-button"
                   label="&backButton1.label;"
                   oncommand="goDoCommand('cmd_goBack')"
                   tooltiptext="&goBackButton.tooltip;"
                   observes="button_goBack">
      <menupopup id="button-goBackPopup" onpopupshowing="backToolbarMenu_init(this)">
        <menuitem id="button-goBack" label="&goBackCmd.label;" command="cmd_goBack"/>
        <menuseparator id="button-goBackSeparator"/>
      </menupopup>
    </toolbarbutton>
    <toolbarbutton id="button-goforward"
                   class="toolbarbutton-1"
                   type="menu-button"
                   label="&goForwardButton1.label;"
                   oncommand="goDoCommand('cmd_goForward')"
                   tooltiptext="&goForwardButton.tooltip;"
                   observes="button_goForward">
      <menupopup id="button-goForwardPopup" onpopupshowing="forwardToolbarMenu_init(this)">
        <menuitem id="button-goForward"
                  label="&goForwardCmd.label;"
                  command="cmd_goForward"/>
        <menuseparator id="button-goForwardSeparator"/>
      </menupopup>
    </toolbarbutton>
    <toolbaritem id="button-previous"
                 title="&previousButtonToolbarItem.label;"
                 align="center"
                 class="chromeclass-toolbar-additional">
      <toolbarbutton id="button-previousUnread"
                     class="toolbarbutton-1"
                     label="&previousButton.label;"
                     oncommand="goDoCommand('button_previous')"
                     tooltiptext="&previousButton.tooltip;"
                     observes="button_previous"/>
    </toolbaritem>
    <toolbarbutton id="button-previousMsg"
                   class="toolbarbutton-1"
                   label="&previousMsgButton.label;"
                   oncommand="goDoCommand('button_previousMsg')"
                   tooltiptext="&previousMsgButton.tooltip;"
                   observes="button_previousMsg"/>
    <toolbaritem id="button-next"
                 title="&nextButtonToolbarItem.label;"
                 align="center"
                 class="chromeclass-toolbar-additional">
      <toolbarbutton id="button-nextUnread"
                     class="toolbarbutton-1"
                     label="&nextButton.label;"
                     oncommand="goDoCommand('button_next')"
                     tooltiptext="&nextButton.tooltip;"
                     observes="button_next"/>
    </toolbaritem>
    <toolbarbutton id="button-nextMsg"
                   class="toolbarbutton-1"
                   label="&nextMsgButton.label;"
                   oncommand="goDoCommand('button_nextMsg');"
                   tooltiptext="&nextMsgButton.tooltip;"
                   observes="button_nextMsg"/>
    <toolbaritem id="button-junk" title="&junkItem.title;">
      <deck id="junk-deck" observes="button_junk">
        <toolbarbutton id="button-isJunk"
                       class="toolbarbutton-1 junk-button"
                       label="&junkButton.label;"
                       tooltiptext="&junkButton.tooltip;"
                       observes="button_junk"
                       oncommand="goDoCommand('button_junk')"/>
        <toolbarbutton id="button-notJunk"
                       class="toolbarbutton-1 junk-button"
                       label="&notJunkButton.label;"
                       tooltiptext="&notJunkButton.tooltip;"
                       observes="button_junk"
                       oncommand="goDoCommand('button_junk')"/>
      </deck>
    </toolbaritem>
    <toolbaritem id="button-delete" title="&deleteItem.title;">
      <deck id="delete-deck" observes="button_delete">
         <toolbarbutton id="button-mark-deleted"
                        class="toolbarbutton-1 delete-button"
                        label="&deleteButton.label;"
                        tooltiptext="&deleteButton.tooltip;"
                        observes="button_delete"
                        oncommand="goDoCommand(event.shiftKey ? 'button_shiftDelete' : 'button_delete')"/>
         <toolbarbutton id="button-mark-undelete"
                        class="toolbarbutton-1 delete-button"
                        label="&undeleteButton.label;"
                        tooltiptext="&undeleteButton.tooltip;"
                        observes="button_delete"
                        oncommand="goDoCommand('button_delete')"/>
      </deck>
    </toolbaritem>
#ifdef XP_MACOSX
    <toolbarbutton id="button-print"
                   class="toolbarbutton-1"
                   label="&printButton.label;"
                   observes="button_print"
                   oncommand="goDoCommand('cmd_print')"
                   tooltiptext="&printButton.tooltip;"/>
#else
    <toolbarbutton id="button-print"
                   class="toolbarbutton-1"
                   label="&printButton.label;"
                   observes="button_print"
                   oncommand="goDoCommand('cmd_print')"
                   tooltiptext="&printButton.tooltip;"
                   type="menu-button">
      <menupopup id="printMenu" onpopupshowing="goUpdateCommand('cmd_printpreview');">
        <menuitem id="button-printMenu"
                  label="&printCmd.label;"
                  accesskey="&printCmd.accesskey;"
                  default="true"/>
        <menuitem id="button-printPreviewMenu"
                  label="&printPreviewCmd.label;"
                  accesskey="&printPreviewCmd.accesskey;"
                  observes="cmd_printpreview"
                  command="cmd_printpreview"/>
      </menupopup>
    </toolbarbutton>
#endif
    <toolbarbutton id="button-mark"
                   type="menu-button"
                   class="toolbarbutton-1"
                   label="&markButton.label;"
                   oncommand="goDoCommand('button_mark')"
                   observes="button_mark"
                   tooltiptext="&markButton.tooltip;">
      <menupopup id="button-markPopup" onpopupshowing="InitMessageMark()">
        <menuitem id="markReadToolbarItem"
                  label="&markAsReadCmd.label;"
                  accesskey="&markAsReadCmd.accesskey;"
                  command="cmd_markAsRead" />
        <menuitem id="markUnreadToolbarItem"
                  label="&markAsUnreadCmd.label;"
                  accesskey="&markAsUnreadCmd.accesskey;"
                  command="cmd_markAsUnread" />
        <menuitem id="button-markThreadAsRead"
                  label="&markThreadAsReadCmd.label;"
                  key="key_markThreadAsRead"
                  accesskey="&markThreadAsReadCmd.accesskey;"
                  command="cmd_markThreadAsRead"/>
        <menuitem id="button-markReadByDate"
                  label="&markReadByDateCmd.label;"
                  key="key_markReadByDate"
                  accesskey="&markReadByDateCmd.accesskey;"
                  command="cmd_markReadByDate"/>
        <menuitem id="button-markAllRead"
                  label="&markAllReadCmd.label;"
                  key="key_markAllRead"
                  accesskey="&markAllReadCmd.accesskey;"
                  command="cmd_markAllRead"/>
        <menuseparator id="button-markAllReadSeparator"/>
        <menuitem id="markFlaggedToolbarItem"
                  type="checkbox"
                  label="&markStarredCmd.label;"
                  accesskey="&markStarredCmd.accesskey;"
                  key="key_toggleFlagged"
                  command="cmd_markAsFlagged"/>
      </menupopup>
    </toolbarbutton>
    <toolbarbutton id="button-tag"
                   type="menu"
                   class="toolbarbutton-1"
                   label="&tagButton.label;"
                   tooltiptext="&tagButton.tooltip;"
                   command="cmd_tag">
      <menupopup id="button-tagpopup"
                 onpopupshowing="InitMessageTags(this);">
        <menuitem id="button-addNewTag"
                  label="&addNewTag.label;"
                  accesskey="&addNewTag.accesskey;"
                  command="cmd_addTag"/>
        <menuitem id="button-manageTags"
                  label="&manageTags.label;"
                  accesskey="&manageTags.accesskey;"
                  command="cmd_manageTags"/>
        <menuseparator id="button-tagpopup-sep-afterTagAddNew"/>
        <menuitem id="button-tagRemoveAll"
                  command="cmd_removeTags"/>
        <menuseparator id="button-afterTagRemoveAllSeparator"/>
      </menupopup>
    </toolbarbutton>
#ifdef MAIN_WINDOW
    <toolbarbutton id="qfb-show-filter-bar"
                   class="toolbarbutton-1"
                   type="checkbox"
                   label="&quickFilterBar.qfbShowFilterBar.label;"
                   command="cmd_toggleQuickFilterBar">
      <observes element="view_toolbars_popup_quickFilterBar"
                attribute="checked"/>
    </toolbarbutton>
#endif
    <toolbarbutton id="button-address"
                   class="toolbarbutton-1"
                   label="&addressBookButton.label;"
                   oncommand="toAddressBook();"
                   tooltiptext="&addressBookButton.tooltip;"/>
    <toolbarbutton id="button-chat"
                   class="toolbarbutton-1"
                   label="&chatButton.label;"
                   command="cmd_chat"
                   observes="cmd_chat"
                   tooltiptext="&chatButton.tooltip;"/>
    <toolbaritem id="throbber-box" title="&throbberItem.title;" align="center" pack="center"
                 mousethrough="always">
       <image/>
    </toolbaritem>
    <toolbarbutton id="button-stop"
                   class="toolbarbutton-1"
                   label="&stopButton.label;"
                   tooltiptext="&stopButton.tooltip;"
                   command="cmd_stop"/>
    <toolbarbutton id="button-appmenu"
                   class="toolbarbutton-1 button-appmenu"
                   label="&appmenuButton.label;"
                   tooltiptext="&appmenuButton1.tooltip;"/>
    <toolbaritem id="folderpane-mode-selector"
                 title="&folderModeSelector.label;"
                 align="center">
      <menulist id="folderpane-mode-selector-menulist"
                sizetopopup="none"
                crop="center"
                flex="1"
                label="&folderModeSelector.label;"
                oncommand="gFolderTreeView.mode = this.value;">
        <menupopup id="folderpane-mode-selector-popup"
                   onpopupshowing="gFolderTreeView._initFolderModeSelector();"/>
      </menulist>
    </toolbaritem>
#ifdef MAIN_WINDOW
    <!-- gloda search widget; provides global (message) searching.  -->
    <toolbaritem id="gloda-search" insertafter="button-stop"
                 title="&glodaSearch.title;"
                 align="center"
                 flex="1"
                 class="chromeclass-toolbar-additional">
        <textbox id="searchInput"
                 class="searchBox"
                 flex="1"
                 type="glodacomplete"
                 searchbutton="true"
                 autocompletesearch="gloda"
                 autocompletepopup="PopupGlodaAutocomplete"
                 autocompletesearchparam="global"
                 timeout="200"
                 maxlength="192"
                 placeholder=""
                 emptytextbase="&search.label.base1;"
                 keyLabelNonMac="&search.keyLabel.nonmac;"
                 keyLabelMac="&search.keyLabel.mac;"
                 >
          <!-- Mimic the search/clear buttons of the standard search textbox,
               but adjust for the reality that clear doesn't make much sense
               since gloda results only show in a tab and the idiom for closing
               tabs is closing the tab.  Our binding does process escape to
               clear the box, if people want to clear it that way...
               -->
          <hbox>
            <image class="gloda-search-icon"/>
          </hbox>
        </textbox>
    </toolbaritem>
#endif
    <toolbarbutton id="button-compact" class="toolbarbutton-1"
                   insertafter="button-mark"
                   label="&compactButton.label;"
                   tooltiptext="&compactButton.tooltip;"
                   oncommand="goDoCommand('button_compact');"
                   observes="button_compact"/>
    <toolbaritem id="folder-location-container" insert-after="button-stop"
                 title="&folderLocationToolbarItem.title;"
                 align="center"
                 class="chromeclass-toolbar-additional">
      <menulist id="locationFolders"
                class="folderMenuItem"
                label="&folderLocationToolbarItem.title;"
                context="folderPaneContext"
                maxwidth="300"
                crop="center">
        <menupopup id="folderLocationPopup"
                   class="menulist-menupopup"
                   type="folder"
                   flex="1"
                   mode="notDeferred"
                   showFileHereLabel="true"
                   oncommand="gFolderTreeView.selectFolder(event.target._folder, true);"/>
      </menulist>
    </toolbaritem>
    <toolbaritem id="mailviews-container"
                 title="&mailViewsToolbarItem.title;"
                 align="center"
                 class="chromeclass-toolbar-additional">
      <label id="viewPickerLabel"
             value="&viewPicker.label;"
             control="viewPicker"
             accesskey="&viewPicker.accesskey;"/>
      <menulist id="viewPicker" oncommand="ViewChangeByMenuitem(event.target);">
        <menupopup id="viewPickerPopup"
                   onpopupshowing="RefreshViewPopup(this);">
          <menuitem id="viewPickerAll" value="0"
                    label="&viewAll.label;"
                    type="radio"/>
          <menuitem id="viewPickerUnread" value="1"
                    label="&viewUnread.label;"
                    type="radio"/>
          <menuitem id="viewPickerNotDeleted" value="3"
                    label="&viewNotDeleted.label;"
                    type="radio"/>
          <menuseparator id="afterViewPickerUnreadSeparator"/>
          <menu id="viewPickerTags" label="&viewTags.label;">
            <menupopup id="viewPickerTagsPopup"
                       class="menulist-menupopup"
                       onpopupshowing="RefreshTagsPopup(this, true);"/>
          </menu>
          <menu id="viewPickerCustomViews" label="&viewCustomViews.label;">
            <menupopup id="viewPickerCustomViewsPopup"
                       class="menulist-menupopup"
                       onpopupshowing="RefreshCustomViewsPopup(this, true);"/>
          </menu>
          <menuseparator id="afterViewPickerCustomViewsSeparator"/>
          <menuitem id="viewPickerVirtualFolder"
                    value="7"
                    label="&viewVirtualFolder.label;"/>
          <menuitem id="viewPickerCustomize"
                    value="8"
                    label="&viewCustomizeView.label;"/>
        </menupopup>
      </menulist>
    </toolbaritem>

  </toolbarpalette>

  <!-- If changes are made to the default set of toolbar buttons, you may need to rev the id
       of mail-bar in order to force the new default items to show up for users who customized their toolbar
       in earlier versions. Bumping the id means users will have to re-customize their toolbar!
  -->

  <toolbar id="mail-bar3"
           class="inline-toolbar chromeclass-toolbar"
           toolbarname="&showMessengerToolbarCmd.label;"
           accesskey="&showMessengerToolbarCmd.accesskey;"
           fullscreentoolbar="true" mode="full"
           customizable="true"
           context="toolbar-context-menu"
#ifdef XP_MACOSX
           iconsize="small"
           defaultset="button-getmsg,button-newmsg,button-chat,button-address,spacer,button-tag,qfb-show-filter-bar,spring,gloda-search,button-appmenu">
#else
           defaultset="button-getmsg,button-newmsg,button-chat,button-address,separator,button-tag,qfb-show-filter-bar,spring,gloda-search,button-appmenu">
#endif
  </toolbar>

  <toolbarset id="customToolbars" context="toolbar-context-menu"/>
</toolbox>