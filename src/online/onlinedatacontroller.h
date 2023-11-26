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

#include "fs/online/onlinetypes.h"
#include "geo/spatialindex.h"
#include "query/querytypes.h"

#include <QDateTime>
#include <QObject>
#include <QTimer>

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
struct MapResult;

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
  explicit OnlinedataController(atools::fs::online::OnlinedataManager *onlineManager, MainWindow *parent);
  virtual ~OnlinedataController() override;

  OnlinedataController(const OnlinedataController& other) = delete;
  OnlinedataController& operator=(const OnlinedataController& other) = delete;

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
                                                               const MapLayer *mapLayer, bool lazy, bool& overflow);

  /* Get aircraft from last bounding rectangle query from cache. */
  const QList<atools::fs::sc::SimConnectAircraft> *getAircraftFromCache();

  /* Fill aircraft object from table "client" in database. */
  atools::fs::sc::SimConnectAircraft getClientAircraftById(int id);

  /* Fill from a record based on table "client". Tries to get sim shadow aircraft and fill additional fields. */
  void fillAircraftFromClient(atools::fs::sc::SimConnectAircraft& ac, const atools::sql::SqlRecord& record);

  /* Removes the online aircraft from "onlineAircraft" which also have a simulator shadow in "simAircraft" */
  void removeOnlineShadowedAircraft(QList<map::MapOnlineAircraft>& onlineAircraftList, const QList<map::MapAiAircraft>& simAircraftList);

  /* Get client record with all field values */
  atools::sql::SqlRecord getClientRecordById(int clientId);

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();

  /* Get number of online clients/aircraft */
  int getNumClients() const;

  /* Get an online network aircraft for given simulator shadow aircraft with updated position */
  atools::fs::sc::SimConnectAircraft getShadowedOnlineAircraft(const atools::fs::sc::SimConnectAircraft& simAircraft);

  /* Get an simulator shadow aircraft for given online aircraft id */
  const atools::fs::sc::SimConnectAircraft& getShadowSimAircraft(int onlineId);

  /* Modify AI and user aircraft and set shadow flag if a online network aircraft with the same id exists.
   * Also stores simulator aircraft data to maintain spatial index.
   * Called by ConnectClient after receiving simulator data package. */
  void updateAircraftShadowState(atools::fs::sc::SimConnectData& dataPacket);

  /* Print the size of all container classes to detect overflow or memory leak conditions */
  void debugDumpContainerSizes() const;

signals:
  /* Sent whenever new data was downloaded */
  void onlineClientAndAtcUpdated(bool loadAll, bool keepSelection, bool force);
  void onlineServersUpdated(bool loadAll, bool keepSelection, bool force);

  /* Sent when network changes via options dialog */
  void onlineNetworkChanged();

private:
  /* True if there is an online network aircraft that has similar position and altitude as the simulator aircraft. */
  bool isShadowAircraft(const atools::fs::sc::SimConnectAircraft& simAircraft);

  /* HTTP download signal slots for all possible files/URLs */
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
  QString uncompress(const QByteArray& data, const QString& func, bool utf8);
  void startDownloader();

  /* Tries to fetch geometry for atc centers from the user geometry database from cache */
  const atools::geo::LineString *airspaceGeometryCallback(const QString& callsign, atools::fs::online::fac::FacilityType type);

  /* Called after each download */
  void updateShadowIndex();
  void clearShadowIndexes();

  /* Return online aircraft for simulator aircraft based on distance and other parameter similarity */
  atools::fs::online::OnlineAircraft shadowAircraftInternal(const atools::fs::sc::SimConnectAircraft& simAircraft);

  /* Database manager */
  atools::fs::online::OnlinedataManager *manager;

  /* Downloader for all files */
  atools::util::HttpDownloader *downloader;

  MainWindow *mainWindow;

  /* State is set before triggering the download and clear on the last download in the chain.
   *  Set to NONE while timeout for whazzup download is running. */
  enum State
  {
    NONE, /* Not downloading anything */
    DOWNLOADING_STATUS, /* Downloading status.txt */
    DOWNLOADING_WHAZZUP, /* Downloading whazzup.txt or JSON file */
    DOWNLOADING_TRANSCEIVERS, /* Downloading transceivers-data-fmt.json */
    DOWNLOADING_WHAZZUP_SERVERS /* Downloading servers */
  };

  QString stateAsStr(OnlinedataController::State state);

  // Criteria used to detect shadow aircraft right after download finished
  float maxShadowDistanceNm = 0.5f, maxShadowAltDiffFt = 500.f, maxShadowGsDiffKts = 30.f, maxShadowHdgDiffDeg = 20.f;

  State currentState = NONE;

  QTimer downloadTimer; /* Triggers recurring downloads OnlinedataController::startDownloadInternal */

  /* Used to check server downloads and limit them to 15 minutes */
  QDateTime lastServerDownload;

  /*  Last update from whazzup only used to display in the user interface */
  QDateTime lastUpdateTime;

  /*  Last update from transceivers - used to trigger a transceivers download */
  QDateTime lastUpdateTimeTransceivers;

  QString whazzupUrlFromStatus;

  QTextCodec *codec = nullptr;

  bool verbose = false;

  // All online aircraft from download for spatial search (nearest)
  atools::geo::SpatialIndex<atools::fs::online::OnlineAircraft> onlineAircraftSpatialIndex;

  // Keys use either online database semi-permanent id or object ID from simulator. Includes user
  // modeS_id for X-Plane: integer 24bit (0-16777215 or 0 - 0xFFFFFF) unique ID of the airframe. This is also known as the ADS-B "hexcode".
  // dwObjectID for SimConnect
  // Table and column "client.client_id" for online aircraft. Calculated as semi-permanent id. */
  QHash<int, int> aircraftIdSimToOnline, // All shadow aircraft mapped from sim key to online value
                  aircraftIdOnlineToSim; // Shadow aircraft mapped from online key to sim value

  // Time series of all data received from simulator. Needed to get a set of aircraft which
  // fit to the last update time of the downloaded whazzup file
  QMap<QDateTime, atools::fs::sc::SimConnectData> currentDataPacketMap;

  // Cache used for map display
  query::SimpleRectCache<atools::fs::sc::SimConnectAircraft> aircraftCache;
  atools::sql::SqlQuery *aircraftByRectQuery = nullptr;
};

#endif // LNM_ONLINECONTROLLER_H
