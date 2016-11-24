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
#include <QEventLoop>

// Checks the first line of an ASN file if it has valid content
const QRegularExpression ASN_VALIDATE_REGEXP("^[A-Z0-9]{3,4}::[A-Z0-9]{3,4} .+$");

using atools::fs::FsPaths;

WeatherReporter::WeatherReporter(MainWindow *parentWindow, atools::fs::FsPaths::SimulatorType type)
  : QObject(parentWindow), noaaCache(WEATHER_TIMEOUT_SECS), vatsimCache(WEATHER_TIMEOUT_SECS), simType(type),
    mainWindow(parentWindow)
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
                          &WeatherReporter::activeSkyWeatherFileChanged);
    delete fsWatcher;
    fsWatcher = nullptr;
  }

  QString asPath;

  activeSkyType = NONE;
  QString manualActiveSkySnapshotPath = OptionData::instance().getWeatherActiveSkyPath();
  if(manualActiveSkySnapshotPath.isEmpty())
  {
    QString asnSnapshotPath = findActiveSkySnapshotPath("ASN");
    qInfo() << "ASN snapshot" << asnSnapshotPath;

    QString as16SnapshotPath = findActiveSkySnapshotPath("AS16_");
    qInfo() << "AS16 snapshot" << as16SnapshotPath;

    if(!as16SnapshotPath.isEmpty())
    {
      // Prefer AS16 befor ASN
      asPath = as16SnapshotPath;
      activeSkyType = AS16;
    }
    else if(!asnSnapshotPath.isEmpty())
    {
      asPath = asnSnapshotPath;
      activeSkyType = ASN;
    }
  }
  else
  {
    // Manual path overrides found path for all simulators
    asPath = manualActiveSkySnapshotPath;
    activeSkyType = MANUAL;
  }

  if(!asPath.isEmpty())
  {
    qDebug() << "Using Active Sky path" << asPath;
    loadActiveSkySnapshot(asPath);
    if(fsWatcher == nullptr)
    {
      // Watch file for changes
      fsWatcher = new QFileSystemWatcher(this);
      fsWatcher->connect(fsWatcher, &QFileSystemWatcher::fileChanged, this,
                         &WeatherReporter::activeSkyWeatherFileChanged);
    }

    if(!fsWatcher->addPath(asPath))
      qWarning() << "cannot watch" << asPath;
  }
  else
  {
    qDebug() << "Active Sky path not found";
    activeSkyMetars.clear();
  }
}

/* Loads complete ASN file into a hash map */
void WeatherReporter::loadActiveSkySnapshot(const QString& path)
{
  // ASN
  // C:\Users\USER\AppData\Roaming\HiFi\ASNFSX\Weather\current_wx_snapshot.txt or wx_station_list.txt
  // AS16
  // C:\Users\USER\AppData\Roaming\HiFi\AS16_FSX\Weather\current_wx_snapshot.txt or wx_station_list.txt

  // AGGH::AGGH 261800Z 20002KT 9999 FEW014 SCT027 25/24 Q1009::AGGH 261655Z 2618/2718 VRB03KT 9999 FEW017 SCT028 FM270000
  // 34010KT 9999 -SH SCT019 SCT03 FM271200 VRB03KT 9999 -SH FEW017 SCT028 PROB30 INTER 2703/27010 5000 TSSH SCT016 FEW017CB BKN028
  // T 25 27 31 32 Q 1009 1011 1011 1009::278,11,24.0/267,12,19.0/263,13,16.1/233,12,7.2/290,7,-3.0/338,8,-13.0/348,18,-27.9/9,19,-37.9/26,15,-51.3

  // TODO overrride with settings
  if(path.isEmpty())
    return;

  QFile file(path);
  if(file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    activeSkyMetars.clear();

    QTextStream sceneryCfg(&file);

    QString line;
    while(sceneryCfg.readLineInto(&line))
    {
      QStringList list = line.split("::");
      activeSkyMetars.insert(list.at(0), list.at(1));
    }
    file.close();
  }
  else
    qWarning() << "cannot open" << file.fileName() << "reason" << file.errorString();
}

bool WeatherReporter::validateActiveSkyFile(const QString& path)
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

QString WeatherReporter::findActiveSkySnapshotPath(const QString& activeSkyPrefix)
{
 #if defined(Q_OS_WIN32)
  // Use the environment variable because QStandardPaths::ConfigLocation returns an unusable path on Windows
  QString appdata = QString(qgetenv("APPDATA"));
#else
  // Use $HOME/.config for testing
  QString appdata = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation).first();
#endif

  QString simPathComponent;
  if(simType == atools::fs::FsPaths::FSX)
    simPathComponent = activeSkyPrefix + "FSX";
  else if(simType == atools::fs::FsPaths::FSX_SE)
    simPathComponent = activeSkyPrefix + "FSX";
  else if(simType == atools::fs::FsPaths::P3D_V2)
    simPathComponent = activeSkyPrefix + "P3D";
  else if(simType == atools::fs::FsPaths::P3D_V3)
    simPathComponent = activeSkyPrefix + "P3D";

  QString weatherFile = appdata +
                        QDir::separator() + "HiFi" +
                        QDir::separator() + simPathComponent +
                        QDir::separator() + "Weather" +
                        QDir::separator() + "current_wx_snapshot.txt";

  if(QFile::exists(weatherFile))
  {
    qInfo() << "found ASN weather file" << weatherFile;
    if(validateActiveSkyFile(weatherFile))
      return weatherFile;
    else
      qWarning() << "is not an ASN weather snapshot file" << weatherFile;
  }
  else
    qInfo() << "file does not exist" << weatherFile;

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
  cancelVatsimReply();

  vatsimRequestIcao = airportIcao;
  QNetworkRequest request(QUrl(OptionData::instance().getWeatherVatsimUrl().arg(airportIcao)));

  vatsimReply = networkManager.get(request);

  if(vatsimReply != nullptr)
    connect(vatsimReply, &QNetworkReply::finished, this, &WeatherReporter::httpFinishedVatsim);
  else
    qWarning() << "Vatsim Reply is null";
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

bool WeatherReporter::testUrl(const QString& url, const QString& airportIcao, QString& result)
{
  QNetworkRequest request(QUrl(url.arg(airportIcao)));
  QNetworkReply *reply = networkManager.get(request);

  QEventLoop eventLoop;
  QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
  eventLoop.exec();

  if(reply->error() == QNetworkReply::NoError)
  {
    result = reply->readAll();
    reply->deleteLater();
    return true;
  }
  else
  {
    result = reply->errorString();
    reply->deleteLater();
    return false;
  }
}

void WeatherReporter::loadNoaaMetar(const QString& airportIcao)
{
  // http://www.aviationweather.gov/static/adds/metars/stations.txt
  // http://weather.noaa.gov/pub/data/observations/metar/stations/EDDL.TXT
  // http://weather.noaa.gov/pub/data/observations/metar/
  // request.setRawHeader("User-Agent", "Qt NetworkAccess 1.3");

  cancelNoaaReply();

  noaaRequestIcao = airportIcao;
  QNetworkRequest request(QUrl(OptionData::instance().getWeatherNoaaUrl().arg(airportIcao)));

  noaaReply = networkManager.get(request);

  if(noaaReply != nullptr)
    connect(noaaReply, &QNetworkReply::finished, this, &WeatherReporter::httpFinishedNoaa);
  else
    qWarning() << "NOAA Reply is null";
}

/* Called by network reply signal */
void WeatherReporter::httpFinishedNoaa()
{
  httpFinished(noaaReply, noaaRequestIcao, noaaCache);
  if(noaaReply != nullptr)
    noaaReply->deleteLater();
  noaaReply = nullptr;
}

/* Called by network reply signal */
void WeatherReporter::httpFinishedVatsim()
{
  httpFinished(vatsimReply, vatsimRequestIcao, vatsimCache);
  if(vatsimReply != nullptr)
    vatsimReply->deleteLater();
  vatsimReply = nullptr;
}

void WeatherReporter::httpFinished(QNetworkReply *reply, const QString& icao,
                                   atools::util::TimedCache<QString, QString>& metars)
{
  if(reply != nullptr)
  {
    if(reply->error() == QNetworkReply::NoError)
    {
      QString metar(reply->readAll());
      if(!metar.contains("no metar available", Qt::CaseInsensitive))
        // Add metar with current time
        metars.insert(icao, metar);
      else
        // Add empty record so we know there is not weather station
        metars.insert(icao, QString());
      // mainWindow->setStatusMessage(tr("Weather information updated."));
      emit weatherUpdated();
    }
    else if(reply->error() != QNetworkReply::OperationCanceledError)
    {
      metars.insert(icao, QString());
      if(reply->error() == QNetworkReply::ContentNotFoundError)
        qInfo() << "Request for" << icao << "failed. Reason:" << reply->errorString();
      else
        qWarning() << "Request for" << icao << "failed. Reason:" << reply->errorString();
    }
    reply->deleteLater();
  }
}

QString WeatherReporter::getActiveSkyMetar(const QString& airportIcao)
{
  return activeSkyMetars.value(airportIcao, QString());
}

QString WeatherReporter::getNoaaMetar(const QString& airportIcao)
{
  QString *metar = noaaCache.value(airportIcao);
  if(metar != nullptr)
    return QString(*metar);
  else
    loadNoaaMetar(airportIcao);

  return QString();
}

QString WeatherReporter::getVatsimMetar(const QString& airportIcao)
{
  QString *metar = vatsimCache.value(airportIcao);
  if(metar != nullptr)
    return QString(*metar);
  else
    loadVatsimMetar(airportIcao);

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

void WeatherReporter::activeSkyWeatherFileChanged(const QString& path)
{
  Q_UNUSED(path);
  qDebug() << "file" << path << "changed";
  loadActiveSkySnapshot(path);
  mainWindow->setStatusMessage(tr("Active Sky weather information updated."));
  emit weatherUpdated();
}
