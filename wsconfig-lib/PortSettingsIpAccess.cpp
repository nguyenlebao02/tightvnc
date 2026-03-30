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

// PortSettingsConfigDialog — IP access control methods.
// Split from PortSettingsConfigDialog.cpp for file size management.

#include "tvnserver/resource.h"
#include "PortSettingsConfigDialog.h"
#include "ConfigDialog.h"
#include "EditIpAccessRuleDialog.h"
#include "server-config-lib/IpAccessControl.h"
#include "util/StringTable.h"
#include "util/AnsiStringStorage.h"

void PortSettingsConfigDialog::refreshIpList()
{
  m_ipList.clear();
  if (m_portConfig == NULL) return;

  IpAccessControl *container = m_portConfig->getIpAccessControl();
  for (size_t i = 0; i < container->size(); i++) {
    IpAccessRule *rule = container->at(i);
    m_ipList.addItem(m_ipList.getCount(), _T(""), (LPARAM)rule);
    setIpListViewItemText((int)i, rule);
  }
}

void PortSettingsConfigDialog::updateIpButtonsState()
{
  int si = m_ipList.getSelectedIndex();
  if (si == -1) {
    m_ipEditButton.setEnabled(false);
    m_ipRemoveButton.setEnabled(false);
    m_ipMoveUpButton.setEnabled(false);
    m_ipMoveDownButton.setEnabled(false);
  } else {
    m_ipEditButton.setEnabled(true);
    m_ipRemoveButton.setEnabled(true);
    m_ipMoveUpButton.setEnabled(si > 0);
    m_ipMoveDownButton.setEnabled(si < m_ipList.getCount() - 1);
  }
}

void PortSettingsConfigDialog::onIpAddClick()
{
  if (m_portConfig == NULL) return;
  IpAccessControl *container = m_portConfig->getIpAccessControl();

  IpAccessRule *rule = new IpAccessRule();
  EditIpAccessRuleDialog editDlg;
  editDlg.setParent(&m_ctrlThis);
  editDlg.setIpAccessControl(rule);
  editDlg.setEditFlag(false);

  if (editDlg.showModal() == IDOK) {
    container->push_back(rule);
    m_ipList.addItem(m_ipList.getCount(), _T(""), (LPARAM)rule);
    setIpListViewItemText(m_ipList.getCount() - 1, rule);
    updateIpButtonsState();
    onIpCheckClick();
    if (m_parentDialog != NULL)
      ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  } else {
    delete rule;
  }
}

void PortSettingsConfigDialog::onIpEditClick()
{
  int si = m_ipList.getSelectedIndex();
  if (si == -1) return;

  IpAccessRule *rule = (IpAccessRule *)m_ipList.getItemData(si);
  EditIpAccessRuleDialog editDlg;
  editDlg.setParent(&m_ctrlThis);
  editDlg.setIpAccessControl(rule);
  editDlg.setEditFlag(true);

  if (editDlg.showModal() == IDOK) {
    setIpListViewItemText(si, rule);
    updateIpButtonsState();
    onIpCheckClick();
    if (m_parentDialog != NULL)
      ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void PortSettingsConfigDialog::onIpRemoveClick()
{
  if (m_portConfig == NULL) return;
  int si = m_ipList.getSelectedIndex();
  if (si == -1) return;

  IpAccessControl *container = m_portConfig->getIpAccessControl();
  IpAccessRule *rule = (IpAccessRule *)m_ipList.getItemData(si);

  for (IpAccessControl::iterator it = container->begin();
       it != container->end(); it++) {
    if (*it == rule) {
      container->erase(it);
      m_ipList.removeItem(si);
      updateIpButtonsState();
      onIpCheckClick();
      if (m_parentDialog != NULL)
        ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
      break;
    }
  }
  delete rule;

  m_ipList.selectItem(si);
  if (m_ipList.getSelectedIndex() == -1)
    m_ipList.selectItem(si - 1);
}

void PortSettingsConfigDialog::onIpMoveUpClick()
{
  int si = m_ipList.getSelectedIndex();
  if (si <= 0) return;

  IpAccessRule *rule     = (IpAccessRule *)m_ipList.getItemData(si);
  IpAccessRule *rulePrev = (IpAccessRule *)m_ipList.getItemData(si - 1);

  setIpListViewItemText(si - 1, rule);
  setIpListViewItemText(si, rulePrev);
  m_ipList.selectItem(si - 1);
  onIpCheckClick();
  if (m_parentDialog != NULL)
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
}

void PortSettingsConfigDialog::onIpMoveDownClick()
{
  int si = m_ipList.getSelectedIndex();
  if (si == -1 || si >= m_ipList.getCount() - 1) return;

  IpAccessRule *rule     = (IpAccessRule *)m_ipList.getItemData(si);
  IpAccessRule *ruleNext = (IpAccessRule *)m_ipList.getItemData(si + 1);

  setIpListViewItemText(si, ruleNext);
  setIpListViewItemText(si + 1, rule);
  m_ipList.selectItem(si + 1);
  onIpCheckClick();
  if (m_parentDialog != NULL)
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
}

void PortSettingsConfigDialog::onIpListSelChange()
{
  updateIpButtonsState();
}

void PortSettingsConfigDialog::onIpCheckClick()
{
  StringStorage ipStorage;
  m_ipCheckEdit.getText(&ipStorage);

  if (!IpAccessRule::isIpAddressStringValid(ipStorage.getString())) {
    if (ipStorage.isEmpty()) {
      m_ipCheckResult.setText(StringTable::getString(IDS_ENTER_IP_HINT));
    } else {
      m_ipCheckResult.setText(StringTable::getString(IDS_BAD_IP_HINT));
    }
    return;
  }

  AnsiStringStorage ansiIp(&ipStorage);
  unsigned int addr = inet_addr(ansiIp.getString());

  IpAccessRule::ActionType action = IpAccessRule::ACTION_TYPE_ALLOW;
  for (int i = 0; i < m_ipList.getCount(); i++) {
    IpAccessRule *rule = (IpAccessRule *)m_ipList.getItemData(i);
    if (rule->isIncludingAddress(addr)) {
      action = rule->getAction();
      break;
    }
  }

  StringStorage desc;
  switch (action) {
  case IpAccessRule::ACTION_TYPE_ALLOW:
    desc.setString(StringTable::getString(IDS_ACTION_ACCEPT_HINT));
    break;
  case IpAccessRule::ACTION_TYPE_DENY:
    desc.setString(StringTable::getString(IDS_ACTION_REJECT_HINT));
    break;
  case IpAccessRule::ACTION_TYPE_QUERY:
    desc.setString(StringTable::getString(IDS_ACTION_QUERY_HINT));
    break;
  default:
    desc.setString(StringTable::getString(IDS_ACTION_UNDEF_HINT));
    break;
  }
  m_ipCheckResult.setText(desc.getString());
}

void PortSettingsConfigDialog::setIpListViewItemText(
  int index, IpAccessRule *rule)
{
  StringStorage firstIp, lastIp;
  rule->getFirstIp(&firstIp);
  rule->getLastIp(&lastIp);

  m_ipList.setSubItemText(index, 0, firstIp.getString());
  m_ipList.setSubItemText(index, 1, lastIp.getString());

  switch (rule->getAction()) {
  case IpAccessRule::ACTION_TYPE_ALLOW:
    m_ipList.setSubItemText(index, 2, StringTable::getString(IDS_ACTION_ACCEPT));
    break;
  case IpAccessRule::ACTION_TYPE_DENY:
    m_ipList.setSubItemText(index, 2, StringTable::getString(IDS_ACTION_DENY));
    break;
  case IpAccessRule::ACTION_TYPE_QUERY:
    m_ipList.setSubItemText(index, 2, StringTable::getString(IDS_ACTION_QUERY));
    break;
  }
  m_ipList.setItemData(index, (LPARAM)rule);
}
