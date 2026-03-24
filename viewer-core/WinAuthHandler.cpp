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

#include "WinAuthHandler.h"

#include "rfb/AuthDefs.h"
#include "rfb/VendorDefs.h"

#include <vector>

WinAuthHandler::WinAuthHandler()
: AuthHandler(AuthDefs::EXTERNAL)
{
}

WinAuthHandler::~WinAuthHandler()
{
}

void WinAuthHandler::authenticate(DataInputStream *input,
                                  DataOutputStream *output)
{
  // Get credentials from subclass (shows UI dialog, etc.)
  StringStorage username, password, domain;
  getCredentials(&username, &password, &domain);

  // Build full username: DOMAIN\username if domain is provided
  StringStorage fullUsername;
  if (domain.getLength() > 0) {
    fullUsername.format(_T("%s\\%s"), domain.getString(), username.getString());
  } else {
    fullUsername = username;
  }

  // Convert username to UTF-8 for wire transmission
  // TCHAR may be wchar_t (Unicode build), so convert to multibyte
  int userLen = WideCharToMultiByte(CP_UTF8, 0,
    fullUsername.getString(), (int)fullUsername.getLength(),
    NULL, 0, NULL, NULL);
  std::vector<char> userBuf(userLen);
  WideCharToMultiByte(CP_UTF8, 0,
    fullUsername.getString(), (int)fullUsername.getLength(),
    userBuf.data(), userLen, NULL, NULL);

  // Convert password to UTF-8
  int passLen = WideCharToMultiByte(CP_UTF8, 0,
    password.getString(), (int)password.getLength(),
    NULL, 0, NULL, NULL);
  std::vector<char> passBuf(passLen);
  WideCharToMultiByte(CP_UTF8, 0,
    password.getString(), (int)password.getLength(),
    passBuf.data(), passLen, NULL, NULL);

  // Wire format: [u32 usernameLen][username][u32 passwordLen][password]
  output->writeUInt32((UINT32)userLen);
  output->writeFully(userBuf.data(), (size_t)userLen);
  output->writeUInt32((UINT32)passLen);
  output->writeFully(passBuf.data(), (size_t)passLen);
  output->flush();

  // Zero out password buffers immediately
  SecureZeroMemory(passBuf.data(), passBuf.size());
}

void WinAuthHandler::addAuthCapability(CapabilitiesManager *capManager)
{
  capManager->addAuthCapability(this,
    AuthDefs::EXTERNAL, VendorDefs::TIGHTVNC, AuthDefs::SIG_EXTERNAL);
}
