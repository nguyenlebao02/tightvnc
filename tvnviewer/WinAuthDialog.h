// Copyright (C) 2024 TightVNC Contributors.
// All rights reserved.
//
//-------------------------------------------------------------------------
// This file is part of the TightVNC software.  Please visit our Web site:
//
//                       http://www.tightvnc.com/
//
// Windows authentication dialog - prompts user for Windows username,
// password, and optional domain to authenticate via EXTERNAL auth type.
//

#ifndef _WIN_AUTH_DIALOG_H_
#define _WIN_AUTH_DIALOG_H_

#include "gui/BaseDialog.h"
#include "gui/TextBox.h"
#include "resource.h"

// Dialog that prompts user for Windows credentials when connecting
// to a VNC server that requires Windows authentication.
class WinAuthDialog : public BaseDialog
{
public:
  WinAuthDialog();

  const StringStorage *getUsername() const;
  const StringStorage *getPassword() const;
  const StringStorage *getDomain() const;

  void setHostName(const StringStorage *hostname);

protected:
  BOOL onInitDialog();
  BOOL onCommand(UINT controlID, UINT notificationID);

  TextBox m_hostEdit;
  TextBox m_userEdit;
  TextBox m_passEdit;
  TextBox m_domainEdit;

  StringStorage m_strHost;
  StringStorage m_strUsername;
  StringStorage m_strPassword;
  StringStorage m_strDomain;
};

#endif // _WIN_AUTH_DIALOG_H_
