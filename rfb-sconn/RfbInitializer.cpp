// Copyright (C) 2009,2010,2011,2012 GlavSoft LLC.
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

#include "RfbInitializer.h"
#include "thread/AutoLock.h"
#include "rfb/VendorDefs.h"
#include "rfb/AuthDefs.h"
#include "CapContainer.h"
#include "server-config-lib/Configurator.h"
#include "AuthException.h"
#include "win-system/Environment.h"
#include "util/AnsiStringStorage.h"
#include "tvnserver-app/NamingDefs.h"
#include "win-auth-lib/WinAuthenticator.h"

#include <stdlib.h>
#include <vector>

RfbInitializer::RfbInitializer(Channel *stream,
                               ClientAuthListener *extAuthListener,
                               RfbClient *client, bool authAllowed,
                               const PortConfig *portConfig)
: m_shared(false),
  m_tightEnabled(false),
  m_minorVerNum(0),
  m_extAuthListener(extAuthListener),
  m_client(client),
  m_authAllowed(authAllowed),
  m_viewOnlyAuth(false),
  m_winAuthUsed(false),
  m_clientPermissions(ClientPermissions::PERM_FULL_CONTROL),
  m_portConfig(portConfig ? *portConfig : PortConfig())
{
  m_output = new DataOutputStream(stream);
  m_input = new DataInputStream(stream);
}

RfbInitializer::~RfbInitializer()
{
  delete m_output;
  delete m_input;
}

void RfbInitializer::authPhase()
{
  initVersion();
  initAuthenticate();
  readClientInit();
}

void RfbInitializer::afterAuthPhase(const CapContainer *srvToClCaps,
                                    const CapContainer *clToSrvCaps,
                                    const CapContainer *encCaps,
                                    const Dimension *dim,
                                    const PixelFormat *pf)
{
  sendServerInit(dim, pf);
  sendDesktopName();
  if (m_tightEnabled) {
    sendInteractionCaps(srvToClCaps, clToSrvCaps, encCaps);
  }
}

void RfbInitializer::initVersion()
{
  char initVersionMsg[] = "RFB 003.008\n";
  char clientVersionMsg[13];
  size_t msgLen = 12;
  m_output->writeFully(initVersionMsg, msgLen);
  m_input->readFully(clientVersionMsg, msgLen);
  clientVersionMsg[12] = 0;
  m_minorVerNum = getProtocolMinorVersion(clientVersionMsg);

  try {
    checkForLoopback();
    // Checking for a ban before auth and then after.
    checkForBan();
  } catch (Exception &e) {
    if (m_minorVerNum == 3) {
      m_output->writeUInt32(0);
    } else {
      m_output->writeUInt8(0);
    }
    AnsiStringStorage reason(&StringStorage(e.getMessage()));
    unsigned int reasonLen = (unsigned int)reason.getLength();
    _ASSERT(reasonLen == reason.getLength());

    m_output->writeUInt32(reasonLen);
    m_output->writeFully(reason.getString(), reasonLen);

    throw;
  }
}

void RfbInitializer::checkForLoopback()
{
  SocketAddressIPv4 sockAddr;
  m_client->getSocketAddr(&sockAddr);
  struct sockaddr_in addrIn = sockAddr.getSockAddr();

  // Check entire 127.0.0.0/8 loopback range, not just 127.0.0.1
  bool isLoopback = (addrIn.sin_addr.S_un.S_un_b.s_b1 == 127);

  ServerConfig *srvConf = Configurator::getInstance()->getServerConfig();
  if (isLoopback && !srvConf->isLoopbackConnectionsAllowed()) {
    throw Exception(_T("Sorry, loopback connections are not enabled"));
  }
  if (srvConf->isOnlyLoopbackConnectionsAllowed() && !isLoopback) {
    throw Exception(_T("Your connection has been rejected"));
  }
}

void RfbInitializer::doTightAuth()
{
  // Negotiate tunneling.
  m_output->writeUInt32(0);

  // Only Windows (EXTERNAL) authentication is supported.
  if (m_authAllowed) {
    CapContainer authInfo;
    authInfo.addCap(AuthDefs::EXTERNAL, VendorDefs::TIGHTVNC,
                    AuthDefs::SIG_EXTERNAL);

    m_output->writeUInt32(authInfo.getCapCount());
    authInfo.sendCaps(m_output);
    // Read the security type selected by the client.
    UINT32 clientAuthValue = m_input->readUInt32();
    if (!authInfo.includes(clientAuthValue)) {
      throw Exception(_T("Client selected unsupported auth type"));
    }
    doAuth(clientAuthValue);
  } else {
    // Auth not allowed for this connection (e.g., loopback exception)
    m_output->writeUInt32(0);
    doAuth(AuthDefs::NONE);
  }
}

void RfbInitializer::doAuth(UINT32 authType)
{
  if (authType == AuthDefs::EXTERNAL) {
    doWinAuth();
  } else if (authType == AuthDefs::NONE) {
    doAuthNone();
  } else {
    throw AuthException(_T("Unsupported authentication type"));
  }
  // Perform additional work via a listener.
  m_extAuthListener->onCheckAccessControl(m_client);
  // Send authentication result.
  if (m_minorVerNum >= 8 || authType != AuthDefs::NONE) {
    m_output->writeUInt32(0); // FIXME: Use a named constant instead of 0.
  }
}

void RfbInitializer::doAuthNone()
{
}

void RfbInitializer::doWinAuth()
{
  // Wire protocol for Windows auth (EXTERNAL):
  // Server → Client: (nothing extra, client knows to send credentials)
  // Client → Server: UINT32 usernameLen, username bytes,
  //                   UINT32 passwordLen, password bytes

  // Read username
  UINT32 usernameLen = m_input->readUInt32();
  if (usernameLen == 0 || usernameLen > 256) {
    throw AuthException(_T("Invalid username length in Windows auth"));
  }
  std::vector<char> usernameBuf(usernameLen + 1, 0);
  m_input->readFully(&usernameBuf[0], usernameLen);
  usernameBuf[usernameLen] = 0;

  // Read password
  UINT32 passwordLen = m_input->readUInt32();
  if (passwordLen > 256) {
    throw AuthException(_T("Invalid password length in Windows auth"));
  }
  std::vector<char> passAnsi(passwordLen + 1, 0);
  m_input->readFully(&passAnsi[0], passwordLen);
  passAnsi[passwordLen] = 0;

  // Check for ban before auth
  checkForBan();

  // Convert ANSI to TCHAR
  StringStorage username;
  AnsiStringStorage ansiUser(&usernameBuf[0]);
  ansiUser.toStringStorage(&username);

  // Split "DOMAIN\\username" if present
  StringStorage domain;
  StringStorage user;
  const TCHAR *backslash = _tcschr(username.getString(), _T('\\'));
  if (backslash != NULL) {
    size_t domainLen = backslash - username.getString();
    TCHAR *domBuf = new TCHAR[domainLen + 1];
    _tcsncpy_s(domBuf, domainLen + 1, username.getString(), domainLen);
    domBuf[domainLen] = 0;
    domain.setString(domBuf);
    delete[] domBuf;
    user.setString(backslash + 1);
  } else {
    domain.setString(_T("."));
    user.setString(username.getString());
  }

  // Convert password ANSI to TCHAR
  AnsiStringStorage ansiPass(&passAnsi[0]);
  StringStorage passStr;
  ansiPass.toStringStorage(&passStr);

  // Copy to mutable buffer for SecureZeroMemory
  size_t passLen = passStr.getLength() + 1;
  std::vector<TCHAR> mutablePass(passLen);
  _tcscpy_s(&mutablePass[0], passLen, passStr.getString());

  // Perform Windows authentication using per-port config
  std::vector<GroupPermissionRule> rules = m_portConfig.getGroupRules();
  UINT32 defaultPerms = m_portConfig.getDefaultWinAuthPermissions();

  WinAuthenticator authenticator(NULL); // No log in initializer context
  WinAuthResult result = authenticator.performAuth(
    user.getString(),
    &mutablePass[0],
    domain.getString(),
    rules,
    defaultPerms);

  // Securely clear password buffers we own.
  // mutablePass is already zeroed by WinAuthenticator::performAuth().
  SecureZeroMemory(&passAnsi[0], passAnsi.size());
  SecureZeroMemory(&mutablePass[0], mutablePass.size() * sizeof(TCHAR));

  if (!result.success) {
    // Notify about failed auth attempt
    m_extAuthListener->onAuthFailed(m_client);

    // Throw generic error to client — detailed reason is server-side only
    throw AuthException(_T("Authentication failed"));
  }

  // Auth succeeded — store permissions and mark as Windows auth
  m_winAuthUsed = true;
  m_clientPermissions = result.permissions;
  if (result.domain.getLength() > 0) {
    m_authenticatedUsername.format(_T("%s\\%s"),
                                   result.domain.getString(),
                                   result.username.getString());
  } else {
    m_authenticatedUsername = result.username;
  }

  // Set view-only flag for backward compatibility
  m_viewOnlyAuth = m_clientPermissions.isViewOnly();
}

void RfbInitializer::initAuthenticate()
{
  try {
    // Determine effective security type — always VNC type (which routes to
    // doTightAuth() for TIGHT-capable clients where Windows auth is offered).
    UINT32 primSecType = SecurityDefs::VNC;
    if (!m_authAllowed) {
      primSecType = SecurityDefs::NONE;
    }
    // Here the protocol varies between versions 3.3 and 3.7+.
    if (m_minorVerNum >= 7) {
      // Send a list with two security types -- VNC-compatible security type
      // and a special code allowing to enable TightVNC protocol extensions.
      m_output->writeUInt8(2);
      m_output->writeUInt8(primSecType);
      m_output->writeUInt8(SecurityDefs::TIGHT);
      // Read what the client has actually selected.
      UINT8 clientSecType = m_input->readUInt8();
      if (clientSecType == SecurityDefs::TIGHT) {
        m_tightEnabled = true;
        doTightAuth();
      } else {
        if (clientSecType != primSecType) {
          throw Exception(_T("Security types do not match"));
        }
        doAuth(AuthDefs::convertFromSecurityType(clientSecType));
      }
    } else {
      // Just tell the client we will use the configured security type.
      m_output->writeUInt32(primSecType);
      doAuth(AuthDefs::convertFromSecurityType(primSecType));
    }
  } catch (AuthException &e) {
    if (m_minorVerNum >= 8) {
      // Protocol 3.8+: send 4-byte failure + error message
      AnsiStringStorage reason(&StringStorage(e.getMessage()));
      unsigned int reasonLen = (unsigned int)reason.getLength();
      _ASSERT(reasonLen == reason.getLength());

      m_output->writeUInt32(1);
      m_output->writeUInt32(reasonLen);
      m_output->writeFully(reason.getString(), reasonLen);
    } else {
      // Protocol 3.3/3.7: send 4-byte failure result so client
      // knows auth failed instead of hanging until timeout.
      m_output->writeUInt32(1);
    }
    throw;
  }
}

void RfbInitializer::readClientInit()
{
  m_shared = m_input->readUInt8() != 0;
}

void RfbInitializer::sendServerInit(const Dimension *dim,
                                    const PixelFormat *pf)
{
  m_output->writeUInt16((UINT16)dim->width);
  m_output->writeUInt16((UINT16)dim->height);
  // Pixel format
  m_output->writeUInt8((UINT8)pf->bitsPerPixel);
  m_output->writeUInt8((UINT8)pf->colorDepth);
  m_output->writeUInt8((UINT8)pf->bigEndian);
  m_output->writeUInt8(1);
  m_output->writeUInt16((UINT16)pf->redMax);
  m_output->writeUInt16((UINT16)pf->greenMax);
  m_output->writeUInt16((UINT16)pf->blueMax);
  m_output->writeUInt8((UINT8)pf->redShift);
  m_output->writeUInt8((UINT8)pf->greenShift);
  m_output->writeUInt8((UINT8)pf->blueShift);
  // Padding
  m_output->writeUInt8(0);
  m_output->writeUInt16(0);
}

void RfbInitializer::sendDesktopName()
{
  StringStorage deskName;
  if (!Environment::getComputerName(&deskName)) {
    deskName.setString(DefaultNames::DEFAULT_COMPUTER_NAME);
  }

  AnsiStringStorage ansiName(&deskName);
  unsigned int dnLen = (unsigned int)ansiName.getLength();
  _ASSERT(dnLen == ansiName.getLength());

  m_output->writeUInt32(dnLen);
  m_output->writeFully(ansiName.getString(), dnLen);
}

void RfbInitializer::sendInteractionCaps(const CapContainer *srvToClCaps,
                                         const CapContainer *clToSrvCaps,
                                         const CapContainer *encCaps)
{
  m_output->writeUInt16(srvToClCaps->getCapCount());
  m_output->writeUInt16(clToSrvCaps->getCapCount());
  m_output->writeUInt16(encCaps->getCapCount());
  m_output->writeUInt16(0); // Pad

  srvToClCaps->sendCaps(m_output);
  clToSrvCaps->sendCaps(m_output);
  encCaps->sendCaps(m_output);
}

unsigned int RfbInitializer::getProtocolMinorVersion(const char str[12])
{
  if ( str[0] != 'R' || str[1] != 'F' || str[2] != 'B' || str[3] != ' ' ||
       !isdigit(str[4]) || !isdigit(str[5]) || !isdigit(str[6]) ||
       str[7] != '.' ||
       !isdigit(str[8]) || !isdigit(str[9]) || !isdigit(str[10]) ||
       str[11] != '\n' ) {
    throw Exception(_T("Invalid format of the RFB version message"));
  }

  unsigned int majorVersion =
    (str[4] - '0') * 100 + (str[5] - '0') * 10 + (str[6] - '0');
  if (majorVersion != 3) {
    throw Exception(_T("Unsupported RFB protocol version requested"));
  }

  unsigned int minorVersion =
    (str[8] - '0') * 100 + (str[9] - '0') * 10 + (str[10] - '0');
  return minorVersion;
}

void RfbInitializer::checkForBan()
{
  if (m_extAuthListener->onCheckForBan(m_client)) {
    throw AuthException(_T("Your connection has been rejected"));
  }
}
