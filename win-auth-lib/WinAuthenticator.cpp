// Copyright (C) 2024 TightVNC Contributors.
// All rights reserved.
//
//-------------------------------------------------------------------------
// This file is part of the TightVNC software.  Please visit our Web site:
//
//                       http://www.tightvnc.com/
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//-------------------------------------------------------------------------
//

#include "WinAuthenticator.h"

#include <algorithm>
#include <Sddl.h>

#pragma comment(lib, "Advapi32.lib")

WinAuthenticator::WinAuthenticator(LogWriter *log)
: m_token(NULL),
  m_log(log)
{
}

WinAuthenticator::~WinAuthenticator()
{
  closeToken();
}

bool WinAuthenticator::authenticate(const TCHAR *username,
                                    const TCHAR *password,
                                    const TCHAR *domain)
{
  closeToken();

  // Use "." for local machine if domain is empty
  const TCHAR *logonDomain = domain;
  if (logonDomain == NULL || logonDomain[0] == 0) {
    logonDomain = _T(".");
  }

  BOOL result = LogonUser(
    username,
    logonDomain,
    password,
    LOGON32_LOGON_NETWORK,    // Network logon: no interactive session
    LOGON32_PROVIDER_DEFAULT,
    &m_token);

  if (!result) {
    DWORD err = GetLastError();
    if (m_log != NULL) {
      m_log->warning(_T("WinAuth: LogonUser failed for user '%s\\%s', error=%d"),
                     logonDomain, username, (int)err);
    }
    m_token = NULL;
    return false;
  }

  // Reject Guest account logons — Windows maps non-existent users to Guest
  // when Guest is enabled, which is a security risk for VNC.
  if (isGuestToken()) {
    if (m_log != NULL) {
      m_log->warning(_T("WinAuth: Rejected Guest logon for '%s\\%s'"),
                     logonDomain, username);
    }
    closeToken();
    return false;
  }

  if (m_log != NULL) {
    m_log->message(_T("WinAuth: LogonUser succeeded for user '%s\\%s'"),
                   logonDomain, username);
  }
  return true;
}

bool WinAuthenticator::getGroupMemberships(std::vector<StringStorage> *groups)
{
  if (m_token == NULL) {
    return false;
  }

  groups->clear();

  // Get token group information size
  DWORD tokenInfoSize = 0;
  GetTokenInformation(m_token, TokenGroups, NULL, 0, &tokenInfoSize);

  if (tokenInfoSize == 0) {
    if (m_log != NULL) {
      m_log->warning(_T("WinAuth: GetTokenInformation returned size 0"));
    }
    return false;
  }

  // Allocate buffer and get token groups
  std::vector<BYTE> buffer(tokenInfoSize);
  TOKEN_GROUPS *tokenGroups = (TOKEN_GROUPS *)&buffer[0];

  if (!GetTokenInformation(m_token, TokenGroups, tokenGroups,
                           tokenInfoSize, &tokenInfoSize)) {
    if (m_log != NULL) {
      m_log->warning(_T("WinAuth: GetTokenInformation failed, error=%d"),
                     (int)GetLastError());
    }
    return false;
  }

  // Enumerate each group SID and convert to "DOMAIN\\Name" format
  for (DWORD i = 0; i < tokenGroups->GroupCount; i++) {
    SID_AND_ATTRIBUTES &groupSid = tokenGroups->Groups[i];

    // Skip disabled groups and logon-id SIDs
    if ((groupSid.Attributes & SE_GROUP_ENABLED) == 0) {
      continue;
    }

    TCHAR name[256] = { 0 };
    TCHAR domain[256] = { 0 };
    DWORD nameSize = 256;
    DWORD domainSize = 256;
    SID_NAME_USE sidType;

    if (LookupAccountSid(NULL, groupSid.Sid,
                         name, &nameSize,
                         domain, &domainSize,
                         &sidType)) {
      StringStorage fullName;
      if (domain[0] != 0) {
        fullName.format(_T("%s\\%s"), domain, name);
      } else {
        fullName.setString(name);
      }
      groups->push_back(fullName);

      if (m_log != NULL) {
        m_log->detail(_T("WinAuth: User group: %s"), fullName.getString());
      }
    }
  }

  if (m_log != NULL) {
    m_log->message(_T("WinAuth: Enumerated %d groups"),
                   (int)groups->size());
  }
  return true;
}

ClientPermissions WinAuthenticator::resolvePermissions(
  const std::vector<StringStorage> &groups,
  const std::vector<GroupPermissionRule> &rules,
  UINT32 defaultPerms)
{
  // Sort rules by priority descending (highest first)
  std::vector<GroupPermissionRule> sortedRules = rules;
  std::sort(sortedRules.begin(), sortedRules.end(),
            GroupPermissionRule::compareByPriority);

  // Find the first matching rule (highest priority wins)
  for (size_t r = 0; r < sortedRules.size(); r++) {
    const GroupPermissionRule &rule = sortedRules[r];
    for (size_t g = 0; g < groups.size(); g++) {
      // Case-insensitive comparison — Windows group names are case-insensitive
      const TCHAR *groupName = groups[g].getString();
      const TCHAR *ruleName = rule.getGroupName().getString();
      if (groupName != NULL && ruleName != NULL &&
          _tcsicmp(groupName, ruleName) == 0) {
        return ClientPermissions(rule.getPermissionFlags());
      }
    }
  }

  // No matching rule, return default
  return ClientPermissions(defaultPerms);
}

WinAuthResult WinAuthenticator::performAuth(
  const TCHAR *username,
  TCHAR *password,
  const TCHAR *domain,
  const std::vector<GroupPermissionRule> &rules,
  UINT32 defaultPerms)
{
  WinAuthResult result;

  // Step 1: Authenticate credentials
  if (!authenticate(username, password, domain)) {
    result.success = false;
    result.errorMessage.setString(_T("Invalid username or password"));
    // Clear password from memory immediately
    size_t passLen = _tcslen(password);
    SecureZeroMemory(password, passLen * sizeof(TCHAR));
    return result;
  }

  // Clear password from memory immediately after LogonUser
  size_t passLen = _tcslen(password);
  SecureZeroMemory(password, passLen * sizeof(TCHAR));

  // Step 2: Get group memberships
  std::vector<StringStorage> groups;
  if (!getGroupMemberships(&groups)) {
    result.success = false;
    result.errorMessage.setString(_T("Failed to enumerate user groups"));
    closeToken();
    return result;
  }

  StringStorage canonicalUsername;
  StringStorage canonicalDomain;
  bool hasCanonicalName = getCanonicalUserName(&canonicalUsername,
                                               &canonicalDomain);

  // Close token as soon as we have the groups and canonical name
  closeToken();

  // Step 3: Resolve permissions from rules
  result.permissions = resolvePermissions(groups, rules, defaultPerms);
  if (hasCanonicalName) {
    result.username = canonicalUsername;
    result.domain = canonicalDomain;
  } else {
    result.username.setString(username);
    result.domain.setString(domain != NULL ? domain : _T(""));
  }

  // Step 4: Check if denied
  if (result.permissions.isDenied()) {
    result.success = false;
    result.errorMessage.setString(_T("Access denied by group policy"));
    if (m_log != NULL) {
      m_log->warning(_T("WinAuth: Access denied for user '%s' by group policy"),
                     username);
    }
    return result;
  }

  result.success = true;
  if (m_log != NULL) {
    m_log->message(_T("WinAuth: User '%s' authenticated with permissions=0x%08X"),
                   username, result.permissions.getFlags());
  }
  return result;
}

void WinAuthenticator::closeToken()
{
  if (m_token != NULL) {
    CloseHandle(m_token);
    m_token = NULL;
  }
}

bool WinAuthenticator::getCanonicalUserName(StringStorage *username,
                                            StringStorage *domain)
{
  if (m_token == NULL) {
    return false;
  }

  DWORD infoSize = 0;
  GetTokenInformation(m_token, TokenUser, NULL, 0, &infoSize);
  if (infoSize == 0) {
    return false;
  }

  std::vector<BYTE> buffer(infoSize);
  TOKEN_USER *tokenUser = (TOKEN_USER *)&buffer[0];
  if (!GetTokenInformation(m_token, TokenUser, tokenUser, infoSize, &infoSize)) {
    return false;
  }

  TCHAR name[256] = { 0 };
  TCHAR domainName[256] = { 0 };
  DWORD nameSize = 256;
  DWORD domainSize = 256;
  SID_NAME_USE sidType;

  if (!LookupAccountSid(NULL, tokenUser->User.Sid,
                        name, &nameSize,
                        domainName, &domainSize,
                        &sidType)) {
    return false;
  }

  username->setString(name);
  domain->setString(domainName);
  return true;
}

bool WinAuthenticator::isGuestToken()
{
  if (m_token == NULL) {
    return false;
  }

  // Get the token user SID
  DWORD infoSize = 0;
  GetTokenInformation(m_token, TokenUser, NULL, 0, &infoSize);
  if (infoSize == 0) {
    return false;
  }

  std::vector<BYTE> buffer(infoSize);
  TOKEN_USER *tokenUser = (TOKEN_USER *)&buffer[0];
  if (!GetTokenInformation(m_token, TokenUser, tokenUser, infoSize, &infoSize)) {
    return false;
  }

  // Check if the SID matches the well-known Guest RID (501).
  // Guest SID format: S-1-5-<domain>-501
  PSID sid = tokenUser->User.Sid;
  PUCHAR subAuthCount = GetSidSubAuthorityCount(sid);
  if (subAuthCount == NULL || *subAuthCount < 1) {
    return false;
  }
  DWORD lastSubAuth = *GetSidSubAuthority(sid, *subAuthCount - 1);
  return (lastSubAuth == DOMAIN_USER_RID_GUEST); // 501
}
