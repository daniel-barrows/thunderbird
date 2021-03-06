/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "msgCore.h"
#include "nsMsgFolderCacheElement.h"
#include "prmem.h"
#include "nsMsgUtils.h"

nsMsgFolderCacheElement::nsMsgFolderCacheElement() {
  m_mdbRow = nullptr;
  m_owningCache = nullptr;
}

nsMsgFolderCacheElement::~nsMsgFolderCacheElement() {}

NS_IMPL_ISUPPORTS(nsMsgFolderCacheElement, nsIMsgFolderCacheElement)

NS_IMETHODIMP nsMsgFolderCacheElement::GetKey(nsACString &aFolderKey) {
  aFolderKey = m_folderKey;
  return NS_OK;
}

NS_IMETHODIMP nsMsgFolderCacheElement::SetKey(const nsACString &aFolderKey) {
  m_folderKey = aFolderKey;
  return NS_OK;
}

void nsMsgFolderCacheElement::SetOwningCache(nsMsgFolderCache *owningCache) {
  m_owningCache = owningCache;
}

NS_IMETHODIMP nsMsgFolderCacheElement::GetStringProperty(
    const char *propertyName, nsACString &result) {
  NS_ENSURE_ARG_POINTER(propertyName);
  NS_ENSURE_TRUE(m_mdbRow && m_owningCache, NS_ERROR_FAILURE);

  mdb_token property_token;
  nsresult ret = m_owningCache->GetStore()->StringToToken(
      m_owningCache->GetEnv(), propertyName, &property_token);
  if (NS_SUCCEEDED(ret))
    ret =
        m_owningCache->RowCellColumnToCharPtr(m_mdbRow, property_token, result);
  return ret;
}

NS_IMETHODIMP nsMsgFolderCacheElement::GetInt32Property(
    const char *propertyName, int32_t *aResult) {
  NS_ENSURE_ARG_POINTER(propertyName);
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_TRUE(m_mdbRow, NS_ERROR_FAILURE);

  nsCString resultStr;
  GetStringProperty(propertyName, resultStr);
  if (resultStr.IsEmpty()) return NS_ERROR_FAILURE;

  // This must be an inverse function to nsCString.AppentInt(),
  // which uses snprintf("%x") internally, so that the wrapped negative numbers
  // are decoded properly.
  if (PR_sscanf(resultStr.get(), "%x", aResult) != 1) {
    NS_WARNING("Unexpected failure to decode hex string.");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgFolderCacheElement::GetInt64Property(
    const char *propertyName, int64_t *aResult) {
  NS_ENSURE_ARG_POINTER(propertyName);
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_TRUE(m_mdbRow, NS_ERROR_FAILURE);

  nsCString resultStr;
  GetStringProperty(propertyName, resultStr);
  if (resultStr.IsEmpty()) return NS_ERROR_FAILURE;

  // This must be an inverse function to nsCString.AppentInt(),
  // which uses snprintf("%x") internally, so that the wrapped negative numbers
  // are decoded properly.
  if (PR_sscanf(resultStr.get(), "%llx", aResult) != 1) {
    NS_WARNING("Unexpected failure to decode hex string.");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP nsMsgFolderCacheElement::SetStringProperty(
    const char *propertyName, const nsACString &propertyValue) {
  NS_ENSURE_ARG_POINTER(propertyName);
  NS_ENSURE_TRUE(m_mdbRow, NS_ERROR_FAILURE);
  nsresult rv = NS_OK;
  mdb_token property_token;

  if (m_owningCache) {
    rv = m_owningCache->GetStore()->StringToToken(
        m_owningCache->GetEnv(), propertyName, &property_token);
    if (NS_SUCCEEDED(rv)) {
      struct mdbYarn yarn;

      yarn.mYarn_Grow = NULL;
      if (m_mdbRow) {
        nsCString propertyVal(propertyValue);
        yarn.mYarn_Buf = (void *)propertyVal.get();
        yarn.mYarn_Size = strlen((const char *)yarn.mYarn_Buf) + 1;
        yarn.mYarn_Fill = yarn.mYarn_Size - 1;
        yarn.mYarn_Form =
            0;  // what to do with this? we're storing csid in the msg hdr...
        rv =
            m_mdbRow->AddColumn(m_owningCache->GetEnv(), property_token, &yarn);
        return rv;
      }
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgFolderCacheElement::SetInt32Property(
    const char *propertyName, int32_t propertyValue) {
  NS_ENSURE_ARG_POINTER(propertyName);
  NS_ENSURE_TRUE(m_mdbRow, NS_ERROR_FAILURE);

  // This also supports encoding negative numbers into hex
  // by integer wrapping them (e.g. -1 -> "ffffffff").
  nsAutoCString propertyStr;
  propertyStr.AppendInt(propertyValue, 16);
  return SetStringProperty(propertyName, propertyStr);
}

NS_IMETHODIMP nsMsgFolderCacheElement::SetInt64Property(
    const char *propertyName, int64_t propertyValue) {
  NS_ENSURE_ARG_POINTER(propertyName);
  NS_ENSURE_TRUE(m_mdbRow, NS_ERROR_FAILURE);

  // This also supports encoding negative numbers into hex
  // by integer wrapping them (e.g. -1 -> "ffffffffffffffff").
  nsAutoCString propertyStr;
  propertyStr.AppendInt(propertyValue, 16);
  return SetStringProperty(propertyName, propertyStr);
}

void nsMsgFolderCacheElement::SetMDBRow(nsIMdbRow *row) { m_mdbRow = row; }
