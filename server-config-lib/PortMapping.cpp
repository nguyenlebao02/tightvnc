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
: m_port(0)
{
}

PortMapping::PortMapping(int nport, PortMappingRect nrect)
: m_port(nport), m_rect(nrect)
{
}

PortMapping::PortMapping(const PortMapping &other)
: m_port(other.m_port),
  m_rect(other.m_rect),
  m_devicePath(other.m_devicePath)
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
  return *this;
}

bool PortMapping::isEqualTo(const PortMapping *other) const
{
  if (other->m_port != m_port) return false;
  if (other->m_rect.isEqualTo(&m_rect) == false) return false;
  if (_tcsicmp(other->m_devicePath.getString(),
               m_devicePath.getString()) != 0) return false;
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

void PortMapping::toString(StringStorage *string) const
{
  StringStorage rectStr;
  m_rect.toString(&rectStr);

  // Format: port:rect or port:rect|devicePath (if device bound)
  if (m_devicePath.getLength() > 0) {
    string->format(_T("%d:%s|%s"), m_port, rectStr.getString(),
                   m_devicePath.getString());
  } else {
    string->format(_T("%d:%s"), m_port, rectStr.getString());
  }
}

bool PortMapping::parse(const TCHAR *str, PortMapping *mapping)
{
  int port;
  TCHAR c;
  PortMappingRect rect;

  // Find colon separator between port and rect
  const TCHAR *colonPos = _tcschr(str, _T(':'));
  if (colonPos == NULL) {
    return false;
  }
  const TCHAR *rectString = colonPos + 1;

  if ((_stscanf(str, _T("%d%c"), &port, &c) != 2) || (c != _T(':'))) {
    return false;
  }
  if (port < 0) {
    return false;
  }

  // Check for pipe-delimited devicePath: "port:rect|devicePath"
  const TCHAR *pipePos = _tcschr(rectString, _T('|'));
  StringStorage rectPart;
  StringStorage devicePath;

  if (pipePos != NULL) {
    // Extract rect portion (before pipe)
    size_t rectLen = pipePos - rectString;
    rectPart.setString(rectString);
    TCHAR *buf = new TCHAR[rectLen + 1];
    _tcsncpy(buf, rectString, rectLen);
    buf[rectLen] = _T('\0');
    rectPart.setString(buf);
    delete[] buf;
    // Extract devicePath (after pipe)
    devicePath.setString(pipePos + 1);
  } else {
    rectPart.setString(rectString);
  }

  if (!PortMappingRect::parse(rectPart.getString(), &rect)) {
    return false;
  }

  if (mapping != NULL) {
    mapping->setPort(port);
    mapping->setRect(rect);
    if (pipePos != NULL) {
      mapping->setDevicePath(devicePath.getString());
    } else {
      mapping->setDevicePath(_T(""));
    }
  }
  return true;
}
