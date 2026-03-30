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
// General session options only. Session sharing is hard-coded to "always shared"
// and disconnect action to "do nothing" — controlled via per-port max connections.
class SessionConfigDialog : public BaseDialog
{
public:
  SessionConfigDialog();
  virtual ~SessionConfigDialog();

  void setParentDialog(BaseDialog *dialog);

  bool validateInput();
  void updateUI();
  void apply();

protected:
  virtual BOOL onInitDialog();
  virtual BOOL onCommand(UINT controlID, UINT notificationID);
  virtual BOOL onNotify(UINT controlID, LPARAM data) { return TRUE; }
  virtual BOOL onDestroy() { return TRUE; }

private:
  void initControls();

protected:
  BaseDialog *m_parent;

  // General session option checkboxes
  CheckBox m_enableFileTransfers;
  CheckBox m_showTrayIcon;
  CheckBox m_connectToRdp;
};

#endif // _SESSION_CONFIG_DIALOG_H_
