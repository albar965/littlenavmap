/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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
  virtual ~UpdateHandler();

  /*
   * Check asynchronously for updates and call updateFound if anything was found.
   * Skips updates that are in the skip list in settings and also checks the timestamp for the last update.
   *
   * @param channelOpts channels to check. Stable, beta or develop.
   * @param manuallyTriggered Ignores timestamp and shows also a dialog if nothing was found.
   * Also ignores the skip list.
   */
  void checkForUpdates(opts::UpdateChannels channelOpts, bool manuallyTriggered);

private:
  void updateFound(atools::util::UpdateList updates);
  void updateFailed(QString errorString);

  bool manual = false;
  atools::util::UpdateCheck *updateCheck;
  MainWindow *mainWindow = nullptr;
};

#endif // LITTLENAVMAP_UPDATEHANDLER_H
