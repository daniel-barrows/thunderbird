/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsMsgXFVirtualFolderDBView_H_
#define _nsMsgXFVirtualFolderDBView_H_

#include "mozilla/Attributes.h"
#include "nsMsgSearchDBView.h"
#include "nsIMsgCopyServiceListener.h"
#include "nsIMsgSearchNotify.h"
#include "nsCOMArray.h"

class nsMsgGroupThread;

class nsMsgXFVirtualFolderDBView : public nsMsgSearchDBView {
 public:
  nsMsgXFVirtualFolderDBView();
  virtual ~nsMsgXFVirtualFolderDBView();

  // we override all the methods, currently. Might change...
  NS_DECL_NSIMSGSEARCHNOTIFY

  virtual const char *GetViewName(void) override {
    return "XFVirtualFolderView";
  }
  NS_IMETHOD Open(nsIMsgFolder *folder, nsMsgViewSortTypeValue sortType,
                  nsMsgViewSortOrderValue sortOrder,
                  nsMsgViewFlagsTypeValue viewFlags, int32_t *pCount) override;
  NS_IMETHOD CloneDBView(nsIMessenger *aMessengerInstance,
                         nsIMsgWindow *aMsgWindow,
                         nsIMsgDBViewCommandUpdater *aCmdUpdater,
                         nsIMsgDBView **_retval) override;
  NS_IMETHOD CopyDBView(nsMsgDBView *aNewMsgDBView,
                        nsIMessenger *aMessengerInstance,
                        nsIMsgWindow *aMsgWindow,
                        nsIMsgDBViewCommandUpdater *aCmdUpdater) override;
  NS_IMETHOD Close() override;
  NS_IMETHOD GetViewType(nsMsgViewTypeValue *aViewType) override;
  NS_IMETHOD DoCommand(nsMsgViewCommandTypeValue command) override;
  NS_IMETHOD SetViewFlags(nsMsgViewFlagsTypeValue aViewFlags) override;
  NS_IMETHOD OnHdrPropertyChanged(nsIMsgDBHdr *aHdrToChange, bool aPreChange,
                                  uint32_t *aStatus,
                                  nsIDBChangeListener *aInstigator) override;
  NS_IMETHOD GetMsgFolder(nsIMsgFolder **aMsgFolder) override;

  virtual nsresult OnNewHeader(nsIMsgDBHdr *newHdr, nsMsgKey parentKey,
                               bool ensureListed) override;
  void UpdateCacheAndViewForPrevSearchedFolders(nsIMsgFolder *curSearchFolder);
  void UpdateCacheAndViewForFolder(nsIMsgFolder *folder, nsMsgKey *newHits,
                                   uint32_t numNewHits);
  void RemovePendingDBListeners();

 protected:
  virtual nsresult GetMessageEnumerator(
      nsISimpleEnumerator **enumerator) override;

  // array index of next folder with cached hits to deal with.
  uint32_t m_cachedFolderArrayIndex;
  nsCOMArray<nsIMsgFolder> m_foldersSearchingOver;
  nsCOMArray<nsIMsgDBHdr> m_hdrHits;
  nsCOMPtr<nsIMsgFolder> m_curFolderGettingHits;
  // keeps track of the index of the first hit from the cur folder
  uint32_t m_curFolderStartKeyIndex;
  bool m_curFolderHasCachedHits;
  bool m_doingSearch;
  // Are we doing a quick search on top of the virtual folder search?
  bool m_doingQuickSearch;
};

#endif
