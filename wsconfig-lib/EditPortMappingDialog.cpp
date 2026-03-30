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

  int port;

  StringStorage portStringStorage;
  m_portTextBox.getText(&portStringStorage);
  StringParser::parseInt(portStringStorage.getString(), &port);

  m_mapping->setPort(port);

  // Determine rect and devicePath from display combo selection
  int displaySel = (int)SendMessage(m_displayCombo.getWindow(),
                                     CB_GETCURSEL, 0, 0);
  if (displaySel > 0 && displaySel <= (int)m_displayInfos.size()) {
    // Specific monitor — use its rect
    const DisplayInfo &di = m_displayInfos[displaySel - 1];
    PortMappingRect rect(di.rect.left, di.rect.top,
                         di.rect.right, di.rect.bottom);
    m_mapping->setRect(rect);
    m_mapping->setDevicePath(di.devicePath.getString());
  } else {
    // "All Displays" — compute bounding rect of all monitors
    PortMappingRect rect;
    if (!m_displayInfos.empty()) {
      int minL = m_displayInfos[0].rect.left;
      int minT = m_displayInfos[0].rect.top;
      int maxR = m_displayInfos[0].rect.right;
      int maxB = m_displayInfos[0].rect.bottom;
      for (size_t i = 1; i < m_displayInfos.size(); i++) {
        if (m_displayInfos[i].rect.left < minL) minL = m_displayInfos[i].rect.left;
        if (m_displayInfos[i].rect.top < minT) minT = m_displayInfos[i].rect.top;
        if (m_displayInfos[i].rect.right > maxR) maxR = m_displayInfos[i].rect.right;
        if (m_displayInfos[i].rect.bottom > maxB) maxB = m_displayInfos[i].rect.bottom;
      }
      rect = PortMappingRect(minL, minT, maxR, maxB);
    }
    m_mapping->setRect(rect);
    m_mapping->setDevicePath(_T(""));
  }

  kill(IDOK);
}

void EditPortMappingDialog::initControls()
{
  HWND dialogHwnd = m_ctrlThis.getWindow();
  m_portTextBox.setWindow(GetDlgItem(dialogHwnd, IDC_PORT_EDIT));
  m_displayCombo.setWindow(GetDlgItem(dialogHwnd, IDC_DISPLAY_COMBO));
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

bool EditPortMappingDialog::isUserDataValid()
{
  StringStorage portStringStorage;
  m_portTextBox.getText(&portStringStorage);

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

  if (m_dialogType == Add) {
    m_portTextBox.setText(_T("5901"));
    // Display combo defaults to "All Displays" (index 0)
  } else if (m_dialogType == Edit) {
    StringStorage portString;
    portString.format(_T("%d"), m_mapping->getPort());
    m_portTextBox.setText(portString.getString());

    // Select correct display in combo from devicePath
    const StringStorage &devPath = m_mapping->getDevicePath();
    int selIdx = 0;
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
  }
  return TRUE;
}
