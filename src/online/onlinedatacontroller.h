/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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
#include "fs/online/onlinetypes.h"

class MapLayer;

namespace Marble {
class GeoDataLatLonBox;
}

namespace atools {

namespace geo {
class Pos;
}
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

namespace map {
struct MapSearchResult;

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

  /* User airspaces where updated. Will trigger download to update geometry in databases. */
  void userAirspacesUpdated();

  const QDateTime& getLastUpdateTime() const
  {
    return lastUpdateTime;
  }

  bool hasData() const;

  /* VATSIM, IVAO or Custom (localized) */
  QString getNetworkTranslated() const;

  /* VATSIM, IVAO or Custom (not localized) */
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

  /* Removes the online aircraft from onlineAircraft which also have a simulator shadow in simAircraft */
  void filterOnlineShadowAircraft(QList<map::MapOnlineAircraft>& onlineAircraft,
                                  const QList<map::MapAiAircraft>& simAircraft);

  /* Get client record with all field values */
  atools::sql::SqlRecord getClientRecordById(int clientId);

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();

  int getNumClients() const;

  /* Get an online network aircraft that has the same registration as the simulator aircraft and is close by */
  bool getShadowAircraft(atools::fs::sc::SimConnectAircraft& onlineClient,
                         const atools::fs::sc::SimConnectAircraft& simAircraft);

  /* True if there is an online network aircraft that has the same registration as the simulator aircraft and is close.
   * Used by connect client to set the flag in the simulator data. */
  bool isShadowAircraft(const atools::fs::sc::SimConnectAircraft& simAircraft);

signals:
  /* Sent whenever new data was downloaded */
  void onlineClientAndAtcUpdated(bool loadAll, bool keepSelection);
  void onlineServersUpdated(bool loadAll, bool keepSelection);

  /* Sent when network changes via options dialog */
  void onlineNetworkChanged();

private:
  /* HTTP download signal slots */
  void downloadFinished(const QByteArray& data, QString url);
  void downloadFailed(const QString& error, int errorCode, QString url);
  void downloadSslErrors(const QStringList& errors, const QString& downloadUrl);
  void statusBarMessage();

  void startDownloadInternal();
  void startDownloadTimer();
  void stopAllProcesses();
  void updateAtcSizes();

  /* Show message from status.txt */
  void showMessageDialog();

  /* Tries to fetch geometry for atc centers from the user geometry database from cache */
  atools::geo::LineString *geometryCallback(const QString& callsign, atools::fs::online::fac::FacilityType type);

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

  /* Simulator aircraft registrations and positions */
  QHash<QString, atools::geo::Pos> simulatorAiRegistrations;

  QHash<QString, atools::geo::Pos> clientCallsignAndPosMap;

  query::SimpleRectCache<atools::fs::sc::SimConnectAircraft> aircraftCache;
  atools::sql::SqlQuery *aircraftByRectQuery = nullptr;
};

#endif // LNM_ONLINECONTROLLER_H
