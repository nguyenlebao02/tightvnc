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
#include "PermissionsConfigDialog.h"
#include "WinAuthResourceIds.h"
#include "ConfigDialog.h"
#include "server-config-lib/ClientPermissions.h"

#include <lm.h>
#include <objsel.h>
#pragma comment(lib, "Netapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

PermissionsConfigDialog::PermissionsConfigDialog()
: BaseDialog(IDD_CONFIG_PERMISSIONS_PAGE),
  m_parentDialog(NULL),
  m_config(NULL)
{
}

PermissionsConfigDialog::~PermissionsConfigDialog()
{
}

void PermissionsConfigDialog::setParentDialog(BaseDialog *dialog)
{
  m_parentDialog = dialog;
}

BOOL PermissionsConfigDialog::onInitDialog()
{
  m_config = Configurator::getInstance()->getServerConfig();
  initControls();
  updateUI();
  return TRUE;
}

void PermissionsConfigDialog::initControls()
{
  HWND hwnd = m_ctrlThis.getWindow();

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

  // Populate permission combo: Full Control / View Only / View+Clipboard / Deny
  SendMessage(m_permissionCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("Full Control"));
  SendMessage(m_permissionCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("View Only"));
  SendMessage(m_permissionCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("View + Clipboard"));
  SendMessage(m_permissionCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("Deny Access"));
  // Default selection: Full Control
  SendMessage(m_permissionCombo.getWindow(), CB_SETCURSEL, 0, 0);

  // Priority spin: range 0-100, step 1
  m_prioritySpin.setRange(0, 100);
  m_prioritySpin.setAccel(0, 1);

  // ListView columns: Group Name / Permission / Priority
  m_ruleList.addColumn(0, _T("Group Name"), 200);
  m_ruleList.addColumn(1, _T("Permission"), 120);
  m_ruleList.addColumn(2, _T("Priority"),    60);
}

void PermissionsConfigDialog::updateUI()
{
  if (m_config == NULL) return;

  // Load group rules into local editable copy
  m_rules = m_config->getGroupRules();
  refreshGroupList();
  updateButtonsState();
}

void PermissionsConfigDialog::refreshGroupList()
{
  m_ruleList.clear();
  for (size_t i = 0; i < m_rules.size(); i++) {
    m_ruleList.addItem((int)i, _T(""));
    setListViewItemText((int)i, m_rules[i]);
  }
}

void PermissionsConfigDialog::setListViewItemText(int index,
                                                   const GroupPermissionRule &rule)
{
  m_ruleList.setSubItemText(index, 0, rule.getGroupName().getString());
  m_ruleList.setSubItemText(index, 1, permissionToString(rule.getPermissionFlags()));

  StringStorage priStr;
  priStr.format(_T("%d"), rule.getPriority());
  m_ruleList.setSubItemText(index, 2, priStr.getString());
}

BOOL PermissionsConfigDialog::onCommand(UINT controlID, UINT notificationID)
{
  switch (controlID) {
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
  }
  return TRUE;
}

BOOL PermissionsConfigDialog::onNotify(UINT controlID, LPARAM data)
{
  NMHDR *hdr = (NMHDR *)data;
  if (hdr->idFrom == IDC_GROUP_LIST) {
    switch (hdr->code) {
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

void PermissionsConfigDialog::onListViewSelChange()
{
  updateButtonsState();
}

void PermissionsConfigDialog::updateButtonsState()
{
  int sel   = m_ruleList.getSelectedIndex();
  int count = (int)m_rules.size();
  bool hasSel = (sel >= 0 && sel < count);

  EnableWindow(m_editButton.getWindow(),     hasSel ? TRUE : FALSE);
  EnableWindow(m_removeButton.getWindow(),   hasSel ? TRUE : FALSE);
  EnableWindow(m_moveUpButton.getWindow(),   (hasSel && sel > 0) ? TRUE : FALSE);
  EnableWindow(m_moveDownButton.getWindow(), (hasSel && sel < count - 1) ? TRUE : FALSE);
}

// --- Rule CRUD ---

void PermissionsConfigDialog::onAddRuleClick()
{
  // Read group name from edit box; use object picker if empty
  StringStorage groupName;
  m_groupNameEdit.getText(&groupName);

  if (groupName.isEmpty()) {
    // Open Windows object picker (multi-select) when name box is empty
    std::vector<StringStorage> selected;
    if (!showObjectPicker(true, &selected) || selected.empty()) {
      return;
    }

    // Read permission and priority once for all selected names
    int permIdx = (int)SendMessage(m_permissionCombo.getWindow(), CB_GETCURSEL, 0, 0);
    if (permIdx == CB_ERR) permIdx = 0;
    UINT32 permFlags = comboIndexToPermission(permIdx);

    StringStorage priStr;
    m_priorityEdit.getText(&priStr);
    int priority = _tstoi(priStr.getString());

    for (size_t i = 0; i < selected.size(); i++) {
      GroupPermissionRule rule(selected[i].getString(), permFlags, priority);
      m_rules.push_back(rule);
      int idx = (int)m_rules.size() - 1;
      m_ruleList.addItem(idx, _T(""));
      setListViewItemText(idx, rule);
    }
  } else {
    // Use the name already typed in the edit box
    int permIdx = (int)SendMessage(m_permissionCombo.getWindow(), CB_GETCURSEL, 0, 0);
    if (permIdx == CB_ERR) permIdx = 0;
    UINT32 permFlags = comboIndexToPermission(permIdx);

    StringStorage priStr;
    m_priorityEdit.getText(&priStr);
    int priority = _tstoi(priStr.getString());

    GroupPermissionRule rule(groupName.getString(), permFlags, priority);
    m_rules.push_back(rule);
    int idx = (int)m_rules.size() - 1;
    m_ruleList.addItem(idx, _T(""));
    setListViewItemText(idx, rule);
  }

  updateButtonsState();
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void PermissionsConfigDialog::onEditRuleClick()
{
  int sel = m_ruleList.getSelectedIndex();
  if (sel < 0 || sel >= (int)m_rules.size()) return;

  // Load selected rule fields into edit controls for in-place editing
  m_groupNameEdit.setText(m_rules[sel].getGroupName().getString());

  int permIdx = permissionToComboIndex(m_rules[sel].getPermissionFlags());
  SendMessage(m_permissionCombo.getWindow(), CB_SETCURSEL, permIdx, 0);

  StringStorage priStr;
  priStr.format(_T("%d"), m_rules[sel].getPriority());
  m_priorityEdit.setText(priStr.getString());

  // Read updated values from controls
  StringStorage groupName;
  m_groupNameEdit.getText(&groupName);

  if (groupName.isEmpty()) return;

  int newPermIdx = (int)SendMessage(m_permissionCombo.getWindow(),
                                     CB_GETCURSEL, 0, 0);
  if (newPermIdx == CB_ERR) newPermIdx = 0;
  UINT32 permFlags = comboIndexToPermission(newPermIdx);

  StringStorage newPriStr;
  m_priorityEdit.getText(&newPriStr);
  int priority = _tstoi(newPriStr.getString());

  // Update rule in-place (non-destructive)
  m_rules[sel] = GroupPermissionRule(groupName.getString(), permFlags, priority);
  setListViewItemText(sel, m_rules[sel]);

  updateButtonsState();
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void PermissionsConfigDialog::onRemoveRuleClick()
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

void PermissionsConfigDialog::onMoveUpRuleClick()
{
  int sel = m_ruleList.getSelectedIndex();
  if (sel <= 0 || sel >= (int)m_rules.size()) return;

  std::swap(m_rules[sel], m_rules[sel - 1]);
  setListViewItemText(sel,     m_rules[sel]);
  setListViewItemText(sel - 1, m_rules[sel - 1]);
  m_ruleList.selectItem(sel - 1);

  updateButtonsState();
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void PermissionsConfigDialog::onMoveDownRuleClick()
{
  int sel = m_ruleList.getSelectedIndex();
  if (sel < 0 || sel >= (int)m_rules.size() - 1) return;

  std::swap(m_rules[sel], m_rules[sel + 1]);
  setListViewItemText(sel,     m_rules[sel]);
  setListViewItemText(sel + 1, m_rules[sel + 1]);
  m_ruleList.selectItem(sel + 1);

  updateButtonsState();
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void PermissionsConfigDialog::onBrowseGroupsClick()
{
  // Single-select: fills group name edit box with the chosen account
  std::vector<StringStorage> selected;
  if (showObjectPicker(false, &selected) && !selected.empty()) {
    m_groupNameEdit.setText(selected[0].getString());
  }
}

// --- Windows COM Object Picker (copied from WinAuthConfigDialog) ---

bool PermissionsConfigDialog::showObjectPicker(
  bool multiSelect, std::vector<StringStorage> *selectedNames)
{
  // Initialize COM (safe to call multiple times in same thread)
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

  // Scope 0: Local computer — local users and groups
  DSOP_SCOPE_INIT_INFO scopes[2];
  ZeroMemory(scopes, sizeof(scopes));

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

  // Scope 1: Domain (silently ignored when not domain-joined)
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
  initInfo.cbSize            = sizeof(initInfo);
  initInfo.pwzTargetComputer = NULL; // local machine
  initInfo.cDsScopeInfos     = 2;
  initInfo.aDsScopeInfos     = scopes;
  initInfo.flOptions         = multiSelect ? DSOP_FLAG_MULTISELECT : 0;

  hr = pPicker->Initialize(&initInfo);
  if (FAILED(hr)) {
    pPicker->Release();
    if (comInit) CoUninitialize();
    MessageBox(m_ctrlThis.getWindow(),
               _T("Failed to initialize Object Picker."),
               _T("TightVNC"), MB_OK | MB_ICONERROR);
    return false;
  }

  // Show "Select Users or Groups" dialog
  IDataObject *pdo = NULL;
  hr = pPicker->InvokeDialog(m_ctrlThis.getWindow(), &pdo);

  bool result = false;
  if (hr == S_OK && pdo != NULL) {
    STGMEDIUM stm;
    ZeroMemory(&stm, sizeof(stm));
    FORMATETC fe;
    ZeroMemory(&fe, sizeof(fe));
    fe.cfFormat = (CLIPFORMAT)RegisterClipboardFormat(CFSTR_DSOP_DS_SELECTION_LIST);
    fe.ptd      = NULL;
    fe.dwAspect = DVASPECT_CONTENT;
    fe.lindex   = -1;
    fe.tymed    = TYMED_HGLOBAL;

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

bool PermissionsConfigDialog::validateInput()
{
  return true; // Rules validated individually on add
}

void PermissionsConfigDialog::apply()
{
  if (m_config == NULL) return;
  m_config->setGroupRules(m_rules);
}

// --- Static permission helpers (mirror WinAuthConfigDialog) ---

const TCHAR *PermissionsConfigDialog::permissionToString(UINT32 flags)
{
  if (flags & ClientPermissions::PERM_DENY)
    return _T("Deny Access");
  if (flags == ClientPermissions::PERM_VIEW_ONLY)
    return _T("View Only");
  if (flags == (ClientPermissions::PERM_VIEW_ONLY | ClientPermissions::PERM_CLIPBOARD))
    return _T("View + Clipboard");
  return _T("Full Control");
}

UINT32 PermissionsConfigDialog::comboIndexToPermission(int index)
{
  switch (index) {
  case 0:  return ClientPermissions::PERM_FULL_CONTROL;
  case 1:  return ClientPermissions::PERM_VIEW_ONLY;
  case 2:  return ClientPermissions::PERM_VIEW_ONLY | ClientPermissions::PERM_CLIPBOARD;
  case 3:  return ClientPermissions::PERM_DENY;
  default: return ClientPermissions::PERM_FULL_CONTROL;
  }
}

int PermissionsConfigDialog::permissionToComboIndex(UINT32 flags)
{
  if (flags & ClientPermissions::PERM_DENY)  return 3;
  if (flags == ClientPermissions::PERM_VIEW_ONLY) return 1;
  if (flags == (ClientPermissions::PERM_VIEW_ONLY | ClientPermissions::PERM_CLIPBOARD))
    return 2;
  return 0; // Full Control
}
