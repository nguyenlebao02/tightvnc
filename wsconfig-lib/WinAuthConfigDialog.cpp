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

#include "WinAuthConfigDialog.h"
#include "WinAuthResourceIds.h"
#include "ConfigDialog.h"
#include "server-config-lib/ClientPermissions.h"
#include "thread/AutoLock.h"

#include <lm.h>
#include <objsel.h>
#pragma comment(lib, "Netapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

WinAuthConfigDialog::WinAuthConfigDialog()
: BaseDialog(IDD_CONFIG_WIN_AUTH_PAGE),
  m_parentDialog(NULL),
  m_config(NULL)
{
}

WinAuthConfigDialog::~WinAuthConfigDialog()
{
}

void WinAuthConfigDialog::setParentDialog(BaseDialog *dialog)
{
  m_parentDialog = dialog;
}

BOOL WinAuthConfigDialog::onInitDialog()
{
  m_config = Configurator::getInstance()->getServerConfig();

  initControls();
  updateUI();
  return TRUE;
}

void WinAuthConfigDialog::initControls()
{
  HWND hwnd = m_ctrlThis.getWindow();

  m_enableCheck.setWindow(GetDlgItem(hwnd, IDC_WIN_AUTH_ENABLE));
  m_authModeCombo.setWindow(GetDlgItem(hwnd, IDC_AUTH_MODE_COMBO));
  m_ruleList.setWindow(GetDlgItem(hwnd, IDC_GROUP_LIST));
  m_groupNameEdit.setWindow(GetDlgItem(hwnd, IDC_GROUP_NAME_EDIT));
  m_permissionCombo.setWindow(GetDlgItem(hwnd, IDC_PERMISSION_COMBO));
  m_priorityEdit.setWindow(GetDlgItem(hwnd, IDC_PRIORITY_EDIT));
  m_prioritySpin.setWindow(GetDlgItem(hwnd, IDC_PRIORITY_SPIN));
  m_addButton.setWindow(GetDlgItem(hwnd, IDC_ADD_RULE_BUTTON));
  m_editButton.setWindow(GetDlgItem(hwnd, IDC_EDIT_RULE_BUTTON));
  m_removeButton.setWindow(GetDlgItem(hwnd, IDC_REMOVE_RULE_BUTTON));
  m_moveUpButton.setWindow(GetDlgItem(hwnd, IDC_MOVE_UP_RULE_BUTTON));
  m_moveDownButton.setWindow(GetDlgItem(hwnd, IDC_MOVE_DOWN_RULE_BUTTON));
  m_browseButton.setWindow(GetDlgItem(hwnd, IDC_BROWSE_GROUPS_BUTTON));
  m_defaultPermCombo.setWindow(GetDlgItem(hwnd, IDC_DEFAULT_PERM_COMBO));

  // Setup auth mode combo
  SendMessage(m_authModeCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("VNC Password Only"));
  SendMessage(m_authModeCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("Windows Auth Only"));
  SendMessage(m_authModeCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("Both (VNC + Windows)"));

  // Setup permission combos
  HWND permHwnds[] = { m_permissionCombo.getWindow(),
                        m_defaultPermCombo.getWindow() };
  for (int i = 0; i < 2; i++) {
    SendMessage(permHwnds[i], CB_ADDSTRING, 0, (LPARAM)_T("Full Control"));
    SendMessage(permHwnds[i], CB_ADDSTRING, 0, (LPARAM)_T("View Only"));
    SendMessage(permHwnds[i], CB_ADDSTRING, 0,
                (LPARAM)_T("View + Clipboard"));
    SendMessage(permHwnds[i], CB_ADDSTRING, 0, (LPARAM)_T("Deny Access"));
  }

  // Setup ListView columns
  m_ruleList.addColumn(0, _T("Group Name"), 180);
  m_ruleList.addColumn(1, _T("Permission"), 120);
  m_ruleList.addColumn(2, _T("Priority"), 60);

  // Set priority spin range
  m_prioritySpin.setRange(0, 100);
  m_prioritySpin.setAccel(0, 1);
}

void WinAuthConfigDialog::updateUI()
{
  if (m_config == NULL) return;

  // Load enable state
  bool enabled = m_config->isWinAuthEnabled();
  m_enableCheck.check(enabled);

  // Load auth mode
  int modeIndex = 0; // VNC only
  switch (m_config->getAuthMode()) {
  case ServerConfig::AUTH_VNC_ONLY:     modeIndex = 0; break;
  case ServerConfig::AUTH_WINDOWS_ONLY: modeIndex = 1; break;
  case ServerConfig::AUTH_BOTH:         modeIndex = 2; break;
  }
  SendMessage(m_authModeCombo.getWindow(), CB_SETCURSEL, modeIndex, 0);

  // Load default permission
  int defPermIdx = permissionToComboIndex(m_config->getDefaultWinAuthPermissions());
  SendMessage(m_defaultPermCombo.getWindow(), CB_SETCURSEL, defPermIdx, 0);

  // Load group rules into local copy and populate list
  m_rules = m_config->getGroupRules();
  m_ruleList.clear();
  for (size_t i = 0; i < m_rules.size(); i++) {
    m_ruleList.addItem((int)i, _T(""));
    setListViewItemText((int)i, m_rules[i]);
  }

  // Enable/disable group controls based on checkbox
  enableGroupControls(enabled);
  updateButtonsState();
}

void WinAuthConfigDialog::enableGroupControls(bool enable)
{
  EnableWindow(m_authModeCombo.getWindow(), enable);
  EnableWindow(m_ruleList.getWindow(), enable);
  EnableWindow(m_groupNameEdit.getWindow(), enable);
  EnableWindow(m_permissionCombo.getWindow(), enable);
  EnableWindow(m_priorityEdit.getWindow(), enable);
  EnableWindow(m_addButton.getWindow(), enable);
  EnableWindow(m_browseButton.getWindow(), enable);
  EnableWindow(m_defaultPermCombo.getWindow(), enable);

  if (enable) {
    updateButtonsState();
  } else {
    EnableWindow(m_editButton.getWindow(), FALSE);
    EnableWindow(m_removeButton.getWindow(), FALSE);
    EnableWindow(m_moveUpButton.getWindow(), FALSE);
    EnableWindow(m_moveDownButton.getWindow(), FALSE);
  }
}

BOOL WinAuthConfigDialog::onCommand(UINT controlID, UINT notificationID)
{
  switch (controlID) {
  case IDC_WIN_AUTH_ENABLE:
    onEnableCheckClick();
    break;
  case IDC_AUTH_MODE_COMBO:
    if (notificationID == CBN_SELCHANGE) onAuthModeChange();
    break;
  case IDC_ADD_RULE_BUTTON:
    onAddRuleClick();
    break;
  case IDC_EDIT_RULE_BUTTON:
    onEditRuleClick();
    break;
  case IDC_REMOVE_RULE_BUTTON:
    onRemoveRuleClick();
    break;
  case IDC_MOVE_UP_RULE_BUTTON:
    onMoveUpRuleClick();
    break;
  case IDC_MOVE_DOWN_RULE_BUTTON:
    onMoveDownRuleClick();
    break;
  case IDC_BROWSE_GROUPS_BUTTON:
    onBrowseGroupsClick();
    break;
  case IDC_DEFAULT_PERM_COMBO:
    if (notificationID == CBN_SELCHANGE) onDefaultPermChange();
    break;
  }
  return TRUE;
}

BOOL WinAuthConfigDialog::onNotify(UINT controlID, LPARAM data)
{
  LPNMHDR nmhdr = (LPNMHDR)data;
  if (nmhdr->idFrom == IDC_GROUP_LIST) {
    switch (nmhdr->code) {
    case LVN_ITEMCHANGED:
      onListViewSelChange();
      break;
    case NM_DBLCLK:
      onEditRuleClick();
      break;
    }
  }
  return TRUE;
}

void WinAuthConfigDialog::onEnableCheckClick()
{
  bool checked = m_enableCheck.isChecked();
  enableGroupControls(checked);
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void WinAuthConfigDialog::onAuthModeChange()
{
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void WinAuthConfigDialog::onDefaultPermChange()
{
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void WinAuthConfigDialog::onAddRuleClick()
{
  // Open Windows "Select Users or Groups" dialog
  std::vector<StringStorage> selectedNames;
  if (!showObjectPicker(true, &selectedNames)) {
    return; // User cancelled or error
  }

  // Read permission from combo
  int permIdx = (int)SendMessage(m_permissionCombo.getWindow(),
                                 CB_GETCURSEL, 0, 0);
  if (permIdx == CB_ERR) permIdx = 0;
  UINT32 permFlags = comboIndexToPermission(permIdx);

  // Read priority from edit
  StringStorage priStr;
  m_priorityEdit.getText(&priStr);
  int priority = _tstoi(priStr.getString());

  // Add a rule for each selected user/group
  for (size_t i = 0; i < selectedNames.size(); i++) {
    GroupPermissionRule rule(selectedNames[i].getString(), permFlags, priority);
    m_rules.push_back(rule);

    int idx = (int)m_rules.size() - 1;
    m_ruleList.addItem(idx, _T(""));
    setListViewItemText(idx, rule);
  }

  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void WinAuthConfigDialog::onEditRuleClick()
{
  int sel = m_ruleList.getSelectedIndex();
  if (sel < 0 || sel >= (int)m_rules.size()) return;

  // Load selected rule into edit fields
  m_groupNameEdit.setText(m_rules[sel].getGroupName().getString());
  int permIdx = permissionToComboIndex(m_rules[sel].getPermissionFlags());
  SendMessage(m_permissionCombo.getWindow(), CB_SETCURSEL, permIdx, 0);

  StringStorage priStr;
  priStr.format(_T("%d"), m_rules[sel].getPriority());
  m_priorityEdit.setText(priStr.getString());

  // Remove old entry — user will re-add with Add button
  m_rules.erase(m_rules.begin() + sel);
  m_ruleList.removeItem(sel);

  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void WinAuthConfigDialog::onRemoveRuleClick()
{
  int sel = m_ruleList.getSelectedIndex();
  if (sel < 0 || sel >= (int)m_rules.size()) return;

  m_rules.erase(m_rules.begin() + sel);
  m_ruleList.removeItem(sel);

  updateButtonsState();
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void WinAuthConfigDialog::onMoveUpRuleClick()
{
  int sel = m_ruleList.getSelectedIndex();
  if (sel <= 0 || sel >= (int)m_rules.size()) return;

  std::swap(m_rules[sel], m_rules[sel - 1]);
  setListViewItemText(sel, m_rules[sel]);
  setListViewItemText(sel - 1, m_rules[sel - 1]);
  m_ruleList.selectItem(sel - 1);

  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void WinAuthConfigDialog::onMoveDownRuleClick()
{
  int sel = m_ruleList.getSelectedIndex();
  if (sel < 0 || sel >= (int)m_rules.size() - 1) return;

  std::swap(m_rules[sel], m_rules[sel + 1]);
  setListViewItemText(sel, m_rules[sel]);
  setListViewItemText(sel + 1, m_rules[sel + 1]);
  m_ruleList.selectItem(sel + 1);

  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void WinAuthConfigDialog::onBrowseGroupsClick()
{
  // Open Windows "Select Users or Groups" dialog (single select)
  // and fill the group name edit box with the selection
  std::vector<StringStorage> selectedNames;
  if (showObjectPicker(false, &selectedNames) && !selectedNames.empty()) {
    m_groupNameEdit.setText(selectedNames[0].getString());
  }
}

bool WinAuthConfigDialog::showObjectPicker(
  bool multiSelect, std::vector<StringStorage> *selectedNames)
{
  // Initialize COM (safe to call multiple times)
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  bool comInit = SUCCEEDED(hr) || hr == S_FALSE || hr == RPC_E_CHANGED_MODE;

  IDsObjectPicker *pPicker = NULL;
  hr = CoCreateInstance(CLSID_DsObjectPicker, NULL, CLSCTX_INPROC_SERVER,
                        IID_IDsObjectPicker, (void **)&pPicker);
  if (FAILED(hr)) {
    if (comInit) CoUninitialize();
    MessageBox(m_ctrlThis.getWindow(),
               _T("Failed to create Object Picker dialog."),
               _T("TightVNC"), MB_OK | MB_ICONERROR);
    return false;
  }

  // Configure scopes: local machine + joined domain (if any)
  DSOP_SCOPE_INIT_INFO scopes[2];
  ZeroMemory(scopes, sizeof(scopes));

  // Scope 0: Local computer — users and groups
  scopes[0].cbSize = sizeof(DSOP_SCOPE_INIT_INFO);
  scopes[0].flType = DSOP_SCOPE_TYPE_TARGET_COMPUTER;
  scopes[0].flScope = DSOP_SCOPE_FLAG_STARTING_SCOPE |
                      DSOP_SCOPE_FLAG_DEFAULT_FILTER_USERS |
                      DSOP_SCOPE_FLAG_DEFAULT_FILTER_GROUPS;
  scopes[0].FilterFlags.flDownlevel =
    DSOP_DOWNLEVEL_FILTER_USERS |
    DSOP_DOWNLEVEL_FILTER_LOCAL_GROUPS |
    DSOP_DOWNLEVEL_FILTER_GLOBAL_GROUPS |
    DSOP_DOWNLEVEL_FILTER_ALL_WELLKNOWN_SIDS;

  // Scope 1: Joined domain (ignored if not domain-joined)
  scopes[1].cbSize = sizeof(DSOP_SCOPE_INIT_INFO);
  scopes[1].flType = DSOP_SCOPE_TYPE_UPLEVEL_JOINED_DOMAIN |
                     DSOP_SCOPE_TYPE_DOWNLEVEL_JOINED_DOMAIN;
  scopes[1].FilterFlags.Uplevel.flBothModes =
    DSOP_FILTER_USERS |
    DSOP_FILTER_BUILTIN_GROUPS |
    DSOP_FILTER_WELL_KNOWN_PRINCIPALS |
    DSOP_FILTER_UNIVERSAL_GROUPS_SE |
    DSOP_FILTER_GLOBAL_GROUPS_SE |
    DSOP_FILTER_DOMAIN_LOCAL_GROUPS_SE;
  scopes[1].FilterFlags.flDownlevel =
    DSOP_DOWNLEVEL_FILTER_USERS |
    DSOP_DOWNLEVEL_FILTER_LOCAL_GROUPS |
    DSOP_DOWNLEVEL_FILTER_GLOBAL_GROUPS;

  DSOP_INIT_INFO initInfo;
  ZeroMemory(&initInfo, sizeof(initInfo));
  initInfo.cbSize = sizeof(initInfo);
  initInfo.pwzTargetComputer = NULL;  // Local machine
  initInfo.cDsScopeInfos = 2;
  initInfo.aDsScopeInfos = scopes;
  initInfo.flOptions = multiSelect ? DSOP_FLAG_MULTISELECT : 0;

  hr = pPicker->Initialize(&initInfo);
  if (FAILED(hr)) {
    pPicker->Release();
    if (comInit) CoUninitialize();
    MessageBox(m_ctrlThis.getWindow(),
               _T("Failed to initialize Object Picker."),
               _T("TightVNC"), MB_OK | MB_ICONERROR);
    return false;
  }

  // Show the "Select Users or Groups" dialog
  IDataObject *pdo = NULL;
  hr = pPicker->InvokeDialog(m_ctrlThis.getWindow(), &pdo);

  bool result = false;
  if (hr == S_OK && pdo != NULL) {
    STGMEDIUM stm;
    ZeroMemory(&stm, sizeof(stm));
    FORMATETC fe;
    ZeroMemory(&fe, sizeof(fe));
    fe.cfFormat = (CLIPFORMAT)RegisterClipboardFormat(CFSTR_DSOP_DS_SELECTION_LIST);
    fe.ptd = NULL;
    fe.dwAspect = DVASPECT_CONTENT;
    fe.lindex = -1;
    fe.tymed = TYMED_HGLOBAL;

    hr = pdo->GetData(&fe, &stm);
    if (SUCCEEDED(hr)) {
      PDS_SELECTION_LIST pList =
        (PDS_SELECTION_LIST)GlobalLock(stm.hGlobal);
      if (pList) {
        for (ULONG i = 0; i < pList->cItems; i++) {
          StringStorage name;
          name.setString(pList->aDsSelection[i].pwzName);
          selectedNames->push_back(name);
        }
        result = (pList->cItems > 0);
        GlobalUnlock(stm.hGlobal);
      }
      ReleaseStgMedium(&stm);
    }
    pdo->Release();
  }

  pPicker->Release();
  if (comInit) CoUninitialize();
  return result;
}

void WinAuthConfigDialog::onListViewSelChange()
{
  updateButtonsState();
}

void WinAuthConfigDialog::updateButtonsState()
{
  int sel = m_ruleList.getSelectedIndex();
  int count = (int)m_rules.size();
  bool hasSel = (sel >= 0 && sel < count);

  EnableWindow(m_editButton.getWindow(), hasSel);
  EnableWindow(m_removeButton.getWindow(), hasSel);
  EnableWindow(m_moveUpButton.getWindow(), hasSel && sel > 0);
  EnableWindow(m_moveDownButton.getWindow(), hasSel && sel < count - 1);
}

void WinAuthConfigDialog::setListViewItemText(int index,
                                               const GroupPermissionRule &rule)
{
  m_ruleList.setSubItemText(index, 0, rule.getGroupName().getString());
  m_ruleList.setSubItemText(index, 1, permissionToString(rule.getPermissionFlags()));

  StringStorage priStr;
  priStr.format(_T("%d"), rule.getPriority());
  m_ruleList.setSubItemText(index, 2, priStr.getString());
}

bool WinAuthConfigDialog::validateInput()
{
  return true; // Rules are validated on add
}

void WinAuthConfigDialog::apply()
{
  if (m_config == NULL) return;

  // Save enable state
  m_config->enableWinAuth(m_enableCheck.isChecked());

  // Save auth mode
  int modeIdx = (int)SendMessage(m_authModeCombo.getWindow(),
                                 CB_GETCURSEL, 0, 0);
  switch (modeIdx) {
  case 0: m_config->setAuthMode(ServerConfig::AUTH_VNC_ONLY); break;
  case 1: m_config->setAuthMode(ServerConfig::AUTH_WINDOWS_ONLY); break;
  case 2: m_config->setAuthMode(ServerConfig::AUTH_BOTH); break;
  }

  // Save default permission
  int defIdx = (int)SendMessage(m_defaultPermCombo.getWindow(),
                                CB_GETCURSEL, 0, 0);
  m_config->setDefaultWinAuthPermissions(comboIndexToPermission(defIdx));

  // Save group rules
  m_config->setGroupRules(m_rules);
}

// Static helpers for permission <-> combo conversion
const TCHAR *WinAuthConfigDialog::permissionToString(UINT32 flags)
{
  if (flags & ClientPermissions::PERM_DENY)
    return _T("Deny Access");
  if (flags == ClientPermissions::PERM_VIEW_ONLY)
    return _T("View Only");
  if (flags == (ClientPermissions::PERM_VIEW_ONLY | ClientPermissions::PERM_CLIPBOARD))
    return _T("View + Clipboard");
  return _T("Full Control");
}

UINT32 WinAuthConfigDialog::comboIndexToPermission(int index)
{
  switch (index) {
  case 0: return ClientPermissions::PERM_FULL_CONTROL;
  case 1: return ClientPermissions::PERM_VIEW_ONLY;
  case 2: return ClientPermissions::PERM_VIEW_ONLY | ClientPermissions::PERM_CLIPBOARD;
  case 3: return ClientPermissions::PERM_DENY;
  default: return ClientPermissions::PERM_FULL_CONTROL;
  }
}

int WinAuthConfigDialog::permissionToComboIndex(UINT32 flags)
{
  if (flags & ClientPermissions::PERM_DENY) return 3;
  if (flags == ClientPermissions::PERM_VIEW_ONLY) return 1;
  if (flags == (ClientPermissions::PERM_VIEW_ONLY | ClientPermissions::PERM_CLIPBOARD))
    return 2;
  return 0; // Full control
}
