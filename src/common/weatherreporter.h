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

class WeatherReporter :
  public QObject
{
  Q_OBJECT

public:
  WeatherReporter(MainWindow *parentWindow, atools::fs::FsPaths::SimulatorType type);
  virtual ~WeatherReporter();

  QString getAsnMetar(const QString& airportIcao);
  QString getNoaaMetar(const QString& airportIcao);
  QString getVatsimMetar(const QString& airportIcao);

  bool hasAsnWeather()
  {
    return !asnSnapshotPath.isEmpty();
  }

  void preDatabaseLoad();
  void postDatabaseLoad(atools::fs::FsPaths::SimulatorType type);

signals:
  void weatherUpdated();

private:
  struct Report
  {
    QString metar;
    QDateTime reportTime;
  };

  void fileChanged(const QString& path);
  void loadVatsimMetar(const QString& airportIcao);
  void loadActiveSkySnapshot();
  void loadNoaaMetar(const QString& airportIcao);

  QHash<QString, QString> asnMetars;
  QHash<QString, Report> noaaMetars, vatsimMetars;
  QString asnSnapshotPath;
  QString getAsnSnapshotPath();

  QFileSystemWatcher *fsWatcher = nullptr;

  QNetworkAccessManager networkManager;

  void httpFinished(QNetworkReply *reply, const QString& icao, QHash<QString, Report>& metars);

  atools::fs::FsPaths::SimulatorType simType = atools::fs::FsPaths::UNKNOWN;
  QString noaaRequestIcao, vatsimRequestIcao;

  QNetworkReply *noaaReply = nullptr, *vatsimReply = nullptr;
  MainWindow *mainWindow;
  void httpFinishedNoaa();
  void httpFinishedVatsim();

  void clearVatsimReply();
  void clearNoaaReply();

  void initActiveSkyNext();

};

#endif // LITTLENAVMAP_WEATHERLOADER_H
