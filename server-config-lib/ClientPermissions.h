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

#ifndef _CLIENT_PERMISSIONS_H_
#define _CLIENT_PERMISSIONS_H_

#include "util/inttypes.h"

// Bitmask-based permission model for VNC client connections.
// Each flag controls a specific capability granted to the client.
class ClientPermissions
{
public:
  // Individual permission flags (bitmask)
  static const UINT32 PERM_NONE          = 0x00;
  static const UINT32 PERM_VIEW_SCREEN   = 0x01;  // Can see remote desktop
  static const UINT32 PERM_KEYBOARD      = 0x02;  // Can send keyboard events
  static const UINT32 PERM_MOUSE         = 0x04;  // Can send mouse events
  static const UINT32 PERM_CLIPBOARD     = 0x08;  // Can exchange clipboard
  static const UINT32 PERM_FILE_TRANSFER = 0x10;  // Can use file transfer
  static const UINT32 PERM_DENY          = 0x80000000; // Connection denied

  // Convenience composite flags
  static const UINT32 PERM_VIEW_ONLY    = PERM_VIEW_SCREEN;
  static const UINT32 PERM_FULL_CONTROL = PERM_VIEW_SCREEN | PERM_KEYBOARD
                                        | PERM_MOUSE | PERM_CLIPBOARD
                                        | PERM_FILE_TRANSFER;

  ClientPermissions() : m_flags(PERM_NONE) {}
  ClientPermissions(UINT32 flags) : m_flags(flags) {}
  ~ClientPermissions() {}

  // Permission check methods
  bool canView()         const { return (m_flags & PERM_VIEW_SCREEN) != 0; }
  bool canKeyboard()     const { return (m_flags & PERM_KEYBOARD) != 0; }
  bool canMouse()        const { return (m_flags & PERM_MOUSE) != 0; }
  bool canClipboard()    const { return (m_flags & PERM_CLIPBOARD) != 0; }
  bool canFileTransfer() const { return (m_flags & PERM_FILE_TRANSFER) != 0; }
  bool isDenied()        const { return (m_flags & PERM_DENY) != 0; }

  // Backward compatibility: returns true if no keyboard AND no mouse
  bool isViewOnly() const { return canView() && !canKeyboard() && !canMouse(); }

  // Check if any permission is granted (not denied and not empty)
  bool hasAnyPermission() const { return !isDenied() && m_flags != PERM_NONE; }

  UINT32 getFlags() const { return m_flags; }
  void setFlags(UINT32 flags) { m_flags = flags; }

  // Create permissions from legacy view-only flag for backward compatibility
  static ClientPermissions fromViewOnlyFlag(bool viewOnly) {
    return ClientPermissions(viewOnly ? PERM_VIEW_ONLY : PERM_FULL_CONTROL);
  }

private:
  UINT32 m_flags;
};

#endif // _CLIENT_PERMISSIONS_H_
