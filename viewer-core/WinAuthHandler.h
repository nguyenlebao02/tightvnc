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

#ifndef _WIN_AUTH_HANDLER_H_
#define _WIN_AUTH_HANDLER_H_

#include "viewer-core/AuthHandler.h"
#include "util/CommonHeader.h"

// Base auth handler for Windows credential authentication (EXTERNAL type).
// Subclasses implement getCredentials() to provide username/password/domain
// (e.g., from a GUI dialog or stored config).
class WinAuthHandler : public AuthHandler
{
public:
  WinAuthHandler();
  virtual ~WinAuthHandler();

  // Sends username+password to server for Windows auth validation.
  // Wire format: [u32 usernameLen][username UTF-8][u32 passwordLen][password UTF-8]
  virtual void authenticate(DataInputStream *input, DataOutputStream *output);

  // Registers EXTERNAL auth capability with the viewer core.
  virtual void addAuthCapability(CapabilitiesManager *capabilitiesManager);

protected:
  // Subclasses must provide credentials (e.g., from UI dialog).
  // Throw AuthCanceledException if user cancels.
  virtual void getCredentials(StringStorage *username,
                              StringStorage *password,
                              StringStorage *domain) = 0;
};

#endif // _WIN_AUTH_HANDLER_H_
