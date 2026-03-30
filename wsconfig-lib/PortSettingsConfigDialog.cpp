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

#include "tvnserver/resource.h"
#include "PortSettingsConfigDialog.h"
#include "ConfigDialog.h"
#include "CommonInputValidation.h"
#include "UIDataAccess.h"
#include "server-config-lib/Configurator.h"
#include "server-config-lib/ClientPermissions.h"
#include "util/StringTable.h"
#include "util/StringParser.h"

#include <lm.h>
#include <objsel.h>
#pragma comment(lib, "Netapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

PortSettingsConfigDialog::PortSettingsConfigDialog()
: BaseDialog(IDD_CONFIG_PORT_SETTINGS_PAGE),
  m_parentDialog(NULL),
  m_config(NULL),
  m_portConfig(NULL),
  m_cpControl(NULL),
  m_portSelectorLabel(NULL)
{
}

PortSettingsConfigDialog::~PortSettingsConfigDialog()
{
  delete m_cpControl;
}

void PortSettingsConfigDialog::setParentDialog(BaseDialog *dialog)
{
  m_parentDialog = dialog;
}

BOOL PortSettingsConfigDialog::onInitDialog()
{
  m_config = Configurator::getInstance()->getServerConfig();
  initControls();
  updateUI();
  return TRUE;
}

void PortSettingsConfigDialog::initControls()
{
  HWND hwnd = m_ctrlThis.getWindow();
  HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);

  // Create port selector label and combo at the top (programmatic)
  int labelW = 80, comboW = 180, comboH = 200, ctrlH = 20;
  int x = 6, y = 2;

  m_portSelectorLabel = CreateWindow(
    _T("STATIC"), _T("Active Port:"),
    WS_CHILD | WS_VISIBLE | SS_RIGHT,
    x, y + 3, labelW, ctrlH,
    hwnd, (HMENU)(UINT_PTR)IDC_PORT_SELECTOR_LABEL, NULL, NULL);
  if (hFont) SendMessage(m_portSelectorLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

  HWND hCombo = CreateWindow(
    _T("COMBOBOX"), _T(""),
    WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
    x + labelW + 4, y, comboW, comboH,
    hwnd, (HMENU)(UINT_PTR)IDC_PORT_SELECTOR_COMBO, NULL, NULL);
  if (hFont) SendMessage(hCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
  m_portSelector.setWindow(hCombo);

  // Shift all existing child controls down by 26px to make room
  int shiftY = 26;
  HWND hChild = GetWindow(hwnd, GW_CHILD);
  while (hChild != NULL) {
    if (hChild != m_portSelectorLabel && hChild != hCombo) {
      RECT rc;
      GetWindowRect(hChild, &rc);
      POINT pt = { rc.left, rc.top };
      ScreenToClient(hwnd, &pt);
      SetWindowPos(hChild, NULL, pt.x, pt.y + shiftY, 0, 0,
                   SWP_NOSIZE | SWP_NOZORDER);
    }
    hChild = GetWindow(hChild, GW_HWNDNEXT);
  }

  // Windows authentication controls
  m_defaultPermCombo.setWindow(GetDlgItem(hwnd, IDC_DEFAULT_PERM_COMBO));
  m_maxConnPerUser.setWindow(GetDlgItem(hwnd, IDC_MAX_CONN_PER_USER));
  m_maxConnPerUserSpin.setWindow(GetDlgItem(hwnd, IDC_MAX_CONN_PER_USER_SPIN));

  // Populate default permission combo
  SendMessage(m_defaultPermCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("Full Control"));
  SendMessage(m_defaultPermCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("View Only"));
  SendMessage(m_defaultPermCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("View + Clipboard"));
  SendMessage(m_defaultPermCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("Deny Access"));

  m_maxConnPerUserSpin.setBuddy(&m_maxConnPerUser);
  m_maxConnPerUserSpin.setAccel(0, 1);
  m_maxConnPerUserSpin.setRange32(0, 999);

  m_cpControl = new PasswordControl(&m_controlPasswordBtn,
                                    &m_unsetControlPasswordBtn);

  // Group permission rules controls
  m_ruleList.setWindow(GetDlgItem(hwnd, IDC_GROUP_LIST));
  m_groupNameEdit.setWindow(GetDlgItem(hwnd, IDC_GROUP_NAME_EDIT));
  m_permissionCombo.setWindow(GetDlgItem(hwnd, IDC_PERMISSION_COMBO));
  m_priorityEdit.setWindow(GetDlgItem(hwnd, IDC_PRIORITY_EDIT));
  m_prioritySpin.setWindow(GetDlgItem(hwnd, IDC_PRIORITY_SPIN));
  m_addRuleButton.setWindow(GetDlgItem(hwnd, IDC_ADD_RULE_BUTTON));
  m_editRuleButton.setWindow(GetDlgItem(hwnd, IDC_EDIT_RULE_BUTTON));
  m_removeRuleButton.setWindow(GetDlgItem(hwnd, IDC_REMOVE_RULE_BUTTON));
  m_moveUpRuleButton.setWindow(GetDlgItem(hwnd, IDC_MOVE_UP_RULE_BUTTON));
  m_moveDownRuleButton.setWindow(GetDlgItem(hwnd, IDC_MOVE_DOWN_RULE_BUTTON));
  m_browseButton.setWindow(GetDlgItem(hwnd, IDC_BROWSE_GROUPS_BUTTON));

  SendMessage(m_permissionCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("Full Control"));
  SendMessage(m_permissionCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("View Only"));
  SendMessage(m_permissionCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("View + Clipboard"));
  SendMessage(m_permissionCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("Deny Access"));
  SendMessage(m_permissionCombo.getWindow(), CB_SETCURSEL, 0, 0);

  m_prioritySpin.setRange(0, 100);
  m_prioritySpin.setAccel(0, 1);

  m_ruleList.addColumn(0, _T("Group Name"), 200);
  m_ruleList.addColumn(1, _T("Permission"), 120);
  m_ruleList.addColumn(2, _T("Priority"),    60);

  // Control interface auth controls
  m_useControlAuth.setWindow(GetDlgItem(hwnd, IDC_USE_CONTROL_AUTH_CHECKBOX));
  m_repeatControlAuth.setWindow(GetDlgItem(hwnd, IDC_REPEAT_CONTROL_AUTH_CHECKBOX));
  m_controlPasswordBtn.setWindow(GetDlgItem(hwnd, IDC_CONTROL_PASSWORD_BUTTON));
  m_unsetControlPasswordBtn.setWindow(GetDlgItem(hwnd, IDC_UNSET_CONTROL_PASWORD_BUTTON));
}

void PortSettingsConfigDialog::updateUI()
{
  if (m_config == NULL) return;

  // --- Windows auth (per-port) ---
  if (m_portConfig != NULL) {
    UINT32 defPerm = m_portConfig->getDefaultWinAuthPermissions();
    SendMessage(m_defaultPermCombo.getWindow(), CB_SETCURSEL,
                permissionToComboIndex(defPerm), 0);

    // Max connections per user
    m_maxConnPerUser.setSignedInt(m_portConfig->getMaxConnectionsPerUser());

    // Group permission rules
    m_rules = m_portConfig->getGroupRules();
  }

  refreshGroupList();
  updateRuleButtonsState();

  // Control interface auth (global)
  m_useControlAuth.check(m_config->isControlAuthEnabled());
  m_repeatControlAuth.check(m_config->getControlAuthAlwaysChecking());

  if (m_config->hasControlPassword()) {
    unsigned char crypted[8];
    m_config->getControlPassword(crypted);
    m_cpControl->setCryptedPassword((char *)crypted);
  }

  updateControlDependencies();
}

void PortSettingsConfigDialog::updateControlDependencies()
{
  bool ctrlEnabled = m_useControlAuth.isChecked();
  m_repeatControlAuth.setEnabled(ctrlEnabled);
  m_cpControl->setEnabled(ctrlEnabled);
}

bool PortSettingsConfigDialog::validateInput()
{
  // Control auth password check
  if (m_useControlAuth.isChecked() && !m_cpControl->hasPassword()) {
    MessageBox(m_ctrlThis.getWindow(),
               StringTable::getString(IDS_SET_CONTROL_PASSWORD_NOTIFICATION),
               StringTable::getString(IDS_CAPTION_BAD_INPUT),
               MB_ICONSTOP | MB_OK);
    return false;
  }

  return true;
}

void PortSettingsConfigDialog::apply()
{
  if (m_config == NULL) return;

  // Per-port settings
  if (m_portConfig != NULL) {
    int defIdx = (int)SendMessage(m_defaultPermCombo.getWindow(),
                                   CB_GETCURSEL, 0, 0);
    m_portConfig->setDefaultWinAuthPermissions(comboIndexToPermission(defIdx));

    // Max connections per user
    StringStorage maxConnStr;
    m_maxConnPerUser.getText(&maxConnStr);
    int maxConn = 0;
    StringParser::parseInt(maxConnStr.getString(), &maxConn);
    if (maxConn < 0) maxConn = 0;
    m_portConfig->setMaxConnectionsPerUser(maxConn);

    // Group rules
    m_portConfig->setGroupRules(m_rules);
  }

  // Global settings
  AutoLock al(m_config);

  // Control interface auth
  m_config->useControlAuth(m_useControlAuth.isChecked());
  m_config->setControlAuthAlwaysChecking(m_repeatControlAuth.isChecked());
  if (m_cpControl->hasPassword()) {
    m_config->setControlPassword(
      (const unsigned char *)m_cpControl->getCryptedPassword());
  } else {
    m_config->deleteControlPassword();
  }
}

BOOL PortSettingsConfigDialog::onCommand(UINT controlID, UINT notificationID)
{
  if (notificationID == BN_CLICKED) {
    switch (controlID) {
    // Group permission rules
    case IDC_ADD_RULE_BUTTON:         onAddRuleClick(); break;
    case IDC_EDIT_RULE_BUTTON:        onEditRuleClick(); break;
    case IDC_REMOVE_RULE_BUTTON:      onRemoveRuleClick(); break;
    case IDC_MOVE_UP_RULE_BUTTON:     onMoveUpRuleClick(); break;
    case IDC_MOVE_DOWN_RULE_BUTTON:   onMoveDownRuleClick(); break;
    case IDC_BROWSE_GROUPS_BUTTON:    onBrowseGroupsClick(); break;
    // Control interface auth
    case IDC_USE_CONTROL_AUTH_CHECKBOX:    onUseControlAuthClick(); break;
    case IDC_REPEAT_CONTROL_AUTH_CHECKBOX: onRepeatControlAuthClick(); break;
    case IDC_CONTROL_PASSWORD_BUTTON:     onChangeControlPasswordClick(); break;
    case IDC_UNSET_CONTROL_PASWORD_BUTTON: onUnsetControlPasswordClick(); break;
    }
  } else if (notificationID == CBN_SELCHANGE) {
    switch (controlID) {
    case IDC_PORT_SELECTOR_COMBO: onPortSelectorChange(); break;
    case IDC_DEFAULT_PERM_COMBO:  onDefaultPermChange(); break;
    }
  } else if (notificationID == EN_UPDATE) {
    switch (controlID) {
    case IDC_MAX_CONN_PER_USER:
      if (m_parentDialog) ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
      break;
    }
  }
  return TRUE;
}

BOOL PortSettingsConfigDialog::onNotify(UINT controlID, LPARAM data)
{
  NMHDR *hdr = (NMHDR *)data;
  if (hdr->idFrom == IDC_GROUP_LIST) {
    switch (hdr->code) {
    case LVN_ITEMCHANGED: onListViewSelChange(); break;
    case NM_DBLCLK:       onEditRuleClick(); break;
    }
  }
  return TRUE;
}

// --- Port selector ---

void PortSettingsConfigDialog::refreshPortSelector(
  const std::vector<PortConfig> &ports, int selectedIndex)
{
  HWND hCombo = m_portSelector.getWindow();
  if (hCombo == NULL) return;

  SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
  for (size_t i = 0; i < ports.size(); i++) {
    StringStorage label;
    label.format(_T("Port %d"), ports[i].getPort());
    SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)label.getString());
  }
  if (selectedIndex >= 0 && selectedIndex < (int)ports.size()) {
    SendMessage(hCombo, CB_SETCURSEL, selectedIndex, 0);
  }
}

void PortSettingsConfigDialog::onPortSelectorChange()
{
  int newIndex = (int)SendMessage(m_portSelector.getWindow(),
                                  CB_GETCURSEL, 0, 0);
  if (newIndex == CB_ERR) return;
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->onPortSelectorChange(newIndex);
  }
}

// --- Windows auth event handlers ---

void PortSettingsConfigDialog::onDefaultPermChange()
{
  if (m_parentDialog != NULL)
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
}

// --- Control interface auth handlers ---

void PortSettingsConfigDialog::onUseControlAuthClick()
{
  updateControlDependencies();
  if (m_parentDialog != NULL)
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
}

void PortSettingsConfigDialog::onRepeatControlAuthClick()
{
  if (m_parentDialog != NULL)
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
}

void PortSettingsConfigDialog::onChangeControlPasswordClick()
{
  if (m_cpControl->showChangePasswordModalDialog(&m_ctrlThis)) {
    if (m_parentDialog != NULL)
      ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void PortSettingsConfigDialog::onUnsetControlPasswordClick()
{
  m_cpControl->unsetPassword(true, m_ctrlThis.getWindow());
  if (m_parentDialog != NULL)
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
}
