// Copyright (C) 2010,2011,2012 GlavSoft LLC.
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

#include "ExtraRfbServers.h"
#include "server-config-lib/Configurator.h"

ExtraRfbServers::Conf::Conf()
: acceptConnections(false),
  loopbackOnly(false),
  extraPorts(),
  portConfigs()
{
}

ExtraRfbServers::Conf::Conf(const Conf &other)
: acceptConnections(other.acceptConnections),
  loopbackOnly(other.loopbackOnly),
  extraPorts(other.extraPorts),
  portConfigs(other.portConfigs)
{
}

ExtraRfbServers::Conf &
ExtraRfbServers::Conf::operator=(const Conf &other)
{
  acceptConnections = other.acceptConnections;
  loopbackOnly = other.loopbackOnly;
  extraPorts = other.extraPorts;
  portConfigs = other.portConfigs;
  return *this;
}

bool ExtraRfbServers::Conf::equals(const Conf *other)
{
  if (acceptConnections != other->acceptConnections ||
      loopbackOnly != other->loopbackOnly) {
    return false;
  }
  // Compare via portConfigs if available, else fall back to extraPorts
  if (!portConfigs.empty() || !other->portConfigs.empty()) {
    if (portConfigs.size() != other->portConfigs.size()) return false;
    for (size_t i = 0; i < portConfigs.size(); i++) {
      if (!portConfigs[i].isEqualTo(&other->portConfigs[i])) return false;
    }
    return true;
  }
  return extraPorts.equals(&other->extraPorts);
}

ExtraRfbServers::ExtraRfbServers(LogWriter *log)
: m_servers(),
  m_effectiveConf(),
  m_log(log)
{
}

ExtraRfbServers::~ExtraRfbServers()
{
  try {
    shutDown();
  } catch (...) { }
}

bool ExtraRfbServers::reload(bool asService, RfbClientManager *mgr)
{
  m_log->detail(_T("Considering to reload extra RFB servers"));

  Conf newConf;
  getConfiguration(&newConf);
  bool noConfigChanges = newConf.equals(&m_effectiveConf);

  // Determine expected server count from portConfigs or extraPorts
  size_t expectedCount = newConf.portConfigs.empty()
    ? newConf.extraPorts.count()
    : newConf.portConfigs.size();
  bool enoughServers = (expectedCount == m_servers.size());

  m_log->detail(_T("Same config = %d, enough servers = %d"),
              (int)noConfigChanges, (int)enoughServers);

  if (noConfigChanges && enoughServers) {
    return true; // no work needed, no errors encountered
  }

  m_log->message(_T("Need to reconfigure RFB servers"));
  shutDown();
  return startUp(asService, mgr);
}

void ExtraRfbServers::shutDown()
{
  m_log->detail(_T("Requested to shut down extra RFB servers"));

  std::list<RfbServer *>::const_iterator i;
  for (i = m_servers.begin(); i != m_servers.end(); i++) {
    int port = (*i)->getBindPort();
    m_log->detail(_T("Stopping RFB server at port %d"), port);
    delete *i;
    m_log->message(_T("Stopped RFB server at port %d"), port);
  }
  m_servers.clear();
}

bool ExtraRfbServers::startUp(bool asService, RfbClientManager *mgr)
{
  m_log->detail(_T("Requested to start up RFB servers"));

  if (!m_servers.empty()) {
    m_log->interror(_T("RFB servers active, will have to stop them"));
    shutDown();
  }

  Conf newConf;
  getConfiguration(&newConf);
  m_effectiveConf = newConf;

  if (!newConf.acceptConnections) {
    return true;
  }

  const TCHAR *bindHost =
    newConf.loopbackOnly ? _T("localhost") : _T("0.0.0.0");

  size_t expectedCount = 0;

  // Use portConfigs if available, otherwise fall back to extraPorts
  if (!newConf.portConfigs.empty()) {
    expectedCount = newConf.portConfigs.size();
    for (size_t i = 0; i < newConf.portConfigs.size(); i++) {
      PortConfig pc = newConf.portConfigs[i];
      PortMappingRect rect = pc.getRect();
      int port = pc.getPort();

      m_log->detail(_T("Starting RFB server at port %d"), port);
      try {
        RfbServer *s = new RfbServer(bindHost, port, mgr, asService,
                                     m_log, &rect, &pc);
        m_servers.push_back(s);
        m_log->message(_T("Started RFB server at port %d"), port);
      } catch (Exception &ex) {
        m_log->error(_T("Failed to start RFB server: \"%s\""),
                     ex.getMessage());
      }
    }
  } else {
    // Legacy path: use extraPorts (no per-port config — use defaults)
    expectedCount = newConf.extraPorts.count();
    for (size_t i = 0; i < newConf.extraPorts.count(); i++) {
      PortMapping pm = *newConf.extraPorts.at(i);
      PortMappingRect rect = pm.getRect();
      int port = pm.getPort();

      m_log->detail(_T("Starting extra RFB server at port %d"), port);
      try {
        RfbServer *s = new RfbServer(bindHost, port, mgr, asService,
                                     m_log, &rect);
        m_servers.push_back(s);
        m_log->message(_T("Started extra RFB server at port %d"), port);
      } catch (Exception &ex) {
        m_log->error(_T("Failed to start extra RFB server: \"%s\""),
                     ex.getMessage());
      }
    }
  }

  return expectedCount == m_servers.size();
}

void ExtraRfbServers::getConfiguration(Conf *out)
{
  ServerConfig *config = Configurator::getInstance()->getServerConfig();
  AutoLock l(config);

  out->acceptConnections = config->isAcceptingRfbConnections();
  out->loopbackOnly = config->isOnlyLoopbackConnectionsAllowed();
  out->portConfigs = config->getAllPortConfigs();
  // Also populate legacy field for backward compat
  out->extraPorts = *config->getPortMappingContainer();
}
