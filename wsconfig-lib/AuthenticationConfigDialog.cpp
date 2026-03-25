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

#include "tvnserver/resource.h"
#include "AuthenticationConfigDialog.h"
#include "ConfigDialog.h"
#include "server-config-lib/Configurator.h"
#include "server-config-lib/ClientPermissions.h"
#include "util/StringTable.h"

AuthenticationConfigDialog::AuthenticationConfigDialog()
: BaseDialog(IDD_CONFIG_AUTHENTICATION_PAGE),
  m_parentDialog(NULL),
  m_config(NULL),
  m_ppControl(NULL),
  m_vpControl(NULL),
  m_cpControl(NULL)
{
}

AuthenticationConfigDialog::~AuthenticationConfigDialog()
{
  delete m_ppControl;
  delete m_vpControl;
  delete m_cpControl;
}

void AuthenticationConfigDialog::setParentDialog(BaseDialog *dialog)
{
  m_parentDialog = dialog;
}

BOOL AuthenticationConfigDialog::onInitDialog()
{
  m_config = Configurator::getInstance()->getServerConfig();
  initControls();
  updateUI();
  return TRUE;
}

void AuthenticationConfigDialog::initControls()
{
  HWND hwnd = m_ctrlThis.getWindow();

  // VNC authentication controls
  m_useAuthentication.setWindow(GetDlgItem(hwnd, IDC_USE_AUTHENTICATION));
  m_primaryPasswordBtn.setWindow(GetDlgItem(hwnd, IDC_PRIMARY_PASSWORD));
  m_unsetPrimaryPasswordBtn.setWindow(GetDlgItem(hwnd, IDC_UNSET_PRIMARY_PASSWORD_BUTTON));
  m_viewOnlyPasswordBtn.setWindow(GetDlgItem(hwnd, IDC_VIEW_ONLY_PASSWORD));
  m_unsetViewOnlyPasswordBtn.setWindow(GetDlgItem(hwnd, IDC_UNSET_READONLY_PASSWORD_BUTTON));

  // Windows authentication controls
  m_winAuthEnable.setWindow(GetDlgItem(hwnd, IDC_WIN_AUTH_ENABLE));
  m_authModeCombo.setWindow(GetDlgItem(hwnd, IDC_AUTH_MODE_COMBO));
  m_defaultPermCombo.setWindow(GetDlgItem(hwnd, IDC_DEFAULT_PERM_COMBO));

  // Control interface auth controls
  m_useControlAuth.setWindow(GetDlgItem(hwnd, IDC_USE_CONTROL_AUTH_CHECKBOX));
  m_repeatControlAuth.setWindow(GetDlgItem(hwnd, IDC_REPEAT_CONTROL_AUTH_CHECKBOX));
  m_controlPasswordBtn.setWindow(GetDlgItem(hwnd, IDC_CONTROL_PASSWORD_BUTTON));
  m_unsetControlPasswordBtn.setWindow(GetDlgItem(hwnd, IDC_UNSET_CONTROL_PASWORD_BUTTON));

  // Populate auth mode combo: VNC Only / Windows Only / Both
  SendMessage(m_authModeCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("VNC Password Only"));
  SendMessage(m_authModeCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("Windows Auth Only"));
  SendMessage(m_authModeCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("Both (VNC + Windows)"));

  // Populate default permission combo
  SendMessage(m_defaultPermCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("Full Control"));
  SendMessage(m_defaultPermCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("View Only"));
  SendMessage(m_defaultPermCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("View + Clipboard"));
  SendMessage(m_defaultPermCombo.getWindow(), CB_ADDSTRING, 0,
              (LPARAM)_T("Deny Access"));

  // Create PasswordControl objects wrapping the set/unset button pairs
  m_ppControl = new PasswordControl(&m_primaryPasswordBtn, &m_unsetPrimaryPasswordBtn);
  m_vpControl = new PasswordControl(&m_viewOnlyPasswordBtn, &m_unsetViewOnlyPasswordBtn);
  m_cpControl = new PasswordControl(&m_controlPasswordBtn, &m_unsetControlPasswordBtn);
}

void AuthenticationConfigDialog::updateUI()
{
  if (m_config == NULL) return;

  // --- VNC password auth section ---
  m_useAuthentication.check(m_config->isUsingAuthentication());

  if (m_config->hasPrimaryPassword()) {
    UINT8 crypted[8];
    m_config->getPrimaryPassword(crypted);
    m_ppControl->setCryptedPassword((const char *)crypted);
  }

  if (m_config->hasReadOnlyPassword()) {
    UINT8 crypted[8];
    m_config->getReadOnlyPassword(crypted);
    m_vpControl->setCryptedPassword((const char *)crypted);
  }

  // --- Windows auth section ---
  bool winEnabled = m_config->isWinAuthEnabled();
  m_winAuthEnable.check(winEnabled);

  int modeIndex = 0;
  switch (m_config->getAuthMode()) {
  case ServerConfig::AUTH_VNC_ONLY:     modeIndex = 0; break;
  case ServerConfig::AUTH_WINDOWS_ONLY: modeIndex = 1; break;
  case ServerConfig::AUTH_BOTH:         modeIndex = 2; break;
  }
  SendMessage(m_authModeCombo.getWindow(), CB_SETCURSEL, modeIndex, 0);

  // Default permission: map flags to combo index
  UINT32 defPerm = m_config->getDefaultWinAuthPermissions();
  int defPermIdx = 0; // Full Control default
  if (defPerm & ClientPermissions::PERM_DENY) {
    defPermIdx = 3;
  } else if (defPerm == ClientPermissions::PERM_VIEW_ONLY) {
    defPermIdx = 1;
  } else if (defPerm == (ClientPermissions::PERM_VIEW_ONLY | ClientPermissions::PERM_CLIPBOARD)) {
    defPermIdx = 2;
  }
  SendMessage(m_defaultPermCombo.getWindow(), CB_SETCURSEL, defPermIdx, 0);

  // --- Control interface auth section ---
  m_useControlAuth.check(m_config->isControlAuthEnabled());
  m_repeatControlAuth.check(m_config->getControlAuthAlwaysChecking());

  if (m_config->hasControlPassword()) {
    unsigned char crypted[8];
    m_config->getControlPassword(crypted);
    m_cpControl->setCryptedPassword((char *)crypted);
  }

  updateControlDependencies();
}

void AuthenticationConfigDialog::updateControlDependencies()
{
  // VNC passwords enabled only when VNC auth checkbox is checked
  bool vncAuthChecked = m_useAuthentication.isChecked();
  m_ppControl->setEnabled(vncAuthChecked);
  m_vpControl->setEnabled(vncAuthChecked);

  // Windows auth sub-controls enabled only when Windows auth is enabled
  bool winEnabled = m_winAuthEnable.isChecked();
  EnableWindow(m_authModeCombo.getWindow(), winEnabled ? TRUE : FALSE);
  EnableWindow(m_defaultPermCombo.getWindow(), winEnabled ? TRUE : FALSE);

  // Control auth: repeat checkbox and password controls follow the main checkbox
  bool ctrlEnabled = m_useControlAuth.isChecked();
  m_repeatControlAuth.setEnabled(ctrlEnabled);
  m_cpControl->setEnabled(ctrlEnabled);
}

BOOL AuthenticationConfigDialog::onCommand(UINT controlID, UINT notificationID)
{
  if (notificationID == BN_CLICKED) {
    switch (controlID) {
    case IDC_USE_AUTHENTICATION:
      onUseAuthenticationClick();
      break;
    case IDC_PRIMARY_PASSWORD:
      onPrimaryPasswordChange();
      break;
    case IDC_UNSET_PRIMARY_PASSWORD_BUTTON:
      onUnsetPrimaryPasswordClick();
      break;
    case IDC_VIEW_ONLY_PASSWORD:
      onViewOnlyPasswordChange();
      break;
    case IDC_UNSET_READONLY_PASSWORD_BUTTON:
      onUnsetViewOnlyPasswordClick();
      break;
    case IDC_WIN_AUTH_ENABLE:
      onWinAuthEnableClick();
      break;
    case IDC_USE_CONTROL_AUTH_CHECKBOX:
      onUseControlAuthClick();
      break;
    case IDC_REPEAT_CONTROL_AUTH_CHECKBOX:
      onRepeatControlAuthClick();
      break;
    case IDC_CONTROL_PASSWORD_BUTTON:
      onChangeControlPasswordClick();
      break;
    case IDC_UNSET_CONTROL_PASWORD_BUTTON:
      onUnsetControlPasswordClick();
      break;
    }
  } else if (notificationID == CBN_SELCHANGE) {
    switch (controlID) {
    case IDC_AUTH_MODE_COMBO:
      onAuthModeChange();
      break;
    case IDC_DEFAULT_PERM_COMBO:
      onDefaultPermChange();
      break;
    }
  }
  return TRUE;
}

bool AuthenticationConfigDialog::validateInput()
{
  // Only require VNC password when RFB connections are enabled AND VNC auth checked
  bool rfbEnabled = (m_config != NULL) ? m_config->isAcceptingRfbConnections() : true;

  if (rfbEnabled && m_useAuthentication.isChecked()) {
    bool hasAnyPassword = m_ppControl->hasPassword() || m_vpControl->hasPassword();
    if (!hasAnyPassword) {
      MessageBox(m_ctrlThis.getWindow(),
                 StringTable::getString(IDS_SET_PASSWORD_NOTIFICATION),
                 StringTable::getString(IDS_CAPTION_BAD_INPUT),
                 MB_ICONSTOP | MB_OK);
      return false;
    }
  }

  // If control auth is enabled, control password must be set
  if (m_useControlAuth.isChecked() && !m_cpControl->hasPassword()) {
    MessageBox(m_ctrlThis.getWindow(),
               StringTable::getString(IDS_SET_CONTROL_PASSWORD_NOTIFICATION),
               StringTable::getString(IDS_CAPTION_BAD_INPUT),
               MB_ICONSTOP | MB_OK);
    return false;
  }

  return true;
}

void AuthenticationConfigDialog::apply()
{
  if (m_config == NULL) return;

  // --- VNC auth ---
  m_config->useAuthentication(m_useAuthentication.isChecked());

  if (m_ppControl->hasPassword()) {
    m_config->setPrimaryPassword((const unsigned char *)m_ppControl->getCryptedPassword());
  } else {
    m_config->deletePrimaryPassword();
  }

  if (m_vpControl->hasPassword()) {
    m_config->setReadOnlyPassword((const unsigned char *)m_vpControl->getCryptedPassword());
  } else {
    m_config->deleteReadOnlyPassword();
  }

  // --- Windows auth ---
  m_config->enableWinAuth(m_winAuthEnable.isChecked());

  int modeIdx = (int)SendMessage(m_authModeCombo.getWindow(), CB_GETCURSEL, 0, 0);
  switch (modeIdx) {
  case 0: m_config->setAuthMode(ServerConfig::AUTH_VNC_ONLY);     break;
  case 1: m_config->setAuthMode(ServerConfig::AUTH_WINDOWS_ONLY); break;
  case 2: m_config->setAuthMode(ServerConfig::AUTH_BOTH);         break;
  }

  int defIdx = (int)SendMessage(m_defaultPermCombo.getWindow(), CB_GETCURSEL, 0, 0);
  UINT32 defPerm = ClientPermissions::PERM_FULL_CONTROL;
  switch (defIdx) {
  case 0: defPerm = ClientPermissions::PERM_FULL_CONTROL; break;
  case 1: defPerm = ClientPermissions::PERM_VIEW_ONLY;    break;
  case 2: defPerm = ClientPermissions::PERM_VIEW_ONLY | ClientPermissions::PERM_CLIPBOARD; break;
  case 3: defPerm = ClientPermissions::PERM_DENY;         break;
  }
  m_config->setDefaultWinAuthPermissions(defPerm);

  // --- Control interface auth ---
  m_config->useControlAuth(m_useControlAuth.isChecked());
  m_config->setControlAuthAlwaysChecking(m_repeatControlAuth.isChecked());

  if (m_cpControl->hasPassword()) {
    m_config->setControlPassword((const unsigned char *)m_cpControl->getCryptedPassword());
  } else {
    m_config->deleteControlPassword();
  }
}

// --- VNC auth event handlers ---

void AuthenticationConfigDialog::onUseAuthenticationClick()
{
  updateControlDependencies();
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void AuthenticationConfigDialog::onPrimaryPasswordChange()
{
  if (m_ppControl->showChangePasswordModalDialog(&m_ctrlThis)) {
    if (m_parentDialog != NULL) {
      ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
    }
  }
}

void AuthenticationConfigDialog::onUnsetPrimaryPasswordClick()
{
  m_ppControl->unsetPassword(true, m_ctrlThis.getWindow());
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void AuthenticationConfigDialog::onViewOnlyPasswordChange()
{
  if (m_vpControl->showChangePasswordModalDialog(&m_ctrlThis)) {
    if (m_parentDialog != NULL) {
      ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
    }
  }
}

void AuthenticationConfigDialog::onUnsetViewOnlyPasswordClick()
{
  m_vpControl->unsetPassword(true, m_ctrlThis.getWindow());
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

// --- Windows auth event handlers ---

void AuthenticationConfigDialog::onWinAuthEnableClick()
{
  updateControlDependencies();
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void AuthenticationConfigDialog::onAuthModeChange()
{
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void AuthenticationConfigDialog::onDefaultPermChange()
{
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

// --- Control interface auth event handlers ---

void AuthenticationConfigDialog::onUseControlAuthClick()
{
  updateControlDependencies();
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void AuthenticationConfigDialog::onRepeatControlAuthClick()
{
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void AuthenticationConfigDialog::onChangeControlPasswordClick()
{
  if (m_cpControl->showChangePasswordModalDialog(&m_ctrlThis)) {
    if (m_parentDialog != NULL) {
      ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
    }
  }
}

void AuthenticationConfigDialog::onUnsetControlPasswordClick()
{
  m_cpControl->unsetPassword(true, m_ctrlThis.getWindow());
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}
