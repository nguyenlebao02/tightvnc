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
  m_portSelectorLabel(NULL),
  m_selectedPortIndex(0)
{
}

ConfigDialog::ConfigDialog(bool forService)
: BaseDialog(IDD_CONFIG),
  m_isConfiguringService(forService),
  m_reloadConfigCommand(NULL),
  m_lastSelectedTabIndex(0),
  m_portSelectorLabel(NULL),
  m_selectedPortIndex(0)
{
}

ConfigDialog::ConfigDialog()
: BaseDialog(IDD_CONFIG),
  m_isConfiguringService(false),
  m_reloadConfigCommand(NULL),
  m_lastSelectedTabIndex(0),
  m_portSelectorLabel(NULL),
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
  case IDC_PORT_SELECTOR_COMBO:
    if (notificationID == CBN_SELCHANGE) {
      onPortSelectorChange();
    }
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
  m_selectedPortIndex = 0;

  // Create port selector label and combo above the tab control
  HWND dialogHwnd = m_ctrlThis.getWindow();
  HFONT hFont = (HFONT)SendMessage(dialogHwnd, WM_GETFONT, 0, 0);

  RECT tabRect;
  GetWindowRect(GetDlgItem(dialogHwnd, IDC_CONFIG_TAB), &tabRect);
  POINT tabPos = { tabRect.left, tabRect.top };
  ScreenToClient(dialogHwnd, &tabPos);

  // Place label and combo above the tab control
  int labelW = 80, comboW = 200, comboH = 200, ctrlH = 22;
  int y = tabPos.y - ctrlH - 6;

  m_portSelectorLabel = CreateWindow(
    _T("STATIC"), _T("Active Port:"),
    WS_CHILD | WS_VISIBLE | SS_RIGHT,
    tabPos.x, y + 3, labelW, ctrlH,
    dialogHwnd, (HMENU)(UINT_PTR)IDC_PORT_SELECTOR_LABEL,
    NULL, NULL);
  if (hFont) SendMessage(m_portSelectorLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

  HWND hCombo = CreateWindow(
    _T("COMBOBOX"), _T(""),
    WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
    tabPos.x + labelW + 4, y, comboW, comboH,
    dialogHwnd, (HMENU)(UINT_PTR)IDC_PORT_SELECTOR_COMBO,
    NULL, NULL);
  if (hFont) SendMessage(hCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  m_portSelector.setWindow(hCombo);

  // Populate port selector combo
  refreshPortSelector();

  m_tabControl.addTab(NULL, _T("Temp"));

  // Set up per-port context for tabs before creating them
  m_connectionDialog.setPortConfigs(&m_portConfigs);

  m_connectionDialog.setParent(&m_ctrlThis);
  m_connectionDialog.setParentDialog(this);
  m_connectionDialog.create();
  moveDialogToTabControl(&m_connectionDialog);

  m_authenticationDialog.setParent(&m_ctrlThis);
  m_authenticationDialog.setParentDialog(this);
  m_authenticationDialog.create();
  moveDialogToTabControl(&m_authenticationDialog);
  m_authenticationDialog.hide();

  m_ipAccessControlDialog.setParent(&m_ctrlThis);
  m_ipAccessControlDialog.setParentDialog(this);
  m_ipAccessControlDialog.create();
  moveDialogToTabControl(&m_ipAccessControlDialog);

  m_displayInputDialog.setParent(&m_ctrlThis);
  m_displayInputDialog.setParentDialog(this);
  m_displayInputDialog.create();
  moveDialogToTabControl(&m_displayInputDialog);

  m_permissionsDialog.setParent(&m_ctrlThis);
  m_permissionsDialog.setParentDialog(this);
  m_permissionsDialog.create();
  moveDialogToTabControl(&m_permissionsDialog);

  m_sessionDialog.setParent(&m_ctrlThis);
  m_sessionDialog.setParentDialog(this);
  m_sessionDialog.create();
  moveDialogToTabControl(&m_sessionDialog);

  m_loggingDialog.setParent(&m_ctrlThis);
  m_loggingDialog.setParentDialog(this);
  m_loggingDialog.create();
  moveDialogToTabControl(&m_loggingDialog);

  m_tabControl.addTab(&m_connectionDialog, _T("Connection"));
  m_tabControl.addTab(&m_authenticationDialog, _T("Authentication"));
  m_tabControl.addTab(&m_ipAccessControlDialog, StringTable::getString(IDS_ACCESS_CONTROL_TAB_CAPTION));
  m_tabControl.addTab(&m_displayInputDialog, _T("Display && Input"));
  m_tabControl.addTab(&m_permissionsDialog, _T("Permissions"));
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
    m_authenticationDialog.apply();
    m_ipAccessControlDialog.apply();
    m_displayInputDialog.apply();
    m_permissionsDialog.apply();
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
      m_ipAccessControlDialog.updateUI();
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
  if (!m_authenticationDialog.validateInput()) {
    m_tabControl.showTab(&m_authenticationDialog);
    return false;
  }
  if (!m_ipAccessControlDialog.validateInput()) {
    m_tabControl.showTab(&m_ipAccessControlDialog);
    return false;
  }
  if (!m_displayInputDialog.validateInput()) {
    m_tabControl.showTab(&m_displayInputDialog);
    return false;
  }
  if (!m_permissionsDialog.validateInput()) {
    m_tabControl.showTab(&m_permissionsDialog);
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
  m_portSelector.removeAllItems();
  for (size_t i = 0; i < m_portConfigs.size(); i++) {
    StringStorage label;
    label.format(_T("Port %d"), m_portConfigs[i].getPort());
    m_portSelector.addItem(label.getString());
  }
  if (m_portConfigs.empty()) {
    m_portSelector.addItem(_T("(no ports)"));
  }
  // Clamp selection
  if (m_selectedPortIndex >= (int)m_portConfigs.size()) {
    m_selectedPortIndex = m_portConfigs.empty() ? 0 : (int)m_portConfigs.size() - 1;
  }
  m_portSelector.setSelectedItem(m_selectedPortIndex);
}

void ConfigDialog::onPortSelectorChange()
{
  // Save current tab state into the old PortConfig
  saveCurrentPortContext();

  // Switch to newly selected port
  m_selectedPortIndex = m_portSelector.getSelectedItemIndex();
  switchPortContext();
}

void ConfigDialog::switchPortContext()
{
  PortConfig *pc = getSelectedPortConfig();

  // Push per-port config to tabs that need it
  m_authenticationDialog.setPortConfig(pc);
  m_authenticationDialog.updateUI();

  m_permissionsDialog.setPortConfig(pc);
  m_permissionsDialog.updateUI();

  m_ipAccessControlDialog.setPortConfig(pc);
  // IpAccessControlDialog reads container in onInitDialog();
  // for subsequent port switches, update the container pointer
  if (pc != NULL) {
    m_ipAccessControlDialog.updateUI();
  }
}

void ConfigDialog::saveCurrentPortContext()
{
  PortConfig *pc = getSelectedPortConfig();
  if (pc == NULL) return;

  // Tell per-port tabs to write their UI state into the PortConfig
  m_authenticationDialog.apply();
  m_permissionsDialog.apply();
  // IP access rules are edited in-place through the IpAccessControl pointer
}
