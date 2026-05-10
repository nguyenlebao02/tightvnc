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
#include "util/VncPassCrypt.h"
#include "Ra2CryptoProvider.h"
#include "Ra2Authenticator.h"

#include <stdlib.h>
#include <time.h>
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
  m_portConfig(portConfig ? *portConfig : PortConfig()),
  m_rawStream(stream),
  m_tlsStream(NULL),
  m_vencryptUsed(false)
{
  m_output = new DataOutputStream(stream);
  m_input = new DataInputStream(stream);
}

RfbInitializer::~RfbInitializer()
{
  delete m_output;
  delete m_input;
  delete m_tlsStream;
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
  } else if (authType == AuthDefs::VNC) {
    doVncAuth();
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

void RfbInitializer::doVncAuth()
{
  ServerConfig *srvConf = Configurator::getInstance()->getServerConfig();
  if (!srvConf->hasPrimaryPassword()) {
    throw AuthException(_T("VNC password is not set"));
  }

  // Decrypt stored password
  VncPassCrypt vncPassCrypt;
  unsigned char cryptedPass[8];
  srvConf->getPrimaryPassword(cryptedPass);
  vncPassCrypt.updatePlain(cryptedPass);

  // Generate random challenge
  UINT8 challenge[16];
  srand((unsigned int)time(0) ^ (unsigned int)GetCurrentProcessId());
  for (int i = 0; i < 16; i++) {
    challenge[i] = (UINT8)(rand() & 0xFF);
  }

  // Send challenge to client
  m_output->writeFully(challenge, 16);

  // Read response from client
  UINT8 response[16];
  m_input->readFully(response, 16);

  // Verify challenge response
  if (!vncPassCrypt.challengeAndResponseIsValid(challenge, response)) {
    throw AuthException(_T("Authentication failed"));
  }

  // VNC auth succeeded — grant full control
  m_winAuthUsed = false;
  m_viewOnlyAuth = false;
  m_clientPermissions = ClientPermissions(ClientPermissions::PERM_FULL_CONTROL);
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

void RfbInitializer::doRa2Auth(int aesKeySize)
{
  Ra2CryptoProvider crypto;
  if (!crypto.initialize()) {
    throw AuthException(_T("RA2: crypto initialization failed"));
  }
  if (!crypto.loadOrCreateKeyPair()) {
    throw AuthException(_T("RA2: key generation failed"));
  }

  Ra2Authenticator auth(m_input, m_output, &crypto);
  StringStorage username;
  StringStorage password;
  if (!auth.performHandshake(&username, &password, aesKeySize)) {
    throw AuthException(_T("RA2: handshake failed"));
  }

  // Check for ban after handshake but before credential validation
  checkForBan();

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

  // Copy password to mutable buffer for SecureZeroMemory
  size_t passLen = password.getLength() + 1;
  std::vector<TCHAR> mutablePass(passLen);
  _tcscpy_s(&mutablePass[0], passLen, password.getString());

  std::vector<GroupPermissionRule> rules = m_portConfig.getGroupRules();
  UINT32 defaultPerms = m_portConfig.getDefaultWinAuthPermissions();

  WinAuthenticator authenticator(NULL);
  WinAuthResult result = authenticator.performAuth(
    user.getString(),
    &mutablePass[0],
    domain.getString(),
    rules,
    defaultPerms);

  SecureZeroMemory(&mutablePass[0], mutablePass.size() * sizeof(TCHAR));

  // Also zero the password from Ra2Authenticator
  TCHAR *passBuf = const_cast<TCHAR *>(password.getString());
  size_t passCharLen = password.getLength();
  SecureZeroMemory(passBuf, passCharLen * sizeof(TCHAR));

  if (!result.success) {
    m_extAuthListener->onAuthFailed(m_client);
    throw AuthException(_T("Authentication failed"));
  }

  m_winAuthUsed = true;
  m_clientPermissions = result.permissions;
  if (result.domain.getLength() > 0) {
    m_authenticatedUsername.format(_T("%s\\%s"),
                                   result.domain.getString(),
                                   result.username.getString());
  } else {
    m_authenticatedUsername = result.username;
  }

  m_viewOnlyAuth = m_clientPermissions.isViewOnly();

  // Perform access control check and send auth result
  m_extAuthListener->onCheckAccessControl(m_client);
  m_output->writeUInt32(0);
}

void RfbInitializer::doVeNCryptAuth()
{
  // Phase 1: VeNCrypt version negotiation (0.2 only)
  m_output->writeUInt8(0);
  m_output->writeUInt8(2);

  UINT8 clientMajor = m_input->readUInt8();
  UINT8 clientMinor = m_input->readUInt8();

  if (clientMajor != 0 || clientMinor > 2) {
    m_output->writeUInt8(0xFF);
    throw AuthException(_T("VeNCrypt: unsupported protocol version"));
  }
  m_output->writeUInt8(0x00);

  // Phase 2: Sub-type negotiation
  std::vector<UINT32> subTypes;
  if (m_authAllowed) {
    subTypes.push_back(VeNCryptDefs::TLSVNC);
    subTypes.push_back(VeNCryptDefs::TLSPLAIN);
  } else {
    subTypes.push_back(VeNCryptDefs::TLSNONE);
  }

  m_output->writeUInt8((UINT8)subTypes.size());
  for (size_t i = 0; i < subTypes.size(); i++) {
    m_output->writeUInt32(subTypes[i]);
  }

  UINT32 chosenSubType = m_input->readUInt32();

  bool valid = false;
  for (size_t i = 0; i < subTypes.size(); i++) {
    if (subTypes[i] == chosenSubType) {
      valid = true;
      break;
    }
  }
  if (!valid) {
    m_output->writeUInt8(0x00);
    throw AuthException(_T("VeNCrypt: client selected unsupported sub-type"));
  }

  // Phase 3: TLS handshake + stream replacement
  if (chosenSubType == VeNCryptDefs::TLSNONE ||
      chosenSubType == VeNCryptDefs::TLSVNC ||
      chosenSubType == VeNCryptDefs::TLSPLAIN) {

    m_output->writeUInt8(0x01);

    m_tlsStream = new SchannelTlsStream(m_rawStream);
    try {
      m_tlsStream->performHandshake();
    } catch (Exception &e) {
      delete m_tlsStream;
      m_tlsStream = NULL;
      throw AuthException(_T("VeNCrypt: TLS handshake failed"));
    }

    // Replace streams — all subsequent I/O goes through TLS
    DataOutputStream *newOut = new DataOutputStream(m_tlsStream);
    DataInputStream *newIn = new DataInputStream(m_tlsStream);
    delete m_output;
    delete m_input;
    m_output = newOut;
    m_input = newIn;
    m_vencryptUsed = true;

    // Phase 4: Inner auth over TLS
    if (chosenSubType == VeNCryptDefs::TLSNONE) {
      m_extAuthListener->onCheckAccessControl(m_client);
      m_output->writeUInt32(0);
    } else if (chosenSubType == VeNCryptDefs::TLSVNC) {
      doAuth(AuthDefs::VNC);
    } else if (chosenSubType == VeNCryptDefs::TLSPLAIN) {
      doAuth(AuthDefs::EXTERNAL);
    }
  } else {
    m_output->writeUInt8(0x00);
    throw AuthException(_T("VeNCrypt: non-TLS sub-types not supported"));
  }
}

void RfbInitializer::initAuthenticate()
{
  try {
    // Here the protocol varies between versions 3.3 and 3.7+.
    if (m_minorVerNum >= 7) {
      if (m_authAllowed) {
        // Offer: VNC(2) standard password, RA2(5)/RA2_256(6) for RealVNC
        // Viewer Windows auth, TIGHT(16) for TIGHT-capable viewers,
        // VENCRYPT(19) for RealVNC encrypted connections.
        m_output->writeUInt8(5);
        m_output->writeUInt8(SecurityDefs::VNC);
        m_output->writeUInt8(SecurityDefs::RA2);
        m_output->writeUInt8(SecurityDefs::RA2_256);
        m_output->writeUInt8(SecurityDefs::TIGHT);
        m_output->writeUInt8(SecurityDefs::VENCRYPT);
        UINT8 clientSecType = m_input->readUInt8();
        if (clientSecType == SecurityDefs::RA2) {
          doRa2Auth(16);
        } else if (clientSecType == SecurityDefs::RA2_256) {
          doRa2Auth(32);
        } else if (clientSecType == SecurityDefs::TIGHT) {
          m_tightEnabled = true;
          doTightAuth();
        } else if (clientSecType == SecurityDefs::VNC) {
          doAuth(AuthDefs::VNC);
        } else if (clientSecType == SecurityDefs::VENCRYPT) {
          doVeNCryptAuth();
        } else {
          throw AuthException(_T("Unsupported security type"));
        }
      } else {
        // Auth not allowed (e.g. loopback) — offer NONE
        m_output->writeUInt8(1);
        m_output->writeUInt8(SecurityDefs::NONE);
        UINT8 clientSecType = m_input->readUInt8();
        if (clientSecType != SecurityDefs::NONE) {
          throw AuthException(_T("Security types do not match"));
        }
        doAuth(AuthDefs::NONE);
      }
    } else {
      // Protocol 3.3: only supported for auth-disabled (loopback) connections
      m_output->writeUInt32(SecurityDefs::NONE);
      doAuth(AuthDefs::NONE);
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
