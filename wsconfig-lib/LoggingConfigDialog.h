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

#ifndef _LOGGING_CONFIG_DIALOG_H_
#define _LOGGING_CONFIG_DIALOG_H_

#include "gui/BaseDialog.h"
#include "gui/TextBox.h"
#include "gui/CheckBox.h"
#include "gui/SpinControl.h"
#include "gui/Control.h"

#include "server-config-lib/Configurator.h"

// Dialog for the Logging tab (IDD_CONFIG_LOGGING_PAGE).
// Handles log verbosity level, log-for-all-users flag, and open-log-folder action.
class LoggingConfigDialog : public BaseDialog
{
public:
  LoggingConfigDialog();
  virtual ~LoggingConfigDialog();

  void setParentDialog(BaseDialog *dialog);

  // Validate log level (0-10); shows error on failure.
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
  void onLogLevelUpdate();
  void onLogForAllUsersClick();
  void onOpenFolderButtonClick();

  // Extract directory portion from a full file path.
  void getFolderName(const TCHAR *path, StringStorage *folder);

protected:
  BaseDialog *m_parent;

  TextBox     m_logLevel;
  SpinControl m_logLevelSpin;
  CheckBox    m_logForAllUsers;
  TextBox     m_logPathTB;      // read-only display of current log folder
  Control     m_openFolderButton;
};

#endif // _LOGGING_CONFIG_DIALOG_H_
