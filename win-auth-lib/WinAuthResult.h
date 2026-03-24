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

#ifndef _WIN_AUTH_RESULT_H_
#define _WIN_AUTH_RESULT_H_

#include "util/StringStorage.h"
#include "server-config-lib/ClientPermissions.h"

// Result of a Windows authentication attempt.
struct WinAuthResult
{
  bool success;
  StringStorage username;
  StringStorage domain;
  ClientPermissions permissions;
  StringStorage errorMessage;

  WinAuthResult()
  : success(false),
    permissions(ClientPermissions::PERM_NONE)
  {
  }
};

#endif // _WIN_AUTH_RESULT_H_
