// Copyright (C) 2008,2009,2010,2011,2012 GlavSoft LLC.
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

#include "PortMapping.h"
#include "util/StringParser.h"

#include <tchar.h>
#include <stdio.h>

PortMapping::PortMapping()
: m_port(0),
  m_defaultPermissions(ClientPermissions::PERM_FULL_CONTROL)
{
}

PortMapping::PortMapping(int nport, PortMappingRect nrect)
: m_port(nport), m_rect(nrect),
  m_defaultPermissions(ClientPermissions::PERM_FULL_CONTROL)
{
}

PortMapping::PortMapping(const PortMapping &other)
: m_port(other.m_port),
  m_rect(other.m_rect),
  m_devicePath(other.m_devicePath),
  m_groupRules(other.m_groupRules),
  m_defaultPermissions(other.m_defaultPermissions)
{
}

PortMapping::~PortMapping()
{
}

PortMapping &PortMapping::operator=(const PortMapping &other)
{
  m_port = other.m_port;
  m_rect = other.m_rect;
  m_devicePath = other.m_devicePath;
  m_groupRules = other.m_groupRules;
  m_defaultPermissions = other.m_defaultPermissions;
  return *this;
}

bool PortMapping::isEqualTo(const PortMapping *other) const
{
  if (other->m_port != m_port) return false;
  if (other->m_rect.isEqualTo(&m_rect) == false) return false;
  if (_tcsicmp(other->m_devicePath.getString(),
               m_devicePath.getString()) != 0) return false;
  if (other->m_defaultPermissions != m_defaultPermissions) return false;
  if (other->m_groupRules.size() != m_groupRules.size()) return false;
  // Compare group rules by serialized form
  for (size_t i = 0; i < m_groupRules.size(); i++) {
    StringStorage a, b;
    m_groupRules[i].toString(&a);
    other->m_groupRules[i].toString(&b);
    if (_tcscmp(a.getString(), b.getString()) != 0) return false;
  }
  return true;
}

void PortMapping::setPort(int nport)
{
  m_port = nport;
}

void PortMapping::setRect(PortMappingRect nrect)
{
  m_rect = nrect;
}

int PortMapping::getPort() const
{
  return m_port;
}

PortMappingRect PortMapping::getRect() const
{
  return m_rect;
}

void PortMapping::setDevicePath(const TCHAR *path)
{
  m_devicePath.setString(path);
}

const StringStorage &PortMapping::getDevicePath() const
{
  return m_devicePath;
}

void PortMapping::setGroupRules(const std::vector<GroupPermissionRule> &rules)
{
  m_groupRules = rules;
}

const std::vector<GroupPermissionRule> &PortMapping::getGroupRules() const
{
  return m_groupRules;
}

void PortMapping::setDefaultPermissions(UINT32 perms)
{
  m_defaultPermissions = perms;
}

UINT32 PortMapping::getDefaultPermissions() const
{
  return m_defaultPermissions;
}

void PortMapping::toString(StringStorage *string) const
{
  // New format: port|devicePath|defaultPerms|rule1;rule2;...
  StringStorage rulesStr;
  rulesStr.setString(_T(""));
  for (size_t i = 0; i < m_groupRules.size(); i++) {
    StringStorage ruleStr;
    m_groupRules[i].toString(&ruleStr);
    if (i > 0) {
      StringStorage combined;
      combined.format(_T("%s;%s"), rulesStr.getString(), ruleStr.getString());
      rulesStr.setString(combined.getString());
    } else {
      rulesStr.setString(ruleStr.getString());
    }
  }
  string->format(_T("%d|%s|%u|%s"),
                 m_port,
                 m_devicePath.getString(),
                 (unsigned int)m_defaultPermissions,
                 rulesStr.getString());
}

bool PortMapping::parse(const TCHAR *str, PortMapping *mapping)
{
  // New format: port|devicePath|defaultPerms|rule1;rule2;...
  // Also support legacy format: port:WxH+X+Y

  // Check for pipe delimiter (new format)
  const TCHAR *pipe1 = _tcschr(str, _T('|'));
  if (pipe1 != NULL) {
    // New pipe-delimited format
    int port = _tstoi(str);
    if (port <= 0) return false;

    const TCHAR *deviceStart = pipe1 + 1;
    const TCHAR *pipe2 = _tcschr(deviceStart, _T('|'));
    if (pipe2 == NULL) return false;

    // Extract device path
    size_t deviceLen = pipe2 - deviceStart;
    TCHAR *deviceBuf = new TCHAR[deviceLen + 1];
    _tcsncpy_s(deviceBuf, deviceLen + 1, deviceStart, deviceLen);
    deviceBuf[deviceLen] = 0;

    const TCHAR *permsStart = pipe2 + 1;
    const TCHAR *pipe3 = _tcschr(permsStart, _T('|'));
    if (pipe3 == NULL) {
      delete[] deviceBuf;
      return false;
    }

    UINT32 defaultPerms = (UINT32)_tstoi(permsStart);
    const TCHAR *rulesStart = pipe3 + 1;

    // Parse group rules (semicolon-separated)
    std::vector<GroupPermissionRule> rules;
    if (_tcslen(rulesStart) > 0) {
      StringStorage rulesStr(rulesStart);
      const TCHAR *ruleToken = rulesStr.getString();
      while (ruleToken != NULL && *ruleToken != 0) {
        const TCHAR *semi = _tcschr(ruleToken, _T(';'));
        size_t tokenLen = (semi != NULL) ? (size_t)(semi - ruleToken) : _tcslen(ruleToken);
        TCHAR *tokenBuf = new TCHAR[tokenLen + 1];
        _tcsncpy_s(tokenBuf, tokenLen + 1, ruleToken, tokenLen);
        tokenBuf[tokenLen] = 0;

        GroupPermissionRule rule;
        if (GroupPermissionRule::parse(tokenBuf, &rule)) {
          rules.push_back(rule);
        }
        delete[] tokenBuf;

        ruleToken = (semi != NULL) ? (semi + 1) : NULL;
      }
    }

    if (mapping != NULL) {
      mapping->setPort(port);
      mapping->setDevicePath(deviceBuf);
      mapping->setDefaultPermissions(defaultPerms);
      mapping->setGroupRules(rules);
    }
    delete[] deviceBuf;
    return true;
  }

  // Legacy format: port:WxH+X+Y
  int port;
  TCHAR c;
  PortMappingRect rect;
  const TCHAR *rectString = _tcschr(str, _T(':')) + 1;
  if (rectString == NULL) {
    return false;
  }
  if ((_stscanf(str, _T("%d%c"), &port, &c) != 2) || (c != _T(':'))) {
    return false;
  }
  if (port < 0) {
    return false;
  }
  if (!PortMappingRect::parse(rectString, &rect)) {
    return false;
  }
  if (mapping != NULL) {
    mapping->setPort(port);
    mapping->setRect(rect);
  }
  return true;
}
