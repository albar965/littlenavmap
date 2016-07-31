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

#ifndef LITTLENAVMAP_WEATHERLOADER_H
#define LITTLENAVMAP_WEATHERLOADER_H

#include "fs/fspaths.h"

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>

class QFileSystemWatcher;
class MainWindow;

/*
 * Provides a source of metar data for airports. Supports ActiveSkyNext, NOAA and VATSIM weather.
 * The ASN weather file if monitored for changes and the signal weatherUpdated will be emitted if
 * the file has changed.
 * NOAA and VATSIM start a request in background and emit the signal weatherUpdated.
 *
 * Uses hashmaps to cache requests. Cache entries will timeout after 15 minutes.
 *
 * Only one request is done. If a request is already waiting a new one will cancel the old one.
 */
// TODO better support for mutliple simulators
class WeatherReporter :
  public QObject
{
  Q_OBJECT

public:
  /* @param type flight simulator type needed to find ASN weather file. */
  WeatherReporter(MainWindow *parentWindow, atools::fs::FsPaths::SimulatorType type);
  virtual ~WeatherReporter();

  /*
   * @return ASN metar or empty if ASN was not found or the airport has no weather report
   */
  QString getAsnMetar(const QString& airportIcao);

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

  bool isAsnDefaultPathFound()
  {
    return !asnSnapshotPath.isEmpty();
  }

  /* Does nothing currently */
  void preDatabaseLoad();

  /* Will reload new ASN data for the changed simulator type, but only if the path was not set manually */
  void postDatabaseLoad(atools::fs::FsPaths::SimulatorType type);

  /* Options dialog changed settings. Will reinitialize ASN file */
  void optionsChanged();

  /* Return true if the file at the given path exists and has valid content */
  static bool validateAsnFile(const QString& path);

signals:
  /* Emitted when ASN file changes or a request to weather was fullfilled */
  void weatherUpdated();

private:
  // Update online reports if older than 15 minutes
  static Q_CONSTEXPR int WEATHER_TIMEOUT_SECS = 900;

  // Report allowing timeouts
  struct Report
  {
    QString metar;
    QDateTime reportTime;
  };

  void asnWeatherFileChanged(const QString& path);

  void loadActiveSkySnapshot(const QString& path);
  void initActiveSkyNext();
  QString findAsnSnapshotPath();

  void loadNoaaMetar(const QString& airportIcao);
  void loadVatsimMetar(const QString& airportIcao);

  void httpFinished(QNetworkReply *reply, const QString& icao, QHash<QString, Report>& metars);
  void httpFinishedNoaa();
  void httpFinishedVatsim();

  void cancelNoaaReply();
  void cancelVatsimReply();

  QHash<QString, QString> asnMetars;
  QHash<QString, Report> noaaMetars, vatsimMetars;

  QString asnSnapshotPath;
  QFileSystemWatcher *fsWatcher = nullptr;
  QNetworkAccessManager networkManager;
  atools::fs::FsPaths::SimulatorType simType = atools::fs::FsPaths::UNKNOWN;

  // Stores the request ICAO so we can send it to httpFinished()
  QString noaaRequestIcao, vatsimRequestIcao;

  // Keeps the reply
  QNetworkReply *noaaReply = nullptr, *vatsimReply = nullptr;
  MainWindow *mainWindow;

};

#endif // LITTLENAVMAP_WEATHERLOADER_H
