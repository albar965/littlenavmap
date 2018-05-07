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

#ifndef LITTLENAVMAP_WEATHERREPORTER_H
#define LITTLENAVMAP_WEATHERREPORTER_H

#include "fs/fspaths.h"

#include <QHash>
#include <QObject>

namespace atools {
namespace geo {
class Pos;
}
namespace fs {
namespace weather {
struct MetarResult;

class WeatherNetSingle;
class WeatherNetDownload;
class XpWeatherReader;
}
}
}

class QFileSystemWatcher;
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
  WeatherReporter(MainWindow *parentWindow, atools::fs::FsPaths::SimulatorType type);
  virtual ~WeatherReporter();

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
  QString getVatsimMetar(const QString& airportIcao);

  /*
   * @return IVAO metar from downloaded file or empty if airport has not report.
   */
  atools::fs::weather::MetarResult getIvaoMetar(const QString& airportIcao, const atools::geo::Pos& pos);

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
  static bool testUrl(const QString& url, const QString& airportIcao, QString& result);

  enum ActiveSkyType
  {
    NONE, /* not found */
    MANUAL, /* Snapshot file is manually selected */
    ASN, /* Active Sky Next */
    AS16,
    ASP4 /* Active Sky for Prepar3D v4 */
  };

  /* Get type of active sky weather snapshot that was found */
  ActiveSkyType getCurrentActiveSkyType() const
  {
    return activeSkyType;
  }

  /* ASN, AS16, ASP4, ... */
  QString getCurrentActiveSkyName() const;

  atools::fs::FsPaths::SimulatorType getSimType() const
  {
    return simType;
  }

  const QString& getActiveSkyDepartureIdent() const
  {
    return activeSkyDepartureIdent;
  }

  const QString& getActiveSkyDestinationIdent() const
  {
    return activeSkyDestinationIdent;
  }

signals:
  /* Emitted when Active Sky or X-Plane weather file changes or a request to weather was fullfilled */
  void weatherUpdated();

private:
  void activeSkyWeatherFileChanged(const QString& path);
  void xplaneWeatherFileChanged();

  void loadActiveSkySnapshot(const QString& path);
  void loadActiveSkyFlightplanSnapshot(const QString& path);
  void initActiveSkyNext();
  void findActiveSkyFiles(QString& asnSnapshot, QString& flightplanSnapshot, const QString& activeSkyPrefix,
                          const QString& activeSkySimSuffix);

  bool validateActiveSkyFlightplanFile(const QString& path);
  void deleteFsWatcher();
  void createFsWatcher();
  void initXplane();

  static void noaaIndexParser(QString& icao, QDateTime& lastUpdate, const QString& line);
  static atools::geo::Pos fetchAirportCoordinates(const QString& airportIdent);

  atools::fs::weather::WeatherNetSingle *noaaWeather = nullptr;
  atools::fs::weather::WeatherNetSingle *vatsimWeather = nullptr;
  atools::fs::weather::WeatherNetDownload *ivaoWeather = nullptr;

  QHash<QString, QString> activeSkyMetars;
  QString activeSkyDepartureMetar, activeSkyDestinationMetar,
          activeSkyDepartureIdent, activeSkyDestinationIdent;

  QString activeSkySnapshotPath;
  QFileSystemWatcher *fsWatcher = nullptr;
  atools::fs::FsPaths::SimulatorType simType = atools::fs::FsPaths::UNKNOWN;

  atools::fs::weather::XpWeatherReader *xpWeatherReader = nullptr;

  MainWindow *mainWindow;

  ActiveSkyType activeSkyType = NONE;
  QString asPath, asFlightplanPath;

  /* Update online reports if older than 10 minutes */
  int onlineWeatherTimeoutSecs = 600;

};

#endif // LITTLENAVMAP_WEATHERREPORTER_H
