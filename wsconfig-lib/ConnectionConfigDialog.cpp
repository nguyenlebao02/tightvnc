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
#include "ConnectionConfigDialog.h"
#include "EditPortMappingDialog.h"
#include "ConfigDialog.h"
#include "CommonInputValidation.h"
#include "UIDataAccess.h"
#include "server-config-lib/Configurator.h"
#include "util/StringParser.h"
#include "win-system/WindowsDisplays.h"

// Resolve a devicePath to a human-readable monitor label.
// Returns e.g. "Display 1 (1920x1080)" or "All Displays" if empty.
static void resolveMonitorLabel(const TCHAR *devicePath,
                                StringStorage *label)
{
  if (devicePath == NULL || devicePath[0] == _T('\0')) {
    label->setString(_T("All Displays"));
    return;
  }

  WindowsDisplays displays;
  std::vector<DisplayInfo> infos = displays.getDisplayInfos();

  for (size_t i = 0; i < infos.size(); i++) {
    if (_tcsicmp(devicePath, infos[i].devicePath.getString()) == 0) {
      label->format(_T("Display %d (%dx%d)"),
                    infos[i].displayNumber,
                    infos[i].rect.getWidth(),
                    infos[i].rect.getHeight());
      return;
    }
  }
  // Fallback: device not currently connected
  label->format(_T("[%s]"), devicePath);
}

// Build a human-readable display string for a port mapping entry.
// Format: ":port  Monitor Label"
static void buildDisplayString(const PortMapping *pm, StringStorage *out)
{
  StringStorage monLabel;
  resolveMonitorLabel(pm->getDevicePath().getString(), &monLabel);

  out->format(_T(":%d  %s"), pm->getPort(), monLabel.getString());
}

ConnectionConfigDialog::ConnectionConfigDialog()
: BaseDialog(IDD_CONFIG_CONNECTION_PAGE),
  m_parent(NULL),
  m_extraPorts(NULL),
  m_portConfigs(NULL)
{
}

ConnectionConfigDialog::~ConnectionConfigDialog()
{
}

void ConnectionConfigDialog::setParentDialog(BaseDialog *dialog)
{
  m_parent = dialog;
}

BOOL ConnectionConfigDialog::onInitDialog()
{
  initControls();
  updateUI();
  return TRUE;
}

void ConnectionConfigDialog::initControls()
{
  HWND hwnd = m_ctrlThis.getWindow();

  // Port mappings controls
  m_mappingsListBox.setWindow(GetDlgItem(hwnd, IDC_MAPPINGS));
  m_editButton.setWindow(GetDlgItem(hwnd, IDC_EDIT_PORT));
  m_removeButton.setWindow(GetDlgItem(hwnd, IDC_REMOVE_PORT));
}

// Build a display string for a PortConfig entry in the listbox.
// Format: ":port  Monitor Label"
static void buildPortConfigDisplayString(const PortConfig *pc, StringStorage *out)
{
  StringStorage monLabel;
  resolveMonitorLabel(pc->getDevicePath().getString(), &monLabel);

  out->format(_T(":%d  %s"), pc->getPort(), monLabel.getString());
}

void ConnectionConfigDialog::updateUI()
{
  ServerConfig *config = Configurator::getInstance()->getServerConfig();

  // Load port mappings into listbox — prefer unified PortConfig list
  m_mappingsListBox.clear();

  if (m_portConfigs != NULL && !m_portConfigs->empty()) {
    StringStorage mappingString;
    for (size_t i = 0; i < m_portConfigs->size(); i++) {
      buildPortConfigDisplayString(&(*m_portConfigs)[i], &mappingString);
      m_mappingsListBox.insertString((int)i, mappingString.getString());
    }
  } else {
    // Fallback: legacy extra ports
    m_extraPorts = config->getPortMappingContainer();
    StringStorage mappingString;
    for (size_t i = 0; i < m_extraPorts->count(); i++) {
      buildDisplayString(m_extraPorts->at(i), &mappingString);
      _ASSERT((int)i == i);
      m_mappingsListBox.insertString((int)i, mappingString.getString());
    }
  }
}

void ConnectionConfigDialog::apply()
{
  ServerConfig *config = Configurator::getInstance()->getServerConfig();

  // Always accept RFB connections
  config->acceptRfbConnections(true);
  // Disable HTTP by default (no UI for it)
  config->acceptHttpConnections(false);

  // Save unified port configs to ServerConfig.
  // ServerConfig::setAllPortConfigs syncs RFB port from the first entry.
  if (m_portConfigs != NULL) {
    config->setAllPortConfigs(*m_portConfigs);
  }
}

bool ConnectionConfigDialog::validateInput()
{
  // No inputs to validate — port mappings are validated in EditPortMappingDialog.
  return true;
}

BOOL ConnectionConfigDialog::onCommand(UINT controlID, UINT notificationID)
{
  if (notificationID == BN_CLICKED) {
    switch (controlID) {
    case IDC_ADD_PORT:
      onAddButtonClick();
      break;
    case IDC_EDIT_PORT:
      onEditButtonClick();
      break;
    case IDC_REMOVE_PORT:
      onRemoveButtonClick();
      break;
    }
  } else if (controlID == IDC_MAPPINGS) {
    switch (notificationID) {
    case LBN_SELCHANGE:
      onMappingsSelChange();
      break;
    case LBN_DBLCLK:
      onMappingsDoubleClick();
      break;
    }
  }
  return TRUE;
}

void ConnectionConfigDialog::onMappingsSelChange()
{
  int sel = m_mappingsListBox.getSelectedIndex();
  m_editButton.setEnabled(sel >= 0);
  m_removeButton.setEnabled(sel >= 0);
}

void ConnectionConfigDialog::onMappingsDoubleClick()
{
  if (m_mappingsListBox.getSelectedIndex() != -1) {
    onEditButtonClick();
  }
}

void ConnectionConfigDialog::onAddButtonClick()
{
  EditPortMappingDialog addDialog(EditPortMappingDialog::Add);
  PortMapping newPM;

  addDialog.setMapping(&newPM);
  addDialog.setParent(&m_ctrlThis);

  if (addDialog.showModal() == IDOK) {
    if (m_portConfigs != NULL) {
      // Create new PortConfig from the mapping
      PortConfig newPC;
      newPC.fromPortMapping(newPM);
      // Inherit auth from first port if available
      if (!m_portConfigs->empty()) {
        PortConfig &first = (*m_portConfigs)[0];
        newPC.setDefaultWinAuthPermissions(first.getDefaultWinAuthPermissions());
        newPC.setGroupRules(first.getGroupRules());
        newPC.setMaxConnectionsPerUser(first.getMaxConnectionsPerUser());
      }
      m_portConfigs->push_back(newPC);
      StringStorage mappingString;
      buildPortConfigDisplayString(&newPC, &mappingString);
      m_mappingsListBox.addString(mappingString.getString());
    } else {
      StringStorage mappingString;
      buildDisplayString(&newPM, &mappingString);
      m_mappingsListBox.addString(mappingString.getString());
      m_extraPorts->pushBack(newPM);
    }
    ((ConfigDialog *)m_parent)->updateApplyButtonState();
    // Refresh port selector in parent dialog
    ((ConfigDialog *)m_parent)->refreshPortSelector();
  }
}

void ConnectionConfigDialog::onEditButtonClick()
{
  int sel = m_mappingsListBox.getSelectedIndex();
  if (sel == -1) {
    return;
  }

  if (m_portConfigs != NULL && sel < (int)m_portConfigs->size()) {
    PortMapping pm = (*m_portConfigs)[sel].toPortMapping();

    EditPortMappingDialog editDialog(EditPortMappingDialog::Edit);
    editDialog.setParent(&m_ctrlThis);
    editDialog.setMapping(&pm);

    if (editDialog.showModal() == IDOK) {
      (*m_portConfigs)[sel].fromPortMapping(pm);
      StringStorage mappingString;
      buildPortConfigDisplayString(&(*m_portConfigs)[sel], &mappingString);
      m_mappingsListBox.setItemText(sel, mappingString.getString());
      ((ConfigDialog *)m_parent)->updateApplyButtonState();
      ((ConfigDialog *)m_parent)->refreshPortSelector();
    }
  } else {
    PortMapping *pPM = m_extraPorts->at(sel);

    EditPortMappingDialog editDialog(EditPortMappingDialog::Edit);
    editDialog.setParent(&m_ctrlThis);
    editDialog.setMapping(pPM);

    if (editDialog.showModal() == IDOK) {
      StringStorage mappingString;
      buildDisplayString(pPM, &mappingString);
      m_mappingsListBox.setItemText(sel, mappingString.getString());
      ((ConfigDialog *)m_parent)->updateApplyButtonState();
    }
  }
}

void ConnectionConfigDialog::onRemoveButtonClick()
{
  int sel = m_mappingsListBox.getSelectedIndex();
  if (sel == -1) {
    return;
  }

  if (m_portConfigs != NULL && sel < (int)m_portConfigs->size()) {
    m_portConfigs->erase(m_portConfigs->begin() + sel);
  } else {
    m_extraPorts->remove(sel);
  }

  m_mappingsListBox.removeString(sel);
  ((ConfigDialog *)m_parent)->updateApplyButtonState();
  ((ConfigDialog *)m_parent)->refreshPortSelector();

  // Restore a reasonable selection after removal
  if (m_mappingsListBox.getCount() > 0) {
    m_mappingsListBox.setSelectedIndex(sel);
    if (m_mappingsListBox.getSelectedIndex() == -1) {
      m_mappingsListBox.setSelectedIndex(sel - 1);
    }
    if (m_mappingsListBox.getSelectedIndex() == -1) {
      m_mappingsListBox.setSelectedIndex(sel + 1);
    }
  } else {
    m_editButton.setEnabled(false);
    m_removeButton.setEnabled(false);
  }
}
