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
#include "DisplayInputConfigDialog.h"
#include "ConfigDialog.h"
#include "CommonInputValidation.h"
#include "UIDataAccess.h"
#include "server-config-lib/Configurator.h"
#include "util/StringParser.h"
#include "util/StringTable.h"
#include "thread/AutoLock.h"
#include "util/CommonHeader.h"
#include <limits.h>

DisplayInputConfigDialog::DisplayInputConfigDialog()
: BaseDialog(IDD_CONFIG_DISPLAY_INPUT_PAGE),
  m_parentDialog(NULL),
  m_config(NULL)
{
}

DisplayInputConfigDialog::~DisplayInputConfigDialog()
{
}

void DisplayInputConfigDialog::setParentDialog(BaseDialog *dialog)
{
  m_parentDialog = dialog;
}

BOOL DisplayInputConfigDialog::onInitDialog()
{
  m_config = Configurator::getInstance()->getServerConfig();
  initControls();
  updateUI();
  return TRUE;
}

void DisplayInputConfigDialog::initControls()
{
  HWND hwnd = m_ctrlThis.getWindow();

  // Screen capture controls
  m_useD3D.setWindow(GetDlgItem(hwnd, IDC_USE_D3D));
  m_useMirrorDriver.setWindow(GetDlgItem(hwnd, IDC_USE_MIRROR_DRIVER));
  m_removeWallpaper.setWindow(GetDlgItem(hwnd, IDC_REMOVE_WALLPAPER));
  m_pollingInterval.setWindow(GetDlgItem(hwnd, IDC_POLLING_INTERVAL));
  m_pollingIntervalSpin.setWindow(GetDlgItem(hwnd, IDC_POLLING_INTERVAL_SPIN));

  // Polling interval spin: range 30-5000, auto acceleration like original
  int limitersTmp[] = {50, 200};
  int deltasTmp[]   = {5, 10};
  std::vector<int> limiters(limitersTmp, limitersTmp + 2);
  std::vector<int> deltas(deltasTmp, deltasTmp + 2);

  m_pollingIntervalSpin.setBuddy(&m_pollingInterval);
  m_pollingIntervalSpin.setAccel(0, 1);
  m_pollingIntervalSpin.setRange32(30, 5000);
  m_pollingIntervalSpin.setAutoAccelerationParams(&limiters, &deltas, 50);
  m_pollingIntervalSpin.enableAutoAcceleration(true);

  // Input handling controls
  m_blockRemoteInput.setWindow(GetDlgItem(hwnd, IDC_BLOCK_REMOTE_INPUT));
  m_blockLocalInput.setWindow(GetDlgItem(hwnd, IDC_BLOCK_LOCAL_INPUT));
  m_localInputPriority.setWindow(GetDlgItem(hwnd, IDC_LOCAL_INPUT_PRIORITY));
  m_localInputPriorityTimeout.setWindow(GetDlgItem(hwnd, IDC_LOCAL_INPUT_PRIORITY_TIMEOUT));
  m_inactivityTimeoutSpin.setWindow(GetDlgItem(hwnd, IDC_INACTIVITY_TIMEOUT_SPIN));

  m_inactivityTimeoutSpin.setBuddy(&m_localInputPriorityTimeout);
  m_inactivityTimeoutSpin.setAccel(0, 1);
  m_inactivityTimeoutSpin.setRange32(1, 999);

  // Video detection controls
  m_videoClassNames.setWindow(GetDlgItem(hwnd, IDC_VIDEO_CLASS_NAMES));
  m_videoRects.setWindow(GetDlgItem(hwnd, IDC_VIDEO_RECTS));
  m_videoRecognitionInterval.setWindow(GetDlgItem(hwnd, IDC_VIDEO_RECOGNITION_INTERVAL));
  m_videoRecognitionIntervalSpin.setWindow(GetDlgItem(hwnd, IDC_VIDEO_RECOGNITION_INTERVAL_SPIN));

  m_videoRecognitionIntervalSpin.setBuddy(&m_videoRecognitionInterval);
  m_videoRecognitionIntervalSpin.setAccel(0, 1);
  m_videoRecognitionIntervalSpin.setRange32(0, INT_MAX);
  m_videoRecognitionIntervalSpin.setAutoAccelerationParams(&limiters, &deltas, 50);
  m_videoRecognitionIntervalSpin.enableAutoAcceleration(true);
}

void DisplayInputConfigDialog::updateUI()
{
  if (m_config == NULL) return;

  // --- Screen capture section ---
  m_useD3D.check(m_config->getD3DIsAllowed());
  m_useMirrorDriver.check(m_config->getMirrorIsAllowed());
  m_removeWallpaper.check(m_config->isRemovingDesktopWallpaperEnabled());
  m_pollingInterval.setUnsignedInt(m_config->getPollingInterval());

  // --- Input handling section ---
  m_blockRemoteInput.check(m_config->isBlockingRemoteInput());
  m_blockLocalInput.check(m_config->isBlockingLocalInput());
  m_localInputPriority.check(m_config->isLocalInputPriorityEnabled());
  m_localInputPriorityTimeout.setUnsignedInt(m_config->getLocalInputPriorityTimeout());

  updateCheckboxesState();

  // --- Video detection section ---
  // Build newline-delimited text from video class names vector
  StringVector *videoClasses = m_config->getVideoClassNames();
  std::vector<Rect> *videoRects = m_config->getVideoRects();
  TCHAR endLine[3] = {13, 10, 0};

  {
    AutoLock al(m_config);
    StringStorage text;
    text.setString(_T(""));
    for (size_t i = 0; i < videoClasses->size(); i++) {
      text.appendString(videoClasses->at(i).getString());
      text.appendString(&endLine[0]);
    }
    m_videoClassNames.setText(text.getString());

    // Build video rects text
    text.setString(_T(""));
    for (size_t i = 0; i < videoRects->size(); i++) {
      Rect r = videoRects->at(i);
      StringStorage s;
      RectSerializer::toString(&r, &s);
      text.appendString(s.getString());
      text.appendString(&endLine[0]);
    }
    m_videoRects.setText(text.getString());

    TCHAR buf[32];
    _ltot(m_config->getVideoRecognitionInterval(), buf, 10);
    m_videoRecognitionInterval.setText(buf);
  }
}

void DisplayInputConfigDialog::updateCheckboxesState()
{
  // When blocking local or remote input, local-priority cannot be active
  if (m_blockLocalInput.isChecked() || m_blockRemoteInput.isChecked()) {
    m_localInputPriority.check(false);
    m_localInputPriority.setEnabled(false);
  } else {
    m_localInputPriority.setEnabled(true);
  }

  // Timeout edit only enabled when local input priority is active
  bool timeoutEnabled = m_localInputPriority.isChecked() &&
                        m_localInputPriority.isEnabled();
  m_localInputPriorityTimeout.setEnabled(timeoutEnabled);
  m_inactivityTimeoutSpin.invalidate();
}

BOOL DisplayInputConfigDialog::onCommand(UINT controlID, UINT notificationID)
{
  if (notificationID == BN_CLICKED) {
    switch (controlID) {
    case IDC_USE_D3D:
      onUseD3DChanged();
      break;
    case IDC_USE_MIRROR_DRIVER:
      onUseMirrorDriverChanged();
      break;
    case IDC_REMOVE_WALLPAPER:
      onRemoveWallpaperChanged();
      break;
    case IDC_BLOCK_REMOTE_INPUT:
      onBlockRemoteInputChanged();
      break;
    case IDC_BLOCK_LOCAL_INPUT:
      onBlockLocalInputChanged();
      break;
    case IDC_LOCAL_INPUT_PRIORITY:
      onLocalInputPriorityChanged();
      break;
    }
  } else if (notificationID == EN_UPDATE) {
    switch (controlID) {
    case IDC_POLLING_INTERVAL:
      onPollingIntervalUpdate();
      break;
    case IDC_LOCAL_INPUT_PRIORITY_TIMEOUT:
      onInactivityTimeoutUpdate();
      break;
    case IDC_VIDEO_CLASS_NAMES:
    case IDC_VIDEO_RECTS:
      onVideoClassNamesUpdate();
      break;
    case IDC_VIDEO_RECOGNITION_INTERVAL:
      onVideoRecognitionIntervalUpdate();
      break;
    }
  }
  return TRUE;
}

BOOL DisplayInputConfigDialog::onNotify(UINT controlID, LPARAM data)
{
  if (controlID == IDC_POLLING_INTERVAL_SPIN) {
    LPNMUPDOWN msg = (LPNMUPDOWN)data;
    if (msg->hdr.code == UDN_DELTAPOS) {
      onPollingIntervalSpinChangePos(msg);
    }
  } else if (controlID == IDC_VIDEO_RECOGNITION_INTERVAL_SPIN) {
    LPNMUPDOWN msg = (LPNMUPDOWN)data;
    if (msg->hdr.code == UDN_DELTAPOS) {
      onVideoRecognitionIntervalSpinChangePos(msg);
    }
  }
  return TRUE;
}

bool DisplayInputConfigDialog::validateInput()
{
  // Validate polling interval is a valid unsigned int
  if (!CommonInputValidation::validateUINT(
        &m_pollingInterval,
        StringTable::getString(IDS_INVALID_POLLING_INTERVAL))) {
    return false;
  }

  unsigned int pollingInterval;
  UIDataAccess::queryValueAsUInt(&m_pollingInterval, &pollingInterval);
  if (pollingInterval < ServerConfig::MINIMAL_POLLING_INTERVAL) {
    CommonInputValidation::notifyValidationError(
      &m_pollingInterval,
      StringTable::getString(IDS_POLL_INTERVAL_TOO_SMALL));
    return false;
  }

  // Validate inactivity timeout
  if (!CommonInputValidation::validateUINT(
        &m_localInputPriorityTimeout,
        StringTable::getString(IDS_INVALID_INACTIVITY_TIMEOUT))) {
    return false;
  }

  unsigned int inactivityTimeout;
  UIDataAccess::queryValueAsUInt(&m_localInputPriorityTimeout, &inactivityTimeout);
  if (inactivityTimeout < ServerConfig::MINIMAL_LOCAL_INPUT_PRIORITY_TIMEOUT) {
    CommonInputValidation::notifyValidationError(
      &m_localInputPriorityTimeout,
      StringTable::getString(IDS_INACTIVITY_TIMEOUT_TOO_SMALL));
    return false;
  }

  // Validate video recognition interval
  if (!CommonInputValidation::validateUINT(
        &m_videoRecognitionInterval,
        StringTable::getString(IDS_INVALID_VIDEO_RECOGNITION_INTERVAL))) {
    return false;
  }

  return true;
}

void DisplayInputConfigDialog::apply()
{
  if (m_config == NULL) return;

  // --- Screen capture ---
  m_config->setD3DAllowing(m_useD3D.isChecked());
  m_config->setMirrorAllowing(m_useMirrorDriver.isChecked());
  m_config->enableRemovingDesktopWallpaper(m_removeWallpaper.isChecked());

  StringStorage pollingText;
  m_pollingInterval.getText(&pollingText);
  int pollingVal = 0;
  StringParser::parseInt(pollingText.getString(), &pollingVal);
  m_config->setPollingInterval(pollingVal);

  // --- Input handling ---
  m_config->blockRemoteInput(m_blockRemoteInput.isChecked());
  m_config->blockLocalInput(m_blockLocalInput.isChecked());
  m_config->setLocalInputPriority(m_localInputPriority.isChecked());

  StringStorage timeoutText;
  m_localInputPriorityTimeout.getText(&timeoutText);
  int timeout = 0;
  if (StringParser::parseInt(timeoutText.getString(), &timeout)) {
    timeout = max(0, timeout);
    m_config->setLocalInputPriorityTimeout((unsigned int)timeout);
  }

  // --- Video detection ---
  AutoLock al(m_config);

  StringVector *videoClasses = m_config->getVideoClassNames();
  videoClasses->clear();
  std::vector<Rect> *videoRects = m_config->getVideoRects();
  videoRects->clear();

  // Parse class names from multiline edit (delimiters: space/newline/tab/comma/semi)
  StringStorage classText;
  m_videoClassNames.getText(&classText);
  size_t count = 0;
  TCHAR delimiters[] = _T(" \n\r\t,;");

  classText.split(delimiters, NULL, &count);
  if (count != 0) {
    std::vector<StringStorage> chunks(count);
    classText.split(delimiters, &chunks.front(), &count);
    for (size_t i = 0; i < count; i++) {
      if (!chunks[i].isEmpty()) {
        videoClasses->push_back(chunks[i].getString());
      }
    }
  }

  // Parse video rects from multiline edit
  StringStorage rectsText;
  m_videoRects.getText(&rectsText);
  count = 0;

  rectsText.split(delimiters, NULL, &count);
  if (count != 0) {
    std::vector<StringStorage> chunks(count);
    rectsText.split(delimiters, &chunks.front(), &count);
    for (size_t i = 0; i < count; i++) {
      if (!chunks[i].isEmpty()) {
        try {
          videoRects->push_back(RectSerializer::toRect(&chunks[i]));
        } catch (...) {
          // Ignore badly formatted rect strings
        }
      }
    }
  }

  // Parse video recognition interval
  StringStorage intervalText;
  m_videoRecognitionInterval.getText(&intervalText);
  int interval = 0;
  StringParser::parseInt(intervalText.getString(), &interval);
  m_config->setVideoRecognitionInterval((unsigned int)interval);
}

// --- Screen capture event handlers ---

void DisplayInputConfigDialog::onUseD3DChanged()
{
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void DisplayInputConfigDialog::onUseMirrorDriverChanged()
{
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void DisplayInputConfigDialog::onRemoveWallpaperChanged()
{
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void DisplayInputConfigDialog::onPollingIntervalUpdate()
{
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void DisplayInputConfigDialog::onPollingIntervalSpinChangePos(LPNMUPDOWN message)
{
  m_pollingIntervalSpin.autoAccelerationHandler(message);
}

// --- Input handling event handlers ---

void DisplayInputConfigDialog::onBlockRemoteInputChanged()
{
  updateCheckboxesState();
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void DisplayInputConfigDialog::onBlockLocalInputChanged()
{
  updateCheckboxesState();
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void DisplayInputConfigDialog::onLocalInputPriorityChanged()
{
  updateCheckboxesState();
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void DisplayInputConfigDialog::onInactivityTimeoutUpdate()
{
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

// --- Video detection event handlers ---

void DisplayInputConfigDialog::onVideoClassNamesUpdate()
{
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void DisplayInputConfigDialog::onVideoRectsUpdate()
{
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void DisplayInputConfigDialog::onVideoRecognitionIntervalUpdate()
{
  if (m_parentDialog != NULL) {
    ((ConfigDialog *)m_parentDialog)->updateApplyButtonState();
  }
}

void DisplayInputConfigDialog::onVideoRecognitionIntervalSpinChangePos(LPNMUPDOWN message)
{
  m_videoRecognitionIntervalSpin.autoAccelerationHandler(message);
}
