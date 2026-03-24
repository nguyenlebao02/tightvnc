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

#ifndef _WIN_AUTH_CONFIG_DIALOG_H_
#define _WIN_AUTH_CONFIG_DIALOG_H_

#include "gui/BaseDialog.h"
#include "gui/ListView.h"
#include "gui/SpinControl.h"
#include "gui/CheckBox.h"
#include "gui/TextBox.h"
#include "gui/ComboBox.h"

#include "server-config-lib/Configurator.h"
#include "server-config-lib/GroupPermissionRule.h"

// Dialog for configuring Windows user/group authentication and
// per-group permission rules. Appears as a tab in the server config.
class WinAuthConfigDialog : public BaseDialog
{
public:
  WinAuthConfigDialog();
  virtual ~WinAuthConfigDialog();

  void setParentDialog(BaseDialog *dialog);

  // BaseDialog overrides
  virtual BOOL onInitDialog();
  virtual BOOL onCommand(UINT controlID, UINT notificationID);
  virtual BOOL onNotify(UINT controlID, LPARAM data);
  virtual BOOL onDestroy() { return TRUE; }

  bool validateInput();
  void updateUI();
  void apply();

private:
  void initControls();

  // Control event handlers
  void onEnableCheckClick();
  void onAuthModeChange();
  void onAddRuleClick();
  void onEditRuleClick();
  void onRemoveRuleClick();
  void onMoveUpRuleClick();
  void onMoveDownRuleClick();
  void onBrowseGroupsClick();
  void onDefaultPermChange();
  void onListViewSelChange();

  // Shows the native Windows "Select Users or Groups" object picker dialog.
  // Returns true if user selected at least one item.
  bool showObjectPicker(bool multiSelect,
                        std::vector<StringStorage> *selectedNames);

  // Helpers
  void updateButtonsState();
  void setListViewItemText(int index, const GroupPermissionRule &rule);
  void enableGroupControls(bool enable);

  static const TCHAR *permissionToString(UINT32 flags);
  static UINT32 comboIndexToPermission(int index);
  static int permissionToComboIndex(UINT32 flags);

  // Configuration
  ServerConfig *m_config;
  std::vector<GroupPermissionRule> m_rules; // Local copy for editing

  // Controls
  CheckBox m_enableCheck;
  Control m_authModeCombo;
  ListView m_ruleList;
  TextBox m_groupNameEdit;
  Control m_permissionCombo;
  TextBox m_priorityEdit;
  SpinControl m_prioritySpin;
  Control m_addButton;
  Control m_editButton;
  Control m_removeButton;
  Control m_moveUpButton;
  Control m_moveDownButton;
  Control m_browseButton;
  Control m_defaultPermCombo;

  BaseDialog *m_parentDialog;
};

#endif // _WIN_AUTH_CONFIG_DIALOG_H_
