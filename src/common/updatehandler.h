/*****************************************************************************
* Copyright 2015-2022 Alexander Barthel alex@littlenavmap.org
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#ifndef LITTLENAVMAP_UPDATEHANDLER_H
#define LITTLENAVMAP_UPDATEHANDLER_H

#include "options/optiondata.h"

#include <QObject>
#include <QTimer>

namespace atools {
namespace util {
class UpdateCheck;
struct Update;
typedef QVector<atools::util::Update> UpdateList;
}
}

class MainWindow;

/*
 * Update handler that drives all checks for updates and show dialogs,
 * stores versions to skip, maintains a timestamp, etc.
 */
class UpdateHandler :
  public QObject
{
  Q_OBJECT

public:
  explicit UpdateHandler(MainWindow *parent = nullptr);
  virtual ~UpdateHandler() override;

  UpdateHandler(const UpdateHandler& other) = delete;
  UpdateHandler& operator=(const UpdateHandler& other) = delete;

  enum UpdateReason
  {
    UPDATE_REASON_UNKNOWN,
    UPDATE_REASON_MANUAL, /* Manual check - show "no updates found" dialog */
    UPDATE_REASON_FORCE, /* Debug option - always shows dialog */
    UPDATE_REASON_TIMER, /* Internal timer event. Not checking if disabled or a modal dialog is opened. */
    UPDATE_REASON_STARTUP /* Also triggers timer */
  };

  Q_ENUM(UpdateReason)

  /*
   * Check asynchronously for updates and call updateFound if anything was found.
   * Skips updates that are in the skip list in settings and also checks the timestamp for the last update.
   *
   * @param channelOpts channels to check. Stable, beta or develop.
   * @param manuallyTriggered Ignores timestamp and shows also a dialog if nothing was found.
   * Also ignores the skip list.
   */
  void checkForUpdates(UpdateReason reason);

  /* Disable or enable update check on timer events */
  void disableUpdateCheck()
  {
    enabled = false;
  }

  void enableUpdateCheck()
  {
    enabled = true;
  }

  void setChannelOpts(opts::UpdateChannels value)
  {
    channelOpts = value;
  }

private:
  void updateFound(atools::util::UpdateList updates);
  void updateFailed(QString errorString);
  void updateCheckTimeout();

  /* Run every 15 minues and checks but not if disabled or modal dialogs visible */
  QTimer updateTimer;

  opts::UpdateChannels channelOpts;
  UpdateReason updateReason = UPDATE_REASON_UNKNOWN;
  bool enabled = true;
  atools::util::UpdateCheck *updateCheck;
  MainWindow *mainWindow = nullptr;
};

#endif // LITTLENAVMAP_UPDATEHANDLER_H
