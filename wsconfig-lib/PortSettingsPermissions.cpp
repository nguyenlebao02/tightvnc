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

// PortSettingsConfigDialog — Group permission rules methods.
// Split from PortSettingsConfigDialog.cpp for file size management.

#include "tvnserver/resource.h"
#include "PortSettingsConfigDialog.h"
#include "ConfigDialog.h"
#include "server-config-lib/ClientPermissions.h"

#include <lm.h>
#include <objsel.h>
#pragma comment(lib, "Netapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

void PortSettingsConfigDialog::refreshGroupList()
{
  m_ruleList.clear();
  for (size_t i = 0; i < m_rules.size(); i++) {
    m_ruleList.addItem((int)i, _T(""));
    setListViewItemText((int)i, m_rules[i]);
  }
}

void PortSettingsConfigDialog::setListViewItemText(
  int index, const GroupPermissionRule &rule)
{
  m_ruleList.setSubItemText(index, 0, rule.getGroupName().getString());
  m_ruleList.setSubItemText(index, 1, permissionToString(rule.getPermissionFlags()));

  StringStorage priStr;
  priStr.format(_T("%d"), rule.getPriority());
  m_ruleList.setSubItemText(index, 2, priStr.getString());
}

void PortSettingsConfigDialog::updateRuleButtonsState()
{
  int sel   = m_ruleList.getSelectedIndex();
  int count = (int)m_rules.size();
  bool hasSel = (sel >= 0 && sel < count);

  EnableWindow(m_editRuleButton.getWindow(),     hasSel ? TRUE : FALSE);
  EnableWindow(m_removeRuleButton.getWindow(),   hasSel ? TRUE : FALSE);
  EnableWindow(m_moveUpRuleButton.getWindow(),   (hasSel && sel > 0) ? TRUE : FALSE);
  EnableWindow(m_moveDownRuleButton.getWindow(), (hasSel && sel < count - 1) ? TRUE : FALSE);
}

void PortSettingsConfigDialog::onAddRuleClick()
{
  StringStorage groupName;
  m_groupNameEdit.getText(&groupName);

  int permIdx = (int)SendMessage(m_permissionCombo.getWindow(),
                                  CB_GETCURSEL, 0, 0);
  if (permIdx == CB_ERR) permIdx = 0;
  UINT32 permFlags = comboIndexToPermission(permIdx);

  StringStorage priStr;
  m_priorityEdit.getText(&priStr);
  int priority = _tstoi(priStr.getString());

  if (groupName.isEmpty()) {
    // Open Windows object picker when name box is empty
    std::vector<StringStorage> selected;
    if (!showObjectPicker(true, &selected) || selected.empty()) return;

    for (size_t i = 0; i < selected.size(); i++) {
      GroupPermissionRule rule(selected[i].getString(), permFlags, priority);
      m_rules.push_back(rule);
      int idx = (int)m_rules.size() - 1;
      m_ruleList.addItem(idx, _T(""));
      setListViewItemText(idx, rule);
    }
  } else {
    GroupPermissionRule rule(groupName.getString(), permFlags, priority);
    m_rules.push_back(rule);
    int idx = (int)m_rules.size() - 1;
    m_ruleList.addItem(idx, _T(""));
    setListViewItemText(idx, rule);
  }

  updateRuleButtonsState();
  if (m_parentDialog != NULL)
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
}

void PortSettingsConfigDialog::onEditRuleClick()
{
  int sel = m_ruleList.getSelectedIndex();
  if (sel < 0 || sel >= (int)m_rules.size()) return;

  // Load rule into edit controls
  m_groupNameEdit.setText(m_rules[sel].getGroupName().getString());
  SendMessage(m_permissionCombo.getWindow(), CB_SETCURSEL,
              permissionToComboIndex(m_rules[sel].getPermissionFlags()), 0);

  StringStorage priStr;
  priStr.format(_T("%d"), m_rules[sel].getPriority());
  m_priorityEdit.setText(priStr.getString());

  // Re-read from controls and update rule in-place
  StringStorage groupName;
  m_groupNameEdit.getText(&groupName);
  if (groupName.isEmpty()) return;

  int newPermIdx = (int)SendMessage(m_permissionCombo.getWindow(),
                                     CB_GETCURSEL, 0, 0);
  if (newPermIdx == CB_ERR) newPermIdx = 0;

  StringStorage newPriStr;
  m_priorityEdit.getText(&newPriStr);

  m_rules[sel] = GroupPermissionRule(groupName.getString(),
                                     comboIndexToPermission(newPermIdx),
                                     _tstoi(newPriStr.getString()));
  setListViewItemText(sel, m_rules[sel]);

  updateRuleButtonsState();
  if (m_parentDialog != NULL)
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
}

void PortSettingsConfigDialog::onRemoveRuleClick()
{
  int sel = m_ruleList.getSelectedIndex();
  if (sel < 0 || sel >= (int)m_rules.size()) return;

  m_rules.erase(m_rules.begin() + sel);
  m_ruleList.removeItem(sel);

  updateRuleButtonsState();
  if (m_parentDialog != NULL)
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
}

void PortSettingsConfigDialog::onMoveUpRuleClick()
{
  int sel = m_ruleList.getSelectedIndex();
  if (sel <= 0 || sel >= (int)m_rules.size()) return;

  std::swap(m_rules[sel], m_rules[sel - 1]);
  setListViewItemText(sel,     m_rules[sel]);
  setListViewItemText(sel - 1, m_rules[sel - 1]);
  m_ruleList.selectItem(sel - 1);

  updateRuleButtonsState();
  if (m_parentDialog != NULL)
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
}

void PortSettingsConfigDialog::onMoveDownRuleClick()
{
  int sel = m_ruleList.getSelectedIndex();
  if (sel < 0 || sel >= (int)m_rules.size() - 1) return;

  std::swap(m_rules[sel], m_rules[sel + 1]);
  setListViewItemText(sel,     m_rules[sel]);
  setListViewItemText(sel + 1, m_rules[sel + 1]);
  m_ruleList.selectItem(sel + 1);

  updateRuleButtonsState();
  if (m_parentDialog != NULL)
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
}

void PortSettingsConfigDialog::onBrowseGroupsClick()
{
  std::vector<StringStorage> selected;
  if (showObjectPicker(false, &selected) && !selected.empty()) {
    m_groupNameEdit.setText(selected[0].getString());
  }
}

void PortSettingsConfigDialog::onListViewSelChange()
{
  updateRuleButtonsState();
}

// --- Windows COM Object Picker ---

bool PortSettingsConfigDialog::showObjectPicker(
  bool multiSelect, std::vector<StringStorage> *selectedNames)
{
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  bool comInit = SUCCEEDED(hr) || hr == S_FALSE || hr == RPC_E_CHANGED_MODE;

  IDsObjectPicker *pPicker = NULL;
  hr = CoCreateInstance(CLSID_DsObjectPicker, NULL, CLSCTX_INPROC_SERVER,
                        IID_IDsObjectPicker, (void **)&pPicker);
  if (FAILED(hr)) {
    if (comInit) CoUninitialize();
    MessageBox(m_ctrlThis.getWindow(),
               _T("Failed to create Object Picker dialog."),
               _T("BaoVNC"), MB_OK | MB_ICONERROR);
    return false;
  }

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
  initInfo.pwzTargetComputer = NULL;
  initInfo.cDsScopeInfos     = 2;
  initInfo.aDsScopeInfos     = scopes;
  initInfo.flOptions         = multiSelect ? DSOP_FLAG_MULTISELECT : 0;

  hr = pPicker->Initialize(&initInfo);
  if (FAILED(hr)) {
    pPicker->Release();
    if (comInit) CoUninitialize();
    return false;
  }

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

// --- Static permission helpers ---

const TCHAR *PortSettingsConfigDialog::permissionToString(UINT32 flags)
{
  if (flags & ClientPermissions::PERM_DENY)
    return _T("Deny Access");
  if (flags == ClientPermissions::PERM_VIEW_ONLY)
    return _T("View Only");
  if (flags == (ClientPermissions::PERM_VIEW_ONLY | ClientPermissions::PERM_CLIPBOARD))
    return _T("View + Clipboard");
  return _T("Full Control");
}

UINT32 PortSettingsConfigDialog::comboIndexToPermission(int index)
{
  switch (index) {
  case 0:  return ClientPermissions::PERM_FULL_CONTROL;
  case 1:  return ClientPermissions::PERM_VIEW_ONLY;
  case 2:  return ClientPermissions::PERM_VIEW_ONLY | ClientPermissions::PERM_CLIPBOARD;
  case 3:  return ClientPermissions::PERM_DENY;
  default: return ClientPermissions::PERM_FULL_CONTROL;
  }
}

int PortSettingsConfigDialog::permissionToComboIndex(UINT32 flags)
{
  if (flags & ClientPermissions::PERM_DENY)  return 3;
  if (flags == ClientPermissions::PERM_VIEW_ONLY) return 1;
  if (flags == (ClientPermissions::PERM_VIEW_ONLY | ClientPermissions::PERM_CLIPBOARD))
    return 2;
  return 0;
}
