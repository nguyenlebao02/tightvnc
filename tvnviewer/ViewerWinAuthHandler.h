// Copyright (C) 2024 TightVNC Contributors.
// All rights reserved.
//
//-------------------------------------------------------------------------
// This file is part of the TightVNC software.  Please visit our Web site:
//
//                       http://www.tightvnc.com/
//
// Viewer-specific Windows auth handler that shows a login dialog
// to collect username/password/domain from the user.
//

#ifndef _VIEWER_WIN_AUTH_HANDLER_H_
#define _VIEWER_WIN_AUTH_HANDLER_H_

#include "viewer-core/WinAuthHandler.h"
#include "ConnectionData.h"

class ViewerWinAuthHandler : public WinAuthHandler
{
public:
  ViewerWinAuthHandler(ConnectionData *connectionData);
  virtual ~ViewerWinAuthHandler();

private:
  // Shows WinAuthDialog to collect Windows credentials from user.
  virtual void getCredentials(StringStorage *username,
                              StringStorage *password,
                              StringStorage *domain);

  ConnectionData *m_connectionData;
};

#endif // _VIEWER_WIN_AUTH_HANDLER_H_
