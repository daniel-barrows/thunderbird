# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

  <menupopup id="messageIdContext" popupanchor="bottomleft"
             onpopupshowing="FillMessageIdContextMenu(document.popupNode);">
    <menuitem id="messageIdContext-messageIdTarget"
              disabled="true"/>
    <menuseparator id="messageIdContext-separator"/>
    <menuitem id="messageIdContext-openMessageForMsgId"
              label="&OpenMessageForMsgId.label;"
              accesskey="&OpenMessageForMsgId.accesskey;"
              oncommand="var messageId = GetMessageIdFromNode(document.popupNode, true);
                         OpenMessageForMessageId(messageId)"/>
    <menuitem id="messageIdContext-openBrowserWithMsgId"
              label="&OpenBrowserWithMsgId.label;"
              accesskey="&OpenBrowserWithMsgId.accesskey;"
              oncommand="var messageId = GetMessageIdFromNode(document.popupNode, true);
                         OpenBrowserWithMessageId(messageId)"/>
    <menuitem id="messageIdContext-copyMessageId"
              label="&CopyMessageId.label;"
              accesskey="&CopyMessageId.accesskey;"
              oncommand="var messageId = GetMessageIdFromNode(document.popupNode, false);
                         CopyMessageId(messageId);"/>
  </menupopup>

  <menupopup id="attachmentItemContext"
             onpopupshowing="return onShowAttachmentItemContextMenu();"
             onpopuphiding="return onHideAttachmentItemContextMenu();">
    <menuitem id="context-openAttachment"
              label="&openAttachmentCmd.label;"
              accesskey="&openAttachmentCmd.accesskey;"
              oncommand="HandleMultipleAttachments(this.parentNode.attachments, 'open');"/>
    <menuitem id="context-saveAttachment"
              label="&saveAsAttachmentCmd.label;"
              accesskey="&saveAsAttachmentCmd.accesskey;"
              oncommand="HandleMultipleAttachments(this.parentNode.attachments, 'saveAs');"/>
    <menuseparator id="context-menu-separator"/>
    <menuitem id="context-detachAttachment"
              label="&detachAttachmentCmd.label;"
              accesskey="&detachAttachmentCmd.accesskey;"
              oncommand="HandleMultipleAttachments(this.parentNode.attachments, 'detach');"/>
    <menuitem id="context-deleteAttachment"
              label="&deleteAttachmentCmd.label;"
              accesskey="&deleteAttachmentCmd.accesskey;"
              oncommand="HandleMultipleAttachments(this.parentNode.attachments, 'delete');"/>
    <menuseparator id="context-menu-copyurl-separator"/>
    <menuitem id="context-copyAttachmentUrl"
              label="&copyLinkCmd.label;"
              accesskey="&copyLinkCmd.accesskey;"
              oncommand="HandleMultipleAttachments(this.parentNode.attachments, 'copyUrl');"/>
  </menupopup>

  <menupopup id="attachmentListContext"
             onpopupshowing="goUpdateAttachmentCommands();">
    <menuitem id="context-openAllAttachments"
              label="&openAllAttachmentsCmd.label;"
              accesskey="&openAllAttachmentsCmd.accesskey;"
              command="cmd_openAllAttachments"/>
    <menuitem id="context-saveAllAttachments"
              label="&saveAllAttachmentsCmd.label;"
              accesskey="&saveAllAttachmentsCmd.accesskey;"
              command="cmd_saveAllAttachments"/>
    <menuseparator id="context-menu-separator-all"/>
    <menuitem id="context-detachAllAttachments"
              label="&detachAllAttachmentsCmd.label;"
              accesskey="&detachAllAttachmentsCmd.accesskey;"
              command="cmd_detachAllAttachments"/>
    <menuitem id="context-deleteAllAttachments"
              label="&deleteAllAttachmentsCmd.label;"
              accesskey="&deleteAllAttachmentsCmd.accesskey;"
              command="cmd_deleteAllAttachments"/>
  </menupopup>

  <menupopup id="attachmentMenuList"
             onpopupshowing="FillAttachmentListPopup(event, this);">
     <menuseparator/>
     <menuitem label="&openAllAttachmentsCmd.label;"
               accesskey="&openAllAttachmentsCmd.accesskey;"
               command="cmd_openAllAttachments" />
     <menuitem label="&saveAllAttachmentsCmd.label;"
               accesskey="&saveAllAttachmentsCmd.accesskey;"
               command="cmd_saveAllAttachments"/>
     <menuitem label="&detachAllAttachmentsCmd.label;"
               accesskey="&detachAllAttachmentsCmd.accesskey;"
               command="cmd_detachAllAttachments" />
     <menuitem label="&deleteAllAttachmentsCmd.label;"
               accesskey="&deleteAllAttachmentsCmd.accesskey;"
               command="cmd_deleteAllAttachments" />
  </menupopup>

  <menupopup id="appmenu_attachmentMenuList"
             onpopupshowing="FillAttachmentListPopup(event, this);">
     <menuseparator/>
     <menuitem label="&openAllAttachmentsCmd.label;"
               accesskey="&openAllAttachmentsCmd.accesskey;"
               command="cmd_openAllAttachments" />
     <menuitem label="&saveAllAttachmentsCmd.label;"
               accesskey="&saveAllAttachmentsCmd.accesskey;"
               command="cmd_saveAllAttachments"/>
     <menuitem label="&detachAllAttachmentsCmd.label;"
               accesskey="&detachAllAttachmentsCmd.accesskey;"
               command="cmd_detachAllAttachments" />
     <menuitem label="&deleteAllAttachmentsCmd.label;"
               accesskey="&deleteAllAttachmentsCmd.accesskey;"
               command="cmd_deleteAllAttachments" />
  </menupopup>

  <menupopup id="header-toolbar-context-menu">
    <menuitem id="CustomizeHeaderToolbar"
              oncommand="CustomizeMailToolbar('header-view-toolbox', 'CustomizeHeaderToolbar')"
              label="&customizeToolbar.label;"
              accesskey="&customizeToolbar.accesskey;"/>
  </menupopup>

  <menupopup id="attachment-toolbar-context-menu"
             onpopupshowing="return onShowAttachmentToolbarContextMenu(event);">
    <menuitem id="context-expandAttachmentBar"
              type="checkbox"
              label="&startExpandedCmd.label;"
              accesskey="&startExpandedCmd.accesskey;"
              oncommand="Services.prefs.setBoolPref('mailnews.attachments.display.start_expanded', this.getAttribute('checked'));"/>
    <menuseparator/>
    <menuitem id="CustomizeAttachmentToolbar"
              oncommand="CustomizeMailToolbar('attachment-view-toolbox', 'CustomizeAttachmentToolbar')"
              label="&customizeToolbar.label;"
              accesskey="&customizeToolbar.accesskey;"/>
  </menupopup>

  <menupopup id="copyUrlPopup"
             popupanchor="bottomleft">
    <menuitem label="&copyLinkCmd.label;"
              accesskey="&copyLinkCmd.accesskey;"
              oncommand="CopyWebsiteAddress(document.popupNode)"/>
  </menupopup>