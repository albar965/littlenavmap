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

#ifndef WEATHERLOADER_H
#define WEATHERLOADER_H

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
  WeatherReporter(MainWindow *parentWindow);
  virtual ~WeatherReporter();

  QString getAsnMetar(const QString& airportIcao);
  QString getNoaaMetar(const QString& airportIcao);
  QString getVatsimMetar(const QString& airportIcao);

  bool hasAsnWeather()
  {
    return !asnSnapshotPath.isEmpty();
  }

  void clearNoaaReply();

signals:
  void weatherUpdated();

private:
  void fileChanged(const QString& path);
  void loadVatsimMetar(const QString& airportIcao);
  void loadActiveSkySnapshot();
  void loadNoaaMetar(const QString& airportIcao);

  QHash<QString, QString> asnMetars, noaaMetars, vatsimMetars;
  QString asnSnapshotPath;
  QString getAsnSnapshotPath();

  QFileSystemWatcher *fsWatcher = nullptr;

  QNetworkAccessManager networkManager;

  void httpFinished(QNetworkReply *reply, const QString& icao, QHash<QString, QString>& metars);

  QString noaaRequestIcao, vatsimRequestIcao;

  QNetworkReply *noaaReply = nullptr, *vatsimReply = nullptr;

  void httpFinishedNoaa();
  void httpFinishedVatsim();

  void clearVatsimReply();

};

#endif // WEATHERLOADER_H
