/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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
#include "fs/common/xpweatherreader.h"
#include "navapp.h"
#include "fs/sc/simconnecttypes.h"
#include "query/mapquery.h"
#include "query/airportquery.h"

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
const QRegularExpression ASN_VALIDATE_FLIGHTPLAN_REGEXP("^DepartureMETAR=.+$");
const QRegularExpression ASN_FLIGHTPLAN_REGEXP("^(DepartureMETAR|DestinationMETAR)=([A-Z0-9]{3,4})?(.*)$");

using atools::fs::FsPaths;

WeatherReporter::WeatherReporter(MainWindow *parentWindow, atools::fs::FsPaths::SimulatorType type)
  : QObject(parentWindow), noaaCache(WEATHER_TIMEOUT_SECS), vatsimCache(WEATHER_TIMEOUT_SECS), simType(type),
  mainWindow(parentWindow)
{
  xpWeatherReader = new atools::fs::common::XpWeatherReader(this);
  initActiveSkyNext();

  // Set callback so the reader can build an index for nearest airports
  xpWeatherReader->setFetchAirportCoords([](const QString& ident) -> atools::geo::Pos
  {
    return NavApp::getAirportQuerySim()->getAirportCoordinatesByIdent(ident);
  });
  initXplane();

  connect(xpWeatherReader, &atools::fs::common::XpWeatherReader::weatherUpdated,
          this, &WeatherReporter::xplaneWeatherFileChanged);
  connect(&flushQueueTimer, &QTimer::timeout, this, &WeatherReporter::flushRequestQueue);

  flushQueueTimer.setInterval(1000);
  flushQueueTimer.start();
}

WeatherReporter::~WeatherReporter()
{
  flushQueueTimer.stop();

  // Remove any outstanding requests
  cancelNoaaReply();
  cancelVatsimReply();

  deleteFsWatcher();

  delete xpWeatherReader;
}

void WeatherReporter::flushRequestQueue()
{
  if(!noaaRequests.isEmpty())
    loadNoaaMetar(noaaRequests.takeLast());

  if(!vatsimRequests.isEmpty())
    loadVatsimMetar(vatsimRequests.takeLast());
}

void WeatherReporter::deleteFsWatcher()
{
  if(fsWatcher != nullptr)
  {
    // Remove watcher just in case the file changes
    fsWatcher->disconnect(fsWatcher, &QFileSystemWatcher::fileChanged, this,
                          &WeatherReporter::activeSkyWeatherFileChanged);
    fsWatcher->deleteLater();
    fsWatcher = nullptr;
  }
}

void WeatherReporter::createFsWatcher()
{
  if(fsWatcher == nullptr)
  {
    // Watch file for changes
    fsWatcher = new QFileSystemWatcher(this);
    fsWatcher->connect(fsWatcher, &QFileSystemWatcher::fileChanged, this,
                       &WeatherReporter::activeSkyWeatherFileChanged);
  }

  if(!fsWatcher->addPath(asPath))
    qWarning() << "cannot watch" << asPath;

  if(!fsWatcher->addPath(asFlightplanPath))
    qWarning() << "cannot watch" << asFlightplanPath;
}

void WeatherReporter::initXplane()
{
  if(simType == atools::fs::FsPaths::XPLANE11)
    xpWeatherReader->readWeatherFile(NavApp::getCurrentSimulatorBasePath() + QDir::separator() + "METAR.rwx");
}

void WeatherReporter::initActiveSkyNext()
{
  deleteFsWatcher();

  activeSkyType = NONE;
  activeSkyMetars.clear();
  activeSkyDepartureMetar.clear();
  activeSkyDestinationMetar.clear();
  activeSkyDepartureIdent.clear();
  activeSkyDestinationIdent.clear();
  asPath.clear();
  asFlightplanPath.clear();

  if(simType != atools::fs::FsPaths::XPLANE11)
  {
    QString manualActiveSkySnapshotPath = OptionData::instance().getWeatherActiveSkyPath();
    if(manualActiveSkySnapshotPath.isEmpty())
    {
      QString asnSnapshotPath, asnFlightplanSnapshotPath;
      QString as16SnapshotPath, as16FlightplanSnapshotPath;
      QString asp4SnapshotPath, asp4FlightplanSnapshotPath;

      findActiveSkyFiles(asnSnapshotPath, asnFlightplanSnapshotPath, "ASN", QString());
      qInfo() << "ASN snapshot" << asnSnapshotPath << "flight plan weather" << asnFlightplanSnapshotPath;

      findActiveSkyFiles(as16SnapshotPath, as16FlightplanSnapshotPath, "AS16_", QString());
      qInfo() << "AS16 snapshot" << as16SnapshotPath << "flight plan weather" << as16FlightplanSnapshotPath;

      if(simType == atools::fs::FsPaths::P3D_V4)
      {
        // C:\Users\USER\AppData\Roaming\Hifi\AS_P3Dv4
        findActiveSkyFiles(asp4SnapshotPath, asp4FlightplanSnapshotPath, "AS_", "P3Dv4");
        qInfo() << "ASP4 snapshot" << asp4SnapshotPath << "flight plan weather" << asp4FlightplanSnapshotPath;
      }

      if(!asp4SnapshotPath.isEmpty())
      {
        // Prefer AS4 before AS16
        asPath = asp4SnapshotPath;
        asFlightplanPath = asp4FlightplanSnapshotPath;
        activeSkyType = ASP4;
      }
      else if(!as16SnapshotPath.isEmpty())
      {
        // Prefer AS16 before ASN
        asPath = as16SnapshotPath;
        asFlightplanPath = as16FlightplanSnapshotPath;
        activeSkyType = AS16;
      }
      else if(!asnSnapshotPath.isEmpty())
      {
        asPath = asnSnapshotPath;
        asFlightplanPath = asnFlightplanSnapshotPath;
        activeSkyType = ASN;
      }
    }
    else
    {
      // Manual path overrides found path for all simulators
      asPath = manualActiveSkySnapshotPath;
      asFlightplanPath = QFileInfo(manualActiveSkySnapshotPath).path() + QDir::separator() + "activeflightplanwx.txt";
      activeSkyType = MANUAL;
    }

    if(!asPath.isEmpty() || !asFlightplanPath.isEmpty())
    {
      qDebug() << "Using Active Sky paths" << asPath << asFlightplanPath;
      loadActiveSkySnapshot(asPath);
      loadActiveSkyFlightplanSnapshot(asFlightplanPath);
      createFsWatcher();
    }
    else
      qDebug() << "Active Sky path not found";
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

    QTextStream weatherSnapshot(&file);

    int lineNum = 1;
    QString line;
    while(weatherSnapshot.readLineInto(&line))
    {
      QStringList list = line.split("::");
      if(list.size() >= 2)
        activeSkyMetars.insert(list.at(0), list.at(1));
      else
      {
        qWarning() << "AS file" << file.fileName() << "has invalid entries";
        qWarning() << "line #" << lineNum << line;
      }
      lineNum++;
    }
    file.close();
  }
  else
    qWarning() << "cannot open" << file.fileName() << "reason" << file.errorString();
}

/* Loads flight plan weather for start and destination */
void WeatherReporter::loadActiveSkyFlightplanSnapshot(const QString& path)
{
  // DepartureMETAR=NZTU 282151Z 33112KT 9999 FEW030 FEW168 11/05 Q0998 RMK ADVANCED INTERPOLATION
  // DestinationMETAR=NZHR 282151Z 31706KT 9999 -RA SCT020 SCT085 11/07 Q1000 RMK ADVANCED INTERPOLATION
  // AlternateMETAR=
  // AverageWinds=273@23
  // AverageHeadwind=-15,2
  // AverageTemperature=6,5
  // CruiseAltitude=9000
  // CruiseSpeed=300

  if(path.isEmpty())
    return;

  QFile file(path);
  if(file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    activeSkyDepartureMetar.clear();
    activeSkyDestinationMetar.clear();
    activeSkyDepartureIdent.clear();
    activeSkyDestinationIdent.clear();

    QTextStream flightplanFile(&file);

    QString line;
    while(flightplanFile.readLineInto(&line))
    {
      QRegularExpressionMatch match = ASN_FLIGHTPLAN_REGEXP.match(line);

      if(match.hasMatch())
      {
        QString type = match.captured(1);
        if(type == "DepartureMETAR")
        {
          activeSkyDepartureIdent = match.captured(2);
          activeSkyDepartureMetar = activeSkyDepartureIdent + match.captured(3);
        }
        else if(type == "DestinationMETAR")
        {
          activeSkyDestinationIdent = match.captured(2);
          activeSkyDestinationMetar = activeSkyDestinationIdent + match.captured(3);
        }
      }
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

bool WeatherReporter::validateActiveSkyFlightplanFile(const QString& path)
{
  bool retval = false;
  QFile file(path);
  if(file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    QTextStream sceneryCfg(&file);
    QString line;
    if(sceneryCfg.readLineInto(&line))
      // Check if the first line matches
      if(ASN_VALIDATE_FLIGHTPLAN_REGEXP.match(line.trimmed()).hasMatch())
        retval = true;
    file.close();
  }
  else
    qWarning() << "cannot open" << file.fileName() << "reason" << file.errorString();
  return retval;
}

void WeatherReporter::findActiveSkyFiles(QString& asnSnapshot, QString& flightplanSnapshot,
                                         const QString& activeSkyPrefix, const QString& activeSkySimSuffix)
{
 #if defined(Q_OS_WIN32)
  // Use the environment variable because QStandardPaths::ConfigLocation returns an unusable path on Windows
  QString appdata = QString(qgetenv("APPDATA"));
#else
  // Use $HOME/.config for testing
  QString appdata = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation).first();
#endif

  QString simPathComponent;
  if(activeSkySimSuffix.isEmpty())
  {
    // Determine suffix from sim type
    if(simType == atools::fs::FsPaths::FSX)
      simPathComponent = activeSkyPrefix + "FSX";
    else if(simType == atools::fs::FsPaths::FSX_SE)
      simPathComponent = activeSkyPrefix + "FSX";
    else if(simType == atools::fs::FsPaths::P3D_V2)
      simPathComponent = activeSkyPrefix + "P3D";
    else if(simType == atools::fs::FsPaths::P3D_V3)
      simPathComponent = activeSkyPrefix + "P3D";
    else if(simType == atools::fs::FsPaths::P3D_V4)
      simPathComponent = activeSkyPrefix + "P3D";
  }
  else
    // Use fixed suffix for AS4
    simPathComponent = activeSkyPrefix + activeSkySimSuffix;

  QString hifiPath = appdata +
                     QDir::separator() + "HiFi" +
                     QDir::separator() + simPathComponent +
                     QDir::separator() + "Weather" +
                     QDir::separator();

  QString weatherFile = hifiPath + "current_wx_snapshot.txt";

  if(QFile::exists(weatherFile))
  {
    qInfo() << "found Active Sky weather file" << weatherFile;
    if(validateActiveSkyFile(weatherFile))
      asnSnapshot = weatherFile;
    else
      qWarning() << "is not an ASN weather snapshot file" << weatherFile;
  }
  else
    qInfo() << "file does not exist" << weatherFile;

  weatherFile = hifiPath + "activeflightplanwx.txt";

  if(QFile::exists(weatherFile))
  {
    qInfo() << "found Active Sky flight plan weathers file" << weatherFile;
    if(validateActiveSkyFlightplanFile(weatherFile))
      flightplanSnapshot = weatherFile;
    else
      qWarning() << "is not an Active Sky flight plan weather snapshot file" << weatherFile;
  }
  else
    qInfo() << "file does not exist" << weatherFile;
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
  if(vatsimReply != nullptr)
    vatsimRequests.append(airportIcao);
  else
  {
    cancelVatsimReply();

    vatsimRequestIcao = airportIcao;
    QNetworkRequest request(QUrl(OptionData::instance().getWeatherVatsimUrl().arg(airportIcao)));

    vatsimReply = networkManager.get(request);

    if(vatsimReply != nullptr)
      connect(vatsimReply, &QNetworkReply::finished, this, &WeatherReporter::httpFinishedVatsim);
    else
      qWarning() << "Vatsim Reply is null";
  }
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

QString WeatherReporter::getCurrentActiveSkyName() const
{
  if(activeSkyType == WeatherReporter::ASP4)
    return tr("ASP4");
  else if(activeSkyType == WeatherReporter::AS16)
    return tr("AS16");
  else if(activeSkyType == WeatherReporter::ASN)
    return tr("ASN");
  else
    // Manually selected
    return tr("Active Sky");
}

void WeatherReporter::loadNoaaMetar(const QString& airportIcao)
{
  // http://www.aviationweather.gov/static/adds/metars/stations.txt
  // http://weather.noaa.gov/pub/data/observations/metar/stations/EDDL.TXT
  // http://weather.noaa.gov/pub/data/observations/metar/
  // request.setRawHeader("User-Agent", "Qt NetworkAccess 1.3");

  if(noaaReply != nullptr)
    noaaRequests.append(airportIcao);
  else
  {
    cancelNoaaReply();

    noaaRequestIcao = airportIcao;
    QNetworkRequest request(QUrl(OptionData::instance().getWeatherNoaaUrl().arg(airportIcao)));

    noaaReply = networkManager.get(request);

    if(noaaReply != nullptr)
      connect(noaaReply, &QNetworkReply::finished, this, &WeatherReporter::httpFinishedNoaa);
    else
      qWarning() << "NOAA Reply is null";
  }
}

/* Called by network reply signal */
void WeatherReporter::httpFinishedNoaa()
{
  // qDebug() << Q_FUNC_INFO << noaaRequestIcao;
  httpFinished(noaaReply, noaaRequestIcao, noaaCache);
  if(noaaReply != nullptr)
    noaaReply->deleteLater();
  noaaReply = nullptr;

  if(!noaaRequests.isEmpty())
    loadNoaaMetar(noaaRequests.takeLast());
}

/* Called by network reply signal */
void WeatherReporter::httpFinishedVatsim()
{
  // qDebug() << Q_FUNC_INFO << vatsimRequestIcao;
  httpFinished(vatsimReply, vatsimRequestIcao, vatsimCache);
  if(vatsimReply != nullptr)
    vatsimReply->deleteLater();
  vatsimReply = nullptr;

  if(!vatsimRequests.isEmpty())
    loadVatsimMetar(vatsimRequests.takeLast());
}

void WeatherReporter::httpFinished(QNetworkReply *reply, const QString& icao,
                                   atools::util::TimedCache<QString, QString>& metars)
{
  if(reply != nullptr)
  {
    if(reply->error() == QNetworkReply::NoError)
    {
      QString metar = reply->readAll().simplified();
      if(!metar.contains("no metar available", Qt::CaseInsensitive))
        // Add metar with current time
        metars.insert(icao, metar);
      else
        // Add empty record so we know there is no weather station
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
  if(activeSkyDepartureIdent == airportIcao)
    return activeSkyDepartureMetar;
  else if(activeSkyDestinationIdent == airportIcao)
    return activeSkyDestinationMetar;
  else
    return activeSkyMetars.value(airportIcao, QString());
}

atools::fs::sc::MetarResult WeatherReporter::getXplaneMetar(const QString& station, const atools::geo::Pos& pos)
{
  return xpWeatherReader->getXplaneMetar(station, pos);
}

QString WeatherReporter::getNoaaMetar(const QString& airportIcao)
{
  // qDebug() << Q_FUNC_INFO << airportIcao;

  QString *metar = noaaCache.value(airportIcao);
  if(metar != nullptr)
    return QString(*metar);
  else
    loadNoaaMetar(airportIcao);

  return QString();
}

QString WeatherReporter::getVatsimMetar(const QString& airportIcao)
{
  // qDebug() << Q_FUNC_INFO << airportIcao;

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
    // Simulator has changed - reload files
    simType = type;
    initActiveSkyNext();
    initXplane();
  }
}

void WeatherReporter::optionsChanged()
{
  initActiveSkyNext();
  initXplane();
}

void WeatherReporter::activeSkyWeatherFileChanged(const QString& path)
{
  Q_UNUSED(path);
  qDebug() << Q_FUNC_INFO << "file" << path << "changed";
  loadActiveSkySnapshot(asPath);
  loadActiveSkyFlightplanSnapshot(asFlightplanPath);
  mainWindow->setStatusMessage(tr("Active Sky weather information updated."));

  emit weatherUpdated();
}

void WeatherReporter::xplaneWeatherFileChanged()
{
  mainWindow->setStatusMessage(tr("X-Plane weather information updated."));
  emit weatherUpdated();
}
