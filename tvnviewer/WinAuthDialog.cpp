// Copyright (C) 2024 TightVNC Contributors.
// All rights reserved.
//
//-------------------------------------------------------------------------
// This file is part of the TightVNC software.  Please visit our Web site:
//
//                       http://www.tightvnc.com/
//
// Windows authentication dialog implementation. Pre-fills username
// and domain from the current Windows session environment.
//

#include "WinAuthDialog.h"

#include <Windows.h>

WinAuthDialog::WinAuthDialog()
: BaseDialog(IDD_WIN_AUTH)
{
}

BOOL WinAuthDialog::onInitDialog()
{
  setControlById(m_hostEdit, IDC_WIN_AUTH_HOST);
  setControlById(m_userEdit, IDC_WIN_AUTH_USER);
  setControlById(m_passEdit, IDC_WIN_AUTH_PASS);
  setControlById(m_domainEdit, IDC_WIN_AUTH_DOMAIN);

  // Show hostname
  m_hostEdit.setText(m_strHost.getString());

  // Pre-fill current Windows username
  TCHAR userName[256] = { 0 };
  DWORD userNameSize = 256;
  if (GetUserName(userName, &userNameSize)) {
    m_userEdit.setText(userName);
  }

  // Pre-fill domain from environment variable USERDOMAIN
  TCHAR domainName[256] = { 0 };
  DWORD domainLen = GetEnvironmentVariable(_T("USERDOMAIN"), domainName, 256);
  if (domainLen > 0) {
    m_domainEdit.setText(domainName);
  }

  m_passEdit.setFocus();
  return FALSE;
}

void WinAuthDialog::setHostName(const StringStorage *hostname)
{
  m_strHost = *hostname;
}

BOOL WinAuthDialog::onCommand(UINT controlID, UINT notificationID)
{
  if (controlID == IDOK) {
    m_userEdit.getText(&m_strUsername);
    m_passEdit.getText(&m_strPassword);
    m_domainEdit.getText(&m_strDomain);
    kill(1); // success
    return TRUE;
  }
  if (controlID == IDCANCEL) {
    kill(0); // canceled
    return TRUE;
  }
  return FALSE;
}

const StringStorage *WinAuthDialog::getUsername() const
{
  return &m_strUsername;
}

const StringStorage *WinAuthDialog::getPassword() const
{
  return &m_strPassword;
}

const StringStorage *WinAuthDialog::getDomain() const
{
  return &m_strDomain;
}
