// Copyright (C) 2008,2009,2010,2011,2012 GlavSoft LLC.
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

#ifndef _CONFIG_DIALOG_H_
#define _CONFIG_DIALOG_H_

#include "config-lib/SettingsManager.h"
#include "config-lib/RegistrySettingsManager.h"

#include "gui/BaseDialog.h"
#include "gui/Control.h"
#include "gui/ComboBox.h"
#include "gui/ListBox.h"
#include "gui/TabControl.h"

#include "server-config-lib/Configurator.h"
#include "server-config-lib/PortConfig.h"

#include <vector>

#include "ConnectionConfigDialog.h"
#include "AuthenticationConfigDialog.h"
#include "IpAccessControlDialog.h"
#include "DisplayInputConfigDialog.h"
#include "PermissionsConfigDialog.h"
#include "SessionConfigDialog.h"
#include "LoggingConfigDialog.h"

#include "tvncontrol-app/ControlCommand.h"

class ConfigDialog : public BaseDialog
{
public:
  ConfigDialog(bool forService, ControlCommand *reloadConfigCommand);
  ConfigDialog(bool forService);
  ConfigDialog();
  virtual ~ConfigDialog();

  void updateApplyButtonState();

  void setConfigReloadCommand(ControlCommand *command);
  void setServiceFlag(bool serviceFlag);

  bool isConfiguringService();

  // Returns pointer to the currently-selected PortConfig in the combo.
  // Valid only while the dialog is alive.
  PortConfig *getSelectedPortConfig();

  // Returns the full editable list of per-port configs.
  std::vector<PortConfig> &getPortConfigs() { return m_portConfigs; }

  // Rebuilds the port selector combo (call after add/remove port).
  void refreshPortSelector();

protected:

  void initControls();
  void loadSettings();
  // Init dialog handler
  virtual BOOL onInitDialog();
  virtual BOOL onCommand(UINT controlID, UINT notificationID);
  virtual BOOL onNotify(UINT controlID, LPARAM data);
  virtual BOOL onDestroy();
  // Button handlers
  void onCancelButtonClick();
  void onOKButtonClick();
  void onApplyButtonClick();
  // Tab handlers
  void onTabChange();
  void onTabChanging();
  // Port selector handler
  void onPortSelectorChange();
private:
  void moveDialogToTabControl(BaseDialog *dialog);
  bool validateInput();
  void updateCaption();
  // Notify per-port tabs to load data from the selected PortConfig.
  void switchPortContext();
  // Write current tab state back into the selected PortConfig before switching.
  void saveCurrentPortContext();
protected:
  // Controls
  Control m_ctrlApplyButton;
  TabControl m_tabControl;
  // Settings
  Configurator *m_config;
  // Dialogs for tab control
  ConnectionConfigDialog m_connectionDialog;
  AuthenticationConfigDialog m_authenticationDialog;
  IpAccessControlDialog m_ipAccessControlDialog;
  DisplayInputConfigDialog m_displayInputDialog;
  PermissionsConfigDialog m_permissionsDialog;
  SessionConfigDialog m_sessionDialog;
  LoggingConfigDialog m_loggingDialog;
  // Other members
  bool m_isConfiguringService;

  ControlCommand *m_reloadConfigCommand;

  int m_lastSelectedTabIndex;

  // Port selector combo — created programmatically above the tab control
  static const UINT IDC_PORT_SELECTOR_COMBO = 1200;
  static const UINT IDC_PORT_SELECTOR_LABEL = 1201;
  ComboBox m_portSelector;
  HWND m_portSelectorLabel;

  // Editable copy of all per-port configs for the dialog session
  std::vector<PortConfig> m_portConfigs;
  int m_selectedPortIndex;
};

#endif
