/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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
#include "util/timedcache.h"

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

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
   * @return NOAA metar from cache or empty if not entry was found in the cache. Once the request was
   * completed the signal weatherUpdated is emitted and calling this method again will return the metar.
   */
  QString getNoaaMetar(const QString& airportIcao);

  /*
   * @return VATSIM metar from cache or empty if not entry was found in the cache. Once the request was
   * completed the signal weatherUpdated is emitted and calling this method again will return the metar.
   */
  QString getVatsimMetar(const QString& airportIcao);

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
  bool testUrl(const QString& url, const QString& airportIcao, QString& result);

  enum ActiveSkyType
  {
    NONE, /* not found */
    MANUAL, /* Snapshot file is manually selected */
    ASN, /* Active Sky Next */
    AS16
  };

  /* Get type of active sky weather snapshot that was found */
  ActiveSkyType getCurrentActiveSkyType()
  {
    return activeSkyType;
  }

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
  /* Emitted when Active Sky file changes or a request to weather was fullfilled */
  void weatherUpdated();

private:
  // Update online reports if older than 15 minutes
  static Q_CONSTEXPR int WEATHER_TIMEOUT_SECS = 300;

  void activeSkyWeatherFileChanged(const QString& path);

  void loadActiveSkySnapshot(const QString& path);
  void loadActiveSkyFlightplanSnapshot(const QString& path);
  void initActiveSkyNext();
  void findActiveSkyFiles(QString& asnSnapshot, QString& flightplanSnapshot, const QString& activeSkyPrefix);

  void loadNoaaMetar(const QString& airportIcao);
  void loadVatsimMetar(const QString& airportIcao);

  void httpFinished(QNetworkReply *reply, const QString& icao,
                    atools::util::TimedCache<QString, QString>& metars);
  void httpFinishedNoaa();
  void httpFinishedVatsim();

  void cancelNoaaReply();
  void cancelVatsimReply();
  void flushRequestQueue();
  bool validateActiveSkyFlightplanFile(const QString& path);

  QHash<QString, QString> activeSkyMetars;
  QString activeSkyDepartureMetar, activeSkyDestinationMetar,
          activeSkyDepartureIdent, activeSkyDestinationIdent;

  atools::util::TimedCache<QString, QString> noaaCache, vatsimCache;

  QString activeSkySnapshotPath;
  QFileSystemWatcher *fsWatcher = nullptr;
  QNetworkAccessManager networkManager;
  atools::fs::FsPaths::SimulatorType simType = atools::fs::FsPaths::UNKNOWN;

  // Stores the request ICAO so we can send it to httpFinished()
  QString noaaRequestIcao, vatsimRequestIcao;

  // Keeps the reply
  QNetworkReply *noaaReply = nullptr, *vatsimReply = nullptr;
  QStringList noaaRequests, vatsimRequests;

  MainWindow *mainWindow;
  QTimer flushQueueTimer;

  ActiveSkyType activeSkyType = NONE;
  QString asPath, asFlightplanPath;

};

#endif // LITTLENAVMAP_WEATHERREPORTER_H
