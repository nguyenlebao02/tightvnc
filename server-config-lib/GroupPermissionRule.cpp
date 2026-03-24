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

#include "GroupPermissionRule.h"
#include <stdlib.h>
#include <stdio.h>

GroupPermissionRule::GroupPermissionRule()
: m_permissionFlags(ClientPermissions::PERM_NONE),
  m_priority(0)
{
}

GroupPermissionRule::GroupPermissionRule(const TCHAR *groupName,
                                       UINT32 permFlags,
                                       int priority)
: m_permissionFlags(permFlags),
  m_priority(priority)
{
  m_groupName.setString(groupName);
}

GroupPermissionRule::~GroupPermissionRule()
{
}

void GroupPermissionRule::toString(StringStorage *output) const
{
  // Format: "GroupName:permFlags:priority"
  output->format(_T("%s:%u:%d"),
                 m_groupName.getString(),
                 (unsigned int)m_permissionFlags,
                 m_priority);
}

bool GroupPermissionRule::parse(const TCHAR *str, GroupPermissionRule *rule)
{
  if (str == NULL || str[0] == 0) {
    return false;
  }

  // Find the last two colons to split "GroupName:flags:priority"
  // Group name may contain colons (unlikely but safe parsing)
  const TCHAR *lastColon = _tcsrchr(str, _T(':'));
  if (lastColon == NULL || lastColon == str) {
    return false;
  }

  // Find second-to-last colon
  StringStorage temp;
  temp.setString(str);
  size_t len = temp.getLength();

  // Parse from the end: find priority after last colon
  const TCHAR *priorityStr = lastColon + 1;

  // Find flags colon (second-to-last)
  size_t lastColonPos = lastColon - str;
  const TCHAR *beforeLastColon = str;
  const TCHAR *flagsColon = NULL;

  for (size_t i = lastColonPos; i > 0; i--) {
    if (str[i - 1] == _T(':')) {
      flagsColon = &str[i - 1];
      break;
    }
  }

  if (flagsColon == NULL) {
    return false;
  }

  // Extract group name (everything before flagsColon)
  size_t nameLen = flagsColon - str;
  if (nameLen == 0) {
    return false;
  }

  // Extract flags string (between flagsColon and lastColon)
  const TCHAR *flagsStr = flagsColon + 1;
  size_t flagsLen = lastColon - flagsStr;
  if (flagsLen == 0) {
    return false;
  }

  if (rule != NULL) {
    // Parse group name
    StringStorage groupName;
    groupName.setString(str);
    // Truncate to nameLen characters
    TCHAR *buf = new TCHAR[nameLen + 1];
    _tcsncpy_s(buf, nameLen + 1, str, nameLen);
    buf[nameLen] = 0;
    rule->m_groupName.setString(buf);
    delete[] buf;

    // Parse flags
    TCHAR *flagsBuf = new TCHAR[flagsLen + 1];
    _tcsncpy_s(flagsBuf, flagsLen + 1, flagsStr, flagsLen);
    flagsBuf[flagsLen] = 0;
    rule->m_permissionFlags = (UINT32)_tcstoul(flagsBuf, NULL, 10);
    delete[] flagsBuf;

    // Parse priority
    rule->m_priority = _ttoi(priorityStr);
  }

  return true;
}

bool GroupPermissionRule::compareByPriority(const GroupPermissionRule &a,
                                           const GroupPermissionRule &b)
{
  return a.m_priority > b.m_priority;
}
