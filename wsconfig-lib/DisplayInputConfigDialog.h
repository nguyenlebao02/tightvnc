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

#ifndef _DISPLAY_INPUT_CONFIG_DIALOG_H_
#define _DISPLAY_INPUT_CONFIG_DIALOG_H_

#include "gui/BaseDialog.h"
#include "gui/CheckBox.h"
#include "gui/TextBox.h"
#include "gui/SpinControl.h"
#include "server-config-lib/ServerConfig.h"

// Consolidated display & input settings dialog (Phase 4 redesign).
// Combines screen capture options, input handling, and video detection
// into a single tab page (IDD_CONFIG_DISPLAY_INPUT_PAGE = 139).
class DisplayInputConfigDialog : public BaseDialog
{
public:
  DisplayInputConfigDialog();
  virtual ~DisplayInputConfigDialog();

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
  void updateCheckboxesState();

  // Screen capture handlers
  void onUseD3DChanged();
  void onUseMirrorDriverChanged();
  void onRemoveWallpaperChanged();
  void onPollingIntervalUpdate();
  void onPollingIntervalSpinChangePos(LPNMUPDOWN message);

  // Input handling handlers
  void onBlockRemoteInputChanged();
  void onBlockLocalInputChanged();
  void onLocalInputPriorityChanged();
  void onInactivityTimeoutUpdate();

  // Video detection handlers
  void onVideoClassNamesUpdate();
  void onVideoRectsUpdate();
  void onVideoRecognitionIntervalUpdate();
  void onVideoRecognitionIntervalSpinChangePos(LPNMUPDOWN message);

protected:
  ServerConfig *m_config;

  // Screen capture controls
  CheckBox    m_useD3D;
  CheckBox    m_useMirrorDriver;
  CheckBox    m_removeWallpaper;
  TextBox     m_pollingInterval;
  SpinControl m_pollingIntervalSpin;

  // Input handling controls
  CheckBox    m_blockRemoteInput;
  CheckBox    m_blockLocalInput;
  CheckBox    m_localInputPriority;
  TextBox     m_localInputPriorityTimeout;
  SpinControl m_inactivityTimeoutSpin;

  // Video detection controls
  TextBox     m_videoClassNames;
  TextBox     m_videoRects;
  TextBox     m_videoRecognitionInterval;
  SpinControl m_videoRecognitionIntervalSpin;

  BaseDialog *m_parentDialog;
};

#endif // _DISPLAY_INPUT_CONFIG_DIALOG_H_
