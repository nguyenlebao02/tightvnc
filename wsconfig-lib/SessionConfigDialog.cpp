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

#include "tvnserver/resource.h"
#include "SessionConfigDialog.h"
#include "ConfigDialog.h"
#include "server-config-lib/Configurator.h"
#include "server-config-lib/ServerConfig.h"

SessionConfigDialog::SessionConfigDialog()
: BaseDialog(IDD_CONFIG_SESSION_PAGE), m_parent(NULL)
{
}

SessionConfigDialog::~SessionConfigDialog()
{
}

void SessionConfigDialog::setParentDialog(BaseDialog *dialog)
{
  m_parent = dialog;
}

BOOL SessionConfigDialog::onInitDialog()
{
  initControls();
  updateUI();
  return TRUE;
}

void SessionConfigDialog::initControls()
{
  HWND hwnd = m_ctrlThis.getWindow();

  m_enableFileTransfers.setWindow(GetDlgItem(hwnd, IDC_ENABLE_FILE_TRANSFERS));
  m_showTrayIcon.setWindow(GetDlgItem(hwnd, IDC_SHOW_TVNCONTROL_ICON_CHECKBOX));
  m_connectToRdp.setWindow(GetDlgItem(hwnd, IDC_CONNECT_RDP_SESSION));
}

void SessionConfigDialog::updateUI()
{
  ServerConfig *config = Configurator::getInstance()->getServerConfig();

  m_enableFileTransfers.check(config->isFileTransfersEnabled());
  m_showTrayIcon.check(config->getShowTrayIconFlag());
  m_connectToRdp.check(config->getConnectToRdpFlag());
}

void SessionConfigDialog::apply()
{
  ServerConfig *config = Configurator::getInstance()->getServerConfig();

  // Hard-code: always shared, do nothing on disconnect
  config->setAlwaysShared(true);
  config->setNeverShared(false);
  config->disconnectExistingClients(false);
  config->setDisconnectAction(ServerConfig::DA_DO_NOTHING);

  // General options
  config->enableFileTransfers(m_enableFileTransfers.isChecked());
  config->setShowTrayIconFlag(m_showTrayIcon.isChecked());
  config->setConnectToRdpFlag(m_connectToRdp.isChecked());
}

bool SessionConfigDialog::validateInput()
{
  return true;
}

BOOL SessionConfigDialog::onCommand(UINT controlID, UINT notificationID)
{
  if (notificationID == BN_CLICKED) {
    switch (controlID) {
    case IDC_ENABLE_FILE_TRANSFERS:
    case IDC_SHOW_TVNCONTROL_ICON_CHECKBOX:
    case IDC_CONNECT_RDP_SESSION:
      ((ConfigDialog *)m_parent)->updateApplyButtonState();
      break;
    }
  }
  return TRUE;
}
