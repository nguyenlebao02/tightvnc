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

#include "tvnserver/resource.h"
#include "EditPortMappingDialog.h"

#include "util/StringParser.h"

#include "server-config-lib/Configurator.h"
#include "server-config-lib/PortMappingContainer.h"

#include <lm.h>
#pragma comment(lib, "Netapi32.lib")

EditPortMappingDialog::EditPortMappingDialog(DialogType dlgType)
: BaseDialog(IDD_EDIT_PORT_MAPPING), m_dialogType(dlgType)
{
}

EditPortMappingDialog::~EditPortMappingDialog()
{
}

void EditPortMappingDialog::setMapping(PortMapping *mapping)
{
  m_mapping = mapping;
}

void EditPortMappingDialog::onCancelButtonClick()
{
  kill(IDCANCEL);
}

void EditPortMappingDialog::onOkButtonClick()
{
  if (!isUserDataValid())
    return ;

  PortMappingRect rect;
  int port;

  StringStorage portStringStorage;
  StringStorage rectStringStorage;

  m_geometryTextBox.getText(&rectStringStorage);
  m_portTextBox.getText(&portStringStorage);

  PortMappingRect::parse(rectStringStorage.getString(), &rect);
  StringParser::parseInt(portStringStorage.getString(), &port);

  m_mapping->setPort(port);
  m_mapping->setRect(rect);

  // Save display selection
  int displaySel = (int)SendMessage(m_displayCombo.getWindow(),
                                     CB_GETCURSEL, 0, 0);
  if (displaySel > 0 && displaySel <= (int)m_displayInfos.size()) {
    m_mapping->setDevicePath(m_displayInfos[displaySel - 1].devicePath.getString());
  } else {
    m_mapping->setDevicePath(_T(""));
  }

  // Save per-port auth data
  savePortAuthData();

  kill(IDOK);
}

void EditPortMappingDialog::initControls()
{
  HWND dialogHwnd = m_ctrlThis.getWindow();
  m_geometryTextBox.setWindow(GetDlgItem(dialogHwnd, IDC_GEOMETRY_EDIT));
  m_portTextBox.setWindow(GetDlgItem(dialogHwnd, IDC_PORT_EDIT));
  m_displayCombo.setWindow(GetDlgItem(dialogHwnd, IDC_DISPLAY_COMBO));
  m_portAuthEnable.setWindow(GetDlgItem(dialogHwnd, IDC_PORT_AUTH_ENABLE));
  m_portPermCombo.setWindow(GetDlgItem(dialogHwnd, IDC_PORT_PERM_COMBO));
  m_portGroupList.setWindow(GetDlgItem(dialogHwnd, IDC_PORT_GROUP_LIST));
  m_portGroupNameEdit.setWindow(GetDlgItem(dialogHwnd, IDC_PORT_GROUP_NAME_EDIT));
  m_portGroupPermCombo.setWindow(GetDlgItem(dialogHwnd, IDC_PORT_GROUP_PERM_COMBO));
}

void EditPortMappingDialog::populateDisplayCombo()
{
  HWND hCombo = m_displayCombo.getWindow();
  SendMessage(hCombo, CB_RESETCONTENT, 0, 0);

  // First item: "All Displays" (no specific device)
  SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)_T("All Displays"));

  WindowsDisplays displays;
  m_displayInfos = displays.getDisplayInfos();

  for (size_t i = 0; i < m_displayInfos.size(); i++) {
    StringStorage label;
    label.format(_T("Display %d (%dx%d)%s"),
                 m_displayInfos[i].displayNumber,
                 m_displayInfos[i].rect.getWidth(),
                 m_displayInfos[i].rect.getHeight(),
                 m_displayInfos[i].isPrimary ? _T(" *") : _T(""));
    SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)label.getString());
  }

  SendMessage(hCombo, CB_SETCURSEL, 0, 0);
}

void EditPortMappingDialog::populatePermCombos()
{
  HWND combos[] = { m_portPermCombo.getWindow(),
                     m_portGroupPermCombo.getWindow() };
  for (int c = 0; c < 2; c++) {
    SendMessage(combos[c], CB_ADDSTRING, 0, (LPARAM)_T("Full Control"));
    SendMessage(combos[c], CB_ADDSTRING, 0, (LPARAM)_T("View Only"));
    SendMessage(combos[c], CB_ADDSTRING, 0, (LPARAM)_T("View + Clipboard"));
    SendMessage(combos[c], CB_ADDSTRING, 0, (LPARAM)_T("Deny Access"));
    SendMessage(combos[c], CB_SETCURSEL, 0, 0);
  }
}

void EditPortMappingDialog::loadPortAuthUI()
{
  // Load per-port rules from mapping
  m_portRules = m_mapping->getGroupRules();

  bool hasRules = !m_portRules.empty() ||
                  m_mapping->getDefaultPermissions() != ClientPermissions::PERM_FULL_CONTROL;
  m_portAuthEnable.check(hasRules);

  // Set default permission combo
  int permIdx = permissionToComboIndex(m_mapping->getDefaultPermissions());
  SendMessage(m_portPermCombo.getWindow(), CB_SETCURSEL, permIdx, 0);

  // Set display combo selection based on device path
  const StringStorage &devPath = m_mapping->getDevicePath();
  int selIdx = 0; // default: "All Displays"
  if (devPath.getLength() > 0) {
    for (size_t i = 0; i < m_displayInfos.size(); i++) {
      if (_tcsicmp(devPath.getString(),
                   m_displayInfos[i].devicePath.getString()) == 0) {
        selIdx = (int)(i + 1);
        break;
      }
    }
  }
  SendMessage(m_displayCombo.getWindow(), CB_SETCURSEL, selIdx, 0);

  // Setup ListView columns
  m_portGroupList.addColumn(0, _T("Group Name"), 160);
  m_portGroupList.addColumn(1, _T("Permission"), 110);

  refreshPortGroupList();
  enablePortAuthControls(hasRules);
}

void EditPortMappingDialog::savePortAuthData()
{
  if (m_portAuthEnable.isChecked()) {
    int permIdx = (int)SendMessage(m_portPermCombo.getWindow(),
                                    CB_GETCURSEL, 0, 0);
    m_mapping->setDefaultPermissions(comboIndexToPermission(permIdx));
    m_mapping->setGroupRules(m_portRules);
  } else {
    // Not using per-port auth — reset to defaults
    m_mapping->setDefaultPermissions(ClientPermissions::PERM_FULL_CONTROL);
    std::vector<GroupPermissionRule> empty;
    m_mapping->setGroupRules(empty);
  }
}

void EditPortMappingDialog::onPortAuthEnableClick()
{
  enablePortAuthControls(m_portAuthEnable.isChecked());
}

void EditPortMappingDialog::enablePortAuthControls(bool enable)
{
  EnableWindow(m_portPermCombo.getWindow(), enable);
  EnableWindow(m_portGroupList.getWindow(), enable);
  EnableWindow(m_portGroupNameEdit.getWindow(), enable);
  EnableWindow(m_portGroupPermCombo.getWindow(), enable);
  EnableWindow(GetDlgItem(m_ctrlThis.getWindow(), IDC_PORT_ADD_RULE_BUTTON), enable);
  EnableWindow(GetDlgItem(m_ctrlThis.getWindow(), IDC_PORT_REMOVE_RULE_BUTTON), enable);
  EnableWindow(GetDlgItem(m_ctrlThis.getWindow(), IDC_PORT_BROWSE_GROUPS_BUTTON), enable);
}

void EditPortMappingDialog::onPortAddRuleClick()
{
  StringStorage groupName;
  m_portGroupNameEdit.getText(&groupName);
  if (groupName.getLength() == 0) {
    MessageBox(m_ctrlThis.getWindow(),
               _T("Please enter a group name."),
               _T("TightVNC"), MB_OK | MB_ICONWARNING);
    return;
  }

  int permIdx = (int)SendMessage(m_portGroupPermCombo.getWindow(),
                                  CB_GETCURSEL, 0, 0);
  if (permIdx == CB_ERR) permIdx = 0;
  UINT32 permFlags = comboIndexToPermission(permIdx);

  GroupPermissionRule rule(groupName.getString(), permFlags, 0);
  m_portRules.push_back(rule);

  refreshPortGroupList();
  m_portGroupNameEdit.setText(_T(""));
}

void EditPortMappingDialog::onPortRemoveRuleClick()
{
  int sel = m_portGroupList.getSelectedIndex();
  if (sel < 0 || sel >= (int)m_portRules.size()) return;

  m_portRules.erase(m_portRules.begin() + sel);
  refreshPortGroupList();
}

void EditPortMappingDialog::refreshPortGroupList()
{
  m_portGroupList.clear();
  for (size_t i = 0; i < m_portRules.size(); i++) {
    m_portGroupList.addItem((int)i, m_portRules[i].getGroupName().getString());
    m_portGroupList.setSubItemText((int)i, 1,
      permissionToString(m_portRules[i].getPermissionFlags()));
  }
}

bool EditPortMappingDialog::isUserDataValid()
{
  StringStorage rectStringStorage;
  StringStorage portStringStorage;

  m_geometryTextBox.getText(&rectStringStorage);
  m_portTextBox.getText(&portStringStorage);

  if (!PortMappingRect::tryParse(rectStringStorage.getString())) {
    MessageBox(m_ctrlThis.getWindow(),
               StringTable::getString(IDS_INVALID_PORT_MAPPING_STRING),
               StringTable::getString(IDS_CAPTION_BAD_INPUT),
               MB_OK | MB_ICONWARNING);
    m_geometryTextBox.setFocus();
    return false;
  }

  int port;

  StringParser::parseInt(portStringStorage.getString(), &port);

  if ((port < 1) || (port > 65535)) {
    MessageBox(m_ctrlThis.getWindow(),
               StringTable::getString(IDS_PORT_RANGE_ERROR),
               StringTable::getString(IDS_CAPTION_BAD_INPUT),
               MB_OK | MB_ICONWARNING);
    m_portTextBox.setFocus();
    return false;
  }

  PortMappingContainer *extraPorts = Configurator::getInstance()->getServerConfig()->getPortMappingContainer();

  size_t index = extraPorts->findByPort(port);

  if ((index != (size_t)-1) && (extraPorts->at(index) != m_mapping)) {
    MessageBox(m_ctrlThis.getWindow(),
               StringTable::getString(IDS_PORT_ALREADY_IN_USE),
               StringTable::getString(IDS_CAPTION_BAD_INPUT),
               MB_OK | MB_ICONWARNING);
    m_portTextBox.setFocus();
    return false;
  }

  return true;
}

BOOL EditPortMappingDialog::onInitDialog()
{
  initControls();
  populateDisplayCombo();
  populatePermCombos();

  if (m_dialogType == Add) {
    m_portTextBox.setText(_T("5901"));
    m_geometryTextBox.setText(_T("640x480+0+0"));
    enablePortAuthControls(false);
  } else if (m_dialogType == Edit) {
    StringStorage portString;
    StringStorage rectString;

    portString.format(_T("%d"), m_mapping->getPort());
    m_mapping->getRect().toString(&rectString);

    m_portTextBox.setText(portString.getString());
    m_geometryTextBox.setText(rectString.getString());

    loadPortAuthUI();
  }

  return TRUE;
}

BOOL EditPortMappingDialog::onCommand(UINT cID, UINT nID)
{
  switch (cID) {
  case IDOK:
    onOkButtonClick();
    break;
  case IDCANCEL:
    onCancelButtonClick();
    break;
  case IDC_PORT_AUTH_ENABLE:
    onPortAuthEnableClick();
    break;
  case IDC_PORT_ADD_RULE_BUTTON:
    onPortAddRuleClick();
    break;
  case IDC_PORT_REMOVE_RULE_BUTTON:
    onPortRemoveRuleClick();
    break;
  case IDC_PORT_BROWSE_GROUPS_BUTTON:
    {
      // Enumerate local groups
      LPBYTE buf = NULL;
      DWORD entriesRead = 0, totalEntries = 0;
      NET_API_STATUS status = NetLocalGroupEnum(
        NULL, 0, &buf, MAX_PREFERRED_LENGTH,
        &entriesRead, &totalEntries, NULL);
      if (status == NERR_Success && buf != NULL) {
        StringStorage groupList;
        groupList.setString(_T("Local Groups:\n\n"));
        LOCALGROUP_INFO_0 *info = (LOCALGROUP_INFO_0 *)buf;
        for (DWORD i = 0; i < entriesRead; i++) {
          StringStorage combined;
          combined.format(_T("%s%s\n"), groupList.getString(), info[i].lgrpi0_name);
          groupList.setString(combined.getString());
        }
        MessageBox(m_ctrlThis.getWindow(), groupList.getString(),
                   _T("Available Groups"), MB_OK | MB_ICONINFORMATION);
        NetApiBufferFree(buf);
      }
    }
    break;
  }
  return TRUE;
}

const TCHAR *EditPortMappingDialog::permissionToString(UINT32 flags)
{
  if (flags & ClientPermissions::PERM_DENY)
    return _T("Deny Access");
  if (flags == ClientPermissions::PERM_VIEW_ONLY)
    return _T("View Only");
  if (flags == (ClientPermissions::PERM_VIEW_ONLY | ClientPermissions::PERM_CLIPBOARD))
    return _T("View + Clipboard");
  return _T("Full Control");
}

UINT32 EditPortMappingDialog::comboIndexToPermission(int index)
{
  switch (index) {
  case 0: return ClientPermissions::PERM_FULL_CONTROL;
  case 1: return ClientPermissions::PERM_VIEW_ONLY;
  case 2: return ClientPermissions::PERM_VIEW_ONLY | ClientPermissions::PERM_CLIPBOARD;
  case 3: return ClientPermissions::PERM_DENY;
  default: return ClientPermissions::PERM_FULL_CONTROL;
  }
}

int EditPortMappingDialog::permissionToComboIndex(UINT32 flags)
{
  if (flags & ClientPermissions::PERM_DENY) return 3;
  if (flags == ClientPermissions::PERM_VIEW_ONLY) return 1;
  if (flags == (ClientPermissions::PERM_VIEW_ONLY | ClientPermissions::PERM_CLIPBOARD))
    return 2;
  return 0;
}
