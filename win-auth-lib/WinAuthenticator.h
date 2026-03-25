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

#ifndef _WIN_AUTHENTICATOR_H_
#define _WIN_AUTHENTICATOR_H_

#include "util/StringStorage.h"
#include "server-config-lib/ClientPermissions.h"
#include "server-config-lib/GroupPermissionRule.h"
#include "WinAuthResult.h"
#include "log-writer/LogWriter.h"

#include <vector>
#include <Windows.h>

// Authenticates users against Windows local/domain accounts using LogonUser API,
// enumerates group memberships via token, and resolves VNC permissions from rules.
class WinAuthenticator
{
public:
  WinAuthenticator(LogWriter *log);
  ~WinAuthenticator();

  // Authenticate user credentials against Windows SAM/AD.
  // domain can be "." for local machine or a domain name.
  // Returns true if credentials are valid.
  bool authenticate(const TCHAR *username,
                    const TCHAR *password,
                    const TCHAR *domain);

  // After successful authenticate(), get the group memberships
  // of the authenticated user from the logon token.
  // Each group is returned as "DOMAIN\\GroupName" format.
  bool getGroupMemberships(std::vector<StringStorage> *groups);

  // Resolve permissions based on user's groups and configured rules.
  // Evaluates rules sorted by priority (highest first), first match wins.
  // Returns defaultPerms if no rule matches.
  static ClientPermissions resolvePermissions(
    const std::vector<StringStorage> &groups,
    const std::vector<GroupPermissionRule> &rules,
    UINT32 defaultPerms);

  // Full authentication flow: authenticate + get groups + resolve permissions.
  // Zeros password memory after use.
  WinAuthResult performAuth(
    const TCHAR *username,
    TCHAR *password,
    const TCHAR *domain,
    const std::vector<GroupPermissionRule> &rules,
    UINT32 defaultPerms);

  // Close any open token handle
  void closeToken();

private:
  // Check if the authenticated token belongs to the Guest account (RID 501).
  // Windows maps non-existent users to Guest when Guest is enabled.
  bool isGuestToken();

  HANDLE m_token;        // Logon token from LogonUser
  LogWriter *m_log;
};

#endif // _WIN_AUTHENTICATOR_H_
