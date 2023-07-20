/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_DATAEXCHANGE_H
#define LNM_DATAEXCHANGE_H

#include <QObject>
#include <QTimer>

namespace atools {
namespace util {
class Properties;
}
}

class QSharedMemory;

/*
 * Implements a mechanism similar to the old Windows DDE to pass parameters to a running instance from another starting instance.
 * Uses shared memory to attach to a running instance which regularily checks the shared memory for messages.
 * A timestamp is saved to avoid dead or crashed instances blocking further startups.
 */
class DataExchange :
  public QObject
{
  Q_OBJECT

public:
  /* Creates or attaches to a shared memory segment and sets exit flag.
   * Sends a message to the other instance if attaching.
   *  Sets a timestamp if creating. */
  explicit DataExchange();

  /* Detaches and deletes shared memory */
  virtual ~DataExchange() override;

  /* Found other instance and sent message. This instance can exit now. */
  bool isExit() const
  {
    return exit;
  }

  /* Start the timer which updates the timestamp in the shared memory segment. Sends messages below if another instance left messages.
   * Currently twice a second. */
  void startTimer();

signals:
  /* Sent if time found messages from other instance. */
  void loadLayout(const QString& filename);
  void loadRoute(const QString& filename);
  void loadRouteDescr(const QString& description);
  void loadPerf(const QString& filename);

  /* Activate and raise main window */
  void activateMain();

private:
  /* Fetch messages from shared memory, read and check files and send messages above. */
  atools::util::Properties fetchSharedMemory();

  /* Called by sharedMemoryCheckTimer and checks for messages from other instances */
  void checkData();

  /* Found other instance if true. This one can exit now. */
  bool exit = false;

  /* Check shared memory for updates by other instance every second. Calls checkSharedMemory() */
  QTimer sharedMemoryCheckTimer;
  QSharedMemory *sharedMemory = nullptr;
};

#endif // LNM_DATAEXCHANGE_H
