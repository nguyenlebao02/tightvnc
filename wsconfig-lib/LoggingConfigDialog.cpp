// Copyright (C) 2009,2010,2011,2012 GlavSoft LLC.
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
#include "LoggingConfigDialog.h"
#include "CommonInputValidation.h"
#include "UIDataAccess.h"
#include "ConfigDialog.h"
#include "file-lib/File.h"
#include "server-config-lib/Configurator.h"
#include "server-config-lib/ServerConfig.h"
#include "util/StringParser.h"
#include "util/StringTable.h"

#include <shellapi.h>
#include <vector>

LoggingConfigDialog::LoggingConfigDialog()
: BaseDialog(IDD_CONFIG_LOGGING_PAGE), m_parent(NULL)
{
}

LoggingConfigDialog::~LoggingConfigDialog()
{
}

void LoggingConfigDialog::setParentDialog(BaseDialog *dialog)
{
  m_parent = dialog;
}

BOOL LoggingConfigDialog::onInitDialog()
{
  initControls();
  updateUI();
  return TRUE;
}

void LoggingConfigDialog::initControls()
{
  HWND hwnd = m_ctrlThis.getWindow();

  m_logLevel.setWindow(GetDlgItem(hwnd, IDC_LOG_LEVEL));
  m_logLevelSpin.setWindow(GetDlgItem(hwnd, IDC_LOG_LEVEL_SPIN));
  m_logForAllUsers.setWindow(GetDlgItem(hwnd, IDC_LOG_FOR_ALL_USERS));
  m_logPathTB.setWindow(GetDlgItem(hwnd, IDC_LOG_FILEPATH_EDIT));
  m_openFolderButton.setWindow(GetDlgItem(hwnd, IDC_OPEN_LOG_FOLDER_BUTTON));

  // Log level spin: range 0-10, step 1
  m_logLevelSpin.setBuddy(&m_logLevel);
  m_logLevelSpin.setRange(0, 10);
  m_logLevelSpin.setAccel(0, 1);
}

void LoggingConfigDialog::updateUI()
{
  ServerConfig *config = Configurator::getInstance()->getServerConfig();

  // Set log level value
  m_logLevel.setSignedInt(config->getLogLevel());

  // Set log-for-all-users checkbox
  m_logForAllUsers.check(config->isSaveLogToAllUsersPathFlagEnabled());

  // Display log folder path (read-only)
  StringStorage logPath;
  config->getLogFileDir(&logPath);

  if (logPath.isEmpty()) {
    logPath.setString(StringTable::getString(IDS_LOGPATH_UNAVALIABLE));
    m_openFolderButton.setEnabled(false);
    m_logPathTB.setEnabled(false);
  }

  m_logPathTB.setText(logPath.getString());

  // Enable open-folder button only if folder is accessible
  StringStorage folder;
  getFolderName(logPath.getString(), &folder);
  File folderFile(folder.getString());

  m_openFolderButton.setEnabled(folderFile.canRead());
}

void LoggingConfigDialog::apply()
{
  ServerConfig *config = Configurator::getInstance()->getServerConfig();

  // Save log level
  StringStorage logLevelText;
  m_logLevel.getText(&logLevelText);
  int logLevel = 0;
  StringParser::parseInt(logLevelText.getString(), &logLevel);
  config->setLogLevel(logLevel);

  // Save log-for-all-users flag
  config->saveLogToAllUsersPath(m_logForAllUsers.isChecked());
}

bool LoggingConfigDialog::validateInput()
{
  if (!CommonInputValidation::validateUINT(
        &m_logLevel,
        StringTable::getString(IDS_INVALID_LOG_LEVEL))) {
    return false;
  }

  unsigned int logLevel = 0;
  UIDataAccess::queryValueAsUInt(&m_logLevel, &logLevel);

  if (logLevel > 10) {
    CommonInputValidation::notifyValidationError(
      &m_logLevel,
      StringTable::getString(IDS_INVALID_LOG_LEVEL));
    return false;
  }

  return true;
}

BOOL LoggingConfigDialog::onCommand(UINT controlID, UINT notificationID)
{
  if (notificationID == BN_CLICKED) {
    switch (controlID) {
    case IDC_LOG_FOR_ALL_USERS:
      onLogForAllUsersClick();
      break;
    case IDC_OPEN_LOG_FOLDER_BUTTON:
      onOpenFolderButtonClick();
      break;
    }
  } else if (notificationID == EN_UPDATE) {
    if (controlID == IDC_LOG_LEVEL) {
      onLogLevelUpdate();
    }
  }
  return TRUE;
}

void LoggingConfigDialog::onLogLevelUpdate()
{
  ((ConfigDialog *)m_parent)->updateApplyButtonState();
}

void LoggingConfigDialog::onLogForAllUsersClick()
{
  ((ConfigDialog *)m_parent)->updateApplyButtonState();
}

void LoggingConfigDialog::onOpenFolderButtonClick()
{
  // Get the log folder path from the read-only text box and open it in Explorer
  StringStorage logPath;
  m_logPathTB.getText(&logPath);

  StringStorage folderPath;
  getFolderName(logPath.getString(), &folderPath);

  ShellExecute(0, _T("open"), folderPath.getString(), NULL, NULL, SW_SHOWNORMAL);
}

// Extract the directory portion of a full file path by stripping the last
// backslash-delimited component. Mirrors RegistrySettingsManager::getFolderName.
void LoggingConfigDialog::getFolderName(const TCHAR *path, StringStorage *folder)
{
  std::vector<TCHAR> buf(_tcslen(path) + 1);
  memcpy(&buf.front(), path, buf.size() * sizeof(TCHAR));

  TCHAR *lastSlash = _tcsrchr(&buf.front(), _T('\\'));
  if (lastSlash != NULL) {
    *lastSlash = _T('\0');
    folder->setString(&buf.front());
  } else {
    folder->setString(_T(""));
  }
}
