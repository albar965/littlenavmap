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

#include "weather/weatherreporter.h"

#include "gui/mainwindow.h"
#include "settings/settings.h"
#include "options/optiondata.h"
#include "common/constants.h"
#include "fs/weather/xpweatherreader.h"
#include "navapp.h"
#include "fs/weather/weathernetdownload.h"
#include "fs/weather/noaaweatherdownloader.h"
#include "fs/sc/simconnecttypes.h"
#include "fs/util/fsutil.h"
#include "query/mapquery.h"
#include "query/airportquery.h"
#include "fs/weather/metar.h"
#include "util/filesystemwatcher.h"
#include "connect/connectclient.h"
#include "fs/weather/metarparser.h"
#include "gui/dialog.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QNetworkReply>
#include <QTimer>
#include <QRegularExpression>
#include <QEventLoop>
#include <functional>

// Checks the first line of an ASN file if it has valid content
static const QRegularExpression ASN_VALIDATE_REGEXP("^[A-Z0-9]{3,4}::[A-Z0-9]{3,4} .+$");
static const QRegularExpression ASN_VALIDATE_FLIGHTPLAN_REGEXP("^DepartureMETAR=.+$");
static const QRegularExpression ASN_FLIGHTPLAN_REGEXP("^(DepartureMETAR|DestinationMETAR)=([A-Z0-9]{3,4})?(.*)$");

using atools::fs::FsPaths;
using atools::fs::weather::NoaaWeatherDownloader;
using atools::fs::weather::WeatherNetDownload;
using atools::fs::weather::MetarResult;
using atools::fs::weather::Metar;
using atools::util::FileSystemWatcher;
using atools::settings::Settings;

WeatherReporter::WeatherReporter(MainWindow *parentWindow, atools::fs::FsPaths::SimulatorType type)
  : QObject(parentWindow), simType(type),
  mainWindow(parentWindow)
{
  using namespace std::placeholders;
  onlineWeatherTimeoutSecs = atools::settings::Settings::instance().valueInt(lnm::OPTIONS_WEATHER_UPDATE, 600);

  verbose = Settings::instance().getAndStoreValue(lnm::OPTIONS_WEATHER_DEBUG, false).toBool();

  auto coordFunc = std::bind(&WeatherReporter::fetchAirportCoordinates, this, _1);

  xpWeatherReader = new atools::fs::weather::XpWeatherReader(this, verbose);

  noaaWeather = new NoaaWeatherDownloader(parentWindow, verbose);
  noaaWeather->setRequestUrl(OptionData::instance().getWeatherNoaaUrl());
  noaaWeather->setFetchAirportCoords(coordFunc);

  vatsimWeather = new WeatherNetDownload(parentWindow, atools::fs::weather::FLAT, verbose);
  vatsimWeather->setRequestUrl(OptionData::instance().getWeatherVatsimUrl());
  vatsimWeather->setFetchAirportCoords(coordFunc);

  ivaoWeather = new WeatherNetDownload(parentWindow, atools::fs::weather::FLAT, verbose);
  ivaoWeather->setRequestUrl(OptionData::instance().getWeatherIvaoUrl());
  // Set callback so the reader can build an index for nearest airports
  ivaoWeather->setFetchAirportCoords(coordFunc);

  // Update IVAO and NOAA timeout periods - timeout is disable if weather services are not used
  updateTimeouts();

  initActiveSkyNext();

  // Set callback so the reader can build an index for nearest airports
  xpWeatherReader->setFetchAirportCoords(coordFunc);
  initXplane();

  connect(xpWeatherReader, &atools::fs::weather::XpWeatherReader::weatherUpdated,
          this, &WeatherReporter::xplaneWeatherFileChanged);

  // Forward signals from clients for updates
  connect(noaaWeather, &NoaaWeatherDownloader::weatherUpdated, this, &WeatherReporter::noaaWeatherUpdated);
  connect(vatsimWeather, &WeatherNetDownload::weatherUpdated, this, &WeatherReporter::vatsimWeatherUpdated);
  connect(ivaoWeather, &WeatherNetDownload::weatherUpdated, this, &WeatherReporter::ivaoWeatherUpdated);

  // Forward signals from clients for errors
  connect(noaaWeather, &NoaaWeatherDownloader::weatherDownloadFailed, this, &WeatherReporter::weatherDownloadFailed);
  connect(vatsimWeather, &WeatherNetDownload::weatherDownloadFailed, this, &WeatherReporter::weatherDownloadFailed);
  connect(ivaoWeather, &WeatherNetDownload::weatherDownloadFailed, this, &WeatherReporter::weatherDownloadFailed);
}

WeatherReporter::~WeatherReporter()
{
  deleteFsWatcher();
  delete noaaWeather;
  delete vatsimWeather;
  delete ivaoWeather;

  delete xpWeatherReader;
}

void WeatherReporter::noaaWeatherUpdated()
{
  mainWindow->setStatusMessage(tr("NOAA weather downloaded."), true /* addToLog */);
  emit weatherUpdated();
}

void WeatherReporter::ivaoWeatherUpdated()
{
  mainWindow->setStatusMessage(tr("IVAO weather downloaded."), true /* addToLog */);
  emit weatherUpdated();
}

void WeatherReporter::vatsimWeatherUpdated()
{
  mainWindow->setStatusMessage(tr("VATSIM weather downloaded."), true /* addToLog */);
  emit weatherUpdated();
}

atools::geo::Pos WeatherReporter::fetchAirportCoordinates(const QString& airportIdent)
{
  if(!NavApp::isLoadingDatabase())
    return NavApp::getAirportQuerySim()->getAirportPosByIdent(airportIdent);
  else
    return atools::geo::EMPTY_POS;
}

void WeatherReporter::deleteFsWatcher()
{
  if(fsWatcherAsPath != nullptr)
  {
    // Remove watcher just in case the file changes
    fsWatcherAsPath->disconnect(fsWatcherAsPath, &FileSystemWatcher::fileUpdated, this,
                                &WeatherReporter::activeSkyWeatherFileChanged);
    fsWatcherAsPath->deleteLater();
    fsWatcherAsPath = nullptr;
  }

  if(fsWatcherAsFlightplanPath != nullptr)
  {
    // Remove watcher just in case the file changes
    fsWatcherAsFlightplanPath->disconnect(fsWatcherAsFlightplanPath, &FileSystemWatcher::fileUpdated, this,
                                          &WeatherReporter::activeSkyWeatherFileChanged);
    fsWatcherAsFlightplanPath->deleteLater();
    fsWatcherAsFlightplanPath = nullptr;
  }
}

void WeatherReporter::createFsWatcher()
{
  if(fsWatcherAsPath == nullptr)
  {
    // Watch file for changes
    fsWatcherAsPath = new FileSystemWatcher(this, verbose);
    fsWatcherAsPath->setMinFileSize(100000);
    fsWatcherAsPath->connect(fsWatcherAsPath, &FileSystemWatcher::fileUpdated, this,
                             &WeatherReporter::activeSkyWeatherFileChanged);
  }
  fsWatcherAsPath->setFilenameAndStart(asPath);

  if(fsWatcherAsFlightplanPath == nullptr)
  {
    // Watch file for changes
    fsWatcherAsFlightplanPath = new FileSystemWatcher(this, verbose);
    fsWatcherAsFlightplanPath->setMinFileSize(50);
    fsWatcherAsFlightplanPath->connect(fsWatcherAsFlightplanPath, &FileSystemWatcher::fileUpdated, this,
                                       &WeatherReporter::activeSkyWeatherFileChanged);
  }
  fsWatcherAsFlightplanPath->setFilenameAndStart(asFlightplanPath);
}

void WeatherReporter::initXplane()
{
  if(simType == atools::fs::FsPaths::XPLANE11 && !NavApp::getCurrentSimulatorBasePath().isEmpty())
  {
    QFileInfo base(NavApp::getCurrentSimulatorBasePath());

    if(base.exists() && base.isDir())
    {
      // Get user path
      QString path = OptionData::instance().getWeatherXplanePath();
      if(path.isEmpty())
        // Use default base path
        path = NavApp::getCurrentSimulatorBasePath() + QDir::separator() + "METAR.rwx";

      xpWeatherReader->setWeatherFile(path);
    }
    else
    {
      qWarning() << Q_FUNC_INFO << "Base path not valid" << base.filePath();
      xpWeatherReader->clear();
    }
  }
  else
    xpWeatherReader->clear();
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

  QString manualActiveSkySnapshotPath = OptionData::instance().getWeatherActiveSkyPath();
  if(manualActiveSkySnapshotPath.isEmpty())
  {
    QString asnSnapshotPath, asnFlightplanSnapshotPath;
    QString as16SnapshotPath, as16FlightplanSnapshotPath;
    QString asp4SnapshotPath, asp4FlightplanSnapshotPath;
    QString asp5SnapshotPath, asp5FlightplanSnapshotPath;
    QString asXplSnapshotPath, aspXplFlightplanSnapshotPath;

    findActiveSkyFiles(asnSnapshotPath, asnFlightplanSnapshotPath, "ASN", QString());
    qInfo() << "ASN snapshot" << asnSnapshotPath << "flight plan weather" << asnFlightplanSnapshotPath;

    findActiveSkyFiles(as16SnapshotPath, as16FlightplanSnapshotPath, "AS16_", QString());
    qInfo() << "AS16 snapshot" << as16SnapshotPath << "flight plan weather" << as16FlightplanSnapshotPath;

    if(simType == atools::fs::FsPaths::P3D_V4 || simType == atools::fs::FsPaths::P3D_V5)
    {
      // C:\Users\USER\AppData\Roaming\Hifi\AS_P3Dv4
      findActiveSkyFiles(asp4SnapshotPath, asp4FlightplanSnapshotPath, "AS_", "P3Dv4");
      qInfo() << "ASP4 snapshot" << asp4SnapshotPath << "flight plan weather" << asp4FlightplanSnapshotPath;
    }
    // else if(simType == atools::fs::FsPaths::P3D_V5)
    // {
    //// C:\Users\USER\AppData\Roaming\Hifi\AS_P3Dv5
    // findActiveSkyFiles(asp4SnapshotPath, asp4FlightplanSnapshotPath, "AS_", "P3Dv5");
    // qInfo() << "ASP5 snapshot" << asp5SnapshotPath << "flight plan weather" << asp5FlightplanSnapshotPath;
    // }
    else if(simType == atools::fs::FsPaths::XPLANE11)
    {
      // C:\Users\USER\AppData\Roaming\Hifi\AS_XPL
      findActiveSkyFiles(asXplSnapshotPath, aspXplFlightplanSnapshotPath, "AS_", "XPL");
      qInfo() << "ASXPL snapshot" << asXplSnapshotPath << "flight plan weather" << aspXplFlightplanSnapshotPath;
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
    else if(!asXplSnapshotPath.isEmpty())
    {
      asPath = asXplSnapshotPath;
      asFlightplanPath = aspXplFlightplanSnapshotPath;
      activeSkyType = ASXPL;
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
    int num = 0;
    activeSkyMetars.clear();

    QTextStream weatherSnapshot(&file);

    int lineNum = 1;
    QString line;
    while(weatherSnapshot.readLineInto(&line))
    {
      QStringList list = line.split("::");
      if(list.size() >= 2)
      {
        num++;
        activeSkyMetars.insert(list.at(0), list.at(1));
      }
      else
      {
        qWarning() << "AS file" << file.fileName() << "has invalid entries";
        qWarning() << "line #" << lineNum << line;
      }
      lineNum++;
    }
    file.close();

    qDebug() << Q_FUNC_INFO << "Loaded" << num << "METARs";
  }
  else
    qWarning() << "cannot open" << file.fileName() << "reason" << file.errorString();
}

/* Loads flight plan weather for start and destination */
void WeatherReporter::loadActiveSkyFlightplanSnapshot(const QString& path)
{
  using atools::fs::weather::MetarParser;
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

          if(MetarParser::extractDateTime(activeSkyDepartureMetar) <
             MetarParser::extractDateTime(activeSkyMetars.value(activeSkyDepartureIdent, QString())))
          {
            // Do not use activeflightplanwx.txt if values are older
            activeSkyDepartureMetar.clear();
            activeSkyDepartureIdent.clear();
          }
        }
        else if(type == "DestinationMETAR")
        {
          activeSkyDestinationIdent = match.captured(2);
          activeSkyDestinationMetar = activeSkyDestinationIdent + match.captured(3);

          if(MetarParser::extractDateTime(activeSkyDestinationMetar) <
             MetarParser::extractDateTime(activeSkyMetars.value(activeSkyDestinationIdent, QString())))
          {
            // Do not use activeflightplanwx.txt if values are older
            activeSkyDestinationMetar.clear();
            activeSkyDestinationIdent.clear();
          }
        }
      }
    }
    file.close();

    qDebug() << Q_FUNC_INFO << "activeSkyDepartureMetar" << activeSkyDepartureMetar
             << "activeSkyDestinationMetar" << activeSkyDestinationMetar;
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
    else if(simType == atools::fs::FsPaths::P3D_V5)
      simPathComponent = activeSkyPrefix + "P3D";
    else if(simType == atools::fs::FsPaths::XPLANE11)
      simPathComponent = activeSkyPrefix + "XP";
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

bool WeatherReporter::testUrl(const QString& url, const QString& airportIcao, QStringList& result)
{
  return atools::fs::weather::testUrl(url, airportIcao, result);
}

QString WeatherReporter::getCurrentActiveSkyName() const
{
  if(activeSkyType == WeatherReporter::ASP4)
    return tr("ASP4");
  else if(activeSkyType == WeatherReporter::AS16)
    return tr("AS16");
  else if(activeSkyType == WeatherReporter::ASN)
    return tr("ASN");
  else if(activeSkyType == WeatherReporter::ASXPL)
    return tr("ASXP");
  else
    // Manually selected
    return tr("Active Sky");
}

void WeatherReporter::updateAirportWeather()
{
  updateTimeouts();
}

void WeatherReporter::weatherDownloadFailed(const QString& error, int errorCode, QString url)
{
  mainWindow->setStatusMessage(tr("Weather download failed."), true /* addToLog */);

  if(!errorReported)
  {
    // Show an error dialog for any failed downloads but only once per session
    errorReported = true;
    atools::gui::Dialog(mainWindow).showWarnMsgBox(lnm::ACTIONS_SHOW_WEATHER_DOWNLOAD_FAIL,
                                                   tr(
                                                     "<p>Download of weather from<br/>\"%1\"<br/>failed.</p><p>Error: %2 (%3)</p>"
                                                       "<p>Check you weather settings or disable weather downloads.</p>"
                                                         "<p>Suppressing further messages during this session.</p>").
                                                   arg(url).arg(error).arg(errorCode),
                                                   tr("Do not &show this dialog again."));
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

atools::fs::weather::MetarResult WeatherReporter::getXplaneMetar(const QString& station, const atools::geo::Pos& pos)
{
  return xpWeatherReader->getXplaneMetar(station, pos);
}

atools::fs::weather::MetarResult WeatherReporter::getNoaaMetar(const QString& airportIcao, const atools::geo::Pos& pos)
{
  return noaaWeather->getMetar(airportIcao, pos);
}

atools::fs::weather::MetarResult WeatherReporter::getVatsimMetar(const QString& airportIcao,
                                                                 const atools::geo::Pos& pos)
{
  return vatsimWeather->getMetar(airportIcao, pos);
}

atools::fs::weather::MetarResult WeatherReporter::getIvaoMetar(const QString& airportIcao, const atools::geo::Pos& pos)
{
  return ivaoWeather->getMetar(airportIcao, pos);
}

atools::fs::weather::Metar WeatherReporter::getAirportWeather(const QString& airportIcao,
                                                              const atools::geo::Pos& airportPos,
                                                              map::MapWeatherSource source)
{
  switch(source)
  {
    case map::WEATHER_SOURCE_SIMULATOR:
      if(NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::XPLANE11)
        // X-Plane weather file
        return Metar(getXplaneMetar(airportIcao, atools::geo::EMPTY_POS).metarForStation);
      else if(NavApp::getConnectClient()->isConnected() /*&& !NavApp::getConnectClient()->isConnectedNetwork()*/)
      {
        atools::fs::weather::MetarResult res =
          NavApp::getConnectClient()->requestWeather(airportIcao, airportPos, true);

        if(res.isValid() && !res.metarForStation.isEmpty())
          // FSX/P3D - Flight simulator fetched weather or network connection
          return Metar(res.metarForStation, res.requestIdent, res.timestamp, true);
      }
      return Metar();

    case map::WEATHER_SOURCE_ACTIVE_SKY:
      return Metar(getActiveSkyMetar(airportIcao));

    case map::WEATHER_SOURCE_NOAA:
      return Metar(getNoaaMetar(airportIcao, atools::geo::EMPTY_POS).metarForStation);

    case map::WEATHER_SOURCE_VATSIM:
      return Metar(getVatsimMetar(airportIcao, atools::geo::EMPTY_POS).metarForStation);

    case map::WEATHER_SOURCE_IVAO:
      return Metar(getIvaoMetar(airportIcao, atools::geo::EMPTY_POS).metarForStation);
  }
  return Metar();
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
    updateTimeouts();
    initActiveSkyNext();
    initXplane();
  }
}

void WeatherReporter::optionsChanged()
{
  vatsimWeather->setRequestUrl(OptionData::instance().getWeatherVatsimUrl());
  noaaWeather->setRequestUrl(OptionData::instance().getWeatherNoaaUrl());
  ivaoWeather->setRequestUrl(OptionData::instance().getWeatherIvaoUrl());

  updateTimeouts();
  initActiveSkyNext();
  initXplane();
}

void WeatherReporter::updateTimeouts()
{
  map::MapWeatherSource airportWeatherSource = NavApp::getMapWidget() !=
                                               nullptr ? NavApp::getAirportWeatherSource() : map::
                                               WEATHER_SOURCE_SIMULATOR;

  // Disable periodic downloads if feature is not needed
  optsw::FlagsWeather flags = OptionData::instance().getFlagsWeather();

  if(flags & optsw::WEATHER_INFO_NOAA ||
     flags & optsw::WEATHER_TOOLTIP_NOAA ||
     airportWeatherSource == map::WEATHER_SOURCE_NOAA)
    noaaWeather->setUpdatePeriod(onlineWeatherTimeoutSecs);
  else
    noaaWeather->setUpdatePeriod(-1);

  if(flags & optsw::WEATHER_INFO_IVAO ||
     flags & optsw::WEATHER_TOOLTIP_IVAO ||
     airportWeatherSource == map::WEATHER_SOURCE_IVAO)
    ivaoWeather->setUpdatePeriod(onlineWeatherTimeoutSecs);
  else
    ivaoWeather->setUpdatePeriod(-1);

  if(flags & optsw::WEATHER_INFO_VATSIM ||
     flags & optsw::WEATHER_TOOLTIP_VATSIM ||
     airportWeatherSource == map::WEATHER_SOURCE_VATSIM)
    ivaoWeather->setUpdatePeriod(onlineWeatherTimeoutSecs);
  else
    ivaoWeather->setUpdatePeriod(-1);
}

void WeatherReporter::activeSkyWeatherFileChanged(const QString& path)
{
  Q_UNUSED(path);
  qDebug() << Q_FUNC_INFO << "file" << path << "changed";
  loadActiveSkySnapshot(asPath);
  loadActiveSkyFlightplanSnapshot(asFlightplanPath);
  mainWindow->setStatusMessage(tr("Active Sky weather information updated."), true /* addToLog */);

  emit weatherUpdated();
}

void WeatherReporter::xplaneWeatherFileChanged()
{
  mainWindow->setStatusMessage(tr("X-Plane weather information updated."), true /* addToLog */);
  emit weatherUpdated();
}
