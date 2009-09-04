/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Howard Chu <hyc@highlandsun.com>
 *   David Bienvenu <bienvenu@nventure.com>
 *   Christian Eyrich <ch.ey@gmx.net>
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
 * ***** END LICENSE BLOCK *****
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Jason Eager <jce2@po.cwru.edu>
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 * 06/07/2000       Jason Eager    Added check for out of disk space
 */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#include "nscore.h"
#include "msgCore.h"    // precompiled header...
#include "nsNetUtil.h"
#include "nspr.h"
#include "plbase64.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsPop3Protocol.h"
#include "MailNewsTypes.h"
#include "nsStringGlue.h"
#include "nsIPrompt.h"
#include "nsIMsgIncomingServer.h"
#include "nsTextFormatter.h"
#include "nsCOMPtr.h"
#include "nsIMsgWindow.h"
#include "nsIMsgFolder.h" // TO include biffState enum. Change to bool later...
#include "nsIDocShell.h"
#include "nsMsgUtils.h"
#include "nsISignatureVerifier.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIPrefLocalizedString.h"
#include "nsISocketTransport.h"
#include "nsISSLSocketControl.h"
#include "nsILineInputStream.h"
#include "nsLocalStrings.h"
#include "nsIInterfaceRequestor.h"
#include "nsMsgMessageFlags.h"

#define EXTRA_SAFETY_SPACE 3096

static PRLogModuleInfo *POP3LOGMODULE = nsnull;


static PRIntn
net_pop3_remove_messages_marked_delete(PLHashEntry* he,
                                       PRIntn msgindex,
                                       void *arg)
{
  Pop3UidlEntry *uidlEntry = (Pop3UidlEntry *) he->value;
  return (uidlEntry->status == DELETE_CHAR)
    ? HT_ENUMERATE_REMOVE : HT_ENUMERATE_NEXT;
}

PRUint32 TimeInSecondsFromPRTime(PRTime prTime)
{
  PRUint32 retTimeInSeconds;

  PRInt64 microSecondsPerSecond, intermediateResult;
  LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
  LL_DIV(intermediateResult, prTime, microSecondsPerSecond);
  LL_L2UI(retTimeInSeconds, intermediateResult);
  return retTimeInSeconds;
}

static void
put_hash(PLHashTable* table, const char* key, char value, PRUint32 dateReceived)
{
  // don't put not used slots or empty uid into hash
  if (key && *key)
  {
    Pop3UidlEntry* tmp = PR_NEWZAP(Pop3UidlEntry);
    if (tmp)
    {
      tmp->uidl = PL_strdup(key);
      if (tmp->uidl)
      {
        tmp->dateReceived = dateReceived;
        tmp->status = value;
        PL_HashTableAdd(table, (const void *)tmp->uidl, (void*) tmp);
      }
      else
        PR_Free(tmp);
    }
  }
}

static PRIntn
net_pop3_copy_hash_entries(PLHashEntry* he, PRIntn msgindex, void *arg)
{
  Pop3UidlEntry *uidlEntry = (Pop3UidlEntry *) he->value;
  put_hash((PLHashTable *) arg, uidlEntry->uidl, uidlEntry->status, uidlEntry->dateReceived);
  return HT_ENUMERATE_NEXT;
}

static void *
AllocUidlTable(void * /* pool */, PRSize size)
{
  return PR_MALLOC(size);
}

static void
FreeUidlTable(void * /* pool */, void *item)
{
    PR_Free(item);
}

static PLHashEntry *
AllocUidlInfo(void *pool, const void *key)
{
    return PR_NEWZAP(PLHashEntry);
}

static void
FreeUidlInfo(void * /* pool */, PLHashEntry *he, PRUintn flag)
{
  if (flag == HT_FREE_ENTRY)
  {
    Pop3UidlEntry *uidlEntry = (Pop3UidlEntry *) he->value;
    if (uidlEntry)
    {
      PR_Free(uidlEntry->uidl);
      PR_Free(uidlEntry);
    }
    PR_Free(he);
  }
}

static PLHashAllocOps gHashAllocOps = {
    AllocUidlTable, FreeUidlTable,
    AllocUidlInfo, FreeUidlInfo
};


static Pop3UidlHost*
net_pop3_load_state(const char* searchhost,
                    const char* searchuser,
                    nsILocalFile *mailDirectory)
{
  Pop3UidlHost* result = nsnull;
  Pop3UidlHost* current = nsnull;
  Pop3UidlHost* tmp;

  result = PR_NEWZAP(Pop3UidlHost);
  if (!result)
    return nsnull;
  result->host = PL_strdup(searchhost);
  result->user = PL_strdup(searchuser);
  result->hash = PL_NewHashTable(20, PL_HashString, PL_CompareStrings, PL_CompareValues, &gHashAllocOps, nsnull);

  if (!result->host || !result->user || !result->hash)
  {
    PR_Free(result->host);
    PR_Free(result->user);
    if (result->hash)
      PL_HashTableDestroy(result->hash);
    PR_Free(result);
    return nsnull;
  }

  nsCOMPtr <nsIFile> clonedDirectory;
  mailDirectory->Clone(getter_AddRefs(clonedDirectory));
  if (!clonedDirectory)
    return nsnull;
  nsCOMPtr <nsILocalFile> popState = do_QueryInterface(clonedDirectory);
  popState->AppendNative(NS_LITERAL_CSTRING("popstate.dat"));

  nsCOMPtr<nsIInputStream> fileStream;
  nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(fileStream), popState);
  NS_ENSURE_SUCCESS(rv, result);

  nsCOMPtr<nsILineInputStream> lineInputStream(do_QueryInterface(fileStream, &rv));
  NS_ENSURE_SUCCESS(rv, result);

  PRBool more = PR_TRUE;
  nsCString line;

  while (more && NS_SUCCEEDED(rv))
  {
    lineInputStream->ReadLine(line, &more);
    if (line.IsEmpty())
      continue;
    char firstChar = line.CharAt(0);
    if (firstChar == '#')
      continue;
    if (firstChar == '*') {
      /* It's a host&user line. */
      current = nsnull;
      char *lineBuf = line.BeginWriting() + 1; // ok because we know the line isn't empty
      char *host = NS_strtok(" \t\r\n", &lineBuf);
      /* without space to also get realnames - see bug 225332 */
      char *user = NS_strtok("\t\r\n", &lineBuf);
      if (!host || !user)
        continue;
      for (tmp = result ; tmp ; tmp = tmp->next)
      {
        if (!strcmp(host, tmp->host) && !strcmp(user, tmp->user))
        {
          current = tmp;
          break;
        }
      }
      if (!current)
      {
        current = PR_NEWZAP(Pop3UidlHost);
        if (current)
        {
          current->host = strdup(host);
          current->user = strdup(user);
          current->hash = PL_NewHashTable(20, PL_HashString, PL_CompareStrings, PL_CompareValues, &gHashAllocOps, nsnull);
          if (!current->host || !current->user || !current->hash)
          {
            PR_Free(current->host);
            PR_Free(current->user);
            if (current->hash)
              PL_HashTableDestroy(current->hash);
            PR_Free(current);
          }
          else
          {
            current->next = result->next;
            result->next = current;
          }
        }
      }
    }
    else
    {
      /* It's a line with a UIDL on it. */
      if (current)
      {
        for (PRInt32 pos = line.FindChar('\t'); pos != -1; pos = line.FindChar('\t', pos))
          line.Replace(pos, 1, ' ');

        nsTArray<nsCString> lineElems;
        ParseString(line, ' ', lineElems);
        if (lineElems.Length() < 2)
          continue;
        nsCString *flags = &lineElems[0];
        nsCString *uidl = &lineElems[1];
        PRUint32 dateReceived = TimeInSecondsFromPRTime(PR_Now()); // if we don't find a date str, assume now.
        if (lineElems.Length() > 2)
          dateReceived = atoi(lineElems[2].get());
        if (!flags->IsEmpty() && !uidl->IsEmpty())
        {
          char flag = flags->CharAt(0);
          if ((flag == KEEP) || (flag == DELETE_CHAR) ||
            (flag == TOO_BIG) || (flag == FETCH_BODY))
          {
            put_hash(current->hash, uidl->get(), flag, dateReceived);
          }
          else
          {
            NS_ASSERTION(PR_FALSE, "invalid flag in popstate.dat");
          }
        }
      }
    }
  }
  fileStream->Close();

  return result;
}

static PRIntn
hash_clear_mapper(PLHashEntry* he, PRIntn msgindex, void* arg)
{
  Pop3UidlEntry *uidlEntry = (Pop3UidlEntry *) he->value;
  PR_Free(uidlEntry->uidl);
  PR_Free(uidlEntry);
  he->value = nsnull;

  return HT_ENUMERATE_REMOVE;
}

static PRIntn
hash_empty_mapper(PLHashEntry* he, PRIntn msgindex, void* arg)
{
  *((PRBool*) arg) = PR_FALSE;
  return HT_ENUMERATE_STOP;
}

static PRBool
hash_empty(PLHashTable* hash)
{
  PRBool result = PR_TRUE;
  PL_HashTableEnumerateEntries(hash, hash_empty_mapper, (void *)&result);
  return result;
}


static PRIntn
net_pop3_write_mapper(PLHashEntry* he, PRIntn msgindex, void* arg)
{
  nsIOutputStream* file = (nsIOutputStream*) arg;
  Pop3UidlEntry *uidlEntry = (Pop3UidlEntry *) he->value;
  NS_ASSERTION((uidlEntry->status == KEEP) ||
    (uidlEntry->status == DELETE_CHAR) ||
    (uidlEntry->status == FETCH_BODY) ||
    (uidlEntry->status == TOO_BIG), "invalid status");
  char* tmpBuffer = PR_smprintf("%c %s %d" MSG_LINEBREAK, uidlEntry->status, (char*)
    uidlEntry->uidl, uidlEntry->dateReceived);
  PR_ASSERT(tmpBuffer);
  PRUint32 numBytesWritten;
  file->Write(tmpBuffer, strlen(tmpBuffer), &numBytesWritten);
  PR_Free(tmpBuffer);
  return HT_ENUMERATE_NEXT;
}

static PRIntn
net_pop3_delete_old_msgs_mapper(PLHashEntry* he, PRIntn msgindex, void* arg)
{
  PRTime cutOffDate = (PRTime) arg;
  Pop3UidlEntry *uidlEntry = (Pop3UidlEntry *) he->value;
  if (uidlEntry->dateReceived < cutOffDate)
    uidlEntry->status = DELETE_CHAR; // mark for deletion
  return HT_ENUMERATE_NEXT;
}

static void
net_pop3_write_state(Pop3UidlHost* host, nsILocalFile *mailDirectory)
{
  PRInt32 len = 0;
  nsCOMPtr <nsIFile> clonedDirectory;

  mailDirectory->Clone(getter_AddRefs(clonedDirectory));
  if (!clonedDirectory)
    return;
  nsCOMPtr <nsILocalFile> popState = do_QueryInterface(clonedDirectory);
  popState->AppendNative(NS_LITERAL_CSTRING("popstate.dat"));

  nsCOMPtr<nsIOutputStream> fileOutputStream;
  nsresult rv = NS_NewLocalFileOutputStream(getter_AddRefs(fileOutputStream), popState, -1, 00600);
  if (NS_FAILED(rv))
    return;

  const char tmpBuffer[] =
    "# POP3 State File" MSG_LINEBREAK
    "# This is a generated file!  Do not edit." MSG_LINEBREAK
    MSG_LINEBREAK;

  PRUint32 numBytesWritten;
  fileOutputStream->Write(tmpBuffer, strlen(tmpBuffer), &numBytesWritten);

  for (; host && (len >= 0); host = host->next)
  {
    if (!hash_empty(host->hash))
    {
      fileOutputStream->Write("*", 1, &numBytesWritten);
      fileOutputStream->Write(host->host, strlen(host->host), &numBytesWritten);
      fileOutputStream->Write(" ", 1, &numBytesWritten);
      fileOutputStream->Write(host->user, strlen(host->user), &numBytesWritten);
      fileOutputStream->Write(MSG_LINEBREAK, MSG_LINEBREAK_LEN, &numBytesWritten);
      PL_HashTableEnumerateEntries(host->hash, net_pop3_write_mapper, (void *)fileOutputStream);
    }
  }
  fileOutputStream->Close();
}

static void
net_pop3_free_state(Pop3UidlHost* host)
{
  Pop3UidlHost* h;
  while (host)
  {
    h = host->next;
    PR_Free(host->host);
    PR_Free(host->user);
    PL_HashTableDestroy(host->hash);
    PR_Free(host);
    host = h;
  }
}

/*
Look for a specific UIDL string in our hash tables, if we have it then we need
to mark the message for deletion so that it can be deleted later. If the uidl of the
message is not found, then the message was downloaded completely and already deleted
from the server. So this only applies to messages kept on the server or too big
for download. */
/* static */
void nsPop3Protocol::MarkMsgInHashTable(PLHashTable *hashTable, const Pop3UidlEntry *uidlE, PRBool *changed)
{
  if (uidlE->uidl)
  {
    Pop3UidlEntry *uidlEntry = (Pop3UidlEntry *) PL_HashTableLookup(hashTable, uidlE->uidl);
    if (uidlEntry)
    {
      if (uidlEntry->status != uidlE->status)
      {
        uidlEntry->status = uidlE->status;
        *changed = PR_TRUE;
      }
    }
  }
}

/* static */
nsresult
nsPop3Protocol::MarkMsgForHost(const char *hostName, const char *userName,
                                      nsILocalFile *mailDirectory,
                                       nsVoidArray &UIDLArray)
{
  if (!hostName || !userName || !mailDirectory)
    return NS_ERROR_NULL_POINTER;

  Pop3UidlHost *uidlHost = net_pop3_load_state(hostName, userName, mailDirectory);
  if (!uidlHost)
    return NS_ERROR_OUT_OF_MEMORY;

  PRBool changed = PR_FALSE;

  PRUint32 count = UIDLArray.Count();
  for (PRUint32 i = 0; i < count; i++)
  {
    MarkMsgInHashTable(uidlHost->hash,
      static_cast<Pop3UidlEntry*>(UIDLArray[i]), &changed);
  }

  if (changed)
    net_pop3_write_state(uidlHost, mailDirectory);
  net_pop3_free_state(uidlHost);
  return NS_OK;
}



NS_IMPL_ADDREF_INHERITED(nsPop3Protocol, nsMsgProtocol)
NS_IMPL_RELEASE_INHERITED(nsPop3Protocol, nsMsgProtocol)

NS_INTERFACE_MAP_BEGIN(nsPop3Protocol)
  NS_INTERFACE_MAP_ENTRY(nsIPop3Protocol)
NS_INTERFACE_MAP_END_INHERITING(nsMsgProtocol)

// nsPop3Protocol class implementation

nsPop3Protocol::nsPop3Protocol(nsIURI* aURL)
: nsMsgProtocol(aURL),
  m_bytesInMsgReceived(0),
  m_totalFolderSize(0),
  m_totalDownloadSize(0),
  m_totalBytesReceived(0),
  m_lineStreamBuffer(nsnull),
  m_pop3ConData(nsnull),
  m_password_already_sent(PR_FALSE)
{
}

nsresult nsPop3Protocol::Initialize(nsIURI * aURL)
{
  nsresult rv = NS_OK;

  m_pop3ConData = (Pop3ConData *)PR_NEWZAP(Pop3ConData);
  if(!m_pop3ConData)
    return NS_ERROR_OUT_OF_MEMORY;

  m_totalBytesReceived = 0;
  m_bytesInMsgReceived = 0;
  m_totalFolderSize = 0;
  m_totalDownloadSize = 0;
  m_totalBytesReceived = 0;
  m_tlsEnabled = PR_FALSE;
  m_socketType = nsIMsgIncomingServer::tryTLS;

  if (aURL)
  {
    // extract out message feedback if there is any.
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(aURL);
    if (mailnewsUrl)
    {
      nsCOMPtr<nsIMsgIncomingServer> server;
      mailnewsUrl->GetStatusFeedback(getter_AddRefs(m_statusFeedback));
      mailnewsUrl->GetServer(getter_AddRefs(server));
      NS_ENSURE_TRUE(server, NS_MSG_INVALID_OR_MISSING_SERVER);

      rv = server->GetSocketType(&m_socketType);
      NS_ENSURE_SUCCESS(rv,rv);

      rv = server->GetUseSecAuth(&m_useSecAuth);
      NS_ENSURE_SUCCESS(rv,rv);

      m_pop3Server = do_QueryInterface(server);
      if (m_pop3Server)
        m_pop3Server->GetPop3CapabilityFlags(&m_pop3ConData->capability_flags);
    }

    m_url = do_QueryInterface(aURL);

    // When we are making a secure connection, we need to make sure that we
    // pass an interface requestor down to the socket transport so that PSM can
    // retrieve a nsIPrompt instance if needed.
    nsCOMPtr<nsIInterfaceRequestor> ir;
    if (m_socketType != nsIMsgIncomingServer::defaultSocket)
    {
      nsCOMPtr<nsIMsgWindow> msgwin;
      mailnewsUrl->GetMsgWindow(getter_AddRefs(msgwin));
      if (msgwin)
      {
        nsCOMPtr<nsIDocShell> docshell;
        msgwin->GetRootDocShell(getter_AddRefs(docshell));
        ir = do_QueryInterface(docshell);
        nsCOMPtr<nsIInterfaceRequestor> notificationCallbacks;
        msgwin->GetNotificationCallbacks(getter_AddRefs(notificationCallbacks));
        if (notificationCallbacks)
        {
          nsCOMPtr<nsIInterfaceRequestor> aggregrateIR;
          NS_NewInterfaceRequestorAggregation(notificationCallbacks, ir, getter_AddRefs(aggregrateIR));
          ir = aggregrateIR;
        }
      }
    }

    PRInt32 port = 0;
    nsCString hostName;
    aURL->GetPort(&port);
    nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_pop3Server);
    if (server)
      server->GetRealHostName(hostName);

    nsCOMPtr<nsIProxyInfo> proxyInfo;
    rv = MsgExamineForProxy("pop", hostName.get(), port, getter_AddRefs(proxyInfo));
    if (NS_FAILED(rv)) proxyInfo = nsnull;

    const char *connectionType = nsnull;
    if (m_socketType == nsIMsgIncomingServer::useSSL)
      connectionType = "ssl";
    else if (m_socketType == nsIMsgIncomingServer::tryTLS ||
          m_socketType == nsIMsgIncomingServer::alwaysUseTLS)
        connectionType = "starttls";

    rv = OpenNetworkSocketWithInfo(hostName.get(), port, connectionType, proxyInfo, ir);
    if (NS_FAILED(rv) && m_socketType == nsIMsgIncomingServer::tryTLS)
    {
      m_socketType = nsIMsgIncomingServer::defaultSocket;
      rv = OpenNetworkSocketWithInfo(hostName.get(), port, nsnull, proxyInfo, ir);
    }

    if(NS_FAILED(rv))
      return rv;
  } // if we got a url...

  if (!POP3LOGMODULE)
      POP3LOGMODULE = PR_NewLogModule("POP3");

  m_lineStreamBuffer = new nsMsgLineStreamBuffer(OUTPUT_BUFFER_SIZE, PR_TRUE);
  if(!m_lineStreamBuffer)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService("@mozilla.org/intl/stringbundle;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  return bundleService->CreateBundle("chrome://messenger/locale/localMsgs.properties", getter_AddRefs(mLocalBundle));
}

nsPop3Protocol::~nsPop3Protocol()
{
  if (m_pop3ConData->newuidl)
    PL_HashTableDestroy(m_pop3ConData->newuidl);

  net_pop3_free_state(m_pop3ConData->uidlinfo);

  FreeMsgInfo();
  PR_Free(m_pop3ConData->only_uidl);
  PR_Free(m_pop3ConData);

  delete m_lineStreamBuffer;
}

void nsPop3Protocol::SetCapFlag(PRUint32 flag)
{
    m_pop3ConData->capability_flags |= flag;
}

void nsPop3Protocol::ClearCapFlag(PRUint32 flag)
{
    m_pop3ConData->capability_flags &= ~flag;
}

PRBool nsPop3Protocol::TestCapFlag(PRUint32 flag)
{
    return m_pop3ConData->capability_flags & flag;
}

void nsPop3Protocol::UpdateStatus(PRInt32 aStatusID)
{
  if (m_statusFeedback)
  {
    nsString statusString;
    mLocalBundle->GetStringFromID(aStatusID, getter_Copies(statusString));
    UpdateStatusWithString(statusString.get());
  }
}

void nsPop3Protocol::UpdateStatusWithString(const PRUnichar * aStatusString)
{
    nsresult rv;
    if (mProgressEventSink)
    {
        rv = mProgressEventSink->OnStatus(this, m_channelContext, NS_OK, aStatusString);      // XXX i18n message
        NS_ASSERTION(NS_SUCCEEDED(rv), "dropping error result");
    }
}

void nsPop3Protocol::UpdateProgressPercent (PRUint32 totalDone, PRUint32 total)
{
  // XXX 64-bit
  if (mProgressEventSink)
    mProgressEventSink->OnProgress(this, m_channelContext, nsUint64(totalDone), nsUint64(total));
}

// note:  SetUsername() expects an unescaped string
// do not pass in an escaped string
void nsPop3Protocol::SetUsername(const char* name)
{
  NS_ASSERTION(name, "no name specified!");
    if (name)
      m_username = name;
}

nsresult nsPop3Protocol::GetPassword(nsCString& aPassword, PRBool *okayValue)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_pop3Server);

  if (server)
  {
    PRBool isAuthenticated;
    m_nsIPop3Sink->GetUserAuthenticated(&isAuthenticated);

    // pass the failed password into the password prompt so that
    // it will be pre-filled, in case it failed because of a
    // server problem and not because it was wrong.
    if (!m_lastPasswordSent.IsEmpty())
      aPassword = m_lastPasswordSent;

    // clear the password if the last one failed
    if (TestFlag(POP3_PASSWORD_FAILED))
    {
      // if we've already gotten a password and it wasn't correct..clear
      // out the password and try again.
      rv = server->SetPassword(EmptyCString());
    }

    // Set up some items that we're going to need for the prompting.
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_url, &rv);
    nsCOMPtr<nsIMsgWindow> msgWindow;
    if (mailnewsUrl)
      mailnewsUrl->GetMsgWindow(getter_AddRefs(msgWindow));

    nsCString userName;
    server->GetRealUsername(userName);

    nsCString hostName;
    server->GetRealHostName(hostName);

    nsString passwordPrompt;
    NS_ConvertUTF8toUTF16 userNameUTF16(userName);
    NS_ConvertUTF8toUTF16 hostNameUTF16(hostName);
    const PRUnichar* passwordParams[] = { userNameUTF16.get(),
                                          hostNameUTF16.get() };

    // if the last prompt got us a bad password then show a special dialog
    if (TestFlag(POP3_PASSWORD_FAILED))
    {
      // if we haven't successfully logged onto the server in this session
      // and tried at least twice or if the server threw the specific error,
      // forget the password.
      // Only do this if it's not check for new mail, since biff shouldn't
      // cause passwords to get forgotten at all.
      if ((!isAuthenticated || m_pop3ConData->logonFailureCount > 2) && msgWindow)
      {
        PRInt32 buttonPressed = 0;
        if (NS_SUCCEEDED(MsgPromptLoginFailed(msgWindow, hostName,
                                              &buttonPressed)))
        {
          if (buttonPressed == 1)
          {
            // Cancel button pressed, about quickly and stop trying for now.
            m_pop3ConData->next_state = POP3_ERROR_DONE;

            // Clear the password we're going to return to force failure in
            // the get mail instance.
            aPassword.Truncate();

            // We also have to clear the password failed flag, otherwise we'll
            // automatically try again.
            ClearFlag(POP3_PASSWORD_FAILED);
            return NS_ERROR_FAILURE;
          }
          if (buttonPressed == 2)
          {
            // Change password was pressed. For now, forget the stored password
            // and we'll prompt for a new one next time around.
            rv = server->ForgetPassword();
            NS_ENSURE_SUCCESS(rv, rv);
          }
          // Reset the logon failure count now that we've prompted the user
          m_pop3ConData->logonFailureCount = 0;
        }
      }
      mLocalBundle->FormatStringFromName(
        NS_LITERAL_STRING("pop3PreviouslyEnteredPasswordIsInvalidPrompt").get(),
        passwordParams, 2, getter_Copies(passwordPrompt));
    }
    else
      // Otherwise this is the first time we've asked about the server's
      // password so show a first time prompt.
      mLocalBundle->FormatStringFromName(
        NS_LITERAL_STRING("pop3EnterPasswordPrompt").get(),
        passwordParams, 2, getter_Copies(passwordPrompt));

    nsString passwordTitle;
    mLocalBundle->GetStringFromName(
      NS_LITERAL_STRING("pop3EnterPasswordPromptTitle").get(),
      getter_Copies(passwordTitle));

    // Now go and get the password.
    if (!passwordPrompt.IsEmpty() && !passwordTitle.IsEmpty())
      rv = server->GetPasswordWithUI(passwordPrompt, passwordTitle,
                                     msgWindow, okayValue, aPassword);
    ClearFlag(POP3_PASSWORD_FAILED|POP3_AUTH_FAILURE);

    // If it failed, then user pressed the cancel button (or some other
    // failure).
    if (NS_FAILED(rv))
      m_pop3ConData->next_state = POP3_ERROR_DONE;
  }
  else
    // No server present :-(
    rv = NS_MSG_INVALID_OR_MISSING_SERVER;

  return rv;
}

NS_IMETHODIMP nsPop3Protocol::OnTransportStatus(nsITransport *aTransport, nsresult aStatus, PRUint64 aProgress, PRUint64 aProgressMax)
{
  return nsMsgProtocol::OnTransportStatus(aTransport, aStatus, aProgress, aProgressMax);
}

// stop binding is a "notification" informing us that the stream associated with aURL is going away.
NS_IMETHODIMP nsPop3Protocol::OnStopRequest(nsIRequest *request, nsISupports * aContext, nsresult aStatus)
{
  nsresult rv = nsMsgProtocol::OnStopRequest(request, aContext, aStatus);
  // turn off the server busy flag on stop request - we know we're done, right?
  if (m_pop3Server)
  {
    nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_pop3Server);
    if (server)
      server->SetServerBusy(PR_FALSE); // the server is not busy
  }
  if(m_pop3ConData->list_done)
    CommitState(PR_TRUE);
  if (NS_FAILED(aStatus) && aStatus != NS_BINDING_ABORTED)
    Abort();
  return rv;
}

void nsPop3Protocol::Abort()
{
  if(m_pop3ConData->msg_closure)
  {
      m_nsIPop3Sink->IncorporateAbort(m_pop3ConData->only_uidl != nsnull);
      m_pop3ConData->msg_closure = nsnull;
  }
  // need this to close the stream on the inbox.
  m_nsIPop3Sink->AbortMailDelivery(this);
  m_pop3Server->SetRunningProtocol(nsnull);

}

NS_IMETHODIMP nsPop3Protocol::Cancel(nsresult status)  // handle stop button
{
  Abort();
  return nsMsgProtocol::Cancel(NS_BINDING_ABORTED);
}


nsresult nsPop3Protocol::LoadUrl(nsIURI* aURL, nsISupports * /* aConsumer */)
{
  nsresult rv = 0;

  if (aURL)
    m_url = do_QueryInterface(aURL);
  else
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIURL> url = do_QueryInterface(aURL, &rv);
  if (NS_FAILED(rv)) return rv;

  PRInt32 port;
  rv = url->GetPort(&port);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_CheckPortSafety(port, "pop");
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString queryPart;
  rv = url->GetQuery(queryPart);
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get the url spect");

  m_pop3ConData->only_check_for_new_mail = (PL_strcasestr(queryPart.get(), "check") != nsnull);
  m_pop3ConData->verify_logon = (PL_strcasestr(queryPart.get(), "verifyLogon") != nsnull);
  m_pop3ConData->get_url = (PL_strcasestr(queryPart.get(), "gurl") != nsnull);

  PRBool deleteByAgeFromServer = PR_FALSE;
  PRInt32 numDaysToLeaveOnServer = -1;
  if (!m_pop3ConData->verify_logon)
  {
    // Pick up pref setting regarding leave messages on server, message size limit

    m_pop3Server->GetLeaveMessagesOnServer(&m_pop3ConData->leave_on_server);
    m_pop3Server->GetHeadersOnly(&m_pop3ConData->headers_only);
    PRBool limitMessageSize = PR_FALSE;

    nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_pop3Server);
    if (server)
    {
      // size limits are superseded by headers_only mode
      if (!m_pop3ConData->headers_only)
      {
        server->GetLimitOfflineMessageSize(&limitMessageSize);
        if (limitMessageSize)
        {
          PRInt32 max_size = 0; // default size
          server->GetMaxMessageSize(&max_size);
          m_pop3ConData->size_limit = (max_size) ? max_size * 1024 : 50 * 1024;
       }
      }
      m_pop3Server->GetDeleteByAgeFromServer(&deleteByAgeFromServer);
      if (deleteByAgeFromServer)
        m_pop3Server->GetNumDaysToLeaveOnServer(&numDaysToLeaveOnServer);
    }
  }

  // UIDL stuff
  nsCOMPtr<nsIPop3URL> pop3Url = do_QueryInterface(m_url);
  if (pop3Url)
    pop3Url->GetPop3Sink(getter_AddRefs(m_nsIPop3Sink));

  nsCOMPtr<nsILocalFile> mailDirectory;

  nsCString hostName;
  nsCString userName;

  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_pop3Server);
  if (server)
  {
    rv = server->GetLocalPath(getter_AddRefs(mailDirectory));
    server->SetServerBusy(PR_TRUE); // the server is now busy
    server->GetHostName(hostName);
    server->GetUsername(userName);
  }

  if (!m_pop3ConData->verify_logon)
    m_pop3ConData->uidlinfo = net_pop3_load_state(hostName.get(), userName.get(), mailDirectory);

  m_pop3ConData->biffstate = nsIMsgFolder::nsMsgBiffState_NoMail;

  if (m_pop3ConData->uidlinfo && numDaysToLeaveOnServer > 0)
  {
    PRUint32 nowInSeconds = TimeInSecondsFromPRTime(PR_Now());
    PRUint32 cutOffDay = nowInSeconds - (60 * 60 * 24 * numDaysToLeaveOnServer);

    PL_HashTableEnumerateEntries(m_pop3ConData->uidlinfo->hash, net_pop3_delete_old_msgs_mapper, (void *) cutOffDay);
  }
  const char* uidl = PL_strcasestr(queryPart.get(), "uidl=");
  PR_FREEIF(m_pop3ConData->only_uidl);

  if (uidl)
  {
    uidl += 5;
    nsCString unescapedData;
    MsgUnescapeString(nsDependentCString(uidl), 0, unescapedData);
    m_pop3ConData->only_uidl = PL_strdup(unescapedData.get());

    mSuppressListenerNotifications = PR_TRUE; // suppress on start and on stop because this url won't have any content to display
  }

  m_pop3ConData->next_state = POP3_START_CONNECT;
  m_pop3ConData->next_state_after_response = POP3_FINISH_CONNECT;
  if (NS_SUCCEEDED(rv))
  {
    m_pop3Server->SetRunningProtocol(this);
    return nsMsgProtocol::LoadUrl(aURL);
  }
  else
    return rv;
}

void
nsPop3Protocol::FreeMsgInfo()
{
  int i;
  if (m_pop3ConData->msg_info)
  {
    for (i=0 ; i<m_pop3ConData->number_of_messages ; i++)
    {
      if (m_pop3ConData->msg_info[i].uidl)
        PR_Free(m_pop3ConData->msg_info[i].uidl);
      m_pop3ConData->msg_info[i].uidl = nsnull;
    }
    PR_Free(m_pop3ConData->msg_info);
    m_pop3ConData->msg_info = nsnull;
  }
}

PRInt32
nsPop3Protocol::WaitForStartOfConnectionResponse(nsIInputStream* aInputStream,
                                                 PRUint32 length)
{
  char * line = nsnull;
  PRUint32 line_length = 0;
  PRBool pauseForMoreData = PR_FALSE;
  nsresult rv;
  line = m_lineStreamBuffer->ReadNextLine(aInputStream, line_length, pauseForMoreData, &rv);

  PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));
  if (NS_FAILED(rv))
    return -1;

  if(pauseForMoreData || !line)
  {
    m_pop3ConData->pause_for_read = PR_TRUE; /* pause */
    PR_Free(line);
    return(line_length);
  }

  if(*line == '+')
  {
    m_pop3ConData->command_succeeded = PR_TRUE;
    if(PL_strlen(line) > 4)
      m_commandResponse = line + 4;
    else
      m_commandResponse = line;

    if (m_useSecAuth)
    {
        nsresult rv;
        nsCOMPtr<nsISignatureVerifier> verifier = do_GetService(SIGNATURE_VERIFIER_CONTRACTID, &rv);
        // this checks if psm is installed...
        if (NS_SUCCEEDED(rv))
        {
          if (NS_SUCCEEDED(GetApopTimestamp()))
            SetCapFlag(POP3_HAS_AUTH_APOP);
        }
    }
    else
      ClearCapFlag(POP3_HAS_AUTH_APOP);

    m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);

    m_pop3ConData->next_state = POP3_PROCESS_AUTH;
    m_pop3ConData->pause_for_read = PR_FALSE; /* don't pause */
  }

  PR_Free(line);
  return(1);  /* everything ok */
}

PRInt32
nsPop3Protocol::WaitForResponse(nsIInputStream* inputStream, PRUint32 length)
{
  char * line;
  PRUint32 ln = 0;
  PRBool pauseForMoreData = PR_FALSE;
  nsresult rv;
  line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData, &rv);
  if (NS_FAILED(rv))
    return -1;

  if(pauseForMoreData || !line)
  {
    m_pop3ConData->pause_for_read = PR_TRUE; /* pause */

    PR_Free(line);
    return(ln);
  }

  PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));

  if(*line == '+')
  {
    m_pop3ConData->command_succeeded = PR_TRUE;
    if(PL_strlen(line) > 4)
    {
      if(!PL_strncasecmp(line, "+OK", 3))
        m_commandResponse = line + 4;
      else  // challenge answer to AUTH CRAM-MD5 and LOGIN username/password
        m_commandResponse = line + 2;
    }
    else
      m_commandResponse = line;
  }
  else
  {
    m_pop3ConData->command_succeeded = PR_FALSE;
    if(PL_strlen(line) > 5)
      m_commandResponse = line + 5;
    else
      m_commandResponse  = line;

    // search for the response codes (RFC 2449, chapter 8 and RFC 3206)
    if(TestCapFlag(POP3_HAS_RESP_CODES | POP3_HAS_AUTH_RESP_CODE))
    {
        // code for authentication failure due to the user's credentials
        if(m_commandResponse.Find("[AUTH", PR_TRUE) >= 0)
          SetFlag(POP3_AUTH_FAILURE);

        // codes for failures due to other reasons
        if(m_commandResponse.Find("[LOGIN-DELAY", PR_TRUE) >= 0 ||
           m_commandResponse.Find("[IN-USE", PR_TRUE) >= 0 ||
           m_commandResponse.Find("[SYS", PR_TRUE) >= 0)
      SetFlag(POP3_STOPLOGIN);

      // remove the codes from the response string presented to the user
      PRInt32 i = m_commandResponse.FindChar(']');
      if(i >= 0)
        m_commandResponse.Cut(0, i + 2);
    }
  }

  m_pop3ConData->next_state = m_pop3ConData->next_state_after_response;
  m_pop3ConData->pause_for_read = PR_FALSE; /* don't pause */

  PR_Free(line);
  return(1);  /* everything ok */
}

PRInt32
nsPop3Protocol::Error(PRInt32 err_code)
{
    PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS, ("ERROR: %d", err_code));

    // the error code is just the resource id for the error string...
    // so print out that error message!
    nsresult rv = NS_OK;
    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_url, &rv);
    // we handle POP3_TMP_DOWNLOAD_FAILED earlier...
    if (err_code != POP3_TMP_DOWNLOAD_FAILED && NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIMsgWindow> msgWindow;
        nsCOMPtr<nsIPrompt> dialog;
        rv = mailnewsUrl->GetMsgWindow(getter_AddRefs(msgWindow)); //it is ok to have null msgWindow, for example when biffing
        if (NS_SUCCEEDED(rv) && msgWindow)
        {
            rv = msgWindow->GetPromptDialog(getter_AddRefs(dialog));
            if (NS_SUCCEEDED(rv))
            {
              nsString alertString;
              mLocalBundle->GetStringFromID(err_code, getter_Copies(alertString));
              if (m_pop3ConData->command_succeeded)  //not a server error message
                dialog->Alert(nsnull, alertString.get());
              else
              {
                nsString serverSaidPrefix;
                nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_pop3Server);
                nsCString hostName;
                // Fomat string with hostname.
                if (server)
                  rv = server->GetRealHostName(hostName);
                if (NS_SUCCEEDED(rv))
                {
                  nsAutoString hostStr;
                  CopyASCIItoUTF16(hostName, hostStr);
                  const PRUnichar *params[] = { hostStr.get() };
                  mLocalBundle->FormatStringFromID(POP3_SERVER_SAID, params, 1, getter_Copies(serverSaidPrefix));
                }

                nsAutoString message(alertString);
                message.AppendLiteral(" ");
                message.Append(serverSaidPrefix);
                message.AppendLiteral(" ");
                message.Append(NS_ConvertASCIItoUTF16(m_commandResponse));
                dialog->Alert(nsnull,message.get());
              }
            }
        }
    }
    m_pop3ConData->next_state = POP3_ERROR_DONE;
    m_pop3ConData->pause_for_read = PR_FALSE;
    return -1;
}

PRInt32 nsPop3Protocol::SendData(nsIURI * aURL, const char * dataBuffer, PRBool aSuppressLogging)
{
  // remove any leftover bytes in the line buffer
  // this can happen if the last message line doesn't end with a (CR)LF
  // or a server sent two reply lines
  m_lineStreamBuffer->ClearBuffer();

  PRInt32 result = nsMsgProtocol::SendData(aURL, dataBuffer);

  if (!aSuppressLogging)
      PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS, ("SEND: %s", dataBuffer));
  else
      PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS, ("Logging suppressed for this command (it probably contained authentication information)"));

  if (result >= 0) // yeah this sucks...i need an error code....
  {
    m_pop3ConData->pause_for_read = PR_TRUE;
    m_pop3ConData->next_state = POP3_WAIT_FOR_RESPONSE;
  }
  else
    m_pop3ConData->next_state = POP3_ERROR_DONE;

  return 0;
}

/*
 * POP3 AUTH extension
 */

PRInt32 nsPop3Protocol::SendAuth()
{
  if(!m_pop3ConData->command_succeeded)
    return(Error(POP3_SERVER_ERROR));

  nsCAutoString command("AUTH" CRLF);

  m_pop3ConData->next_state_after_response = POP3_AUTH_RESPONSE;
  return SendData(m_url, command.get());
}

PRInt32 nsPop3Protocol::AuthResponse(nsIInputStream* inputStream,
                             PRUint32 length)
{
    char * line;
    PRUint32 ln = 0;
    nsresult rv;

    if (TestCapFlag(POP3_AUTH_MECH_UNDEFINED))
    {
        ClearCapFlag(POP3_AUTH_MECH_UNDEFINED);
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
    }

    if (!m_pop3ConData->command_succeeded)
    {
        /* AUTH command not implemented
         * so no secure mechanisms available
         */
        m_pop3ConData->command_succeeded = PR_TRUE;
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
        m_pop3ConData->next_state = POP3_SEND_CAPA;
        return 0;
    }

    PRBool pauseForMoreData = PR_FALSE;
    line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData, &rv);
    if (NS_FAILED(rv))
      return -1;

    if(pauseForMoreData || !line)
    {
        m_pop3ConData->pause_for_read = PR_TRUE; /* pause */
        PR_Free(line);
        return(0);
    }

    PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));

    if (!PL_strcmp(line, "."))
    {
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);

        // now that we've read all the AUTH responses, go for it
        m_pop3ConData->next_state = POP3_SEND_CAPA;
        m_pop3ConData->pause_for_read = PR_FALSE; /* don't pause */
    }
    else if (!PL_strcasecmp (line, "CRAM-MD5"))
    {
        nsCOMPtr<nsISignatureVerifier> verifier = do_GetService(SIGNATURE_VERIFIER_CONTRACTID, &rv);
        // this checks if psm is installed...
        if (NS_SUCCEEDED(rv))
            SetCapFlag(POP3_HAS_AUTH_CRAM_MD5);
    }
    else if (!PL_strcasecmp (line, "NTLM"))
    {
        nsCOMPtr<nsISignatureVerifier> verifier = do_GetService(SIGNATURE_VERIFIER_CONTRACTID, &rv);
        // this checks if psm is installed...
        if (NS_SUCCEEDED(rv))
            SetCapFlag(POP3_HAS_AUTH_NTLM);
    }
    else if (!PL_strcasecmp (line, "MSN"))
    {
        nsCOMPtr<nsISignatureVerifier> verifier = do_GetService(SIGNATURE_VERIFIER_CONTRACTID, &rv);
        // this checks if psm is installed...
        if (NS_SUCCEEDED(rv))
            SetCapFlag(POP3_HAS_AUTH_NTLM|POP3_HAS_AUTH_MSN);
    }
    else if (!PL_strcasecmp (line, "GSSAPI"))
        SetCapFlag(POP3_HAS_AUTH_GSSAPI);
    else if (!PL_strcasecmp (line, "PLAIN"))
        SetCapFlag(POP3_HAS_AUTH_PLAIN);
    else if (!PL_strcasecmp (line, "LOGIN"))
        SetCapFlag(POP3_HAS_AUTH_LOGIN);

    PR_Free(line);
    return 0;
}

/*
 * POP3 CAPA extension, see RFC 2449, chapter 5
 */

PRInt32 nsPop3Protocol::SendCapa()
{
    if(!m_pop3ConData->command_succeeded)
        return(Error(POP3_SERVER_ERROR));

    // for use after mechs disabled fallbacks when login failed
    // should better live in AuthResponse(), but it would only
    // be called the first time then
    BackupAuthFlags();

    nsCAutoString command("CAPA" CRLF);

    m_pop3ConData->next_state_after_response = POP3_CAPA_RESPONSE;
    return SendData(m_url, command.get());
}

PRInt32 nsPop3Protocol::CapaResponse(nsIInputStream* inputStream,
                             PRUint32 length)
{
    char * line;
    PRUint32 ln = 0;

    if (!m_pop3ConData->command_succeeded)
    {
        /* CAPA command not implemented */
        m_pop3ConData->command_succeeded = PR_TRUE;
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
        m_pop3ConData->next_state = POP3_PROCESS_AUTH;
        return 0;
    }

    PRBool pauseForMoreData = PR_FALSE;
    nsresult rv;
    line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData, &rv);
    if (NS_FAILED(rv))
      return -1;

    if(pauseForMoreData || !line)
    {
        m_pop3ConData->pause_for_read = PR_TRUE; /* pause */
        PR_Free(line);
        return(0);
    }

    PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));

    if (!PL_strcmp(line, "."))
    {
        // now that we've read all the CAPA responses, go for it
        m_pop3ConData->next_state = POP3_PROCESS_AUTH;
        m_pop3ConData->pause_for_read = PR_FALSE; /* don't pause */
    }
    else
    if (!PL_strcasecmp(line, "XSENDER"))
    {
        SetCapFlag(POP3_HAS_XSENDER);
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
    }
    else
    // see RFC 2449, chapter 6.4
    if (!PL_strcasecmp(line, "RESP-CODES"))
    {
        SetCapFlag(POP3_HAS_RESP_CODES);
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
    }
    else
    // see RFC 3206, chapter 6
    if (!PL_strcasecmp(line, "AUTH-RESP-CODE"))
    {
        SetCapFlag(POP3_HAS_AUTH_RESP_CODE);
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
    }
    else
    // see RFC 2595, chapter 4
    if (!PL_strcasecmp(line, "STLS"))
    {
        nsresult rv;
        nsCOMPtr<nsISignatureVerifier> verifier = do_GetService(SIGNATURE_VERIFIER_CONTRACTID, &rv);
        // this checks if psm is installed...
        if (NS_SUCCEEDED(rv))
        {
            SetCapFlag(POP3_HAS_STLS);
            m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
        }
    }
    else
    // see RFC 2449, chapter 6.3
    if (!PL_strncasecmp(line, "SASL", 4) && strlen(line) > 6)
    {
        nsCAutoString responseLine;
        responseLine.Assign(line + 5);

        if (responseLine.Find("PLAIN", PR_TRUE) >= 0)
            SetCapFlag(POP3_HAS_AUTH_PLAIN);

        if (responseLine.Find("LOGIN", PR_TRUE) >= 0)
            SetCapFlag(POP3_HAS_AUTH_LOGIN);

        if (responseLine.Find("GSSAPI", PR_TRUE) >= 0)
            SetCapFlag(POP3_HAS_AUTH_GSSAPI);

        nsresult rv;
        nsCOMPtr<nsISignatureVerifier> verifier = do_GetService(SIGNATURE_VERIFIER_CONTRACTID, &rv);
        // this checks if psm is installed...
        if (NS_SUCCEEDED(rv))
        {
            if (responseLine.Find("CRAM-MD5", PR_TRUE) >= 0)
                SetCapFlag(POP3_HAS_AUTH_CRAM_MD5);

            if (responseLine.Find("NTLM", PR_TRUE) >= 0)
                SetCapFlag(POP3_HAS_AUTH_NTLM);

            if (responseLine.Find("MSN", PR_TRUE) >= 0)
                SetCapFlag(POP3_HAS_AUTH_NTLM|POP3_HAS_AUTH_MSN);
        }

        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
        // for use after mechs disabled fallbacks when login failed
        BackupAuthFlags();
    }

    PR_Free(line);
    return 0;
}

PRInt32 nsPop3Protocol::SendTLSResponse()
{
  // only tear down our existing connection and open a new one if we received
  // a +OK response from the pop server after we issued the STLS command
  nsresult rv = NS_OK;
  if (m_pop3ConData->command_succeeded)
  {
      nsCOMPtr<nsISupports> secInfo;
      nsCOMPtr<nsISocketTransport> strans = do_QueryInterface(m_transport, &rv);
      if (NS_FAILED(rv)) return rv;

      rv = strans->GetSecurityInfo(getter_AddRefs(secInfo));

      if (NS_SUCCEEDED(rv) && secInfo)
      {
          nsCOMPtr<nsISSLSocketControl> sslControl = do_QueryInterface(secInfo, &rv);

          if (NS_SUCCEEDED(rv) && sslControl)
              rv = sslControl->StartTLS();
      }

    if (NS_SUCCEEDED(rv))
    {
      m_pop3ConData->next_state = POP3_SEND_AUTH;
      m_tlsEnabled = PR_TRUE;

      // certain capabilities like POP3_HAS_AUTH_APOP should be
      // preserved across the connections.
      PRUint32 preservedCapFlags = m_pop3ConData->capability_flags & POP3_HAS_AUTH_APOP;
      m_pop3ConData->capability_flags =     // resetting the flags
        POP3_AUTH_MECH_UNDEFINED |
        POP3_HAS_AUTH_USER |                // should be always there
        POP3_GURL_UNDEFINED |
        POP3_UIDL_UNDEFINED |
        POP3_TOP_UNDEFINED |
        POP3_XTND_XLST_UNDEFINED |
        preservedCapFlags;
      m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
      return rv;
    }
  }

  ClearFlag(POP3_HAS_STLS);
  m_pop3ConData->next_state = POP3_PROCESS_AUTH;

  return rv;
}

PRInt32 nsPop3Protocol::ProcessAuth()
{
    if (!m_tlsEnabled)
    {
      if(TestCapFlag(POP3_HAS_STLS))
      {
        if (m_socketType == nsIMsgIncomingServer::tryTLS ||
            m_socketType == nsIMsgIncomingServer::alwaysUseTLS)
        {
            nsCAutoString command("STLS" CRLF);

            m_pop3ConData->next_state_after_response = POP3_TLS_RESPONSE;
            return SendData(m_url, command.get());
        }
      }
      else if (m_socketType == nsIMsgIncomingServer::alwaysUseTLS)
      {
          m_pop3ConData->next_state = POP3_ERROR_DONE;
          return(Error(NS_ERROR_COULD_NOT_CONNECT_VIA_TLS));
      }
    }

    m_password_already_sent = PR_FALSE;

    if(m_useSecAuth)
    {
      if (TestCapFlag(POP3_HAS_AUTH_GSSAPI))
          m_pop3ConData->next_state = POP3_AUTH_GSSAPI;
      else if (TestCapFlag(POP3_HAS_AUTH_CRAM_MD5))
          m_pop3ConData->next_state = POP3_SEND_USERNAME;
      else
        if (TestCapFlag(POP3_HAS_AUTH_NTLM))
            m_pop3ConData->next_state = POP3_AUTH_NTLM;
        else
        if (TestCapFlag(POP3_HAS_AUTH_APOP))
            m_pop3ConData->next_state = POP3_SEND_PASSWORD;
        else
          return(Error(CANNOT_PROCESS_SECURE_AUTH));
    }
    else
    {
        if (TestCapFlag(POP3_HAS_AUTH_PLAIN))
            m_pop3ConData->next_state = POP3_SEND_USERNAME;
        else
        if (TestCapFlag(POP3_HAS_AUTH_LOGIN))
            m_pop3ConData->next_state = POP3_AUTH_LOGIN;
        else
        if (TestCapFlag(POP3_HAS_AUTH_USER))
            m_pop3ConData->next_state = POP3_SEND_USERNAME;
        else
            return(Error(POP3_SERVER_ERROR));
    }

    m_pop3ConData->pause_for_read = PR_FALSE;

    return 0;
}

void nsPop3Protocol::BackupAuthFlags()
{
  m_origAuthFlags = m_pop3ConData->capability_flags &
                    (POP3_HAS_AUTH_ANY | POP3_HAS_AUTH_ANY_SEC);
}

void nsPop3Protocol::RestoreAuthFlags()
{
  m_pop3ConData->capability_flags |= m_origAuthFlags;
}

PRInt32 nsPop3Protocol::AuthFallback()
{
    if (m_pop3ConData->command_succeeded)
        if(m_password_already_sent)
        {
            m_nsIPop3Sink->SetUserAuthenticated(PR_TRUE);
            ClearFlag(POP3_PASSWORD_FAILED);
            if (m_pop3ConData->verify_logon)
              m_pop3ConData->next_state = POP3_SEND_QUIT;
            else
              m_pop3ConData->next_state = (m_pop3ConData->get_url)
                                          ? POP3_SEND_GURL : POP3_SEND_STAT;
        }
        else
            m_pop3ConData->next_state = POP3_SEND_PASSWORD;
    else
    {
        // response code received shows that login failed not because of
        // wrong credential -> stop login without retry or pw dialog, only alert
        if (TestFlag(POP3_STOPLOGIN))
            return(Error((m_password_already_sent)
                         ? POP3_PASSWORD_FAILURE : POP3_USERNAME_FAILURE));

        // response code received shows that server is certain about the
        // credential was wrong, or fallback has been disabled by pref
        // -> no fallback, show alert and pw dialog
        PRBool logonFallback = PR_TRUE;
        nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_pop3Server);
        if (server)
          server->GetLogonFallback(&logonFallback);
        if (!logonFallback)
          SetFlag(POP3_AUTH_FAILURE);

        if (TestFlag(POP3_AUTH_FAILURE))
        {
            Error((m_password_already_sent)
                         ? POP3_PASSWORD_FAILURE : POP3_USERNAME_FAILURE);
            SetFlag(POP3_PASSWORD_FAILED);
            ClearFlag(POP3_AUTH_FAILURE);
            m_pop3ConData->logonFailureCount++;
            return 0;
        }

        // we have no certain response code -> fallback and try again
        if (m_useSecAuth)
        {
            // If one authentication failed, we're going to
            // fall back on a less secure login method.
            if (TestCapFlag(POP3_HAS_AUTH_GSSAPI))
                ClearCapFlag(POP3_HAS_AUTH_GSSAPI);
            else if (TestCapFlag(POP3_HAS_AUTH_CRAM_MD5))
                // if CRAM-MD5 enabled, disable it
                ClearCapFlag(POP3_HAS_AUTH_CRAM_MD5);
            else if (TestCapFlag(POP3_HAS_AUTH_NTLM))
                // if NTLM enabled, disable it
                ClearCapFlag(POP3_HAS_AUTH_NTLM|POP3_HAS_AUTH_MSN);
            else if (TestCapFlag(POP3_HAS_AUTH_APOP))
            {
                // if APOP enabled, disable it
                ClearCapFlag(POP3_HAS_AUTH_APOP);
                // unsure because APOP failed and we can't determine why
                Error(CANNOT_PROCESS_APOP_AUTH);
            }
        }
        else
        {
            // If one authentication failed, we're going to
            // fall back on a less secure login method.
            if (TestCapFlag(POP3_HAS_AUTH_PLAIN))
                // if PLAIN enabled, disable it
                ClearCapFlag(POP3_HAS_AUTH_PLAIN);
            else if(TestCapFlag(POP3_HAS_AUTH_LOGIN))
                // if LOGIN enabled, disable it
                ClearCapFlag(POP3_HAS_AUTH_LOGIN);
            else if(TestCapFlag(POP3_HAS_AUTH_USER))
            {
                if(m_password_already_sent)
                    // if USER enabled, disable it
                    ClearCapFlag(POP3_HAS_AUTH_USER);
                else
                    // if USER enabled,
                    // it was the username which was wrong -
                    // no fallback but return error
                    return(Error(POP3_USERNAME_FAILURE));
            }
        }

        // Only forget the password if we've no mechanism left.
        if (m_useSecAuth && !TestCapFlag(POP3_HAS_AUTH_ANY_SEC) ||
            !m_useSecAuth && !TestCapFlag(POP3_HAS_AUTH_ANY))
        {
            // Let's restore the original auth flags from AuthResponse so we can
            // try them again with new password and username
            RestoreAuthFlags();
            m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);

            Error(POP3_PASSWORD_FAILURE);
            /* The password failed.

               Sever the connection and go back to the `read password' state,
               which, upon success, will re-open the connection.  Set a flag
               which causes the prompt to be different that time (to indicate
               that the old password was bogus.)

               But if we're just checking for new mail (biff) then don't bother
               prompting the user for a password: just fail silently.
            */

            SetFlag(POP3_PASSWORD_FAILED);
            m_pop3ConData->logonFailureCount++;

            if (m_nsIPop3Sink)
                m_nsIPop3Sink->SetMailAccountURL(NULL);

            return 0;
        }

        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);

        m_pop3ConData->command_succeeded = PR_TRUE;

        m_pop3ConData->next_state = POP3_PROCESS_AUTH;
    }

    if (TestCapFlag(POP3_AUTH_MECH_UNDEFINED))
    {
        ClearCapFlag(POP3_AUTH_MECH_UNDEFINED);
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
    }

    m_pop3ConData->pause_for_read = PR_FALSE;

    return 0;
}

// LOGIN consists of three steps not two as USER/PASS or CRAM-MD5,
// so we've to start here and continue in SendUsername if the server
// responds + to "AUTH LOGIN"
PRInt32 nsPop3Protocol::AuthLogin()
{
    nsCAutoString command("AUTH LOGIN" CRLF);
    m_pop3ConData->next_state_after_response = POP3_AUTH_LOGIN_RESPONSE;
    m_pop3ConData->pause_for_read = PR_TRUE;

    return SendData(m_url, command.get());
}

PRInt32 nsPop3Protocol::AuthLoginResponse()
{
    // need the test to be here instead in AuthFallback() to
    // differentiate between command AUTH LOGIN failed and
    // sending username using LOGIN mechanism failed.
    if (!m_pop3ConData->command_succeeded)
    {
        // we failed with LOGIN, remove it
        ClearCapFlag(POP3_HAS_AUTH_LOGIN);

        m_pop3ConData->next_state = POP3_PROCESS_AUTH;
    }
    else
        m_pop3ConData->next_state = POP3_SEND_USERNAME;

    m_pop3ConData->pause_for_read = PR_FALSE;

    return 0;
}

// NTLM, like LOGIN consists of three steps not two as USER/PASS or CRAM-MD5,
// so we've to start here and continue in SendUsername if the server
// responds + to "AUTH NTLM"
PRInt32 nsPop3Protocol::AuthNtlm()
{
    nsCAutoString command (TestCapFlag(POP3_HAS_AUTH_MSN) ? "AUTH MSN" CRLF :
                                                            "AUTH NTLM" CRLF);
    m_pop3ConData->next_state_after_response = POP3_AUTH_NTLM_RESPONSE;
    m_pop3ConData->pause_for_read = PR_TRUE;

    return SendData(m_url, command.get());
}

PRInt32 nsPop3Protocol::AuthNtlmResponse()
{
    // need the test to be here instead in AuthFallback() to
    // differentiate between command AUTH NTLM failed and
    // sending username using NTLM mechanism failed.
    if (!m_pop3ConData->command_succeeded)
    {
        // we failed with NTLM, remove it
        ClearCapFlag(POP3_HAS_AUTH_NTLM|POP3_HAS_AUTH_MSN);

        m_pop3ConData->next_state = POP3_PROCESS_AUTH;
    }
    else
        m_pop3ConData->next_state = POP3_SEND_USERNAME;

    m_pop3ConData->pause_for_read = PR_FALSE;

    return 0;
}

PRInt32 nsPop3Protocol::AuthGSSAPI()
{
    nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_pop3Server);
    if (server) {
        nsCAutoString cmd;
        nsCAutoString service("pop@");
        nsCString hostName;
        nsresult rv;
        server->GetRealHostName(hostName);
        service.Append(hostName);
        rv = DoGSSAPIStep1(service.get(), m_username.get(), cmd);
        if (NS_SUCCEEDED(rv)) {
            m_GSSAPICache.Assign(cmd);
            m_pop3ConData->next_state_after_response = POP3_AUTH_GSSAPI_FIRST;
            m_pop3ConData->pause_for_read = PR_TRUE;
            return SendData(m_url, "AUTH GSSAPI" CRLF);
        }
    }

    ClearCapFlag(POP3_HAS_AUTH_GSSAPI);
    m_pop3ConData->next_state = POP3_PROCESS_AUTH;
    m_pop3ConData->pause_for_read = PR_FALSE;
    return NS_OK;
}

PRInt32 nsPop3Protocol::AuthGSSAPIResponse(PRBool first)
{
    if (!m_pop3ConData->command_succeeded)
    {
        if (first)
            m_GSSAPICache.Truncate();
        ClearCapFlag(POP3_HAS_AUTH_GSSAPI);
        m_pop3ConData->next_state = POP3_PROCESS_AUTH;
        m_pop3ConData->pause_for_read = PR_FALSE;
        return NS_OK;
    }

    nsresult rv;

    m_pop3ConData->next_state_after_response = POP3_AUTH_GSSAPI_STEP;
    m_pop3ConData->pause_for_read = PR_TRUE;

    if (first) {
        m_GSSAPICache += CRLF;
        rv = SendData(m_url, m_GSSAPICache.get());
        m_GSSAPICache.Truncate();
    }
    else {
        nsCAutoString cmd;
        rv = DoGSSAPIStep2(m_commandResponse, cmd);
        if (NS_FAILED(rv))
            cmd = "*";
        if (rv == NS_SUCCESS_AUTH_FINISHED) {
            m_pop3ConData->next_state_after_response = POP3_AUTH_FALLBACK;
            m_password_already_sent = PR_TRUE;
        }
        cmd += CRLF;
        rv = SendData(m_url, cmd.get());
    }

    return rv;
}

PRInt32 nsPop3Protocol::SendUsername()
{
    if(m_username.IsEmpty())
      return(Error(POP3_USERNAME_UNDEFINED));

    nsCString password;
    PRBool okayValue = PR_TRUE;
    nsresult rv = GetPassword(password, &okayValue);
    if (NS_SUCCEEDED(rv) && !okayValue)
    {
      // user has canceled the password prompt
      m_pop3ConData->next_state = POP3_ERROR_DONE;
      return NS_ERROR_ABORT;
    }
    else if (NS_FAILED(rv) || password.IsEmpty())
      return Error(POP3_PASSWORD_UNDEFINED);

    nsCAutoString cmd;

    if (m_useSecAuth)
    {
      if (TestCapFlag(POP3_HAS_AUTH_CRAM_MD5))
          cmd = "AUTH CRAM-MD5";
      else if (TestCapFlag(POP3_HAS_AUTH_NTLM))
          rv = DoNtlmStep1(m_username.get(), password.get(), cmd);
    }
    else
    {
        if (TestCapFlag(POP3_HAS_AUTH_PLAIN))
            cmd = "AUTH PLAIN";
        else if (TestCapFlag(POP3_HAS_AUTH_LOGIN))
        {
            char *base64Str = PL_Base64Encode(m_username.get(), m_username.Length(), nsnull);
            cmd = base64Str;
            PR_Free(base64Str);
        }
        else
        {
            cmd = "USER ";
            cmd += m_username;
        }
    }
    cmd += CRLF;

    m_pop3ConData->next_state_after_response = POP3_AUTH_FALLBACK;

    m_pop3ConData->pause_for_read = PR_TRUE;

    return SendData(m_url, cmd.get());
}

PRInt32 nsPop3Protocol::SendPassword()
{
    if (m_username.IsEmpty())
        return(Error(POP3_USERNAME_UNDEFINED));

    nsCString password;
    PRBool okayValue = PR_TRUE;
    nsresult rv = GetPassword(password, &okayValue);
    if (NS_SUCCEEDED(rv) && !okayValue)
    {
        // user has canceled the password prompt
        m_pop3ConData->next_state = POP3_ERROR_DONE;
        return NS_ERROR_ABORT;
    }
    else if (NS_FAILED(rv) || password.IsEmpty())
    {
      return Error(POP3_PASSWORD_UNDEFINED);
    }

    nsCAutoString cmd;
    if (m_useSecAuth)
    {
        if (TestCapFlag(POP3_HAS_AUTH_CRAM_MD5))
        {
            char buffer[512];
            unsigned char digest[DIGEST_LENGTH];

            char *decodedChallenge = PL_Base64Decode(m_commandResponse.get(),
            m_commandResponse.Length(), nsnull);

            if (decodedChallenge)
                rv = MSGCramMD5(decodedChallenge, strlen(decodedChallenge), password.get(), password.Length(), digest);
            else
                rv = NS_ERROR_FAILURE;

            if (NS_SUCCEEDED(rv) && digest)
            {
                nsCAutoString encodedDigest;
                char hexVal[8];

                for (PRUint32 j=0; j<16; j++)
                {
                    PR_snprintf (hexVal,8, "%.2x", 0x0ff & (unsigned short)digest[j]);
                    encodedDigest.Append(hexVal);
                }

                PR_snprintf(buffer, sizeof(buffer), "%s %s", m_username.get(), encodedDigest.get());
                char *base64Str = PL_Base64Encode(buffer, strlen(buffer), nsnull);
                cmd = base64Str;
                PR_Free(base64Str);
            }

            if (NS_FAILED(rv))
                cmd = "*";
        }
        else if (TestCapFlag(POP3_HAS_AUTH_NTLM))
            rv = DoNtlmStep2(m_commandResponse, cmd);
        else if (TestCapFlag(POP3_HAS_AUTH_APOP))
        {
            char buffer[512];
            unsigned char digest[DIGEST_LENGTH];

            rv = MSGApopMD5(m_ApopTimestamp.get(), m_ApopTimestamp.Length(), password.get(), password.Length(), digest);

            if (NS_SUCCEEDED(rv) && digest)
            {
                nsCAutoString encodedDigest;
                char hexVal[8];

                for (PRUint32 j=0; j<16; j++)
                {
                    PR_snprintf (hexVal,8, "%.2x", 0x0ff & (unsigned short)digest[j]);
                    encodedDigest.Append(hexVal);
                }

                PR_snprintf(buffer, sizeof(buffer), "APOP %s %s", m_username.get(), encodedDigest.get());
                cmd = buffer;
            }

            if (NS_FAILED(rv))
                cmd = "*";
        }
    }
    else
    {
        if (TestCapFlag(POP3_HAS_AUTH_PLAIN))
        {
            // workaround for IPswitch's IMail server software
            // this server goes into LOGIN mode even if we send "AUTH PLAIN"
            // "VXNlc" is the begin of the base64 encoded prompt for LOGIN
            if (StringBeginsWith(m_commandResponse, NS_LITERAL_CSTRING("VXNlc")))
            {
                // disable PLAIN and enable LOGIN (in case it's not already enabled)
                ClearCapFlag(POP3_HAS_AUTH_PLAIN);
                SetCapFlag(POP3_HAS_AUTH_LOGIN);
                m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);

                // reenter authentication again at LOGIN response handler
                m_pop3ConData->next_state = POP3_AUTH_LOGIN_RESPONSE;
                m_pop3ConData->pause_for_read = PR_FALSE;
                return 0;
            }

            char plain_string[512];
            int len = 1; /* first <NUL> char */

            memset(plain_string, 0, 512);
            PR_snprintf(&plain_string[1], 510, "%s", m_username.get());
            len += m_username.Length();
            len++; /* second <NUL> char */
            PR_snprintf(&plain_string[len], 511-len, "%s", password.get());
            len += password.Length();

            char *base64Str = PL_Base64Encode(plain_string, len, nsnull);
            cmd = base64Str;
            PR_Free(base64Str);
        }
        else if (TestCapFlag(POP3_HAS_AUTH_LOGIN))
        {
            char * base64Str =
                PL_Base64Encode(password.get(), password.Length(), nsnull);
            cmd = base64Str;
            PR_Free(base64Str);
        }
        else
        {
            cmd = "PASS ";
            cmd += password;
        }
    }
    cmd += CRLF;

    m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);

    m_pop3ConData->next_state_after_response = POP3_AUTH_FALLBACK;

    m_pop3ConData->pause_for_read = PR_TRUE;

    m_password_already_sent = PR_TRUE;
    m_lastPasswordSent = password;
    return SendData(m_url, cmd.get(), PR_TRUE);
}

PRInt32 nsPop3Protocol::SendStatOrGurl(PRBool sendStat)
{
  nsCAutoString cmd;
  if (sendStat)
  {
    cmd  = "STAT" CRLF;
    m_pop3ConData->next_state_after_response = POP3_GET_STAT;
  }
  else
  {
    cmd = "GURL" CRLF;
    m_pop3ConData->next_state_after_response = POP3_GURL_RESPONSE;
  }
  return SendData(m_url, cmd.get());
}


PRInt32
nsPop3Protocol::SendStat()
{
  return SendStatOrGurl(PR_TRUE);
}


PRInt32
nsPop3Protocol::GetStat()
{
  // check stat response
  if (!m_pop3ConData->command_succeeded)
    return(Error(POP3_STAT_FAILURE));

    /* stat response looks like:  %d %d
     * The first number is the number of articles
     * The second number is the number of bytes
     *
     *  grab the first and second arg of stat response
     */
  nsCString oldStr (m_commandResponse);
  char *newStr = oldStr.BeginWriting();
  char *num = NS_strtok(" ", &newStr);  // msg num
  if (num)
  {
    m_pop3ConData->number_of_messages = atol(num);  // bytes
    num = NS_strtok(" ", &newStr);
    m_commandResponse = newStr;
    if (num)
      m_totalFolderSize = (PRInt32) atol(num);  //we always initialize m_totalFolderSize to 0
  }
  else
    m_pop3ConData->number_of_messages = 0;

  m_pop3ConData->really_new_messages = 0;
  m_pop3ConData->real_new_counter = 1;

  m_totalDownloadSize = -1; // Means we need to calculate it, later.

  if (m_pop3ConData->number_of_messages <= 0)
  {
    // We're all done. We know we have no mail.
    m_pop3ConData->next_state = POP3_SEND_QUIT;
    PL_HashTableEnumerateEntries(m_pop3ConData->uidlinfo->hash, hash_clear_mapper, nsnull);
    // Hack - use nsPop3Sink to wipe out any stale Partial messages
    m_nsIPop3Sink->BeginMailDelivery(PR_FALSE, nsnull, nsnull);
    m_nsIPop3Sink->AbortMailDelivery(this);
    return(0);
  }

  /* We're just checking for new mail, and we're not playing any games that
     involve keeping messages on the server.  Therefore, we now know enough
     to finish up.  If we had no messages, that would have been handled
     above; therefore, we know we have some new messages. 
  */
  if (m_pop3ConData->only_check_for_new_mail && !m_pop3ConData->leave_on_server)
  {
    m_nsIPop3Sink->SetBiffStateAndUpdateFE(nsIMsgFolder::nsMsgBiffState_NewMail,
                                           m_pop3ConData->number_of_messages,
                                           PR_TRUE);
    m_pop3ConData->next_state = POP3_SEND_QUIT;
    return(0);
  }


  if (!m_pop3ConData->only_check_for_new_mail)
  {
      /* The following was added to prevent the loss of Data when we try and
         write to somewhere we don't have write access error to (See bug 62480)
         (Note: This is only a temp hack until the underlying XPCOM is fixed
         to return errors) */

      nsresult rv;
      nsCOMPtr <nsIMsgWindow> msgWindow;
      nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_url);
      if (mailnewsUrl)
        rv = mailnewsUrl->GetMsgWindow(getter_AddRefs(msgWindow));
//      NS_ASSERTION(NS_SUCCEEDED(rv) && msgWindow, "no msg window");

      rv = m_nsIPop3Sink->BeginMailDelivery(m_pop3ConData->only_uidl != nsnull, msgWindow,
                                                    &m_pop3ConData->msg_del_started);
      if (NS_FAILED(rv))
        if (rv == NS_MSG_FOLDER_BUSY)
          return(Error(POP3_MESSAGE_FOLDER_BUSY));
        else
          return(Error(POP3_MESSAGE_WRITE_ERROR));

      if(!m_pop3ConData->msg_del_started)
        return(Error(POP3_MESSAGE_WRITE_ERROR));
  }

  m_pop3ConData->next_state = POP3_SEND_LIST;
  return 0;
}



PRInt32
nsPop3Protocol::SendGurl()
{
    if (m_pop3ConData->capability_flags == POP3_CAPABILITY_UNDEFINED ||
        TestCapFlag(POP3_GURL_UNDEFINED | POP3_HAS_GURL))
        return SendStatOrGurl(PR_FALSE);
    else
        return -1;
}


PRInt32
nsPop3Protocol::GurlResponse()
{
    ClearCapFlag(POP3_GURL_UNDEFINED);

    if (m_pop3ConData->command_succeeded)
    {
        SetCapFlag(POP3_HAS_GURL);
        if (m_nsIPop3Sink)
            m_nsIPop3Sink->SetMailAccountURL(m_commandResponse.get());
    }
    else
    {
        ClearCapFlag(POP3_HAS_GURL);
    }
    m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
    m_pop3ConData->next_state = POP3_SEND_QUIT;

    return 0;
}

PRInt32 nsPop3Protocol::SendList()
{
    // check for server returning number of messages that will cause the calculation
    // of the size of the block for msg_info to
    // overflow a 32 bit int, in turn causing us to allocate a block of memory much
    // smaller than we think we're allocating, and
    // potentially allowing the server to make us overwrite memory outside our heap
    // block.

    if (m_pop3ConData->number_of_messages > (int) (0xFFFFF000 / sizeof(Pop3MsgInfo)))
        return MK_OUT_OF_MEMORY;


    m_pop3ConData->msg_info = (Pop3MsgInfo *)
      PR_CALLOC(sizeof(Pop3MsgInfo) * m_pop3ConData->number_of_messages);
    if (!m_pop3ConData->msg_info)
        return(MK_OUT_OF_MEMORY);
    m_pop3ConData->next_state_after_response = POP3_GET_LIST;
    m_listpos = 0;
    return SendData(m_url, "LIST"CRLF);
}



PRInt32
nsPop3Protocol::GetList(nsIInputStream* inputStream,
                        PRUint32 length)
{
  /* check list response
  * This will get called multiple times
  * but it's alright since command_succeeded
  * will remain constant
  */
  if(!m_pop3ConData->command_succeeded)
    return(Error(POP3_LIST_FAILURE));

  PRUint32 ln = 0;
  PRBool pauseForMoreData = PR_FALSE;
  nsresult rv;
  char *line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData, &rv);
  if (NS_FAILED(rv))
    return -1;

  if (pauseForMoreData || !line)
  {
    m_pop3ConData->pause_for_read = PR_TRUE;
    PR_Free(line);
    return(ln);
  }

  PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));

  /* parse the line returned from the list command
  * it looks like
  * #msg_number #bytes
  *
  * list data is terminated by a ".CRLF" line
  */
  if (!PL_strcmp(line, "."))
  {
    // limit the list if fewer entries than given in STAT response
    if(m_listpos < m_pop3ConData->number_of_messages)
      m_pop3ConData->number_of_messages = m_listpos;
    m_pop3ConData->next_state = POP3_SEND_UIDL_LIST;
    m_pop3ConData->pause_for_read = PR_FALSE;
    PR_Free(line);
    return(0);
  }

  char *newStr = line;
  char *token = NS_strtok(" ", &newStr);
  if (token)
  {
    PRInt32 msg_num = atol(token);

    if (++m_listpos <= m_pop3ConData->number_of_messages)
    {
      token = NS_strtok(" ", &newStr);
      if (token)
      {
        m_pop3ConData->msg_info[m_listpos-1].size = atol(token);
        m_pop3ConData->msg_info[m_listpos-1].msgnum = msg_num;
      }
    }
  }

  PR_Free(line);
  return(0);
}


/* UIDL and XTND are both unsupported for this mail server.
   If not enabled any advanced features, we're able to live
   without them. We're simply downloading and deleting everything
   on the server.

   Advanced features are:
   *'Keep Mail on Server' with aging or deletion support
   *'Fetch Headers Only'
   *'Limit Message Size'
   *only download a specific UID

   These require knowledge of of all messages UID's on the server at
   least when it comes to deleting deleting messages on server that
   have been deleted on client or vice versa. TOP doesn't help here
   without generating huge traffic and is mostly not supported at all
   if the server lacks UIDL and XTND XLST.

   In other cases the user has to join the 20th century.
   Tell the user this, and refuse to download any messages until
   they've gone into preferences and turned off any of the above
   prefs.
*/
PRInt32 nsPop3Protocol::HandleNoUidListAvailable()
{
  m_pop3ConData->pause_for_read = PR_FALSE;

  if(!m_pop3ConData->leave_on_server &&
     !m_pop3ConData->headers_only &&
     m_pop3ConData->size_limit <= 0 &&
     !m_pop3ConData->only_uidl)
    m_pop3ConData->next_state = POP3_GET_MSG;
  else
  {
    m_pop3ConData->next_state = POP3_SEND_QUIT;

    nsresult rv;

    nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_url, &rv);
    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIMsgWindow> msgWindow;
      rv = mailnewsUrl->GetMsgWindow(getter_AddRefs(msgWindow));
      if (NS_SUCCEEDED(rv) && msgWindow)
      {
        nsCOMPtr<nsIPrompt> dialog;
        rv = msgWindow->GetPromptDialog(getter_AddRefs(dialog));
        if (NS_SUCCEEDED(rv))
        {
          nsCString hostName;
          nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_pop3Server);
          if (server)
            rv = server->GetRealHostName(hostName);
          if (NS_SUCCEEDED(rv))
          {
            nsAutoString hostNameUnicode;
            CopyASCIItoUTF16(hostName, hostNameUnicode);
            const PRUnichar *formatStrings[] = { hostNameUnicode.get() };
            nsString alertString;
            rv = mLocalBundle->FormatStringFromID(POP3_SERVER_DOES_NOT_SUPPORT_UIDL_ETC,
              formatStrings, 1, getter_Copies(alertString));
            NS_ENSURE_SUCCESS(rv, -1);

            dialog->Alert(nsnull, alertString.get());
          }
        }
      }
    }
  }

  return(0);
}


/* km
 *
 *  net_pop3_send_xtnd_xlst_msgid
 *
 *  Process state: POP3_SEND_XTND_XLST_MSGID
 *
 *  If we get here then UIDL is not supported by the mail server.
 *  Some mail servers support a similar command:
 *
 *    XTND XLST Message-Id
 *
 *  Here is a sample transaction from a QUALCOMM server

 >>XTND XLST Message-Id
 <<+OK xlst command accepted; headers coming.
 <<1 Message-ID: <3117E4DC.2699@netscape.com>
 <<2 Message-Id: <199602062335.PAA19215@lemon.mcom.com>

 * This function will send the xtnd command and put us into the
 * POP3_GET_XTND_XLST_MSGID state
 *
*/
PRInt32 nsPop3Protocol::SendXtndXlstMsgid()
{
  if (TestCapFlag(POP3_HAS_XTND_XLST | POP3_XTND_XLST_UNDEFINED))
  {
    m_pop3ConData->next_state_after_response = POP3_GET_XTND_XLST_MSGID;
    m_pop3ConData->pause_for_read = PR_TRUE;
    m_listpos = 0;
    return SendData(m_url, "XTND XLST Message-Id" CRLF);
  }
  else
    return HandleNoUidListAvailable();
}


/* km
 *
 *  net_pop3_get_xtnd_xlst_msgid
 *
 *  This code was created from the net_pop3_get_uidl_list boiler plate.
 *  The difference is that the XTND reply strings have one more token per
 *  string than the UIDL reply strings do.
 *
 */

PRInt32
nsPop3Protocol::GetXtndXlstMsgid(nsIInputStream* inputStream,
                                 PRUint32 length)
{
  /* check list response
  * This will get called multiple times
  * but it's alright since command_succeeded
  * will remain constant
  */
  ClearCapFlag(POP3_XTND_XLST_UNDEFINED);

  if (!m_pop3ConData->command_succeeded)
  {
    ClearCapFlag(POP3_HAS_XTND_XLST);
    m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
    HandleNoUidListAvailable();
    return(0);
  }
  else
  {
    SetCapFlag(POP3_HAS_XTND_XLST);
    m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
  }

  PRUint32 ln = 0;
  PRBool pauseForMoreData = PR_FALSE;
  nsresult rv;
  char *line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData, &rv);
  if (NS_FAILED(rv))
    return -1;

  if (pauseForMoreData || !line)
  {
    m_pop3ConData->pause_for_read = PR_TRUE;
    PR_Free(line);
    return ln;
  }

  PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));

  /* parse the line returned from the list command
  * it looks like
  * 1 Message-ID: <3117E4DC.2699@netscape.com>
  *
  * list data is terminated by a ".CRLF" line
  */
  if (!PL_strcmp(line, "."))
  {
    // limit the list if fewer entries than given in STAT response
    if(m_listpos < m_pop3ConData->number_of_messages)
      m_pop3ConData->number_of_messages = m_listpos;
    m_pop3ConData->list_done = PR_TRUE;
    m_pop3ConData->next_state = POP3_GET_MSG;
    m_pop3ConData->pause_for_read = PR_FALSE;
    PR_Free(line);
    return(0);
  }

  char *newStr = line;
  char *token = NS_strtok(" ", &newStr);  // msg num
  if (token)
  {
    PRInt32 msg_num = atol(token);
    if (++m_listpos <= m_pop3ConData->number_of_messages)
    {
      NS_strtok(" ", &newStr);  // eat message ID token
      char *uid = NS_strtok(" ", &newStr); // not really a UID but a unique token -km
      if (!uid)
        /* This is bad.  The server didn't give us a UIDL for this message.
        I've seen this happen when somehow the mail spool has a message
        that contains a header that reads "X-UIDL: \n".  But how that got
        there, I have no idea; must be a server bug.  Or something. */
        uid = "";

      // seeking right entry, but try the one that should it be first
      PRInt32 i;
      if(m_pop3ConData->msg_info[m_listpos - 1].msgnum == msg_num)
        i = m_listpos - 1;
      else
        for(i = 0; i < m_pop3ConData->number_of_messages &&
                   m_pop3ConData->msg_info[i].msgnum != msg_num; i++)
          ;

      // only if found a matching slot
      if (i < m_pop3ConData->number_of_messages)
      {
        // to protect us from memory leak in case of getting a msg num twice
        m_pop3ConData->msg_info[i].uidl = PL_strdup(uid);
        if (!m_pop3ConData->msg_info[i].uidl)
        {
          PR_Free(line);
          return MK_OUT_OF_MEMORY;
        }
      }
    }
  }

  PR_Free(line);
  return(0);
}


PRInt32 nsPop3Protocol::SendUidlList()
{
    if (TestCapFlag(POP3_HAS_UIDL | POP3_UIDL_UNDEFINED))
    {
      m_pop3ConData->next_state_after_response = POP3_GET_UIDL_LIST;
      m_pop3ConData->pause_for_read = PR_TRUE;
      m_listpos = 0;
      return SendData(m_url,"UIDL" CRLF);
    }
    else
      return SendXtndXlstMsgid();
}


PRInt32 nsPop3Protocol::GetUidlList(nsIInputStream* inputStream,
                            PRUint32 length)
{
    /* check list response
     * This will get called multiple times
     * but it's alright since command_succeeded
     * will remain constant
     */
    ClearCapFlag(POP3_UIDL_UNDEFINED);

    if (!m_pop3ConData->command_succeeded)
    {
      m_pop3ConData->next_state = POP3_SEND_XTND_XLST_MSGID;
      m_pop3ConData->pause_for_read = PR_FALSE;
      ClearCapFlag(POP3_HAS_UIDL);
      m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
      return(0);
    }
    else
    {
      SetCapFlag(POP3_HAS_UIDL);
      m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
    }

    PRUint32 ln = 0;
    PRBool pauseForMoreData = PR_FALSE;
    nsresult rv;
    char *line = m_lineStreamBuffer->ReadNextLine(inputStream, ln, pauseForMoreData, &rv);
    if (NS_FAILED(rv))
      return -1;

    if (pauseForMoreData || !line)
    {
      PR_Free(line);
      m_pop3ConData->pause_for_read = PR_TRUE;
      return ln;
    }

    PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));

    /* parse the line returned from the list command
     * it looks like
     * #msg_number uidl
     *
     * list data is terminated by a ".CRLF" line
     */
    if (!PL_strcmp(line, "."))
    {
      // limit the list if fewer entries than given in STAT response
      if (m_listpos < m_pop3ConData->number_of_messages)
        m_pop3ConData->number_of_messages = m_listpos;
      m_pop3ConData->list_done = PR_TRUE;
      m_pop3ConData->next_state = POP3_GET_MSG;
      m_pop3ConData->pause_for_read = PR_FALSE;
      PR_Free(line);
      return(0);
    }

    char *newStr = line;
    char *token = NS_strtok(" ", &newStr);  // msg num
    if (token)
    {
      PRInt32 msg_num = atol(token);
      if (++m_listpos <= m_pop3ConData->number_of_messages)
      {
        char *uid = NS_strtok(" ", &newStr); // UID
        if (!uid)
          /* This is bad.  The server didn't give us a UIDL for this message.
             I've seen this happen when somehow the mail spool has a message
             that contains a header that reads "X-UIDL: \n".  But how that got
             there, I have no idea; must be a server bug.  Or something. */
          uid = "";

        // seeking right entry, but try the one that should it be first
        PRInt32 i;
        if(m_pop3ConData->msg_info[m_listpos - 1].msgnum == msg_num)
          i = m_listpos - 1;
        else
          for(i = 0; i < m_pop3ConData->number_of_messages &&
                     m_pop3ConData->msg_info[i].msgnum != msg_num; i++)
            ;

        // only if found a matching slot
        if (i < m_pop3ConData->number_of_messages)
        {
          // to protect us from memory leak in case of getting a msg num twice
          m_pop3ConData->msg_info[i].uidl = PL_strdup(uid);
          if (!m_pop3ConData->msg_info[i].uidl)
          {
            PR_Free(line);
            return MK_OUT_OF_MEMORY;
          }
        }
      }
    }
    PR_Free(line);
    return(0);
}



/* this function decides if we are going to do a
 * normal RETR or a TOP.  The first time, it also decides the total number
 * of bytes we're probably going to get.
 */
PRInt32 nsPop3Protocol::GetMsg()
{
  PRInt32 popstateTimestamp = TimeInSecondsFromPRTime(PR_Now());

  if (m_pop3ConData->last_accessed_msg >= m_pop3ConData->number_of_messages)
  {
    /* Oh, gee, we're all done. */
    if(m_pop3ConData->msg_del_started)
    {
      if (!m_pop3ConData->only_uidl)
      {
        if (m_pop3ConData->only_check_for_new_mail)
          m_nsIPop3Sink->SetBiffStateAndUpdateFE(m_pop3ConData->biffstate, m_pop3ConData->really_new_messages, PR_TRUE);
        /* update old style biff */
        else
          m_nsIPop3Sink->SetBiffStateAndUpdateFE(nsIMsgFolder::nsMsgBiffState_NewMail, m_pop3ConData->really_new_messages, PR_FALSE);
      }
      m_nsIPop3Sink->EndMailDelivery(this);
    }

    m_pop3ConData->next_state = POP3_SEND_QUIT;
    return 0;
  }

  if (m_totalDownloadSize < 0)
  {
    /* First time.  Figure out how many bytes we're about to get.
    If we didn't get any message info, then we are going to get
    everything, and it's easy.  Otherwise, if we only want one
    uidl, than that's the only one we'll get.  Otherwise, go
    through each message info, decide if we're going to get that
    message, and add the number of bytes for it. When a message is too
    large (per user's preferences) only add the size we are supposed
    to get. */
    m_pop3ConData->really_new_messages = 0;
    m_pop3ConData->real_new_counter = 1;
    if (m_pop3ConData->msg_info)
    {
      m_totalDownloadSize = 0;
      for (PRInt32 i = 0; i < m_pop3ConData->number_of_messages; i++)
      {
        if (m_pop3ConData->only_uidl)
        {
          if (m_pop3ConData->msg_info[i].uidl &&
            !PL_strcmp(m_pop3ConData->msg_info[i].uidl, m_pop3ConData->only_uidl))
          {
            m_totalDownloadSize = m_pop3ConData->msg_info[i].size;
            m_pop3ConData->really_new_messages = 1;
            // we are only getting one message
            m_pop3ConData->real_new_counter = 1;
            break;
          }
          continue;
        }

        char c = 0;
        popstateTimestamp = TimeInSecondsFromPRTime(PR_Now());
        if (m_pop3ConData->msg_info[i].uidl)
        {
          Pop3UidlEntry *uidlEntry = (Pop3UidlEntry *) PL_HashTableLookup(m_pop3ConData->uidlinfo->hash,
            m_pop3ConData->msg_info[i].uidl);
          if (uidlEntry)
          {
            c = uidlEntry->status;
            popstateTimestamp = uidlEntry->dateReceived;
          }
        }
        if ((c == KEEP) && !m_pop3ConData->leave_on_server)
        { /* This message has been downloaded but kept on server, we
           * no longer want to keep it there */
          if (!m_pop3ConData->newuidl)
          {
            m_pop3ConData->newuidl = PL_NewHashTable(20, PL_HashString, PL_CompareStrings,
                                              PL_CompareValues, &gHashAllocOps, nsnull);
            if (!m_pop3ConData->newuidl)
              return MK_OUT_OF_MEMORY;
          }
          c = DELETE_CHAR;
          // Mark message to be deleted in new table
          put_hash(m_pop3ConData->newuidl,
            m_pop3ConData->msg_info[i].uidl, DELETE_CHAR, popstateTimestamp);
          // and old one too
          put_hash(m_pop3ConData->uidlinfo->hash,
            m_pop3ConData->msg_info[i].uidl, DELETE_CHAR, popstateTimestamp);
        }
        if ((c != KEEP) && (c != DELETE_CHAR) && (c != TOO_BIG))
        { // message left on server
          m_totalDownloadSize += m_pop3ConData->msg_info[i].size;
          m_pop3ConData->really_new_messages++;
          // a message we will really download
        }
      }
    }
    else
    {
      m_totalDownloadSize = m_totalFolderSize;
    }
    if (m_pop3ConData->only_check_for_new_mail)
    {
      if (m_totalDownloadSize > 0)
      {
        m_pop3ConData->biffstate = nsIMsgFolder::nsMsgBiffState_NewMail;
        m_nsIPop3Sink->SetBiffStateAndUpdateFE(nsIMsgFolder::nsMsgBiffState_NewMail, m_pop3ConData->really_new_messages, PR_TRUE);
      }
      m_pop3ConData->next_state = POP3_SEND_QUIT;
      return(0);
    }
    /* get the amount of available space on the drive
     * and make sure there is enough
     */
    if (m_totalDownloadSize > 0) // skip all this if there aren't any messages
    {
      nsresult rv;
      PRInt64 mailboxSpaceLeft = LL_Zero();
      nsCOMPtr <nsIMsgFolder> folder;
      nsCOMPtr <nsILocalFile> path;

      // Get the path to the current mailbox
      //
      NS_ENSURE_TRUE(m_nsIPop3Sink, NS_ERROR_UNEXPECTED);
      rv = m_nsIPop3Sink->GetFolder(getter_AddRefs(folder));
      if (NS_FAILED(rv)) return rv;
      rv = folder->GetFilePath(getter_AddRefs(path));
      if (NS_FAILED(rv)) return rv;

      // call GetDiskSpaceAvailable on the directory
      nsCOMPtr <nsIFile> parent;
      path->GetParent(getter_AddRefs(parent));
      nsCOMPtr <nsILocalFile> localParent = do_QueryInterface(parent, &rv);
      if (localParent)
        rv = localParent->GetDiskSpaceAvailable(&mailboxSpaceLeft);
      if (NS_FAILED(rv))
      {
        // The call to GetDiskSpaceAvailable FAILED!
        // This will happen on certain platforms where GetDiskSpaceAvailable
        // is not implemented. Since people on those platforms still need
        // to check mail, we will simply bypass the disk-space check.
        //
        // We'll leave a debug message to warn people.

#ifdef DEBUG
        printf("Call to GetDiskSpaceAvailable FAILED! \n");
#endif
      }
      else
      {
#ifdef DEBUG
        printf("GetDiskSpaceAvailable returned: %lld bytes\n", mailboxSpaceLeft);
#endif

        /* When checking for disk space available, take into consideration
        * possible database
        * changes, therefore ask for a little more than what the message
        * size is. Also, due to disk sector sizes, allocation blocks,
        * etc. The space "available" may be greater than the actual space
        * usable. */

        PRInt64 llResult;
        PRInt64 llExtraSafetySpace;
        PRInt64 llTotalDownloadSize;
        LL_I2L(llExtraSafetySpace, EXTRA_SAFETY_SPACE);
        LL_I2L(llTotalDownloadSize, m_totalDownloadSize);

        LL_ADD(llResult, llTotalDownloadSize, llExtraSafetySpace);
        if (LL_CMP(llResult, >, mailboxSpaceLeft))
        {
          // Not enough disk space!
#ifdef DEBUG
          printf("Not enough disk space! Raising error! \n");
#endif
          return (Error(MK_POP3_OUT_OF_DISK_SPACE));
        }
      }
    }
  }

  /* Look at this message, and decide whether to ignore it, get it, just get
  the TOP of it, or delete it. */

  // if this is a message we've seen for the first time, we won't find it in
  // m_pop3ConData-uidlinfo->hash.  By default, we retrieve messages, unless they have a status,
  // or are too big, in which case we figure out what to do.
  PRBool prefBool = PR_FALSE;
  m_pop3Server->GetAuthLogin(&prefBool);

  if (prefBool && (TestCapFlag(POP3_HAS_XSENDER)))
    m_pop3ConData->next_state = POP3_SEND_XSENDER;
  else
    m_pop3ConData->next_state = POP3_SEND_RETR;
  m_pop3ConData->truncating_cur_msg = PR_FALSE;
  m_pop3ConData->pause_for_read = PR_FALSE;
  if (m_pop3ConData->msg_info)
  {
    Pop3MsgInfo* info = m_pop3ConData->msg_info + m_pop3ConData->last_accessed_msg;
    if (m_pop3ConData->only_uidl)
    {
      if (info->uidl == NULL || PL_strcmp(info->uidl, m_pop3ConData->only_uidl))
        m_pop3ConData->next_state = POP3_GET_MSG;
      else
        m_pop3ConData->next_state = POP3_SEND_RETR;
    }
    else
    {
      char c = 0;
      if (!m_pop3ConData->newuidl)
      {
        m_pop3ConData->newuidl = PL_NewHashTable(20, PL_HashString, PL_CompareStrings, PL_CompareValues, &gHashAllocOps, nsnull);
        if (!m_pop3ConData->newuidl)
          return MK_OUT_OF_MEMORY;
      }
      if (info->uidl)
      {
        Pop3UidlEntry *uidlEntry = (Pop3UidlEntry *) PL_HashTableLookup(m_pop3ConData->uidlinfo->hash, info->uidl);
        if (uidlEntry)
        {
          c = uidlEntry->status;
          popstateTimestamp = uidlEntry->dateReceived;
        }
      }
      if (c == DELETE_CHAR)
      {
        m_pop3ConData->next_state = POP3_SEND_DELE;
      }
      else if (c == KEEP)
      {
        // this is a message we've already downloaded and left on server;
        // Advance to next message.
        m_pop3ConData->next_state = POP3_GET_MSG;
      }
      else if (c == FETCH_BODY)
      {
        m_pop3ConData->next_state = POP3_SEND_RETR;
        PL_HashTableRemove (m_pop3ConData->uidlinfo->hash, (void*)info->uidl);
      }
      else if ((c != TOO_BIG) &&
        (TestCapFlag(POP3_TOP_UNDEFINED | POP3_HAS_TOP)) &&
        (m_pop3ConData->headers_only ||
         ((m_pop3ConData->size_limit > 0) &&
          (info->size > m_pop3ConData->size_limit) &&
          !m_pop3ConData->only_uidl)) &&
        info->uidl && *info->uidl)
      {
        // message is too big
        m_pop3ConData->truncating_cur_msg = PR_TRUE;
        m_pop3ConData->next_state = POP3_SEND_TOP;
        put_hash(m_pop3ConData->newuidl, info->uidl, TOO_BIG, popstateTimestamp);
      }
      else if (c == TOO_BIG)
      {
        /* message previously left on server, see if the max download size
        has changed, because we may want to download the message this time
        around. Otherwise ignore the message, we have the header. */
        if ((m_pop3ConData->size_limit > 0) && (info->size <=
          m_pop3ConData->size_limit))
          PL_HashTableRemove (m_pop3ConData->uidlinfo->hash, (void*)info->uidl);
        // remove from our table, and download
        else
        {
          m_pop3ConData->truncating_cur_msg = PR_TRUE;
          m_pop3ConData->next_state = POP3_GET_MSG;
          // ignore this message and get next one
          put_hash(m_pop3ConData->newuidl, info->uidl, TOO_BIG, popstateTimestamp);
        }
      }

      if (m_pop3ConData->next_state != POP3_SEND_DELE &&
          info->uidl)
      {
        /* This is a message we have decided to keep on the server. Notate
            that now for the future. (Don't change the popstate file at all
            if only_uidl is set; in that case, there might be brand new messages
            on the server that we *don't* want to mark KEEP; we just want to
            leave them around until the user next does a GetNewMail.) */

        /* If this is a message we already know about (i.e., it was
            in popstate.dat already), we need to maintain the original
            date the message was downloaded. */
        if (m_pop3ConData->truncating_cur_msg)
          put_hash(m_pop3ConData->newuidl, info->uidl, TOO_BIG, popstateTimestamp);
        else
          put_hash(m_pop3ConData->newuidl, info->uidl, KEEP, popstateTimestamp);
      }
    }
    if (m_pop3ConData->next_state == POP3_GET_MSG)
      m_pop3ConData->last_accessed_msg++;
    // Make sure we check the next message next time!
  }
  return 0;
}


/* start retreiving just the first 20 lines
 */
PRInt32 nsPop3Protocol::SendTop()
{
   char * cmd = PR_smprintf( "TOP %ld %d" CRLF,
     m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg].msgnum,
     m_pop3ConData->headers_only ? 0 : 20);
   PRInt32 status = -1;
   if (cmd)
   {
     m_pop3ConData->next_state_after_response = POP3_TOP_RESPONSE;
     m_pop3ConData->cur_msg_size = -1;

     /* zero the bytes received in message in preparation for
     * the next
     */
     m_bytesInMsgReceived = 0;
     status = SendData(m_url,cmd);
   }
   PR_Free(cmd);
   return status;
}

/* send the xsender command
 */
PRInt32 nsPop3Protocol::SendXsender()
{
  char * cmd = PR_smprintf("XSENDER %ld" CRLF, m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg].msgnum);
  PRInt32 status = -1;
  if (cmd)
  {
    m_pop3ConData->next_state_after_response = POP3_XSENDER_RESPONSE;
    status = SendData(m_url, cmd);
    PR_Free(cmd);
  }
  return status;
}

PRInt32 nsPop3Protocol::XsenderResponse()
{
    m_pop3ConData->seenFromHeader = PR_FALSE;
    m_senderInfo = "";

    if (m_pop3ConData->command_succeeded) {
        if (m_commandResponse.Length() > 4)
            m_senderInfo = m_commandResponse;
    }
    else {
        ClearCapFlag(POP3_HAS_XSENDER);
        m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
    }

    if (m_pop3ConData->truncating_cur_msg)
        m_pop3ConData->next_state = POP3_SEND_TOP;
    else
        m_pop3ConData->next_state = POP3_SEND_RETR;
    return 0;
}

/* retreive the whole message
 */
PRInt32
nsPop3Protocol::SendRetr()
{

  char * cmd = PR_smprintf("RETR %ld" CRLF, m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg].msgnum);
  PRInt32 status = -1;
  if (cmd)
  {
    m_pop3ConData->next_state_after_response = POP3_RETR_RESPONSE;
    m_pop3ConData->cur_msg_size = -1;


    /* zero the bytes received in message in preparation for
    * the next
    */
    m_bytesInMsgReceived = 0;

    if (m_pop3ConData->only_uidl)
    {
      /* Display bytes if we're only downloading one message. */
      PR_ASSERT(!m_pop3ConData->graph_progress_bytes_p);
      UpdateProgressPercent(0, m_totalDownloadSize);
      m_pop3ConData->graph_progress_bytes_p = PR_TRUE;
    }
    else
    {
      nsresult rv;

      nsAutoString realNewString;
      realNewString.AppendInt(m_pop3ConData->real_new_counter);

      nsAutoString reallyNewMessages;
      reallyNewMessages.AppendInt(m_pop3ConData->really_new_messages);

      const PRUnichar *formatStrings[] = {
        realNewString.get(),
          reallyNewMessages.get(),
      };

      nsString finalString;
      rv = mLocalBundle->FormatStringFromID(LOCAL_STATUS_RECEIVING_MESSAGE_OF,
        formatStrings, 2,
        getter_Copies(finalString));
      NS_ASSERTION(NS_SUCCEEDED(rv), "couldn't format string");
      if (m_statusFeedback)
        m_statusFeedback->ShowStatusString(finalString);
    }

    status = SendData(m_url, cmd);
  } // if cmd
  PR_Free(cmd);
  return status;
}

/* digest the message
 */
PRInt32
nsPop3Protocol::RetrResponse(nsIInputStream* inputStream,
                             PRUint32 length)
{
    PRUint32 buffer_size;
    PRInt32 flags = 0;
    char *uidl = NULL;
    nsresult rv;
#if 0
    PRInt32 old_bytes_received = m_totalBytesReceived;
#endif
    PRUint32 status = 0;

    if(m_pop3ConData->cur_msg_size == -1)
    {
        /* this is the beginning of a message
         * get the response code and byte size
         */
        if(!m_pop3ConData->command_succeeded)
            return Error(POP3_RETR_FAILURE);

        /* a successful RETR response looks like: #num_bytes Junk
           from TOP we only get the +OK and data
           */
        if (m_pop3ConData->truncating_cur_msg)
        { /* TOP, truncated message */
            flags |= nsMsgMessageFlags::Partial;
        }
        else
        {
          nsCString cmdResp(m_commandResponse);
          char *newStr = cmdResp.BeginWriting();
          char *num = NS_strtok( " ", &newStr);
          if (num)
            m_pop3ConData->cur_msg_size = atol(num);
          m_commandResponse = newStr;
        }

        /* RETR complete message */
        if (!m_senderInfo.IsEmpty())
            flags |= nsMsgMessageFlags::SenderAuthed;

        if(m_pop3ConData->cur_msg_size <= 0)
        {
          if (m_pop3ConData->msg_info)
            m_pop3ConData->cur_msg_size = m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg].size;
          else
            m_pop3ConData->cur_msg_size = 0;
        }

        if (m_pop3ConData->msg_info &&
            m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg].uidl)
            uidl = m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg].uidl;

        m_pop3ConData->parsed_bytes = 0;
        m_pop3ConData->pop3_size = m_pop3ConData->cur_msg_size;
        m_pop3ConData->assumed_end = PR_FALSE;

        m_pop3Server->GetDotFix(&m_pop3ConData->dot_fix);

        PR_LOG(POP3LOGMODULE,PR_LOG_ALWAYS,
               ("Opening message stream: MSG_IncorporateBegin"));

        /* open the message stream so we have someplace
         * to put the data
         */
        m_pop3ConData->real_new_counter++;
        /* (rb) count only real messages being downloaded */
        rv = m_nsIPop3Sink->IncorporateBegin(uidl, m_url, flags,
                                        &m_pop3ConData->msg_closure);

        PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS, ("Done opening message stream!"));

        if(!m_pop3ConData->msg_closure || NS_FAILED(rv))
            return(Error(POP3_MESSAGE_WRITE_ERROR));
    }

    m_pop3ConData->pause_for_read = PR_TRUE;

    PRBool pauseForMoreData = PR_FALSE;
    char *line = m_lineStreamBuffer->ReadNextLine(inputStream, status, pauseForMoreData, &rv, PR_TRUE);
    PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));
    if (NS_FAILED(rv))
      return -1;

    buffer_size = status;

    if (status == 0 && !line)  // no bytes read in...
      return (0);

    if (m_pop3ConData->msg_closure) /* not done yet */
    {
      // buffer the line we just read in, and buffer all remaining lines in the stream
      status = buffer_size;
      do
      {
        if (m_pop3ConData->msg_closure)
        {
          rv = HandleLine(line, buffer_size);
          if (NS_FAILED(rv))
            return (Error(POP3_MESSAGE_WRITE_ERROR));

          // buffer_size already includes MSG_LINEBREAK_LEN so
          // subtract and add CRLF
          // but not really sure we always had CRLF in input since
          // we also treat a single LF as line ending!
          m_pop3ConData->parsed_bytes += buffer_size - MSG_LINEBREAK_LEN + 2;
        }

        // now read in the next line
        PR_Free(line);
        line = m_lineStreamBuffer->ReadNextLine(inputStream, buffer_size,
                                                pauseForMoreData, &rv, PR_TRUE);
        if (NS_FAILED(rv))
          return -1;

        PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,("RECV: %s", line));
        // buffer_size already includes MSG_LINEBREAK_LEN so
        // subtract and add CRLF
        // but not really sure we always had CRLF in input since
        // we also treat a single LF as line ending!
        status += buffer_size - MSG_LINEBREAK_LEN + 2;
      } while (line);
    }

    buffer_size = status;  // status holds # bytes we've actually buffered so far...

    /* normal read. Yay! */
    if ((PRInt32) (m_bytesInMsgReceived + buffer_size) > m_pop3ConData->cur_msg_size)
        buffer_size = m_pop3ConData->cur_msg_size - m_bytesInMsgReceived;

    m_bytesInMsgReceived += buffer_size;
    m_totalBytesReceived += buffer_size;

    // *** jefft in case of the message size that server tells us is different
    // from the actual message size
    if (pauseForMoreData && m_pop3ConData->dot_fix &&
        m_pop3ConData->assumed_end && m_pop3ConData->msg_closure)
    {
        nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_url, &rv);
        nsCOMPtr<nsIMsgWindow> msgWindow;
        if (NS_SUCCEEDED(rv))
          rv = mailnewsUrl->GetMsgWindow(getter_AddRefs(msgWindow));
        rv = m_nsIPop3Sink->IncorporateComplete(msgWindow,
          m_pop3ConData->truncating_cur_msg ? m_pop3ConData->cur_msg_size : 0);

        // The following was added to prevent the loss of Data when we try
        // and write to somewhere we don't have write access error to (See
        // bug 62480)
        // (Note: This is only a temp hack until the underlying XPCOM is
        // fixed to return errors)

        if (NS_FAILED(rv))
            return (Error((rv == NS_MSG_ERROR_COPYING_FROM_TMP_DOWNLOAD)
                           ? POP3_TMP_DOWNLOAD_FAILED
                           : POP3_MESSAGE_WRITE_ERROR));

        m_pop3ConData->msg_closure = nsnull;
    }

    if (!m_pop3ConData->msg_closure)
        /* meaning _handle_line read ".\r\n" at end-of-msg */
    {
        m_pop3ConData->pause_for_read = PR_FALSE;

        if (m_pop3ConData->truncating_cur_msg ||
            m_pop3ConData->leave_on_server )
        {
            Pop3UidlEntry *uidlEntry = NULL;
            Pop3MsgInfo* info = m_pop3ConData->msg_info + m_pop3ConData->last_accessed_msg;

            /* Check for filter actions - FETCH or DELETE */
            if ((m_pop3ConData->newuidl) && (info->uidl))
              uidlEntry = (Pop3UidlEntry *)PL_HashTableLookup(m_pop3ConData->newuidl, info->uidl);

            if (uidlEntry && uidlEntry->status == FETCH_BODY &&
                m_pop3ConData->truncating_cur_msg)
            {
            /* A filter decided to retrieve this full msg.
               Use GetMsg() so the popstate will update correctly,
               but don't let this msg get counted twice. */
               m_pop3ConData->next_state = POP3_GET_MSG;
               m_pop3ConData->real_new_counter--;
            /* Make sure we don't try to come through here again. */
               PL_HashTableRemove (m_pop3ConData->newuidl, (void*)info->uidl);
               put_hash(m_pop3ConData->uidlinfo->hash, info->uidl, FETCH_BODY, uidlEntry->dateReceived);

            } else if (uidlEntry && uidlEntry->status == DELETE_CHAR)
            {
            // A filter decided to delete this msg from the server
               m_pop3ConData->next_state = POP3_SEND_DELE;
            } else
            {
            /* We've retrieved all or part of this message, but we want to
               keep it on the server.  Go on to the next message. */
               m_pop3ConData->last_accessed_msg++;
               m_pop3ConData->next_state = POP3_GET_MSG;
            }
            if (m_pop3ConData->only_uidl)
            {
            /* GetMsg didn't update this field. Do it now */
              uidlEntry = (Pop3UidlEntry *)PL_HashTableLookup(m_pop3ConData->uidlinfo->hash, m_pop3ConData->only_uidl);
              NS_ASSERTION(uidlEntry, "uidl not found in uidlinfo");
              if (uidlEntry)
                put_hash(m_pop3ConData->uidlinfo->hash, m_pop3ConData->only_uidl, KEEP, uidlEntry->dateReceived);
            }
        }
        else
        {
           m_pop3ConData->next_state = POP3_SEND_DELE;
        }

        /* if we didn't get the whole message add the bytes that we didn't get
           to the bytes received part so that the progress percent stays sane.
           */
        if(m_bytesInMsgReceived < m_pop3ConData->cur_msg_size)
            m_totalBytesReceived += (m_pop3ConData->cur_msg_size -
                                   m_bytesInMsgReceived);
    }

    /* set percent done to portion of total bytes of all messages
       that we're going to download. */
    if (m_totalDownloadSize)
      UpdateProgressPercent(m_totalBytesReceived, m_totalDownloadSize);

    PR_Free(line);
    return(0);
}


PRInt32
nsPop3Protocol::TopResponse(nsIInputStream* inputStream, PRUint32 length)
{
  if (TestCapFlag(POP3_TOP_UNDEFINED))
  {
    ClearCapFlag(POP3_TOP_UNDEFINED);
    if (m_pop3ConData->command_succeeded)
      SetCapFlag(POP3_HAS_TOP);
    else
      ClearCapFlag(POP3_HAS_TOP);
    m_pop3Server->SetPop3CapabilityFlags(m_pop3ConData->capability_flags);
  }

  if(m_pop3ConData->cur_msg_size == -1 &&  /* first line after TOP command sent */
    !m_pop3ConData->command_succeeded)  /* and TOP command failed */
  {
  /* TOP doesn't work so we can't retrieve the first part of this msg.
  So just go download the whole thing, and warn the user.

    Note that the progress bar will not be accurate in this case.
    Oops. #### */
    PRBool prefBool = PR_FALSE;
    m_pop3ConData->truncating_cur_msg = PR_FALSE;

    nsString statusTemplate;
    mLocalBundle->GetStringFromID(POP3_SERVER_DOES_NOT_SUPPORT_THE_TOP_COMMAND, getter_Copies(statusTemplate));
    if (!statusTemplate.IsEmpty())
    {
      nsCAutoString hostName;
      PRUnichar * statusString = nsnull;
      m_url->GetHost(hostName);

      statusString = nsTextFormatter::smprintf(statusTemplate.get(), hostName.get());
      UpdateStatusWithString(statusString);
      nsTextFormatter::smprintf_free(statusString);
    }

    m_pop3Server->GetAuthLogin(&prefBool);

    if (prefBool &&
      (TestCapFlag(POP3_HAS_XSENDER)))
      m_pop3ConData->next_state = POP3_SEND_XSENDER;
    else
      m_pop3ConData->next_state = POP3_SEND_RETR;
    return(0);
  }

  /* If TOP works, we handle it in the same way as RETR. */
  return RetrResponse(inputStream, length);
}

/* line is handed over as null-terminated string with MSG_LINEBREAK */
nsresult
nsPop3Protocol::HandleLine(char *line, PRUint32 line_length)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(m_pop3ConData->msg_closure, "m_pop3ConData->msg_closure is null in nsPop3Protocol::HandleLine()");
    if (!m_pop3ConData->msg_closure)
        return NS_ERROR_NULL_POINTER;

    if (!m_senderInfo.IsEmpty() && !m_pop3ConData->seenFromHeader)
    {
        if (line_length > 6 && !PL_strncasecmp("From: ", line, 6))
        {
            m_pop3ConData->seenFromHeader = PR_TRUE;
            if (PL_strstr(line, m_senderInfo.get()) == NULL)
                m_nsIPop3Sink->SetSenderAuthedFlag(m_pop3ConData->msg_closure,
                                                     PR_FALSE);
        }
    }

    // line contains only a single dot and linebreak -> message end
    if (line_length == 1 + MSG_LINEBREAK_LEN && line[0] == '.')
    {
        m_pop3ConData->assumed_end = PR_TRUE;  /* in case byte count from server is */
                                    /* wrong, mark we may have had the end */
        if (!m_pop3ConData->dot_fix || m_pop3ConData->truncating_cur_msg ||
            (m_pop3ConData->parsed_bytes >= (m_pop3ConData->pop3_size -3)))
        {
            nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(m_url, &rv);
            nsCOMPtr<nsIMsgWindow> msgWindow;
            if (NS_SUCCEEDED(rv))
              rv = mailnewsUrl->GetMsgWindow(getter_AddRefs(msgWindow));
            rv = m_nsIPop3Sink->IncorporateComplete(msgWindow,
              m_pop3ConData->truncating_cur_msg ? m_pop3ConData->cur_msg_size : 0);

            // The following was added to prevent the loss of Data when we try
            // and write to somewhere we don't have write access error to (See
            // bug 62480)
            // (Note: This is only a temp hack until the underlying XPCOM is
            // fixed to return errors)

            if (NS_FAILED(rv))
              return (Error((rv == NS_MSG_ERROR_COPYING_FROM_TMP_DOWNLOAD)
                             ? POP3_TMP_DOWNLOAD_FAILED
                             : POP3_MESSAGE_WRITE_ERROR));

            m_pop3ConData->msg_closure = nsnull;
            return rv;
        }
    }
    /* Check if the line begins with the termination octet. If so
       and if another termination octet follows, we step over the
       first occurence of it. */
    else if (line_length > 1 && line[0] == '.' && line[1] == '.') {
        line++;
        line_length--;

    }

    return m_nsIPop3Sink->IncorporateWrite(line, line_length);
}

PRInt32 nsPop3Protocol::SendDele()
{
    /* increment the last accessed message since we have now read it
     */
    char * cmd = PR_smprintf("DELE %ld" CRLF, m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg].msgnum);
    m_pop3ConData->last_accessed_msg++;
    PRInt32 status = -1;
    if (cmd)
    {
      m_pop3ConData->next_state_after_response = POP3_DELE_RESPONSE;
      status = SendData(m_url, cmd);
    }
    PR_Free(cmd);
    return status;
}

PRInt32 nsPop3Protocol::DeleResponse()
{
  Pop3UidlHost *host = NULL;

  host = m_pop3ConData->uidlinfo;

  /* the return from the delete will come here
  */
  if(!m_pop3ConData->command_succeeded)
    return(Error(POP3_DELE_FAILURE));


  /*  ###chrisf
  the delete succeeded.  Write out state so that we
  keep track of all the deletes which have not yet been
  committed on the server.  Flush this state upon successful
  QUIT.

  We will do this by adding each successfully deleted message id
  to a list which we will write out to popstate.dat in
  net_pop3_write_state().
  */
  if (host)
  {
    if (m_pop3ConData->msg_info &&
      m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg-1].uidl)
    {
      if (m_pop3ConData->newuidl)
        if (m_pop3ConData->leave_on_server)
        {
          PL_HashTableRemove(m_pop3ConData->newuidl, (void*)
            m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg-1].uidl);
        }
        else
        {
          put_hash(m_pop3ConData->newuidl,
            m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg-1].uidl, DELETE_CHAR, 0);
          /* kill message in new hash table */
        }
      else
        PL_HashTableRemove(host->hash,
            (void*) m_pop3ConData->msg_info[m_pop3ConData->last_accessed_msg-1].uidl);
    }
  }

  m_pop3ConData->next_state = POP3_GET_MSG;
  m_pop3ConData->pause_for_read = PR_FALSE;

  return(0);
}


PRInt32
nsPop3Protocol::CommitState(PRBool remove_last_entry)
{
  // only use newuidl if we successfully finished looping through all the
  // messages in the inbox.
  if (m_pop3ConData->newuidl)
  {
    if (m_pop3ConData->last_accessed_msg >= m_pop3ConData->number_of_messages)
    {
      PL_HashTableDestroy(m_pop3ConData->uidlinfo->hash);
      m_pop3ConData->uidlinfo->hash = m_pop3ConData->newuidl;
      m_pop3ConData->newuidl = nsnull;
    }
    else
    {
      /* If we are leaving messages on the server, pull out the last
        uidl from the hash, because it might have been put in there before
        we got it into the database.
      */
      if (remove_last_entry && m_pop3ConData->msg_info &&
          !m_pop3ConData->only_uidl && m_pop3ConData->newuidl->nentries > 0)
      {
        Pop3MsgInfo* info = m_pop3ConData->msg_info + m_pop3ConData->last_accessed_msg;
        if (info && info->uidl)
        {
          PRBool val = PL_HashTableRemove(m_pop3ConData->newuidl, info->uidl);
          NS_ASSERTION(val, "uidl not in hash table");
        }
      }

      // Add the entries in newuidl to m_pop3ConData->uidlinfo->hash to keep
      // track of the messages we *did* download in this session.
      PL_HashTableEnumerateEntries(m_pop3ConData->newuidl, net_pop3_copy_hash_entries, (void *)m_pop3ConData->uidlinfo->hash);
    }
  }

  if (!m_pop3ConData->only_check_for_new_mail)
  {
    nsresult rv;
    nsCOMPtr<nsILocalFile> mailDirectory;

    // get the mail directory
    nsCOMPtr<nsIMsgIncomingServer> server =
      do_QueryInterface(m_pop3Server, &rv);
    if (NS_FAILED(rv)) return -1;

    rv = server->GetLocalPath(getter_AddRefs(mailDirectory));
    if (NS_FAILED(rv)) return -1;

    // write the state in the mail directory
    net_pop3_write_state(m_pop3ConData->uidlinfo, mailDirectory.get());
  }
  return 0;
}


/* NET_process_Pop3  will control the state machine that
 * loads messages from a pop3 server
 *
 * returns negative if the transfer is finished or error'd out
 *
 * returns zero or more if the transfer needs to be continued.
 */
nsresult nsPop3Protocol::ProcessProtocolState(nsIURI * url, nsIInputStream * aInputStream,
                                              PRUint32 sourceOffset, PRUint32 aLength)
{
  PRInt32 status = 0;
  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsurl = do_QueryInterface(m_url);

  PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS, ("Entering NET_ProcessPop3 %d",
    aLength));

  m_pop3ConData->pause_for_read = PR_FALSE; /* already paused; reset */

  if(m_username.IsEmpty())
  {
    // net_pop3_block = PR_FALSE;
    return(Error(POP3_USERNAME_UNDEFINED));
  }

  while(!m_pop3ConData->pause_for_read)
  {
    PR_LOG(POP3LOGMODULE, PR_LOG_ALWAYS,
      ("POP3: Entering state: %d", m_pop3ConData->next_state));

    switch(m_pop3ConData->next_state)
    {
    case POP3_READ_PASSWORD:
    /* This is a separate state so that we're waiting for the
    user to type in a password while we don't actually have
    a connection to the pop server open; this saves us from
    having to worry about the server timing out on us while
      we wait for user input. */
      {
      /* If we're just checking for new mail (biff) then don't
      prompt the user for a password; just tell him we don't
        know whether he has new mail. */
        nsCString password;
        PRBool okayValue;
        GetPassword(password, &okayValue);
        const char * pwd = password.get();
        if (password.IsEmpty() || m_username.IsEmpty())
        {
          status = MK_POP3_PASSWORD_UNDEFINED;
          m_pop3ConData->biffstate = nsIMsgFolder::nsMsgBiffState_Unknown;
          m_nsIPop3Sink->SetBiffStateAndUpdateFE(m_pop3ConData->biffstate, 0, PR_FALSE);

          /* update old style biff */
          m_pop3ConData->next_state = POP3_FREE;
          m_pop3ConData->pause_for_read = PR_FALSE;
          break;
        }

        if (m_username.IsEmpty() || !pwd)
        {
          m_pop3ConData->next_state = POP3_ERROR_DONE;
          m_pop3ConData->pause_for_read = PR_FALSE;
        }
        else
        {
          // we are already connected so just go on and send the username
          PRBool prefBool = PR_FALSE;
          m_pop3ConData->pause_for_read = PR_FALSE;
          m_pop3Server->GetAuthLogin(&prefBool);

          if (prefBool)
          {
            if (TestCapFlag(POP3_AUTH_MECH_UNDEFINED))
              m_pop3ConData->next_state = POP3_SEND_AUTH;
            else
              m_pop3ConData->next_state = POP3_SEND_CAPA;
          }
          else
            m_pop3ConData->next_state = POP3_SEND_USERNAME;
        }
        break;
      }


    case POP3_START_CONNECT:
      {
        m_pop3ConData->next_state = POP3_FINISH_CONNECT;
        m_pop3ConData->pause_for_read = PR_FALSE;
        break;
      }

    case POP3_FINISH_CONNECT:
      {
        m_pop3ConData->pause_for_read = PR_FALSE;
        m_pop3ConData->next_state = POP3_WAIT_FOR_START_OF_CONNECTION_RESPONSE;
        break;
      }

    case POP3_WAIT_FOR_RESPONSE:
      status = WaitForResponse(aInputStream, aLength);
      break;

    case POP3_WAIT_FOR_START_OF_CONNECTION_RESPONSE:
      {
        status = WaitForStartOfConnectionResponse(aInputStream, aLength);

        if(status)
        {
          PRBool prefBool = PR_FALSE;
          m_pop3Server->GetAuthLogin(&prefBool);

          if (prefBool)
          {
            if (TestCapFlag(POP3_AUTH_MECH_UNDEFINED))
              m_pop3ConData->next_state = POP3_SEND_AUTH;
            else
              m_pop3ConData->next_state = POP3_SEND_CAPA;
          }
          else
            m_pop3ConData->next_state = POP3_SEND_USERNAME;
        }

        break;
      }

    case POP3_SEND_AUTH:
      status = SendAuth();
      break;

    case POP3_AUTH_RESPONSE:
      status = AuthResponse(aInputStream, aLength);
      break;

   case POP3_SEND_CAPA:
      status = SendCapa();
      break;

    case POP3_CAPA_RESPONSE:
      status = CapaResponse(aInputStream, aLength);
      break;

    case POP3_TLS_RESPONSE:
      status = SendTLSResponse();
      break;

    case POP3_PROCESS_AUTH:
      status = ProcessAuth();
      break;

    case POP3_AUTH_FALLBACK:
      status = AuthFallback();
      break;

    case POP3_AUTH_LOGIN:
      status = AuthLogin();
      break;

    case POP3_AUTH_LOGIN_RESPONSE:
      status = AuthLoginResponse();
      break;

    case POP3_AUTH_NTLM:
      status = AuthNtlm();
      break;

    case POP3_AUTH_NTLM_RESPONSE:
      status = AuthNtlmResponse();
      break;

    case POP3_AUTH_GSSAPI:
      status = AuthGSSAPI();
      break;

    case POP3_AUTH_GSSAPI_FIRST:
      UpdateStatus(POP3_CONNECT_HOST_CONTACTED_SENDING_LOGIN_INFORMATION);
      status = AuthGSSAPIResponse(true);
      break;

    case POP3_AUTH_GSSAPI_STEP:
      status = AuthGSSAPIResponse(false);
      break;

    case POP3_SEND_USERNAME:
      UpdateStatus(POP3_CONNECT_HOST_CONTACTED_SENDING_LOGIN_INFORMATION);
      status = SendUsername();
      break;

    case POP3_SEND_PASSWORD:
      status = SendPassword();
      break;

    case POP3_SEND_GURL:
      status = SendGurl();
      break;

    case POP3_GURL_RESPONSE:
      status = GurlResponse();
      break;

    case POP3_SEND_STAT:
      status = SendStat();
      break;

    case POP3_GET_STAT:
      status = GetStat();
      break;

    case POP3_SEND_LIST:
      status = SendList();
      break;

    case POP3_GET_LIST:
      status = GetList(aInputStream, aLength);
      break;

    case POP3_SEND_UIDL_LIST:
      status = SendUidlList();
      break;

    case POP3_GET_UIDL_LIST:
      status = GetUidlList(aInputStream, aLength);
      break;

    case POP3_SEND_XTND_XLST_MSGID:
      status = SendXtndXlstMsgid();
      break;

    case POP3_GET_XTND_XLST_MSGID:
      status = GetXtndXlstMsgid(aInputStream, aLength);
      break;

    case POP3_GET_MSG:
      status = GetMsg();
      break;

    case POP3_SEND_TOP:
      status = SendTop();
      break;

    case POP3_TOP_RESPONSE:
      status = TopResponse(aInputStream, aLength);
      break;

    case POP3_SEND_XSENDER:
      status = SendXsender();
      break;

    case POP3_XSENDER_RESPONSE:
      status = XsenderResponse();
      break;

    case POP3_SEND_RETR:
      status = SendRetr();
      break;

    case POP3_RETR_RESPONSE:
      status = RetrResponse(aInputStream, aLength);
      break;

    case POP3_SEND_DELE:
      status = SendDele();
      break;

    case POP3_DELE_RESPONSE:
      status = DeleResponse();
      break;

    case POP3_SEND_QUIT:
    /* attempt to send a server quit command.  Since this means
    everything went well, this is a good time to update the
    status file and the FE's biff state.
      */
      if (!m_pop3ConData->only_uidl)
      {
        /* update old style biff */
        if (!m_pop3ConData->only_check_for_new_mail)
        {
        /* We don't want to pop up a warning message any more (see
        bug 54116), so instead we put the "no new messages" or
        "retrieved x new messages"
        in the status line.  Unfortunately, this tends to be running
        in a progress pane, so we try to get the real pane and
          show the message there. */

          if (m_totalDownloadSize <= 0)
          {
            UpdateStatus(POP3_NO_MESSAGES);
            /* There are no new messages.  */
          }
          else
          {
            nsString statusTemplate;
            mLocalBundle->GetStringFromID(POP3_DOWNLOAD_COUNT, getter_Copies(statusTemplate));
            if (!statusTemplate.IsEmpty())
            {
              PRUnichar * statusString = nsTextFormatter::smprintf(statusTemplate.get(),
                m_pop3ConData->real_new_counter - 1,
                m_pop3ConData->really_new_messages);
              UpdateStatusWithString(statusString);
              nsTextFormatter::smprintf_free(statusString);
            }
          }
        }
      }

      status = SendData(mailnewsurl, "QUIT" CRLF);
      m_pop3ConData->next_state = POP3_WAIT_FOR_RESPONSE;
      m_pop3ConData->next_state_after_response = POP3_QUIT_RESPONSE;
      break;

    case POP3_QUIT_RESPONSE:
      if(m_pop3ConData->command_succeeded)
      {
      /*  the QUIT succeeded.  We can now flush the state in popstate.dat which
        keeps track of any uncommitted DELE's */

        /* clear the hash of all our uncommitted deletes */
        if (!m_pop3ConData->leave_on_server && m_pop3ConData->newuidl)
        {
          PL_HashTableEnumerateEntries(m_pop3ConData->newuidl,
                                        net_pop3_remove_messages_marked_delete,
                                       (void *)m_pop3ConData);
        }
        m_pop3ConData->next_state = POP3_DONE;
      }
      else
      {
        m_pop3ConData->next_state = POP3_ERROR_DONE;
      }
      break;

    case POP3_DONE:
      CommitState(PR_FALSE);

      if (mailnewsurl)
        mailnewsurl->SetUrlState(PR_FALSE, NS_OK);
      m_pop3ConData->next_state = POP3_FREE;
      break;

    case POP3_ERROR_DONE:
      /*  write out the state */
      if(m_pop3ConData->list_done)
        CommitState(PR_TRUE);

      if(m_pop3ConData->msg_closure)
      {
        m_nsIPop3Sink->IncorporateAbort(m_pop3ConData->only_uidl != nsnull);
        m_pop3ConData->msg_closure = NULL;
        m_nsIPop3Sink->AbortMailDelivery(this);
      }

      if(m_pop3ConData->msg_del_started)
      {
        nsString statusTemplate;
        mLocalBundle->GetStringFromID(POP3_DOWNLOAD_COUNT, getter_Copies(statusTemplate));
        if (!statusTemplate.IsEmpty())
        {
          PRUnichar * statusString = nsTextFormatter::smprintf(statusTemplate.get(),
            m_pop3ConData->real_new_counter - 1,
            m_pop3ConData->really_new_messages);
          UpdateStatusWithString(statusString);
          nsTextFormatter::smprintf_free(statusString);
        }

        NS_ASSERTION (!TestFlag(POP3_PASSWORD_FAILED), "POP3_PASSWORD_FAILED set when del_started");
        m_nsIPop3Sink->AbortMailDelivery(this);
      }

      if (TestFlag(POP3_PASSWORD_FAILED) && m_pop3ConData->logonFailureCount < 6)
      {
      /* We got here because the password was wrong, so go
        read a new one and re-open the connection. */
        m_pop3ConData->next_state = POP3_READ_PASSWORD;
        m_pop3ConData->command_succeeded = PR_TRUE;
        status = 0;
        break;
      }
      else
        /* Else we got a "real" error, so finish up. */
        m_pop3ConData->next_state = POP3_FREE;

      if (mailnewsurl)
        mailnewsurl->SetUrlState(PR_FALSE, NS_ERROR_FAILURE);
      m_pop3ConData->pause_for_read = PR_FALSE;
      break;

    case POP3_FREE:
      UpdateProgressPercent(0,0); // clear out the progress meter
      NS_ASSERTION(m_nsIPop3Sink, "with no sink, can't clear busy flag");
      if (m_nsIPop3Sink)
      {
        nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_pop3Server);
        if (server)
          server->SetServerBusy(PR_FALSE); // the server is now not busy
      }
      m_pop3Server->SetRunningProtocol(nsnull);

      CloseSocket();
      return NS_OK;
      break;

    default:
      PR_ASSERT(0);

    }  /* end switch */

    if((status < 0) && m_pop3ConData->next_state != POP3_FREE)
    {
      m_pop3ConData->pause_for_read = PR_FALSE;
      m_pop3ConData->next_state = POP3_ERROR_DONE;
    }

  }  /* end while */

  return NS_OK;

}

nsresult nsPop3Protocol::CloseSocket()
{
    nsresult rv = nsMsgProtocol::CloseSocket();
    m_url = nsnull;
    return rv;
}

NS_IMETHODIMP nsPop3Protocol::MarkMessages(nsVoidArray *aUIDLArray)
{
  NS_ENSURE_ARG_POINTER(aUIDLArray);
  PRUint32 count = aUIDLArray->Count();

  for (PRUint32 i = 0; i < count; i++)
  {
    PRBool changed;
    if (m_pop3ConData->newuidl)
      MarkMsgInHashTable(m_pop3ConData->newuidl, static_cast<Pop3UidlEntry*>(aUIDLArray->ElementAt(i)), &changed);
    if (m_pop3ConData->uidlinfo)
      MarkMsgInHashTable(m_pop3ConData->uidlinfo->hash, static_cast<Pop3UidlEntry*>(aUIDLArray->ElementAt(i)), &changed);
  }
  return NS_OK;
}

NS_IMETHODIMP nsPop3Protocol::CheckMessage(const char *aUidl, PRBool *aBool)
{
  Pop3UidlEntry *uidlEntry = nsnull;

  if (aUidl)
  {
    if (m_pop3ConData->newuidl)
      uidlEntry = (Pop3UidlEntry *) PL_HashTableLookup(m_pop3ConData->newuidl, aUidl);
    else if (m_pop3ConData->uidlinfo)
      uidlEntry = (Pop3UidlEntry *) PL_HashTableLookup(m_pop3ConData->uidlinfo->hash, aUidl);
  }

  *aBool = uidlEntry ? PR_TRUE : PR_FALSE;
  return NS_OK;
}


/* Function for finding an APOP Timestamp and simple check
   it for its validity. If returning NS_OK m_ApopTimestamp
   contains the validated substring of m_commandResponse. */
nsresult nsPop3Protocol::GetApopTimestamp()
{
  PRInt32 startMark = m_commandResponse.Length(), endMark = -1;

  while (PR_TRUE)
  {
    // search for previous <
    if ((startMark = m_commandResponse.RFindChar('<', startMark - 1)) < 0)
      return NS_ERROR_FAILURE;

    // search for next >
    if ((endMark = m_commandResponse.FindChar('>', startMark)) < 0)
      continue;

    // look for an @ between start and end as a raw test
    PRInt32 at = m_commandResponse.FindChar('@', startMark);
    if (at < 0 || at >= endMark)
      continue;

    // now test if sub only consists of chars in ASCII range
    nsCString sub(Substring(m_commandResponse, startMark, endMark - startMark + 1));
    if (NS_IsAscii(sub.get()))
    {
      // set m_ApopTimestamp to the validated substring
      m_ApopTimestamp.Assign(sub);
      break;
    }
  }

  return NS_OK;
}
