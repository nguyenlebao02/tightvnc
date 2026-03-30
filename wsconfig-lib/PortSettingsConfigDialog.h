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

#ifndef _PORT_SETTINGS_CONFIG_DIALOG_H_
#define _PORT_SETTINGS_CONFIG_DIALOG_H_

#include "gui/BaseDialog.h"
#include "gui/CheckBox.h"
#include "gui/ComboBox.h"
#include "gui/ListView.h"
#include "gui/SpinControl.h"
#include "gui/TextBox.h"

#include "server-config-lib/ServerConfig.h"
#include "server-config-lib/PortConfig.h"
#include "server-config-lib/GroupPermissionRule.h"
#include "PasswordControl.h"  // Still needed for control interface auth

#include <vector>

// Consolidated per-port settings dialog (Port Settings tab).
// Merges: Windows auth, group permission rules,
//         IP access control, and control interface auth.
// Uses IDD_CONFIG_PORT_SETTINGS_PAGE (143).
class PortSettingsConfigDialog : public BaseDialog
{
public:
  PortSettingsConfigDialog();
  virtual ~PortSettingsConfigDialog();

  void setParentDialog(BaseDialog *dialog);
  void setPortConfig(PortConfig *pc) { m_portConfig = pc; }

  // Refresh the port selector combo with the given port list.
  void refreshPortSelector(const std::vector<PortConfig> &ports,
                           int selectedIndex);

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
  void updateControlDependencies();

  // Port selector
  static const UINT IDC_PORT_SELECTOR_COMBO = 1200;
  static const UINT IDC_PORT_SELECTOR_LABEL = 1201;
  void onPortSelectorChange();

  // Windows auth handlers
  void onDefaultPermChange();

  // Group permission rule handlers
  void onAddRuleClick();
  void onEditRuleClick();
  void onRemoveRuleClick();
  void onMoveUpRuleClick();
  void onMoveDownRuleClick();
  void onBrowseGroupsClick();
  void onListViewSelChange();
  void refreshGroupList();
  void setListViewItemText(int index, const GroupPermissionRule &rule);
  void updateRuleButtonsState();

  // Windows object picker (shared with PermissionsConfigDialog)
  bool showObjectPicker(bool multiSelect,
                        std::vector<StringStorage> *selectedNames);

  // Permission helpers
  static const TCHAR *permissionToString(UINT32 flags);
  static UINT32 comboIndexToPermission(int index);
  static int permissionToComboIndex(UINT32 flags);

  // Control interface auth handlers
  void onUseControlAuthClick();
  void onRepeatControlAuthClick();
  void onChangeControlPasswordClick();
  void onUnsetControlPasswordClick();

protected:
  ServerConfig *m_config;
  PortConfig   *m_portConfig;

  // Port selector (programmatic)
  ComboBox m_portSelector;
  HWND     m_portSelectorLabel;

  // Windows auth controls
  Control  m_defaultPermCombo;
  TextBox  m_maxConnPerUser;
  SpinControl m_maxConnPerUserSpin;

  // Group permission rules
  ListView    m_ruleList;
  TextBox     m_groupNameEdit;
  Control     m_permissionCombo;
  TextBox     m_priorityEdit;
  SpinControl m_prioritySpin;
  Control     m_addRuleButton;
  Control     m_editRuleButton;
  Control     m_removeRuleButton;
  Control     m_moveUpRuleButton;
  Control     m_moveDownRuleButton;
  Control     m_browseButton;
  std::vector<GroupPermissionRule> m_rules;

  // Control interface auth
  CheckBox m_useControlAuth;
  CheckBox m_repeatControlAuth;
  Control  m_controlPasswordBtn;
  Control  m_unsetControlPasswordBtn;

  // Password control objects (control interface only)
  PasswordControl *m_cpControl;

  BaseDialog *m_parentDialog;
};

#endif // _PORT_SETTINGS_CONFIG_DIALOG_H_
