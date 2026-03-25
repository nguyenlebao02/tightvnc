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

#ifndef _AUTHENTICATION_CONFIG_DIALOG_H_
#define _AUTHENTICATION_CONFIG_DIALOG_H_

#include "gui/BaseDialog.h"
#include "gui/CheckBox.h"
#include "gui/TextBox.h"
#include "server-config-lib/ServerConfig.h"
#include "PasswordControl.h"

// Consolidated authentication settings dialog (Phase 3 redesign).
// Combines VNC password auth, Windows auth mode, and control interface auth
// into a single tab page (IDD_CONFIG_AUTHENTICATION_PAGE = 138).
class AuthenticationConfigDialog : public BaseDialog
{
public:
  AuthenticationConfigDialog();
  virtual ~AuthenticationConfigDialog();

  void setParentDialog(BaseDialog *dialog);

  // BaseDialog overrides
  virtual BOOL onInitDialog();
  virtual BOOL onCommand(UINT controlID, UINT notificationID);
  virtual BOOL onNotify(UINT controlID, LPARAM data) { return TRUE; }
  virtual BOOL onDestroy() { return TRUE; }

  bool validateInput();
  void updateUI();
  void apply();

private:
  void initControls();
  void updateControlDependencies();

  // VNC password auth handlers
  void onUseAuthenticationClick();
  void onPrimaryPasswordChange();
  void onUnsetPrimaryPasswordClick();
  void onViewOnlyPasswordChange();
  void onUnsetViewOnlyPasswordClick();

  // Windows auth handlers
  void onWinAuthEnableClick();
  void onAuthModeChange();
  void onDefaultPermChange();

  // Control interface auth handlers
  void onUseControlAuthClick();
  void onRepeatControlAuthClick();
  void onChangeControlPasswordClick();
  void onUnsetControlPasswordClick();

protected:
  ServerConfig *m_config;

  // VNC auth controls
  CheckBox m_useAuthentication;
  Control  m_primaryPasswordBtn;
  Control  m_unsetPrimaryPasswordBtn;
  Control  m_viewOnlyPasswordBtn;
  Control  m_unsetViewOnlyPasswordBtn;

  // Windows auth controls
  CheckBox m_winAuthEnable;
  Control  m_authModeCombo;
  Control  m_defaultPermCombo;

  // Control interface auth controls
  CheckBox m_useControlAuth;
  CheckBox m_repeatControlAuth;
  Control  m_controlPasswordBtn;
  Control  m_unsetControlPasswordBtn;

  // Password control objects (manage set/unset button state)
  PasswordControl *m_ppControl;   // primary VNC password
  PasswordControl *m_vpControl;   // view-only VNC password
  PasswordControl *m_cpControl;   // control interface password

  BaseDialog *m_parentDialog;
};

#endif // _AUTHENTICATION_CONFIG_DIALOG_H_
