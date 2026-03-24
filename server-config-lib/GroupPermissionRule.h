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

#ifndef _GROUP_PERMISSION_RULE_H_
#define _GROUP_PERMISSION_RULE_H_

#include "util/StringStorage.h"
#include "ClientPermissions.h"

// Maps a Windows group name to a set of VNC client permissions.
// Format for serialization: "GroupName:permFlags:priority"
// Higher priority rules are evaluated first during permission resolution.
class GroupPermissionRule
{
public:
  GroupPermissionRule();
  GroupPermissionRule(const TCHAR *groupName, UINT32 permFlags, int priority);
  ~GroupPermissionRule();

  // Getters
  const StringStorage &getGroupName() const { return m_groupName; }
  UINT32 getPermissionFlags() const { return m_permissionFlags; }
  int getPriority() const { return m_priority; }

  // Setters
  void setGroupName(const TCHAR *name) { m_groupName.setString(name); }
  void setPermissionFlags(UINT32 flags) { m_permissionFlags = flags; }
  void setPriority(int priority) { m_priority = priority; }

  // Serialize rule to string format: "GroupName:permFlags:priority"
  void toString(StringStorage *output) const;

  // Parse rule from string format. Returns true on success.
  static bool parse(const TCHAR *str, GroupPermissionRule *rule);

  // Compare by priority (descending) for sorting
  static bool compareByPriority(const GroupPermissionRule &a,
                                const GroupPermissionRule &b);

private:
  StringStorage m_groupName;    // Windows group name (e.g. "BUILTIN\\Administrators")
  UINT32 m_permissionFlags;     // Bitmask from ClientPermissions
  int m_priority;               // Higher = evaluated first
};

#endif // _GROUP_PERMISSION_RULE_H_
