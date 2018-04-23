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

#ifndef LNM_ONLINECONTROLLER_H
#define LNM_ONLINECONTROLLER_H

#include <QDateTime>
#include <QObject>
#include <QTimer>

#include "query/querytypes.h"

class MapLayer;

namespace Marble {
class GeoDataLatLonBox;
}

namespace atools {
namespace util {
class HttpDownloader;
}
namespace sql {
class SqlDatabase;
class SqlRecord;
}

namespace fs {
namespace sc {
class SimConnectAircraft;
}
namespace online {
class OnlinedataManager;
}
}
}

class MainWindow;
class QTextCodec;

/*
 * Manages recurring download of online network data from the status.txt and whazzup.txt files.
 * Uses options to determine how to download data.
 */
class OnlinedataController :
  public QObject
{
  Q_OBJECT

public:
  OnlinedataController(atools::fs::online::OnlinedataManager *onlineManager, MainWindow *parent);
  virtual ~OnlinedataController();

  /* Start download and after downloading timer which triggers recurring download */
  void startProcessing();

  atools::sql::SqlDatabase *getDatabase();

  /* Options dialog changed settings. Will trigger download. */
  void optionsChanged();

  const QDateTime& getLastUpdateTime() const
  {
    return lastUpdateTime;
  }

  bool hasData() const;

  /* VATSIM, IVAO or Custom */
  QString getNetwork() const;
  bool isNetworkActive() const;

  /* Get aircraft within bounding rectangle. Objects are cached. */
  const QList<atools::fs::sc::SimConnectAircraft> *getAircraft(const Marble::GeoDataLatLonBox& rect,
                                                               const MapLayer *mapLayer, bool lazy);

  /* Get aircraft from last bounding rectangle query from cache. */
  const QList<atools::fs::sc::SimConnectAircraft> *getAircraftFromCache();

  /* Fill aircraft object from table client */
  void getClientAircraftById(atools::fs::sc::SimConnectAircraft& aircraft, int id);

  static void fillAircraftFromClient(atools::fs::sc::SimConnectAircraft& ac, const atools::sql::SqlRecord& record);

  /* Get client record with all field values */
  atools::sql::SqlRecord getClientRecordById(int clientId);

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();

  int getNumClients() const;

signals:
  /* Sent whenever new data was downloaded */
  void onlineClientAndAtcUpdated(bool loadAll, bool keepSelection);
  void onlineServersUpdated(bool loadAll, bool keepSelection);

  /* Sent when network changes via options dialog */
  void onlineNetworkChanged();

private:
  /* HTTP download signal slots */
  void downloadFinished(const QByteArray& data, QString url);
  void downloadFailed(const QString& error, QString url);

  void startDownloadInternal();
  void startDownloadTimer();
  void stopAllProcesses();

  /* Show message from status.txt */
  void showMessageDialog();

  atools::fs::online::OnlinedataManager *manager;
  atools::util::HttpDownloader *downloader;
  MainWindow *mainWindow;

  enum State
  {
    NONE, /* Not downloading anything */
    DOWNLOADING_STATUS, /* Downloading status.txt */
    DOWNLOADING_WHAZZUP, /* Downloading whazzup.txt */
    DOWNLOADING_WHAZZUP_SERVERS /* Downloading servers */
  };

  State currentState = NONE;

  QTimer downloadTimer; /* Triggers recurring downloads */

  /* Used to check server downloads and limit them to 15 minutes */
  QDateTime lastServerDownload;

  /*  Last update from whazzup */
  QDateTime lastUpdateTime;

  /* Set after parsing status.txt to indicate compressed file */
  bool whazzupGzipped = false;

  QTextCodec *codec = nullptr;

  SimpleRectCache<atools::fs::sc::SimConnectAircraft> aircraftCache;
  atools::sql::SqlQuery *aircraftByRectQuery = nullptr;
};

#endif // LNM_ONLINECONTROLLER_H
