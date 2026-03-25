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

  // Session sharing radio buttons
  m_shared[0].setWindow(GetDlgItem(hwnd, IDC_SHARED_RADIO1));
  m_shared[1].setWindow(GetDlgItem(hwnd, IDC_SHARED_RADIO2));
  m_shared[2].setWindow(GetDlgItem(hwnd, IDC_SHARED_RADIO3));
  m_shared[3].setWindow(GetDlgItem(hwnd, IDC_SHARED_RADIO4));
  m_shared[4].setWindow(GetDlgItem(hwnd, IDC_SHARED_RADIO5));

  // Disconnect action radio buttons
  m_disconnectAction[0].setWindow(GetDlgItem(hwnd, IDC_DO_NOTHING));
  m_disconnectAction[1].setWindow(GetDlgItem(hwnd, IDC_LOCK_WORKSTATION));
  m_disconnectAction[2].setWindow(GetDlgItem(hwnd, IDC_LOGOFF_WORKSTATION));

  // General option checkboxes
  m_enableFileTransfers.setWindow(GetDlgItem(hwnd, IDC_ENABLE_FILE_TRANSFERS));
  m_showTrayIcon.setWindow(GetDlgItem(hwnd, IDC_SHOW_TVNCONTROL_ICON_CHECKBOX));
  m_connectToRdp.setWindow(GetDlgItem(hwnd, IDC_CONNECT_RDP_SESSION));
}

void SessionConfigDialog::updateUI()
{
  ServerConfig *config = Configurator::getInstance()->getServerConfig();

  // --- Session sharing ---
  // Clear all first, then set the matching radio
  for (int i = 0; i < 5; i++) {
    m_shared[i].check(false);
  }

  // Radio 1: always shared
  if (config->isAlwaysShared() && !config->isNeverShared()) {
    m_shared[0].check(true);
  }
  // Radio 2: never shared, keep existing clients
  else if (!config->isAlwaysShared() && config->isNeverShared() &&
           !config->isDisconnectingExistingClients()) {
    m_shared[1].check(true);
  }
  // Radio 3: never shared, disconnect existing clients
  else if (!config->isAlwaysShared() && config->isNeverShared() &&
           config->isDisconnectingExistingClients()) {
    m_shared[2].check(true);
  }
  // Radio 4: block new non-shared when someone is connected
  else if (!config->isAlwaysShared() && !config->isNeverShared() &&
           !config->isDisconnectingExistingClients()) {
    m_shared[3].check(true);
  }
  // Radio 5: disconnect existing on new non-shared connection
  else if (!config->isAlwaysShared() && !config->isNeverShared() &&
           config->isDisconnectingExistingClients()) {
    m_shared[4].check(true);
  }

  // --- Disconnect action ---
  for (int i = 0; i < 3; i++) {
    m_disconnectAction[i].check(false);
  }
  switch (config->getDisconnectAction()) {
  case ServerConfig::DA_DO_NOTHING:
    m_disconnectAction[0].check(true);
    break;
  case ServerConfig::DA_LOCK_WORKSTATION:
    m_disconnectAction[1].check(true);
    break;
  case ServerConfig::DA_LOGOUT_WORKSTATION:
    m_disconnectAction[2].check(true);
    break;
  }

  // --- General options ---
  m_enableFileTransfers.check(config->isFileTransfersEnabled());
  m_showTrayIcon.check(config->getShowTrayIconFlag());
  m_connectToRdp.check(config->getConnectToRdpFlag());
}

void SessionConfigDialog::apply()
{
  ServerConfig *config = Configurator::getInstance()->getServerConfig();

  // --- Session sharing ---
  bool alwaysShared     = false;
  bool neverShared      = false;
  bool disconnectClients = false;

  if (m_shared[0].isChecked()) {
    // Radio 1: always shared
    alwaysShared     = true;
    neverShared      = false;
    disconnectClients = false;
  } else if (m_shared[1].isChecked()) {
    // Radio 2: never shared, keep existing
    alwaysShared     = false;
    neverShared      = true;
    disconnectClients = false;
  } else if (m_shared[2].isChecked()) {
    // Radio 3: never shared, disconnect existing
    alwaysShared     = false;
    neverShared      = true;
    disconnectClients = true;
  } else if (m_shared[3].isChecked()) {
    // Radio 4: block new non-shared
    alwaysShared     = false;
    neverShared      = false;
    disconnectClients = false;
  } else if (m_shared[4].isChecked()) {
    // Radio 5: disconnect existing on new non-shared
    alwaysShared     = false;
    neverShared      = false;
    disconnectClients = true;
  }

  config->setAlwaysShared(alwaysShared);
  config->setNeverShared(neverShared);
  config->disconnectExistingClients(disconnectClients);

  // --- Disconnect action ---
  if (m_disconnectAction[0].isChecked()) {
    config->setDisconnectAction(ServerConfig::DA_DO_NOTHING);
  } else if (m_disconnectAction[1].isChecked()) {
    config->setDisconnectAction(ServerConfig::DA_LOCK_WORKSTATION);
  } else if (m_disconnectAction[2].isChecked()) {
    config->setDisconnectAction(ServerConfig::DA_LOGOUT_WORKSTATION);
  }

  // --- General options ---
  config->enableFileTransfers(m_enableFileTransfers.isChecked());
  config->setShowTrayIconFlag(m_showTrayIcon.isChecked());
  config->setConnectToRdpFlag(m_connectToRdp.isChecked());
}

bool SessionConfigDialog::validateInput()
{
  // No numeric fields — always valid
  return true;
}

BOOL SessionConfigDialog::onCommand(UINT controlID, UINT notificationID)
{
  if (notificationID == BN_CLICKED) {
    switch (controlID) {
    // Session sharing radios
    case IDC_SHARED_RADIO1: onShareRadioButtonClick(0); break;
    case IDC_SHARED_RADIO2: onShareRadioButtonClick(1); break;
    case IDC_SHARED_RADIO3: onShareRadioButtonClick(2); break;
    case IDC_SHARED_RADIO4: onShareRadioButtonClick(3); break;
    case IDC_SHARED_RADIO5: onShareRadioButtonClick(4); break;
    // Disconnect action radios
    case IDC_DO_NOTHING:        onDARadioButtonClick(0); break;
    case IDC_LOCK_WORKSTATION:  onDARadioButtonClick(1); break;
    case IDC_LOGOFF_WORKSTATION: onDARadioButtonClick(2); break;
    // General option checkboxes — just mark apply dirty
    case IDC_ENABLE_FILE_TRANSFERS:
    case IDC_SHOW_TVNCONTROL_ICON_CHECKBOX:
    case IDC_CONNECT_RDP_SESSION:
      ((ConfigDialog *)m_parent)->updateApplyButtonState();
      break;
    }
  }
  return TRUE;
}

void SessionConfigDialog::onShareRadioButtonClick(int number)
{
  if (!m_shared[number].isChecked()) {
    m_shared[number].check(true);
    for (int i = 0; i < 5; i++) {
      if (i != number) {
        m_shared[i].check(false);
      }
    }
    ((ConfigDialog *)m_parent)->updateApplyButtonState();
  }
}

void SessionConfigDialog::onDARadioButtonClick(int number)
{
  if (!m_disconnectAction[number].isChecked()) {
    m_disconnectAction[number].check(true);
    for (int i = 0; i < 3; i++) {
      if (i != number) {
        m_disconnectAction[i].check(false);
      }
    }
    ((ConfigDialog *)m_parent)->updateApplyButtonState();
  }
}
