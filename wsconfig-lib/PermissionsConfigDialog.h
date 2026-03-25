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

#ifndef _PERMISSIONS_CONFIG_DIALOG_H_
#define _PERMISSIONS_CONFIG_DIALOG_H_

#include "gui/BaseDialog.h"
#include "gui/ListView.h"
#include "gui/SpinControl.h"
#include "gui/TextBox.h"

#include "server-config-lib/Configurator.h"
#include "server-config-lib/GroupPermissionRule.h"

#include <vector>

// Standalone permissions / group-rules dialog (Phase 5 redesign).
// Manages the full ListView-based group permission rules list with
// add / edit / remove / move-up / move-down / browse operations.
// Maps to IDD_CONFIG_PERMISSIONS_PAGE (140).
class PermissionsConfigDialog : public BaseDialog
{
public:
  PermissionsConfigDialog();
  virtual ~PermissionsConfigDialog();

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

  // Rule CRUD handlers
  void onAddRuleClick();
  void onEditRuleClick();
  void onRemoveRuleClick();
  void onMoveUpRuleClick();
  void onMoveDownRuleClick();
  void onBrowseGroupsClick();
  void onListViewSelChange();

  // Refreshes the entire ListView from m_rules vector
  void refreshGroupList();

  // Updates a single ListView row from its rule
  void setListViewItemText(int index, const GroupPermissionRule &rule);

  // Enables/disables edit/remove/move buttons based on current selection
  void updateButtonsState();

  // Shows the native Windows "Select Users or Groups" object picker.
  // multiSelect=true allows picking multiple objects at once.
  // Returns true if at least one item was selected.
  bool showObjectPicker(bool multiSelect,
                        std::vector<StringStorage> *selectedNames);

  // Permission <-> combo index conversion helpers
  static const TCHAR *permissionToString(UINT32 flags);
  static UINT32       comboIndexToPermission(int index);
  static int          permissionToComboIndex(UINT32 flags);

protected:
  ServerConfig *m_config;
  std::vector<GroupPermissionRule> m_rules; // local editable copy

  // Controls
  ListView    m_ruleList;
  TextBox     m_groupNameEdit;
  Control     m_permissionCombo;
  TextBox     m_priorityEdit;
  SpinControl m_prioritySpin;
  Control     m_addButton;
  Control     m_editButton;
  Control     m_removeButton;
  Control     m_moveUpButton;
  Control     m_moveDownButton;
  Control     m_browseButton;

  BaseDialog *m_parentDialog;
};

#endif // _PERMISSIONS_CONFIG_DIALOG_H_
