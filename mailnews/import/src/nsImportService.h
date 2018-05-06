/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsImportService_h__
#define nsImportService_h__

#include "nsString.h"
#include "nsMemory.h"
#include "nsIImportModule.h"
#include "nsIImportService.h"
#include "nsIStringBundle.h"

class nsImportModuleList;

class nsImportService : public nsIImportService
{
public:

  nsImportService();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIIMPORTSERVICE

private:
  virtual ~nsImportService();
  nsresult LoadModuleInfo(const char*pClsId, const char *pSupports);
  nsresult DoDiscover(void);

private:
  nsImportModuleList * m_pModules;
  bool m_didDiscovery;
  nsCString m_sysCharset;
  nsCOMPtr<nsIStringBundle> m_stringBundle;
};

class ImportModuleDesc {
public:
  ImportModuleDesc() { m_pModule = nullptr;}
  ~ImportModuleDesc() { ReleaseModule();  }

  void  SetCID(const nsCID& cid) { m_cid = cid;}
  void  SetName(const char16_t *pName) { m_name = pName;}
  void  SetDescription(const char16_t *pDesc) { m_description = pDesc;}
  void  SetSupports(const char *pSupports) { m_supports = pSupports;}

  nsCID      GetCID(void) { return m_cid;}
  const char16_t *GetName(void) { return m_name.get();}
  const char16_t *GetDescription(void) { return m_description.get();}
  const char *  GetSupports(void) { return m_supports.get();}

  void        GetModule(nsIImportModule **);
  void        ReleaseModule(void);

  bool        SupportsThings(const char *pThings);

private:
  nsCID m_cid;
  nsString m_name;
  nsString m_description;
  nsCString m_supports;
  nsCOMPtr<nsIImportModule> m_pModule;
};

class nsImportModuleList {
public:
  nsImportModuleList() { m_pList = nullptr; m_alloc = 0; m_count = 0;}
  ~nsImportModuleList() { ClearList(); }

  void  AddModule(const nsCID& cid, const char *pSupports, const char16_t *pName, const char16_t *pDesc);

  void  ClearList(void);

  int32_t  GetCount(void) { return m_count;}

  ImportModuleDesc *  GetModuleDesc(int32_t idx)
    { if ((idx < 0) || (idx >= m_count)) return nullptr; else return m_pList[idx];}

private:

private:
    ImportModuleDesc **  m_pList;
  int32_t        m_alloc;
  int32_t        m_count;
};

#endif // nsImportService_h__
