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
  m_defaultWinAuthPermissions(ClientPermissions::PERM_VIEW_ONLY),
  m_maxConnectionsPerUser(0)
{
}

PortConfig::PortConfig(const PortConfig &other)
: m_port(other.m_port),
  m_rect(other.m_rect),
  m_devicePath(other.m_devicePath),
  m_groupRules(other.m_groupRules),
  m_defaultWinAuthPermissions(other.m_defaultWinAuthPermissions),
  m_maxConnectionsPerUser(other.m_maxConnectionsPerUser)
{
}

PortConfig &PortConfig::operator=(const PortConfig &other)
{
  if (this != &other) {
    m_port = other.m_port;
    m_rect = other.m_rect;
    m_devicePath = other.m_devicePath;
    m_groupRules = other.m_groupRules;
    m_defaultWinAuthPermissions = other.m_defaultWinAuthPermissions;
    m_maxConnectionsPerUser = other.m_maxConnectionsPerUser;
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
  if (other->m_maxConnectionsPerUser != m_maxConnectionsPerUser) return false;
  return true;
}

// --- Port & display ---

int PortConfig::getPort() const { return m_port; }
void PortConfig::setPort(int port) { m_port = port; }

PortMappingRect PortConfig::getRect() const { return m_rect; }
void PortConfig::setRect(PortMappingRect rect) { m_rect = rect; }

const StringStorage &PortConfig::getDevicePath() const { return m_devicePath; }
void PortConfig::setDevicePath(const TCHAR *path) { m_devicePath.setString(path); }

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

// --- Max connections per user ---

int PortConfig::getMaxConnectionsPerUser() const { return m_maxConnectionsPerUser; }
void PortConfig::setMaxConnectionsPerUser(int maxConn) { m_maxConnectionsPerUser = maxConn; }

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
