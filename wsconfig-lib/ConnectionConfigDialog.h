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

#ifndef _CONNECTION_CONFIG_DIALOG_H_
#define _CONNECTION_CONFIG_DIALOG_H_

#include "gui/BaseDialog.h"
#include "gui/TextBox.h"
#include "gui/CheckBox.h"
#include "gui/SpinControl.h"
#include "gui/Control.h"
#include "gui/ListBox.h"

#include "server-config-lib/Configurator.h"
#include "server-config-lib/PortConfig.h"

#include <vector>

// Dialog for the Connection tab (IDD_CONFIG_CONNECTION_PAGE).
// Handles RFB port, HTTP port, and extra port mappings.
class ConnectionConfigDialog : public BaseDialog
{
public:
  ConnectionConfigDialog();
  virtual ~ConnectionConfigDialog();

  void setParentDialog(BaseDialog *dialog);

  // Set pointer to ConfigDialog's editable port configs vector.
  void setPortConfigs(std::vector<PortConfig> *portConfigs) { m_portConfigs = portConfigs; }

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
  // Enable/disable port fields based on checkbox state.
  void updatePortDependencies();

  // Control event handlers
  void onAcceptRfbConnectionsClick();
  void onAcceptHttpConnectionsClick();
  void onRfbPortUpdate();
  void onHttpPortUpdate();
  void onAddButtonClick();
  void onEditButtonClick();
  void onRemoveButtonClick();
  void onMappingsSelChange();
  void onMappingsDoubleClick();

protected:
  BaseDialog *m_parent;

  // RFB connection controls
  CheckBox    m_acceptRfbConnections;
  TextBox     m_rfbPort;
  SpinControl m_rfbPortSpin;

  // HTTP connection controls
  CheckBox    m_acceptHttpConnections;
  TextBox     m_httpPort;
  SpinControl m_httpPortSpin;

  // Extra port mappings controls
  ListBox     m_mappingsListBox;
  Control     m_editButton;
  Control     m_removeButton;

  // Pointer into ServerConfig's port mapping container (not owned)
  PortMappingContainer *m_extraPorts;

  // Pointer to ConfigDialog's editable port configs vector (not owned)
  std::vector<PortConfig> *m_portConfigs;
};

#endif // _CONNECTION_CONFIG_DIALOG_H_
