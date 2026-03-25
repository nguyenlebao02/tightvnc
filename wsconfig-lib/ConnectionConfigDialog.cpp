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

// Build a human-readable display string for a port mapping entry.
// Format: ":port  rect  [device]" or ":port  rect"
static void buildDisplayString(const PortMapping *pm, StringStorage *out)
{
  StringStorage rectStr;
  pm->getRect().toString(&rectStr);

  const StringStorage &devPath = pm->getDevicePath();
  bool hasDevice = (devPath.getLength() > 0);

  if (hasDevice) {
    out->format(_T(":%d  %s  [%s]"),
                pm->getPort(), rectStr.getString(), devPath.getString());
  } else {
    out->format(_T(":%d  %s"),
                pm->getPort(), rectStr.getString());
  }
}

ConnectionConfigDialog::ConnectionConfigDialog()
: BaseDialog(IDD_CONFIG_CONNECTION_PAGE),
  m_parent(NULL),
  m_extraPorts(NULL)
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

  // RFB port controls
  m_acceptRfbConnections.setWindow(GetDlgItem(hwnd, IDC_ACCEPT_RFB_CONNECTIONS));
  m_rfbPort.setWindow(GetDlgItem(hwnd, IDC_RFB_PORT));
  m_rfbPortSpin.setWindow(GetDlgItem(hwnd, IDC_RFB_PORT_SPIN));

  m_rfbPortSpin.setBuddy(&m_rfbPort);
  m_rfbPortSpin.setAccel(0, 1);
  m_rfbPortSpin.setRange32(1, 65535);

  // HTTP port controls
  m_acceptHttpConnections.setWindow(GetDlgItem(hwnd, IDC_ACCEPT_HTTP_CONNECTIONS));
  m_httpPort.setWindow(GetDlgItem(hwnd, IDC_HTTP_PORT));
  m_httpPortSpin.setWindow(GetDlgItem(hwnd, IDC_HTTP_PORT_SPIN));

  m_httpPortSpin.setBuddy(&m_httpPort);
  m_httpPortSpin.setAccel(0, 1);
  m_httpPortSpin.setRange32(1, 65535);

  // Extra port mappings controls
  m_mappingsListBox.setWindow(GetDlgItem(hwnd, IDC_MAPPINGS));
  m_editButton.setWindow(GetDlgItem(hwnd, IDC_EDIT_PORT));
  m_removeButton.setWindow(GetDlgItem(hwnd, IDC_REMOVE_PORT));
}

void ConnectionConfigDialog::updateUI()
{
  ServerConfig *config = Configurator::getInstance()->getServerConfig();

  // Load RFB settings
  m_acceptRfbConnections.check(config->isAcceptingRfbConnections());
  m_rfbPort.setSignedInt(config->getRfbPort());

  // Load HTTP settings
  m_acceptHttpConnections.check(config->isAcceptingHttpConnections());
  m_httpPort.setSignedInt(config->getHttpPort());

  // Load extra port mappings into listbox
  m_extraPorts = config->getPortMappingContainer();
  m_mappingsListBox.clear();

  StringStorage mappingString;
  for (size_t i = 0; i < m_extraPorts->count(); i++) {
    buildDisplayString(m_extraPorts->at(i), &mappingString);
    _ASSERT((int)i == i);
    m_mappingsListBox.insertString((int)i, mappingString.getString());
  }

  // Sync enabled state of dependent controls
  updatePortDependencies();

  m_rfbPortSpin.invalidate();
  m_httpPortSpin.invalidate();
}

void ConnectionConfigDialog::apply()
{
  ServerConfig *config = Configurator::getInstance()->getServerConfig();

  // Save RFB settings
  config->acceptRfbConnections(m_acceptRfbConnections.isChecked());

  StringStorage rfbPortText;
  m_rfbPort.getText(&rfbPortText);
  int rfbPort = 0;
  StringParser::parseInt(rfbPortText.getString(), &rfbPort);
  config->setRfbPort(rfbPort);

  // Save HTTP settings
  config->acceptHttpConnections(m_acceptHttpConnections.isChecked());

  StringStorage httpPortText;
  m_httpPort.getText(&httpPortText);
  int httpPort = 0;
  StringParser::parseInt(httpPortText.getString(), &httpPort);
  config->setHttpPort(httpPort);

  // Extra ports are saved in-place through the PortMappingContainer pointer;
  // add/edit/remove operations already mutated m_extraPorts directly.
}

bool ConnectionConfigDialog::validateInput()
{
  if (!CommonInputValidation::validatePort(&m_rfbPort)) {
    return false;
  }
  if (!CommonInputValidation::validatePort(&m_httpPort)) {
    return false;
  }

  // Ports must differ when both are active
  if (m_acceptHttpConnections.isChecked() && m_acceptHttpConnections.isEnabled()) {
    int rfbPort = 0, httpPort = 0;
    UIDataAccess::queryValueAsInt(&m_rfbPort, &rfbPort);
    UIDataAccess::queryValueAsInt(&m_httpPort, &httpPort);
    if (rfbPort == httpPort) {
      CommonInputValidation::notifyValidationError(
        &m_httpPort,
        StringTable::getString(IDS_HTTP_RFB_PORTS_ARE_EQUAL));
      return false;
    }
  }

  return true;
}

BOOL ConnectionConfigDialog::onCommand(UINT controlID, UINT notificationID)
{
  if (notificationID == BN_CLICKED) {
    switch (controlID) {
    case IDC_ACCEPT_RFB_CONNECTIONS:
      onAcceptRfbConnectionsClick();
      break;
    case IDC_ACCEPT_HTTP_CONNECTIONS:
      onAcceptHttpConnectionsClick();
      break;
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
  } else if (notificationID == EN_UPDATE) {
    switch (controlID) {
    case IDC_RFB_PORT:
      onRfbPortUpdate();
      break;
    case IDC_HTTP_PORT:
      onHttpPortUpdate();
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

//
// Enable/disable port edit fields based on checkbox state.
//
void ConnectionConfigDialog::updatePortDependencies()
{
  bool rfbEnabled = m_acceptRfbConnections.isChecked();
  m_rfbPort.setEnabled(rfbEnabled);
  m_rfbPortSpin.invalidate();

  // HTTP checkbox itself is only meaningful when RFB is on
  m_acceptHttpConnections.setEnabled(rfbEnabled);

  bool httpEnabled = rfbEnabled && m_acceptHttpConnections.isChecked();
  m_httpPort.setEnabled(httpEnabled);
  m_httpPortSpin.invalidate();
}

void ConnectionConfigDialog::onAcceptRfbConnectionsClick()
{
  updatePortDependencies();
  ((ConfigDialog *)m_parent)->updateApplyButtonState();
}

void ConnectionConfigDialog::onAcceptHttpConnectionsClick()
{
  updatePortDependencies();
  ((ConfigDialog *)m_parent)->updateApplyButtonState();
}

void ConnectionConfigDialog::onRfbPortUpdate()
{
  ((ConfigDialog *)m_parent)->updateApplyButtonState();
}

void ConnectionConfigDialog::onHttpPortUpdate()
{
  ((ConfigDialog *)m_parent)->updateApplyButtonState();
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
    StringStorage mappingString;
    buildDisplayString(&newPM, &mappingString);
    m_mappingsListBox.addString(mappingString.getString());
    m_extraPorts->pushBack(newPM);
    ((ConfigDialog *)m_parent)->updateApplyButtonState();
  }
}

void ConnectionConfigDialog::onEditButtonClick()
{
  int sel = m_mappingsListBox.getSelectedIndex();
  if (sel == -1) {
    return;
  }

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

void ConnectionConfigDialog::onRemoveButtonClick()
{
  int sel = m_mappingsListBox.getSelectedIndex();
  if (sel == -1) {
    return;
  }

  m_mappingsListBox.removeString(sel);
  m_extraPorts->remove(sel);
  ((ConfigDialog *)m_parent)->updateApplyButtonState();

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
