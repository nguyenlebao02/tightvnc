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

#ifndef _SESSION_CONFIG_DIALOG_H_
#define _SESSION_CONFIG_DIALOG_H_

#include "gui/BaseDialog.h"
#include "gui/CheckBox.h"
#include "server-config-lib/Configurator.h"

// Dialog for the Session tab (IDD_CONFIG_SESSION_PAGE).
// Handles session sharing mode, disconnect action, and general session options.
class SessionConfigDialog : public BaseDialog
{
public:
  SessionConfigDialog();
  virtual ~SessionConfigDialog();

  void setParentDialog(BaseDialog *dialog);

  // Validate user input before apply; shows error on failure.
  bool validateInput();
  // Load current ServerConfig values into controls.
  void updateUI();
  // Save control values back to ServerConfig.
  void apply();

protected:
  virtual BOOL onInitDialog();
  virtual BOOL onCommand(UINT controlID, UINT notificationID);
  virtual BOOL onNotify(UINT controlID, LPARAM data) { return TRUE; }
  virtual BOOL onDestroy() { return TRUE; }

private:
  void initControls();

  // Control event handlers
  void onShareRadioButtonClick(int number);
  void onDARadioButtonClick(int number);

protected:
  BaseDialog *m_parent;

  // Session sharing — 5 mutually exclusive radio buttons
  // Radio 1: always shared
  // Radio 2: never shared, keep existing
  // Radio 3: never shared, disconnect existing
  // Radio 4: block new non-shared if connected
  // Radio 5: disconnect existing on new non-shared
  CheckBox m_shared[5];

  // Disconnect action — 3 mutually exclusive radio buttons
  // [0]=Do nothing, [1]=Lock, [2]=Logoff
  CheckBox m_disconnectAction[3];

  // General session option checkboxes
  CheckBox m_enableFileTransfers;
  CheckBox m_showTrayIcon;
  CheckBox m_connectToRdp;
};

#endif // _SESSION_CONFIG_DIALOG_H_
