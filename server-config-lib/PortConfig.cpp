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

#include "PortConfig.h"

#include <cstring>

PortConfig::PortConfig()
: m_port(0),
  m_authMode(AUTH_VNC_ONLY),
  m_hasPrimaryPassword(false),
  m_hasReadonlyPassword(false),
  m_useAuthentication(true),
  m_defaultWinAuthPermissions(ClientPermissions::PERM_VIEW_ONLY)
{
  memset(m_primaryPassword, 0, VNC_PASSWORD_SIZE);
  memset(m_readonlyPassword, 0, VNC_PASSWORD_SIZE);
}

PortConfig::PortConfig(const PortConfig &other)
: m_port(other.m_port),
  m_rect(other.m_rect),
  m_devicePath(other.m_devicePath),
  m_authMode(other.m_authMode),
  m_hasPrimaryPassword(other.m_hasPrimaryPassword),
  m_hasReadonlyPassword(other.m_hasReadonlyPassword),
  m_useAuthentication(other.m_useAuthentication),
  m_groupRules(other.m_groupRules),
  m_defaultWinAuthPermissions(other.m_defaultWinAuthPermissions),
  m_ipAccessRules(other.m_ipAccessRules)
{
  memcpy(m_primaryPassword, other.m_primaryPassword, VNC_PASSWORD_SIZE);
  memcpy(m_readonlyPassword, other.m_readonlyPassword, VNC_PASSWORD_SIZE);
}

PortConfig &PortConfig::operator=(const PortConfig &other)
{
  if (this != &other) {
    m_port = other.m_port;
    m_rect = other.m_rect;
    m_devicePath = other.m_devicePath;
    m_authMode = other.m_authMode;
    memcpy(m_primaryPassword, other.m_primaryPassword, VNC_PASSWORD_SIZE);
    memcpy(m_readonlyPassword, other.m_readonlyPassword, VNC_PASSWORD_SIZE);
    m_hasPrimaryPassword = other.m_hasPrimaryPassword;
    m_hasReadonlyPassword = other.m_hasReadonlyPassword;
    m_useAuthentication = other.m_useAuthentication;
    m_groupRules = other.m_groupRules;
    m_defaultWinAuthPermissions = other.m_defaultWinAuthPermissions;
    m_ipAccessRules = other.m_ipAccessRules;
  }
  return *this;
}

PortConfig::~PortConfig()
{
}

bool PortConfig::isEqualTo(const PortConfig *other) const
{
  if (other->m_port != m_port) return false;
  if (!other->m_rect.isEqualTo(&m_rect)) return false;
  if (_tcsicmp(other->m_devicePath.getString(),
               m_devicePath.getString()) != 0) return false;
  if (other->m_authMode != m_authMode) return false;
  if (other->m_hasPrimaryPassword != m_hasPrimaryPassword) return false;
  if (other->m_hasReadonlyPassword != m_hasReadonlyPassword) return false;
  if (other->m_useAuthentication != m_useAuthentication) return false;
  if (memcmp(other->m_primaryPassword, m_primaryPassword,
             VNC_PASSWORD_SIZE) != 0) return false;
  if (memcmp(other->m_readonlyPassword, m_readonlyPassword,
             VNC_PASSWORD_SIZE) != 0) return false;
  if (other->m_defaultWinAuthPermissions !=
      m_defaultWinAuthPermissions) return false;
  // Compare group rules
  if (other->m_groupRules.size() != m_groupRules.size()) return false;
  for (size_t i = 0; i < m_groupRules.size(); i++) {
    if (m_groupRules[i].getPermissionFlags() !=
        other->m_groupRules[i].getPermissionFlags()) return false;
    if (m_groupRules[i].getPriority() !=
        other->m_groupRules[i].getPriority()) return false;
    if (_tcsicmp(m_groupRules[i].getGroupName().getString(),
                 other->m_groupRules[i].getGroupName().getString()) != 0)
      return false;
  }
  // Note: IpAccessControl comparison skipped (pointer-based vector,
  // will be added when deep comparison is needed in later phases)
  return true;
}

// --- Port & display ---

int PortConfig::getPort() const { return m_port; }
void PortConfig::setPort(int port) { m_port = port; }

PortMappingRect PortConfig::getRect() const { return m_rect; }
void PortConfig::setRect(PortMappingRect rect) { m_rect = rect; }

const StringStorage &PortConfig::getDevicePath() const { return m_devicePath; }
void PortConfig::setDevicePath(const TCHAR *path) { m_devicePath.setString(path); }

// --- Auth mode ---

int PortConfig::getAuthMode() const { return m_authMode; }
void PortConfig::setAuthMode(int mode) { m_authMode = mode; }

// --- VNC passwords ---

void PortConfig::getPrimaryPassword(unsigned char *out) const
{
  memcpy(out, m_primaryPassword, VNC_PASSWORD_SIZE);
}

void PortConfig::setPrimaryPassword(const unsigned char *value)
{
  memcpy(m_primaryPassword, value, VNC_PASSWORD_SIZE);
  m_hasPrimaryPassword = true;
}

bool PortConfig::hasPrimaryPassword() const { return m_hasPrimaryPassword; }

void PortConfig::deletePrimaryPassword()
{
  memset(m_primaryPassword, 0, VNC_PASSWORD_SIZE);
  m_hasPrimaryPassword = false;
}

void PortConfig::getReadOnlyPassword(unsigned char *out) const
{
  memcpy(out, m_readonlyPassword, VNC_PASSWORD_SIZE);
}

void PortConfig::setReadOnlyPassword(const unsigned char *value)
{
  memcpy(m_readonlyPassword, value, VNC_PASSWORD_SIZE);
  m_hasReadonlyPassword = true;
}

bool PortConfig::hasReadOnlyPassword() const { return m_hasReadonlyPassword; }

void PortConfig::deleteReadOnlyPassword()
{
  memset(m_readonlyPassword, 0, VNC_PASSWORD_SIZE);
  m_hasReadonlyPassword = false;
}

bool PortConfig::isUsingAuthentication() const { return m_useAuthentication; }
void PortConfig::setUseAuthentication(bool use) { m_useAuthentication = use; }

// --- Windows auth group rules ---

std::vector<GroupPermissionRule> PortConfig::getGroupRules() const
{
  return m_groupRules;
}

void PortConfig::setGroupRules(const std::vector<GroupPermissionRule> &rules)
{
  m_groupRules = rules;
}

UINT32 PortConfig::getDefaultWinAuthPermissions() const
{
  return m_defaultWinAuthPermissions;
}

void PortConfig::setDefaultWinAuthPermissions(UINT32 perms)
{
  m_defaultWinAuthPermissions = perms;
}

// --- IP access control ---

IpAccessControl *PortConfig::getIpAccessControl() { return &m_ipAccessRules; }
const IpAccessControl *PortConfig::getIpAccessControl() const { return &m_ipAccessRules; }

// --- Legacy PortMapping conversion ---

PortMapping PortConfig::toPortMapping() const
{
  PortMapping pm;
  pm.setPort(m_port);
  pm.setRect(m_rect);
  pm.setDevicePath(m_devicePath.getString());
  return pm;
}

void PortConfig::fromPortMapping(const PortMapping &pm)
{
  m_port = pm.getPort();
  m_rect = pm.getRect();
  m_devicePath.setString(pm.getDevicePath().getString());
}
