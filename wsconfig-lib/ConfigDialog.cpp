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

#include "ConfigDialog.h"
#include "tvnserver/resource.h"
#include "util/CommonHeader.h"

ConfigDialog::ConfigDialog(bool forService, ControlCommand *reloadConfigCommand)
: BaseDialog(IDD_CONFIG),
  m_isConfiguringService(forService),
  m_reloadConfigCommand(reloadConfigCommand),
  m_lastSelectedTabIndex(0),
  m_selectedPortIndex(0)
{
}

ConfigDialog::ConfigDialog(bool forService)
: BaseDialog(IDD_CONFIG),
  m_isConfiguringService(forService),
  m_reloadConfigCommand(NULL),
  m_lastSelectedTabIndex(0),
  m_selectedPortIndex(0)
{
}

ConfigDialog::ConfigDialog()
: BaseDialog(IDD_CONFIG),
  m_isConfiguringService(false),
  m_reloadConfigCommand(NULL),
  m_lastSelectedTabIndex(0),
  m_selectedPortIndex(0)
{
}

ConfigDialog::~ConfigDialog()
{
}

// FIXME: Unimplemented
void ConfigDialog::updateApplyButtonState()
{
  m_ctrlApplyButton.setEnabled(true);
}

void ConfigDialog::setConfigReloadCommand(ControlCommand *command)
{
  m_reloadConfigCommand = command;

  updateCaption();
}

void ConfigDialog::setServiceFlag(bool serviceFlag)
{
  m_isConfiguringService = serviceFlag;

  updateCaption();
}

bool ConfigDialog::isConfiguringService()
{
  return m_isConfiguringService;
}

void ConfigDialog::initControls()
{
  HWND dialogHwnd = m_ctrlThis.getWindow();

  m_ctrlApplyButton.setWindow(GetDlgItem(dialogHwnd, IDC_APPLY));
  m_tabControl.setWindow(GetDlgItem(dialogHwnd, IDC_CONFIG_TAB));

  //
  // Change caption of dialog
  //

  updateCaption();
}

void ConfigDialog::loadSettings()
{
  m_config->load();
}

BOOL ConfigDialog::onCommand(UINT controlID, UINT notificationID)
{
  switch (controlID) {
  case IDOK:
    onOKButtonClick();
    break;
  case IDCANCEL:
    onCancelButtonClick();
    break;
  case IDC_APPLY:
    onApplyButtonClick();
    break;
  }
  return TRUE;
}

BOOL ConfigDialog::onNotify(UINT controlID, LPARAM data)
{
  switch (controlID) {
  case IDC_CONFIG_TAB:
    switch (((LPNMHDR)data)->code) {
      case TCN_SELCHANGE:
        onTabChange();
        break;
      case TCN_SELCHANGING:
        onTabChanging();
        break;
    }
    break;
  }
  return TRUE;
}

BOOL ConfigDialog::onInitDialog()
{
  m_config = Configurator::getInstance();
  m_config->setServiceFlag(m_isConfiguringService);

  initControls();

  // Load per-port configs into editable copy
  ServerConfig *srvConfig = m_config->getServerConfig();
  m_portConfigs = srvConfig->getAllPortConfigs();

  // Ensure at least a default port config so the port selector is never empty
  if (m_portConfigs.empty()) {
    PortConfig defaultPc;
    defaultPc.setPort(srvConfig->getRfbPort());
    m_portConfigs.push_back(defaultPc);
  }

  m_selectedPortIndex = 0;

  m_tabControl.addTab(NULL, _T("Temp"));

  // Set up per-port context for Connection tab
  m_connectionDialog.setPortConfigs(&m_portConfigs);

  m_connectionDialog.setParent(&m_ctrlThis);
  m_connectionDialog.setParentDialog(this);
  m_connectionDialog.create();
  moveDialogToTabControl(&m_connectionDialog);

  m_portSettingsDialog.setParent(&m_ctrlThis);
  m_portSettingsDialog.setParentDialog(this);
  m_portSettingsDialog.create();
  moveDialogToTabControl(&m_portSettingsDialog);

  m_displayInputDialog.setParent(&m_ctrlThis);
  m_displayInputDialog.setParentDialog(this);
  m_displayInputDialog.create();
  moveDialogToTabControl(&m_displayInputDialog);

  m_sessionDialog.setParent(&m_ctrlThis);
  m_sessionDialog.setParentDialog(this);
  m_sessionDialog.create();
  moveDialogToTabControl(&m_sessionDialog);

  m_loggingDialog.setParent(&m_ctrlThis);
  m_loggingDialog.setParentDialog(this);
  m_loggingDialog.create();
  moveDialogToTabControl(&m_loggingDialog);

  m_tabControl.addTab(&m_connectionDialog, _T("Connection"));
  m_tabControl.addTab(&m_portSettingsDialog, _T("Port Settings"));
  m_tabControl.addTab(&m_displayInputDialog, _T("Display && Input"));
  m_tabControl.addTab(&m_sessionDialog, _T("Session"));
  m_tabControl.addTab(&m_loggingDialog, _T("Logging"));

  m_tabControl.removeTab(0);

  // Push initial per-port context to tabs
  switchPortContext();

  m_tabControl.showTab(m_lastSelectedTabIndex);
  m_tabControl.setFocus();

  m_ctrlApplyButton.setEnabled(false);
  m_ctrlThis.setForeground();

  return FALSE;
}

BOOL ConfigDialog::onDestroy()
{
  m_lastSelectedTabIndex = m_tabControl.getSelectedTabIndex();
  m_tabControl.deleteAllTabs();
  return TRUE;
}

void ConfigDialog::onCancelButtonClick()
{
  kill(0);
}

void ConfigDialog::onOKButtonClick()
{
  onApplyButtonClick();
  if (!m_ctrlApplyButton.isEnabled()) { // onApplyButtonClick() has been successfully processed.
    kill(0);
  }
}

void ConfigDialog::onApplyButtonClick()
{
  // Check values that specified in gui.
  bool canApply = m_ctrlApplyButton.isEnabled() && validateInput();

  // Save current per-port tab state into the selected PortConfig before applying
  saveCurrentPortContext();

  // Fill global server configuration with values from gui.
  if (canApply) {
    m_connectionDialog.apply();
    m_portSettingsDialog.apply();
    m_displayInputDialog.apply();
    m_sessionDialog.apply();
    m_loggingDialog.apply();

    // Write the full PortConfig vector to ServerConfig
    ServerConfig *srvConfig = m_config->getServerConfig();
    srvConfig->setAllPortConfigs(m_portConfigs);
  } else {
    return ;
  }

  // If reload command is specified then we're working in online mode
  // and we don't to save configuration locally.
  if (m_reloadConfigCommand != NULL) {
    m_reloadConfigCommand->execute();

    if (m_reloadConfigCommand->executionResultOk()) {
      m_sessionDialog.updateUI();
      m_loggingDialog.updateUI();
      m_portSettingsDialog.updateUI();
      m_ctrlApplyButton.setEnabled(false);
    }
  } else {
     // Else we're working in offline mode and we need to save config
    if (!m_config->save()) {
      MessageBox(m_ctrlThis.getWindow(),
                 StringTable::getString(IDS_CANNOT_SAVE_CONFIG),
                 StringTable::getString(IDS_MBC_ERROR),
                 MB_OK | MB_ICONERROR);
    } else {
      m_ctrlApplyButton.setEnabled(false);
      MessageBox(m_ctrlThis.getWindow(),
        StringTable::getString(IDS_OFFLINE_CONFIG_SAVE_NOTIFICATION),
        StringTable::getString(IDS_MBC_TVNCONTROL),
        MB_OK | MB_ICONINFORMATION);
    } // if cannot save.
  } // if offline mode (reload command not specified).
}

void ConfigDialog::onTabChange()
{
  int currentTabIndex = m_tabControl.getSelectedTabIndex();
  Tab *tab = m_tabControl.getTab(currentTabIndex);
  tab->setVisible(true);
}

void ConfigDialog::onTabChanging()
{
  int currentTabIndex = m_tabControl.getSelectedTabIndex();
  Tab *tab = m_tabControl.getTab(currentTabIndex);
  tab->setVisible(false);
}

void ConfigDialog::moveDialogToTabControl(BaseDialog *dialog)
{
  RECT rect;
  POINT first, last;

  m_tabControl.adjustRect(&rect);

  first.x = rect.left;
  first.y = rect.top;
  last.x = rect.right;
  last.y = rect.bottom;

  HWND hwndFrom = m_tabControl.getWindow();
  HWND hwndTo = dialog->getControl()->getWindow();

  MapWindowPoints(hwndFrom, hwndTo, &first, 1);
  MapWindowPoints(hwndFrom, hwndTo, &last, 1);

  MoveWindow(dialog->getControl()->getWindow(),
             first.x, first.y, last.x - first.x, last.y - first.y, TRUE);
}

bool ConfigDialog::validateInput()
{
  if (!m_connectionDialog.validateInput()) {
    m_tabControl.showTab(&m_connectionDialog);
    return false;
  }
  if (!m_portSettingsDialog.validateInput()) {
    m_tabControl.showTab(&m_portSettingsDialog);
    return false;
  }
  if (!m_displayInputDialog.validateInput()) {
    m_tabControl.showTab(&m_displayInputDialog);
    return false;
  }
  if (!m_sessionDialog.validateInput()) {
    m_tabControl.showTab(&m_sessionDialog);
    return false;
  }
  if (!m_loggingDialog.validateInput()) {
    m_tabControl.showTab(&m_loggingDialog);
    return false;
  }
  return true;
}

void ConfigDialog::updateCaption()
{
  StringStorage caption;

  caption.format(StringTable::getString(IDS_SERVER_CONFIG_CAPTION_FORMAT),
                 StringTable::getString(m_isConfiguringService ? IDS_SERVICE : IDS_SERVER),
                 m_reloadConfigCommand == 0 ? StringTable::getString(IDS_OFFLINE_MODE) : _T(""));

  m_ctrlThis.setText(caption.getString());
}

PortConfig *ConfigDialog::getSelectedPortConfig()
{
  if (m_selectedPortIndex >= 0 &&
      m_selectedPortIndex < (int)m_portConfigs.size()) {
    return &m_portConfigs[m_selectedPortIndex];
  }
  return NULL;
}

void ConfigDialog::refreshPortSelector()
{
  // Delegate to PortSettingsConfigDialog which owns the port selector combo
  m_portSettingsDialog.refreshPortSelector(m_portConfigs, m_selectedPortIndex);
}

void ConfigDialog::onPortSelectorChange(int newIndex)
{
  // Save current tab state into the old PortConfig
  saveCurrentPortContext();

  // Switch to newly selected port
  m_selectedPortIndex = newIndex;
  switchPortContext();
}

void ConfigDialog::switchPortContext()
{
  PortConfig *pc = getSelectedPortConfig();

  // Refresh the port selector combo inside the Port Settings tab
  refreshPortSelector();

  // Push per-port config to the merged Port Settings tab
  m_portSettingsDialog.setPortConfig(pc);
  m_portSettingsDialog.updateUI();
}

void ConfigDialog::saveCurrentPortContext()
{
  PortConfig *pc = getSelectedPortConfig();
  if (pc == NULL) return;

  // Tell the Port Settings tab to write UI state into the PortConfig
  m_portSettingsDialog.apply();
}
