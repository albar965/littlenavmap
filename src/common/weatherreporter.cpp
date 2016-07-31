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

#include "common/weatherreporter.h"

#include "gui/mainwindow.h"
#include "settings/settings.h"
#include "options/optiondata.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QNetworkReply>
#include <QTimer>
#include <QRegularExpression>

// Checks the first line of an ASN file if it has valid content
const QRegularExpression ASN_VALIDATE_REGEXP("^[A-Z0-9]{3,4}::[A-Z0-9]{3,4} .+$");

using atools::fs::FsPaths;

WeatherReporter::WeatherReporter(MainWindow *parentWindow, atools::fs::FsPaths::SimulatorType type)
  : QObject(parentWindow), simType(type), mainWindow(parentWindow)
{
  initActiveSkyNext();
}

WeatherReporter::~WeatherReporter()
{
  // Remove any outstanding requests
  cancelNoaaReply();
  cancelVatsimReply();

  delete fsWatcher;
}

void WeatherReporter::initActiveSkyNext()
{
  if(fsWatcher != nullptr)
  {
    // Remove watcher just in case the file changes
    fsWatcher->disconnect(fsWatcher, &QFileSystemWatcher::fileChanged, this,
                          &WeatherReporter::asnWeatherFileChanged);
    delete fsWatcher;
    fsWatcher = nullptr;
  }

  QString manualAsnSnapshotPath = OptionData::instance().getWeatherActiveSkyPath();
  asnSnapshotPath = findAsnSnapshotPath();

  // Manual path overrides found path for all simulators
  QString asnPath = manualAsnSnapshotPath.isEmpty() ? asnSnapshotPath : manualAsnSnapshotPath;

  if(!asnPath.isEmpty())
  {
    qDebug() << "Using ASN path" << asnPath;
    loadActiveSkySnapshot(asnPath);
    if(fsWatcher == nullptr)
    {
      // Watch file for changes
      fsWatcher = new QFileSystemWatcher(this);
      fsWatcher->connect(fsWatcher, &QFileSystemWatcher::fileChanged, this,
                         &WeatherReporter::asnWeatherFileChanged);
    }

    if(!fsWatcher->addPath(asnPath))
      qWarning() << "cannot watch" << asnPath;
  }
}

/* Loads complete ASN file into a hash map */
void WeatherReporter::loadActiveSkySnapshot(const QString& path)
{
  // C:\Users\USER\AppData\Roaming\HiFi\ASNFSX\Weather\wx_station_list.txt
  // AGGH::AGGH 261800Z 20002KT 9999 FEW014 SCT027 25/24 Q1009::AGGH 261655Z 2618/2718 VRB03KT 9999 FEW017 SCT028 FM270000
  // 34010KT 9999 -SH SCT019 SCT03 FM271200 VRB03KT 9999 -SH FEW017 SCT028 PROB30 INTER 2703/27010 5000 TSSH SCT016 FEW017CB BKN028
  // T 25 27 31 32 Q 1009 1011 1011 1009::278,11,24.0/267,12,19.0/263,13,16.1/233,12,7.2/290,7,-3.0/338,8,-13.0/348,18,-27.9/9,19,-37.9/26,15,-51.3

  // TODO overrride with settings
  if(path.isEmpty())
    return;

  QFile file(path);
  if(file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    asnMetars.clear();

    QTextStream sceneryCfg(&file);

    QString line;
    while(sceneryCfg.readLineInto(&line))
    {
      QStringList list = line.split("::");
      asnMetars.insert(list.at(0), list.at(1));
    }
    file.close();
  }
  else
    qWarning() << "cannot open" << file.fileName() << "reason" << file.errorString();
}

bool WeatherReporter::validateAsnFile(const QString& path)
{
  bool retval = false;
  QFile file(path);
  if(file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QTextStream sceneryCfg(&file);
    QString line;
    if(sceneryCfg.readLineInto(&line))
      // Check if the first line matches
      if(ASN_VALIDATE_REGEXP.match(line.trimmed()).hasMatch())
        retval = true;
    file.close();
  }
  else
    qWarning() << "cannot open" << file.fileName() << "reason" << file.errorString();
  return retval;
}

QString WeatherReporter::findAsnSnapshotPath()
{
  QString appdata = atools::settings::Settings::instance().getPath();

  QDir dir(appdata);

  QString simPath;
  if(simType == atools::fs::FsPaths::FSX)
    simPath = "ASNFSX";
  else if(simType == atools::fs::FsPaths::FSX_SE)
    simPath = "ASNFSX";
  else if(simType == atools::fs::FsPaths::P3D_V2)
    simPath = "ASNP3D";
  else if(simType == atools::fs::FsPaths::P3D_V3)
    simPath = "ASNP3D";

  if(!simPath.isEmpty())
  {
    // TODO find better way to get to Roaming directory
    if(dir.cdUp() && dir.cd("HiFi") && dir.cd(simPath) && dir.cd("Weather"))
    {
      QString file = dir.filePath("current_wx_snapshot.txt");
      if(QFile::exists(file))
      {
        qDebug() << "found ASN weather file" << file;
        if(validateAsnFile(file))
          return file;
        else
          qWarning() << "is not an ASN weather snapshot file" << file;
      }
      else
        qWarning() << "file does not exist" << file;
    }
    else
      qWarning().noquote().nospace() << "HiFi/" + simPath + "/Weather not found";
  }
  else
    qWarning() << "Invalid simulator type" << simType;

  return QString();
}

void WeatherReporter::cancelVatsimReply()
{
  if(vatsimReply != nullptr)
  {
    disconnect(vatsimReply, &QNetworkReply::finished, this, &WeatherReporter::httpFinishedVatsim);
    vatsimReply->abort();
    vatsimReply->deleteLater();
    vatsimReply = nullptr;
    vatsimRequestIcao.clear();
  }
}

void WeatherReporter::loadVatsimMetar(const QString& airportIcao)
{
  // http://metar.vatsim.net/metar.php?id=EDDF
  qDebug() << "Vatsim Request for" << airportIcao;
  cancelVatsimReply();

  vatsimRequestIcao = airportIcao;
  QNetworkRequest request(QUrl("http://metar.vatsim.net/metar.php?id=" + airportIcao));

  vatsimReply = networkManager.get(request);

  if(vatsimReply != nullptr)
    connect(vatsimReply, &QNetworkReply::finished, this, &WeatherReporter::httpFinishedVatsim);
  else
    qWarning() << "Vatsim Reply is null";
  qDebug() << "Vatsim Request for" << airportIcao << "done";
}

void WeatherReporter::cancelNoaaReply()
{
  if(noaaReply != nullptr)
  {
    disconnect(noaaReply, &QNetworkReply::finished, this, &WeatherReporter::httpFinishedNoaa);
    noaaReply->abort();
    noaaReply->deleteLater();
    noaaReply = nullptr;
    noaaRequestIcao.clear();
  }
}

void WeatherReporter::loadNoaaMetar(const QString& airportIcao)
{
  // http://www.aviationweather.gov/static/adds/metars/stations.txt
  // http://weather.noaa.gov/pub/data/observations/metar/stations/EDDL.TXT
  // http://weather.noaa.gov/pub/data/observations/metar/
  // request.setRawHeader("User-Agent", "Qt NetworkAccess 1.3");
  qDebug() << "NOAA Request for" << airportIcao;

  cancelNoaaReply();

  noaaRequestIcao = airportIcao;
  QNetworkRequest request(QUrl("http://weather.noaa.gov/pub/data/observations/metar/stations/" +
                               airportIcao + ".TXT"));

  noaaReply = networkManager.get(request);

  if(noaaReply != nullptr)
    connect(noaaReply, &QNetworkReply::finished, this, &WeatherReporter::httpFinishedNoaa);
  else
    qWarning() << "NOAA Reply is null";
  qDebug() << "NOAA Request for" << airportIcao << "done";
}

/* Called by network reply signal */
void WeatherReporter::httpFinishedNoaa()
{
  httpFinished(noaaReply, noaaRequestIcao, noaaMetars);
  if(noaaReply != nullptr)
    noaaReply->deleteLater();
  noaaReply = nullptr;
}

/* Called by network reply signal */
void WeatherReporter::httpFinishedVatsim()
{
  httpFinished(vatsimReply, vatsimRequestIcao, vatsimMetars);
  if(vatsimReply != nullptr)
    vatsimReply->deleteLater();
  vatsimReply = nullptr;
}

void WeatherReporter::httpFinished(QNetworkReply *reply, const QString& icao, QHash<QString, Report>& metars)
{
  if(reply != nullptr)
  {
    if(reply->error() == QNetworkReply::NoError)
    {
      QString metar(reply->readAll());
      if(!metar.contains("no metar available", Qt::CaseInsensitive))
        // Add metar with current time
        metars.insert(icao, {metar, QDateTime::currentDateTime()});
      else
        // Add empty record so we know there is not weather station
        metars.insert(icao, Report());
      qDebug() << "Request for" << icao << "succeeded.";
      // mainWindow->setStatusMessage(tr("Weather information updated."));
      emit weatherUpdated();
    }
    else if(reply->error() != QNetworkReply::OperationCanceledError)
    {
      metars.insert(icao, Report());
      qWarning() << "Request for" << icao << "failed. Reason:" << reply->errorString();
    }
    reply->deleteLater();
  }
}

QString WeatherReporter::getAsnMetar(const QString& airportIcao)
{
  return asnMetars.value(airportIcao, QString());
}

QString WeatherReporter::getNoaaMetar(const QString& airportIcao)
{
  if(!noaaMetars.contains(airportIcao))
    // Not in cache
    loadNoaaMetar(airportIcao);
  else
  {
    const Report& report = noaaMetars.value(airportIcao);
    if(!report.metar.isEmpty())
    {
      if(report.reportTime.addSecs(WEATHER_TIMEOUT_SECS) < QDateTime::currentDateTime())
        // In cache but timed out
        loadNoaaMetar(airportIcao);
      else
        // In cache and valid
        return report.metar;
    }
  }
  return QString();
}

QString WeatherReporter::getVatsimMetar(const QString& airportIcao)
{
  if(!vatsimMetars.contains(airportIcao))
    // Not in cache
    loadVatsimMetar(airportIcao);
  else
  {
    const Report& report = vatsimMetars.value(airportIcao);
    if(!report.metar.isEmpty())
    {
      if(report.reportTime.addSecs(WEATHER_TIMEOUT_SECS) < QDateTime::currentDateTime())
        // In cache but timed out
        loadVatsimMetar(airportIcao);
      else
        // In cache and valid
        return report.metar;
    }
  }
  return QString();
}

void WeatherReporter::preDatabaseLoad()
{

}

void WeatherReporter::postDatabaseLoad(atools::fs::FsPaths::SimulatorType type)
{
  if(type != simType)
  {
    // Simulator has changed - reload ASN file
    simType = type;
    initActiveSkyNext();
  }
}

void WeatherReporter::optionsChanged()
{
  initActiveSkyNext();
}

void WeatherReporter::asnWeatherFileChanged(const QString& path)
{
  Q_UNUSED(path);
  qDebug() << "file" << path << "changed";
  loadActiveSkySnapshot(path);
  mainWindow->setStatusMessage(tr("Active Sky Next weather information updated."));
  emit weatherUpdated();
}
