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

#ifndef LITTLENAVMAP_WEATHERREPORTER_H
#define LITTLENAVMAP_WEATHERREPORTER_H

#include "fs/fspaths.h"

#include <QHash>
#include <QObject>

namespace map {
struct MapAirport;
}
namespace atools {

namespace util {

class FileChecker;
class FileSystemWatcher;
}
namespace geo {
class Pos;
}
namespace fs {
namespace weather {
struct MetarResult;

class Metar;

class WeatherNetSingle;
class WeatherNetDownload;
class XpWeatherReader;
class NoaaWeatherDownloader;
}
}
}

class MainWindow;

/*
 * Provides a source of metar data for airports. Supports ActiveSkyNext, NOAA and VATSIM weather.
 * The Active Sky (Next and 16) weather files are monitored for changes and the signal
 * weatherUpdated will be emitted if the file has changed.
 * NOAA and VATSIM start a request in background and emit the signal weatherUpdated.
 *
 * Uses hashmaps to cache online requests. Cache entries will timeout after 15 minutes.
 *
 * Only one request is done. If a request is already waiting a new one will cancel the old one.
 */
// TODO better support for mutliple simulators
class WeatherReporter :
  public QObject
{
  Q_OBJECT

public:
  /* @param type flight simulator type needed to find Active Sky weather file. */
  explicit WeatherReporter(MainWindow *parentWindow, atools::fs::FsPaths::SimulatorType type);
  virtual ~WeatherReporter() override;

  WeatherReporter(const WeatherReporter& other) = delete;
  WeatherReporter& operator=(const WeatherReporter& other) = delete;

  /*
   * @return Active Sky metar or empty if Active Sky was not found or the airport has no weather report
   */
  QString getActiveSkyMetar(const QString& airportIcao);

  /*
   * @return X-Plane metar or empty if file not found or X-Plane base directory not found. Gives the nearest
   * weather if station has no weather report.
   */
  atools::fs::weather::MetarResult getXplaneMetar(const QString& station, const atools::geo::Pos& pos);

  /*
   * @return NOAA metar from cache or empty if not entry was found in the cache. Once the request was
   * completed the signal weatherUpdated is emitted and calling this method again will return the metar.
   */
  atools::fs::weather::MetarResult getNoaaMetar(const QString& airportIcao, const atools::geo::Pos& pos);

  /*
   * @return VATSIM metar from cache or empty if not entry was found in the cache. Once the request was
   * completed the signal weatherUpdated is emitted and calling this method again will return the metar.
   */
  atools::fs::weather::MetarResult getVatsimMetar(const QString& airportIcao, const atools::geo::Pos& pos);

  /*
   * @return IVAO metar from downloaded file or empty if airport has not report.
   */
  atools::fs::weather::MetarResult getIvaoMetar(const QString& airportIcao, const atools::geo::Pos& pos);

  /* For display. Source depends on settings and parsed objects are cached. */
  atools::fs::weather::Metar getAirportWeather(const map::MapAirport& airport, bool stationOnly);

  /* Get wind at airport. No nearest values for stationOnly=true. */
  void getAirportWind(int& windDirectionDeg, float& windSpeedKts, const map::MapAirport& airport, bool stationOnly);

  /* Gives preferred runways with title text like "Prefers runway:". Runways might be grouped. */
  void getBestRunwaysTextShort(QString& title, QString& runwayNumbers, QString& sourceText, const map::MapAirport& airport);

  /* Does nothing currently */
  void preDatabaseLoad();

  /* Will reload new Active Sky data for the changed simulator type, but only if the path was not set manually */
  void postDatabaseLoad(atools::fs::FsPaths::SimulatorType type);

  /* Options dialog changed settings. Will reinitialize Active Sky file */
  void optionsChanged();

  /* Return true if the file at the given path exists and has valid content */
  static bool validateActiveSkyFile(const QString& path);

  /*
   * Test the weather server URL synchronously
   * @param url URL containing a %1 placeholder for the metar
   * @param url an airport ICAO
   * @param result metar if successfull - otherwise error message
   * @return true if successfull
   */
  static bool testUrl(QStringList& result, const QString& url, const QString& airportIcao,
                      const QHash<QString, QString>& headerParameters = QHash<QString, QString>());

  enum ActiveSkyType
  {
    NONE, /* not found */
    MANUAL, /* Snapshot file is manually selected for all simulators */
    ASN, /* Active Sky Next */
    AS16,
    ASP4, /* Active Sky for Prepar3D v4 */
    ASP5, /* Active Sky for Prepar3D v5 */
    ASXPL11, /* Active Sky for X-Plane 11 */
    ASXPL12 /* Active Sky for X-Plane 12 */
  };

  /* Get type of active sky weather snapshot that was found */
  ActiveSkyType getCurrentActiveSkyType() const
  {
    return activeSkyType;
  }

  bool hasAnyActiveSkyWeather() const
  {
    return activeSkyType != WeatherReporter::NONE;
  }

  /* ASN, AS16, ASP4, ... */
  QString getCurrentActiveSkyName() const;

  atools::fs::FsPaths::SimulatorType getSimType() const
  {
    return simType;
  }

  const QString& getActiveSkyDepartureIdent();

  const QString& getActiveSkyDestinationIdent();

  /* Map weather source has changed */
  void updateAirportWeather();

  /* Print the size of all container classes to detect overflow or memory leak conditions */
  void debugDumpContainerSizes() const;

signals:
  /* Emitted when Active Sky or X-Plane weather file changes or a request to weather was fullfilled */
  void weatherUpdated();

private:
  void weatherDownloadFailed(const QString& error, int errorCode, QString url);
  void weatherDownloadSslErrors(const QStringList& errors, const QString& downloadUrl);

  /* Called by file watcher */
  void activeSkyWeatherFilesChanged(const QStringList& paths);
  void xplaneWeatherFileChanged();

  /* Clear watcher, METARs and detect paths */
  void initActiveSkyPaths();
  void findActiveSkyFiles(QString& asnSnapshot, QString& flightplanSnapshot, const QString& activeSkyPrefix,
                          const QString& activeSkySimSuffix);

  /* Load METARs from all AS files on demand if not yet loaded after initActiveSkyPaths(). Create file watcher. */
  void loadAllActiveSkyFiles();

  /* Load METARs from files */
  void loadActiveSkySnapshot(const QString& path);
  void loadActiveSkyFlightplanSnapshot(const QString& path);

  bool validateActiveSkyFlightplanFile(const QString& path);
  void createActiveSkyFsWatcher();
  void deleteActiveSkyFsWatcher();

  void initXplane();
  void disableXplane();

  /* From download finished signals */
  void noaaWeatherUpdated();
  void ivaoWeatherUpdated();
  void vatsimWeatherUpdated();

  /* Call process events to minimize program stutters */
  void weatherDownloadProgress(qint64 bytesReceived, qint64 bytesTotal, QString downloadUrl);

  /* Reset the error timer in all weather downloaders */
  void resetErrorState();

  /* Check if currently selected X-Plane paths exist and return true if valid */
  bool checkXplanePaths();

  /* Show warning dialog in main loop to avoid issues when being called from draw handler */
  void showXplaneWarningDialog(const QString& message);

  atools::geo::Pos fetchAirportCoordinates(const QString& metarAirportIdent);

  /* Update IVAO and NOAA timeout periods - timeout is disable if weather services are not used */
  void updateTimeouts();

  atools::fs::weather::NoaaWeatherDownloader *noaaWeather = nullptr;
  atools::fs::weather::WeatherNetDownload *vatsimWeather = nullptr;
  atools::fs::weather::WeatherNetDownload *ivaoWeather = nullptr;

  QHash<QString, QString> activeSkyMetars;
  QString activeSkyDepartureMetar, activeSkyDestinationMetar,
          activeSkyDepartureIdent, activeSkyDestinationIdent;

  QString activeSkySnapshotPath;
  atools::util::FileSystemWatcher *fsWatcherAsPath = nullptr;
  atools::util::FileSystemWatcher *fsWatcherAsFlightplanPath = nullptr;
  atools::fs::FsPaths::SimulatorType simType = atools::fs::FsPaths::NONE;
  atools::util::FileChecker *asSnapshotPathChecker, *asFlightplanPathChecker;

  atools::fs::weather::XpWeatherReader *xpWeatherReader = nullptr;

  MainWindow *mainWindow;

  ActiveSkyType activeSkyType = NONE;
  QString asSnapshotPath, asFlightplanPath;

  // Remember warnings shown for database and session
  bool xp11WarningPathShown = false, xp12WarningPathShown = false;
  QString xplaneFileWarningMsg, xplaneMissingWarningMsg;

  // Warning dialog is currently open
  bool showingXplaneFileWarning = false;

  /* Update online reports if older than 10 minutes */
  int onlineWeatherTimeoutSecs = 600;

  bool errorReported = false;

  bool verbose = false;
};

#endif // LITTLENAVMAP_WEATHERREPORTER_H
