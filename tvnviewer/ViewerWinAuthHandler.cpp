// Copyright (C) 2024 TightVNC Contributors.
// All rights reserved.
//
//-------------------------------------------------------------------------
// This file is part of the TightVNC software.  Please visit our Web site:
//
//                       http://www.tightvnc.com/
//
// Viewer-specific Windows auth handler implementation.
// Presents WinAuthDialog to the user to collect credentials.
//

#include "ViewerWinAuthHandler.h"
#include "WinAuthDialog.h"

ViewerWinAuthHandler::ViewerWinAuthHandler(ConnectionData *connectionData)
: m_connectionData(connectionData)
{
}

ViewerWinAuthHandler::~ViewerWinAuthHandler()
{
}

void ViewerWinAuthHandler::getCredentials(StringStorage *username,
                                          StringStorage *password,
                                          StringStorage *domain)
{
  WinAuthDialog dialog;

  // Set hostname for display in the dialog
  StringStorage hostname = m_connectionData->getHost();
  dialog.setHostName(&hostname);

  if (dialog.showModal()) {
    *username = *dialog.getUsername();
    *password = *dialog.getPassword();
    *domain = *dialog.getDomain();
  } else {
    throw AuthCanceledException();
  }
}
