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
#include "IpAccessControl.h"
#include "ClientPermissions.h"
#include "GroupPermissionRule.h"

#include <vector>

// Per-port configuration: auth, passwords, permissions, display.
// Value type — safe to copy across threads via snapshot.
class PortConfig
{
public:
  static const int VNC_PASSWORD_SIZE = 8;

  // Auth mode values (matches ServerConfig::AuthMode to avoid circular include)
  static const int AUTH_VNC_ONLY     = 0;
  static const int AUTH_WINDOWS_ONLY = 1;
  static const int AUTH_BOTH         = 2;

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

  // Auth mode (0=VNC_ONLY, 1=WINDOWS_ONLY, 2=BOTH)
  int getAuthMode() const;
  void setAuthMode(int mode);

  // VNC passwords (raw 8-byte encrypted arrays)
  void getPrimaryPassword(unsigned char *out) const;
  void setPrimaryPassword(const unsigned char *value);
  bool hasPrimaryPassword() const;
  void deletePrimaryPassword();

  void getReadOnlyPassword(unsigned char *out) const;
  void setReadOnlyPassword(const unsigned char *value);
  bool hasReadOnlyPassword() const;
  void deleteReadOnlyPassword();

  bool isUsingAuthentication() const;
  void setUseAuthentication(bool use);

  // Windows auth group rules
  std::vector<GroupPermissionRule> getGroupRules() const;
  void setGroupRules(const std::vector<GroupPermissionRule> &rules);
  UINT32 getDefaultWinAuthPermissions() const;
  void setDefaultWinAuthPermissions(UINT32 perms);

  // IP access control (per-port)
  IpAccessControl *getIpAccessControl();
  const IpAccessControl *getIpAccessControl() const;

  // Convert to/from legacy PortMapping (display fields only)
  PortMapping toPortMapping() const;
  void fromPortMapping(const PortMapping &pm);

protected:
  int m_port;
  PortMappingRect m_rect;
  StringStorage m_devicePath;

  int m_authMode;
  unsigned char m_primaryPassword[VNC_PASSWORD_SIZE];
  unsigned char m_readonlyPassword[VNC_PASSWORD_SIZE];
  bool m_hasPrimaryPassword;
  bool m_hasReadonlyPassword;
  bool m_useAuthentication;

  std::vector<GroupPermissionRule> m_groupRules;
  UINT32 m_defaultWinAuthPermissions;

  IpAccessControl m_ipAccessRules;
};

#endif // _PORT_CONFIG_H_
