/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#include "util/properties.h"

#include <QObject>
#include <QTimer>

class DataExchangeFetcher;

class QSharedMemory;

/*
 * Implements a mechanism similar to the old Windows DDE to pass parameters to a running instance from another starting instance.
 * Uses shared memory to attach to a running instance which regularily checks the shared memory for messages.
 * A timestamp is saved and updated periodically in a thread to avoid dead or crashed instances blocking further startups.
 */
class DataExchange :
  public QObject
{
  Q_OBJECT

public:
  /* Creates or attaches to a shared memory segment and sets exit flag.
   * Sends a message to the other instance if attaching.
   *  Sets a timestamp if creating. */
  explicit DataExchange(bool verboseParam, const QString& programGuid);

  /* Detaches and deletes shared memory */
  virtual ~DataExchange() override;

  /* Found other instance and sent message. This instance can exit now. */
  bool isExit() const
  {
    return exit;
  }

  /* Start the timer which updates the timestamp in the shared memory segment.
   * Sends messages below if another instance left messages. */
  void startTimer();

signals:
  /* Sent if time found messages from other instance. */
  void loadLayout(const QString& filename);
  void loadRoute(const QString& filename);
  void loadRouteDescr(const QString& description);
  void loadPerf(const QString& filename);
  void quit();

  /* Activate and raise main window */
  void activateMain();

private:
  /* Called by DataExchangeFetcher::dataFetched() */
  void loadData(atools::util::Properties properties);

  /* Found other instance if true. This one can exit now. */
  bool exit = false;

  /* Check shared memory for updates by other instance every second. Calls checkSharedMemory()
   * Shared memory layout:
   * - quint32 Size of serialized property list
   * - qint64 Timestamp milliseconds since Epoch
   * - properties Serialized properties object */
  QSharedMemory *sharedMemory = nullptr;

  /* Worker object for dataFetcherThread checking for messages and updates timestamp. */
  DataExchangeFetcher *dataFetcher = nullptr;
  QThread *dataFetcherThread = nullptr;
  bool verbose = false;
};

/* Private worker object for dataFetcherThread */
class DataExchangeFetcher
  : public QObject
{
  Q_OBJECT

public:
  explicit DataExchangeFetcher(bool verboseParam);
  virtual ~DataExchangeFetcher() override;

  /* Starts check for messages and timestamp. Called by QThread::started() */
  void startTimer();

  void setSharedMemory(QSharedMemory *value)
  {
    sharedMemory = value;
  }

signals:
  void dataFetched(atools::util::Properties properties);

private:
  /* Fetch messages from shared memory sent by other instance, read and check files and send messages above.
   * Called by timer->timeout() in thread context. */
  void fetchSharedMemory();

  QTimer *timer;

  /* Pointer to SHM */
  QSharedMemory *sharedMemory = nullptr;
  bool verbose = false;
};

#endif // LNM_DATAEXCHANGE_H
