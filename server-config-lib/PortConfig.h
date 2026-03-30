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

#ifndef _PORT_CONFIG_H_
#define _PORT_CONFIG_H_

#include "util/StringStorage.h"
#include "PortMapping.h"
#include "PortMappingRect.h"
#include "ClientPermissions.h"
#include "GroupPermissionRule.h"

#include <vector>

// Per-port configuration: Windows auth, permissions, display.
// Value type — safe to copy across threads via snapshot.
class PortConfig
{
public:
  PortConfig();
  PortConfig(const PortConfig &other);
  PortConfig &operator=(const PortConfig &other);
  virtual ~PortConfig();

  bool isEqualTo(const PortConfig *other) const;

  // Port & display
  int getPort() const;
  void setPort(int port);
  PortMappingRect getRect() const;
  void setRect(PortMappingRect rect);
  const StringStorage &getDevicePath() const;
  void setDevicePath(const TCHAR *path);

  // Windows auth group rules
  std::vector<GroupPermissionRule> getGroupRules() const;
  void setGroupRules(const std::vector<GroupPermissionRule> &rules);
  UINT32 getDefaultWinAuthPermissions() const;
  void setDefaultWinAuthPermissions(UINT32 perms);

  // Max concurrent connections per user (Windows auth only, 0=unlimited)
  int getMaxConnectionsPerUser() const;
  void setMaxConnectionsPerUser(int maxConn);

  // Convert to/from legacy PortMapping (display fields only)
  PortMapping toPortMapping() const;
  void fromPortMapping(const PortMapping &pm);

protected:
  int m_port;
  PortMappingRect m_rect;
  StringStorage m_devicePath;

  std::vector<GroupPermissionRule> m_groupRules;
  UINT32 m_defaultWinAuthPermissions;

  int m_maxConnectionsPerUser;  // 0 = unlimited
};

#endif // _PORT_CONFIG_H_
