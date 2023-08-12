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

#include "weather/weatherreporter.h"

#include "atools.h"
#include "common/constants.h"
#include "common/maptools.h"
#include "common/maptypes.h"
#include "connect/connectclient.h"
#include "fs/weather/metar.h"
#include "fs/weather/metarparser.h"
#include "fs/weather/noaaweatherdownloader.h"
#include "fs/weather/weathernetdownload.h"
#include "fs/weather/xpweatherreader.h"
#include "gui/dialog.h"
#include "gui/mainwindow.h"
#include "app/navapp.h"
#include "options/optiondata.h"
#include "query/airportquery.h"
#include "query/infoquery.h"
#include "settings/settings.h"
#include "util/filesystemwatcher.h"
#include "util/filechecker.h"

#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QRegularExpression>
#include <QStringBuilder>

#if defined(Q_OS_WIN32)
#include <QProcessEnvironment>
#endif

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
  : QObject(parentWindow), simType(type), mainWindow(parentWindow)
{
  xplaneFileWarningMsg = QString(tr("\n\nMake sure that your X-Plane base path is correct and\n"
                                    "weather files as well as directories exist.\n\n"
                                    "Click \"Reset paths\" in the Little Navmap dialog \"Load scenery library\"\n"
                                    "to fix the base path after moving a X-Plane installation.\n\n"
                                    "Also check the paths in the Little Navmap options on page \"Weather Files\".\n"
                                    "These path should be empty to use the default.\n\n"
                                    "Restart Little Navmap after correcting the weather paths."));

  xplaneMissingWarningMsg = QString(tr("Cannot access weather files.\n"
                                       "X-Plane is not installed on this computer.\n\n"
                                       "If you use this as a remote installation:\n"
                                       "Share the weather files on the flying computer and\n"
                                       "adapt the X-Plane weather path in options on page \"Weather Files\" to point to the network share."));

  // Init file checkers to avoid expensive file access while trying to load weather while painting
  asSnapshotPathChecker = new atools::util::FileChecker;
  asFlightplanPathChecker = new atools::util::FileChecker;

  onlineWeatherTimeoutSecs = atools::settings::Settings::instance().valueInt(lnm::OPTIONS_WEATHER_UPDATE, 600);

  verbose = Settings::instance().getAndStoreValue(lnm::OPTIONS_WEATHER_DEBUG, false).toBool();

  auto coordFunc = std::bind(&WeatherReporter::fetchAirportCoordinates, this, std::placeholders::_1);

  xpWeatherReader = new atools::fs::weather::XpWeatherReader(this, verbose);

  noaaWeather = new NoaaWeatherDownloader(parentWindow, verbose);
  noaaWeather->setRequestUrl(OptionData::instance().getWeatherNoaaUrl());
  noaaWeather->setFetchAirportCoords(coordFunc); //// Set callback so the reader can build an index for nearest airports

  vatsimWeather = new WeatherNetDownload(parentWindow, atools::fs::weather::FLAT, verbose);
  vatsimWeather->setRequestUrl(OptionData::instance().getWeatherVatsimUrl());
  vatsimWeather->setFetchAirportCoords(coordFunc);

  ivaoWeather = new WeatherNetDownload(parentWindow, atools::fs::weather::JSON, verbose);
  const QLatin1String KEY(":/littlenavmap/little_navmap_keys/ivao_weather_api_key.bin");
  if(QFile::exists(KEY))
  {
    ivaoWeather->setRequestUrl(OptionData::instance().getWeatherIvaoUrl());
    ivaoWeather->setHeaderParameters({
      {"accept", "application/json"},
      {"apiKey", atools::strFromCryptFile(KEY, 0x2B1A96468EB62460)}
    });
  }
  ivaoWeather->setFetchAirportCoords(coordFunc);

  // Update IVAO and NOAA timeout periods - timeout is disable if weather services are not used
  updateTimeouts();

  initActiveSkyPaths();

  // Set callback so the reader can build an index for nearest airports
  xpWeatherReader->setFetchAirportCoords(coordFunc);
  initXplane();

  connect(xpWeatherReader, &atools::fs::weather::XpWeatherReader::weatherUpdated, this, &WeatherReporter::xplaneWeatherFileChanged);

  // Forward signals from clients for updates
  connect(noaaWeather, &NoaaWeatherDownloader::weatherUpdated, this, &WeatherReporter::noaaWeatherUpdated);
  connect(vatsimWeather, &WeatherNetDownload::weatherUpdated, this, &WeatherReporter::vatsimWeatherUpdated);
  connect(ivaoWeather, &WeatherNetDownload::weatherUpdated, this, &WeatherReporter::ivaoWeatherUpdated);

  // Forward signals from clients for errors
  connect(noaaWeather, &NoaaWeatherDownloader::weatherDownloadFailed, this, &WeatherReporter::weatherDownloadFailed);
  connect(vatsimWeather, &WeatherNetDownload::weatherDownloadFailed, this, &WeatherReporter::weatherDownloadFailed);
  connect(ivaoWeather, &WeatherNetDownload::weatherDownloadFailed, this, &WeatherReporter::weatherDownloadFailed);

  // Forward signals from clients for SSL errors
  connect(noaaWeather, &NoaaWeatherDownloader::weatherDownloadSslErrors, this, &WeatherReporter::weatherDownloadSslErrors);
  connect(vatsimWeather, &WeatherNetDownload::weatherDownloadSslErrors, this, &WeatherReporter::weatherDownloadSslErrors);
  connect(ivaoWeather, &WeatherNetDownload::weatherDownloadSslErrors, this, &WeatherReporter::weatherDownloadSslErrors);

  // Forward signals from clients for progress
  connect(noaaWeather, &NoaaWeatherDownloader::weatherDownloadProgress, this, &WeatherReporter::weatherDownloadProgress);
  connect(vatsimWeather, &WeatherNetDownload::weatherDownloadProgress, this, &WeatherReporter::weatherDownloadProgress);
  connect(ivaoWeather, &WeatherNetDownload::weatherDownloadProgress, this, &WeatherReporter::weatherDownloadProgress);
}

WeatherReporter::~WeatherReporter()
{
  deleteActiveSkyFsWatcher();

  qDebug() << Q_FUNC_INFO << "delete noaaWeather";
  delete noaaWeather;
  noaaWeather = nullptr;

  qDebug() << Q_FUNC_INFO << "delete vatsimWeather";
  delete vatsimWeather;
  vatsimWeather = nullptr;

  qDebug() << Q_FUNC_INFO << "delete ivaoWeather";
  delete ivaoWeather;
  ivaoWeather = nullptr;

  qDebug() << Q_FUNC_INFO << "delete xpWeatherReader";
  delete xpWeatherReader;
  xpWeatherReader = nullptr;

  qDebug() << Q_FUNC_INFO << "delete asSnapshotPathChecker";
  delete asSnapshotPathChecker;
  asSnapshotPathChecker = nullptr;

  qDebug() << Q_FUNC_INFO << "delete asFlightplanPathChecker";
  delete asFlightplanPathChecker;
  asFlightplanPathChecker = nullptr;
}

void WeatherReporter::weatherDownloadProgress(qint64 bytesReceived, qint64 bytesTotal, QString downloadUrl)
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << "bytesReceived" << bytesReceived << "bytesTotal" << bytesTotal << "downloadUrl" << downloadUrl;

  QApplication::processEvents(QEventLoop::WaitForMoreEvents);
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

atools::geo::Pos WeatherReporter::fetchAirportCoordinates(const QString& metarAirportIdent)
{
  if(!NavApp::isLoadingDatabase())
  {
    if(atools::fs::FsPaths::isAnyXplane(simType))
      return NavApp::getAirportQuerySim()->getAirportPosByIdentOrIcao(metarAirportIdent);
    else
      return NavApp::getAirportQuerySim()->getAirportPosByIdent(metarAirportIdent);
  }
  else
    return atools::geo::EMPTY_POS;
}

void WeatherReporter::deleteActiveSkyFsWatcher()
{
  if(fsWatcherAsPath != nullptr)
  {
    // Remove watcher just in case the file changes
    FileSystemWatcher::disconnect(fsWatcherAsPath, &FileSystemWatcher::filesUpdated, this,
                                  &WeatherReporter::activeSkyWeatherFilesChanged);
    fsWatcherAsPath->deleteLater();
    fsWatcherAsPath = nullptr;
  }

  if(fsWatcherAsFlightplanPath != nullptr)
  {
    // Remove watcher just in case the file changes
    FileSystemWatcher::disconnect(fsWatcherAsFlightplanPath, &FileSystemWatcher::filesUpdated, this,
                                  &WeatherReporter::activeSkyWeatherFilesChanged);
    fsWatcherAsFlightplanPath->deleteLater();
    fsWatcherAsFlightplanPath = nullptr;
  }
}

void WeatherReporter::createActiveSkyFsWatcher()
{
  if(fsWatcherAsPath == nullptr)
  {
    // Watch file for changes
    fsWatcherAsPath = new FileSystemWatcher(this, verbose);
    fsWatcherAsPath->setMinFileSize(1000);
    FileSystemWatcher::connect(fsWatcherAsPath, &FileSystemWatcher::filesUpdated, this, &WeatherReporter::activeSkyWeatherFilesChanged);
  }

  // Set path only if valid to avoid recursion through paint routine if files are not available
  if(asSnapshotPathChecker->checkFile(Q_FUNC_INFO, asSnapshotPath, false /* warn */))
  {
    // Call notification direct
    activeSkyWeatherFilesChanged({asSnapshotPath});

    fsWatcherAsPath->setFilenameAndStart(asSnapshotPath);
  }

  if(fsWatcherAsFlightplanPath == nullptr)
  {
    // Watch file for changes
    fsWatcherAsFlightplanPath = new FileSystemWatcher(this, verbose);
    fsWatcherAsFlightplanPath->setMinFileSize(50);
    FileSystemWatcher::connect(fsWatcherAsFlightplanPath, &FileSystemWatcher::filesUpdated, this,
                               &WeatherReporter::activeSkyWeatherFilesChanged);
  }

  // Set path only if valid to avoid recursion through paint routine if files are not available
  if(asFlightplanPathChecker->checkFile(Q_FUNC_INFO, asFlightplanPath, false /* warn */))
    fsWatcherAsFlightplanPath->setFilenameAndStart(asFlightplanPath);
}

void WeatherReporter::disableXplane()
{
  if(verbose)
    qDebug() << Q_FUNC_INFO;
  xpWeatherReader->clear();
}

void WeatherReporter::initXplane()
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << simType;

  if(simType == atools::fs::FsPaths::XPLANE_11)
  {
    QString path = OptionData::instance().getWeatherXplane11Path();
    if(path.isEmpty() && !NavApp::getCurrentSimulatorBasePath().isEmpty())
      // Use default base path if simulator installation was found
      path = NavApp::getCurrentSimulatorBasePath() % atools::SEP % "METAR.rwx";

    // Set path but do not start watching - loading starts on first access
    xpWeatherReader->setWeatherPath(path, atools::fs::weather::WEATHER_XP11);
  }
  else if(simType == atools::fs::FsPaths::XPLANE_12)
  {
    QString path = OptionData::instance().getWeatherXplane12Path();
    if(path.isEmpty() && !NavApp::getCurrentSimulatorBasePath().isEmpty())
      // Use default base path if simulator installation was found
      path = NavApp::getCurrentSimulatorBasePath() % atools::SEP % "Output" % atools::SEP % "real weather";

    // Set path but do not start watching - loading starts on first access
    xpWeatherReader->setWeatherPath(path, atools::fs::weather::WEATHER_XP12);
  }
  else
    disableXplane();
}

bool WeatherReporter::checkXplanePaths()
{
  bool valid = false;
  bool xp11 = xpWeatherReader->getWeatherType() == atools::fs::weather::WEATHER_XP11;
  bool& warningShown = xp11 ? xp11WarningPathShown : xp12WarningPathShown;

  // Check only if warning was not already shown and dialog is not open right now
  if(!showingXplaneFileWarning && !warningShown)
  {
    if(atools::fs::FsPaths::isAnyXplane(simType))
    {
      // Validate path
      QString path = xpWeatherReader->getWeatherPath();
      QString message = xp11 ? atools::checkFileMsg(path, 80, false /* warn */) : atools::checkDirMsg(path, 80, false /* warn */);

      // Valid if no error message returned
      valid = message.isEmpty();

      if(!valid && !warningShown)
      {
        // Call dialog in background since this function might be called from a draw handler
        QTimer::singleShot(0, this, std::bind(&WeatherReporter::showXplaneWarningDialog, this, message));
        warningShown = true;
      }
    }
  }

  if(!valid)
    disableXplane();

  return valid;
}

void WeatherReporter::showXplaneWarningDialog(const QString& message)
{
  // Avoid repeated entry
  showingXplaneFileWarning = true;

  NavApp::closeSplashScreen();

  if(NavApp::hasInstalledSimulator(simType))
    // Path not valid ==========
    atools::gui::Dialog(mainWindow).showWarnMsgBox(
      xpWeatherReader->getWeatherType() == atools::fs::weather::WEATHER_XP11 ?
      lnm::ACTIONS_SHOW_XP11_WEATHER_FILE_INVALID : lnm::ACTIONS_SHOW_XP12_WEATHER_FILE_INVALID,
      message % xplaneFileWarningMsg, tr("Do not &show this dialog again."));
  else
    // X-Plane is not installed ==========
    atools::gui::Dialog(mainWindow).showWarnMsgBox(
      xpWeatherReader->getWeatherType() == atools::fs::weather::WEATHER_XP11 ?
      lnm::ACTIONS_SHOW_XP11_WEATHER_FILE_NO_SIM : lnm::ACTIONS_SHOW_XP12_WEATHER_FILE_NO_SIM,
      xplaneMissingWarningMsg, tr("Do not &show this dialog again."));

  showingXplaneFileWarning = false;
}

void WeatherReporter::initActiveSkyPaths()
{
  deleteActiveSkyFsWatcher();

  activeSkyType = NONE;
  activeSkyMetars.clear();
  activeSkyDepartureMetar.clear();
  activeSkyDestinationMetar.clear();
  activeSkyDepartureIdent.clear();
  activeSkyDestinationIdent.clear();

  asSnapshotPath.clear();
  asFlightplanPath.clear();

  QString manualActiveSkySnapshotPath = OptionData::instance().getWeatherActiveSkyPath();
  if(manualActiveSkySnapshotPath.isEmpty())
  {
    // Manual path setting is empty - detect files automatically =======================================================
    QString asnSnapshotPath, asnFlightplanSnapshotPath, as16SnapshotPath, as16FlightplanSnapshotPath,
            asp4SnapshotPath, asp4FlightplanSnapshotPath, asp5SnapshotPath, asp5FlightplanSnapshotPath,
            asXpl11SnapshotPath, aspXpl11FlightplanSnapshotPath, asXpl12SnapshotPath, aspXpl12FlightplanSnapshotPath;

    // Find paths for old sim independent files ===================================================
    findActiveSkyFiles(asnSnapshotPath, asnFlightplanSnapshotPath, "ASN", QString());
    qInfo() << Q_FUNC_INFO << "ASN snapshot" << asnSnapshotPath << "flight plan weather" << asnFlightplanSnapshotPath;

    findActiveSkyFiles(as16SnapshotPath, as16FlightplanSnapshotPath, "AS16_", QString());
    qInfo() << Q_FUNC_INFO << "AS16 snapshot" << as16SnapshotPath << "flight plan weather" << as16FlightplanSnapshotPath;

    // Find paths for sim specific files ===================================================
    if(simType == atools::fs::FsPaths::P3D_V4)
    {
      // C:\Users\USER\AppData\Roaming\Hifi\AS_P3Dv4
      findActiveSkyFiles(asp4SnapshotPath, asp4FlightplanSnapshotPath, "AS_", "P3Dv4");
      qInfo() << Q_FUNC_INFO << "ASP4 snapshot" << asp4SnapshotPath << "flight plan weather" << asp4FlightplanSnapshotPath;
    }
    else if(simType == atools::fs::FsPaths::P3D_V5)
    {
      // C:\Users\USER\AppData\Roaming\Hifi\AS_P3Dv5
      findActiveSkyFiles(asp5SnapshotPath, asp5FlightplanSnapshotPath, "AS_", "P3Dv5");
      qInfo() << Q_FUNC_INFO << "ASP5 snapshot" << asp5SnapshotPath << "flight plan weather" << asp5FlightplanSnapshotPath;
    }
    else if(simType == atools::fs::FsPaths::XPLANE_11)
    {
      // C:\Users\USER\AppData\Roaming\Hifi\AS_XPL
      findActiveSkyFiles(asXpl11SnapshotPath, aspXpl11FlightplanSnapshotPath, "AS_", "XPL");
      qInfo() << Q_FUNC_INFO << "ASXPL11 snapshot" << asXpl11SnapshotPath << "flight plan weather" << aspXpl11FlightplanSnapshotPath;
    }
    else if(simType == atools::fs::FsPaths::XPLANE_12)
    {
      // C:\Users\USER\AppData\Roaming\Hifi\AS_XPL12
      findActiveSkyFiles(asXpl12SnapshotPath, aspXpl12FlightplanSnapshotPath, "AS_", "XPL12");

      // Fall back to C:\Users\USER\AppData\Roaming\Hifi\AS_XPL since documentation is not clear
      if(asXpl12SnapshotPath.isEmpty())
        findActiveSkyFiles(asXpl12SnapshotPath, aspXpl12FlightplanSnapshotPath, "AS_", "XPL");
      qInfo() << Q_FUNC_INFO << "ASXPL12 snapshot" << asXpl12SnapshotPath << "flight plan weather" << aspXpl12FlightplanSnapshotPath;
    }

    // Assign status depending on found files ===================================================
    if(!asXpl12SnapshotPath.isEmpty())
    {
      asSnapshotPath = asXpl12SnapshotPath;
      asFlightplanPath = aspXpl12FlightplanSnapshotPath;
      activeSkyType = ASXPL12;
    }
    else if(!asXpl11SnapshotPath.isEmpty())
    {
      asSnapshotPath = asXpl11SnapshotPath;
      asFlightplanPath = aspXpl11FlightplanSnapshotPath;
      activeSkyType = ASXPL11;
    }
    else if(!asp5SnapshotPath.isEmpty())
    {
      asSnapshotPath = asp5SnapshotPath;
      asFlightplanPath = asp5FlightplanSnapshotPath;
      activeSkyType = ASP5;
    }
    else if(!asp4SnapshotPath.isEmpty())
    {
      asSnapshotPath = asp4SnapshotPath;
      asFlightplanPath = asp4FlightplanSnapshotPath;
      activeSkyType = ASP4;
    }
    else if(!as16SnapshotPath.isEmpty())
    {
      asSnapshotPath = as16SnapshotPath;
      asFlightplanPath = as16FlightplanSnapshotPath;
      activeSkyType = AS16;
    }
    else if(!asnSnapshotPath.isEmpty())
    {
      asSnapshotPath = asnSnapshotPath;
      asFlightplanPath = asnFlightplanSnapshotPath;
      activeSkyType = ASN;
    }
    else
      qWarning() << Q_FUNC_INFO << "No Active Sky weather files found";
  }
  else
  {
    // Manual path overrides found path for all simulators
    asSnapshotPath = manualActiveSkySnapshotPath;
    asFlightplanPath = QFileInfo(manualActiveSkySnapshotPath).path() % atools::SEP % "activeflightplanwx.txt";
    activeSkyType = MANUAL;
  }
}

void WeatherReporter::loadAllActiveSkyFiles()
{
  if(asSnapshotPathChecker->checkFile(Q_FUNC_INFO, asSnapshotPath, false /* warn */))
  {
    if(verbose)
      qDebug() << Q_FUNC_INFO << "Using Active Sky paths" << asSnapshotPath << asFlightplanPath;

    if(activeSkyMetars.isEmpty())
      // Calls loadActiveSkySnapshot() and loadActiveSkyFlightplanSnapshot()
      createActiveSkyFsWatcher();
  }
  else if(verbose)
    qWarning() << Q_FUNC_INFO << "asSnapshotPath" << asSnapshotPath << "asFlightplanPath" << asFlightplanPath << "not found";

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
        qWarning() << Q_FUNC_INFO << "AS file" << file.fileName() << "has invalid entries";
        qWarning() << Q_FUNC_INFO << "line #" << lineNum << line;
      }
      lineNum++;
    }
    file.close();

    qDebug() << Q_FUNC_INFO << "Loaded" << num << "METARs";
  }
  else if(verbose)
    qWarning() << Q_FUNC_INFO << "cannot open" << file.fileName() << "reason" << file.errorString();
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
          activeSkyDepartureMetar = activeSkyDepartureIdent % match.captured(3);

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
          activeSkyDestinationMetar = activeSkyDestinationIdent % match.captured(3);

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
    qWarning() << Q_FUNC_INFO << "cannot open" << file.fileName() << "reason" << file.errorString();
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
    qWarning() << Q_FUNC_INFO << "cannot open" << file.fileName() << "reason" << file.errorString();
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
    qWarning() << Q_FUNC_INFO << "cannot open" << file.fileName() << "reason" << file.errorString();
  return retval;
}

void WeatherReporter::findActiveSkyFiles(QString& asnSnapshot, QString& flightplanSnapshot,
                                         const QString& activeSkyPrefix, const QString& activeSkySimSuffix)
{
 #if defined(Q_OS_WIN32)
  // Use the environment variable because QStandardPaths::ConfigLocation returns an unusable path on Windows
  QString appdata = QProcessEnvironment::systemEnvironment().value("APPDATA");
#else
  // Use $HOME/.config for testing
  QString appdata = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation).constFirst();
#endif

  QString simPathComponent;
  if(activeSkySimSuffix.isEmpty())
  {
    // Determine suffix from sim type
    if(simType == atools::fs::FsPaths::FSX)
      simPathComponent = activeSkyPrefix % "FSX";
    else if(simType == atools::fs::FsPaths::FSX_SE)
      simPathComponent = activeSkyPrefix % "FSX";
    else if(simType == atools::fs::FsPaths::P3D_V3)
      simPathComponent = activeSkyPrefix % "P3D";
    else if(simType == atools::fs::FsPaths::P3D_V4)
      simPathComponent = activeSkyPrefix % "P3D";
    else if(simType == atools::fs::FsPaths::P3D_V5)
      simPathComponent = activeSkyPrefix % "P3D";
    else if(simType == atools::fs::FsPaths::XPLANE_11)
      simPathComponent = activeSkyPrefix % "XP";
    else if(simType == atools::fs::FsPaths::XPLANE_12)
      simPathComponent = activeSkyPrefix % "XP"; // Unknown for now
  }
  else
    // Use fixed suffix for AS4
    simPathComponent = activeSkyPrefix % activeSkySimSuffix;

  QString hifiPath = appdata % atools::SEP % "HiFi" % atools::SEP % simPathComponent % atools::SEP % "Weather" % atools::SEP;

  QString weatherFile = hifiPath % "current_wx_snapshot.txt";

  if(QFile::exists(weatherFile))
  {
    qInfo() << Q_FUNC_INFO << "found Active Sky weather file" << weatherFile;
    if(validateActiveSkyFile(weatherFile))
      asnSnapshot = weatherFile;
    else
      qWarning() << Q_FUNC_INFO << "is not an ASN weather snapshot file" << weatherFile;
  }
  else
    qInfo() << Q_FUNC_INFO << "file does not exist" << weatherFile;

  weatherFile = hifiPath % "activeflightplanwx.txt";

  if(QFile::exists(weatherFile))
  {
    qInfo() << Q_FUNC_INFO << "found Active Sky flight plan weathers file" << weatherFile;
    if(validateActiveSkyFlightplanFile(weatherFile))
      flightplanSnapshot = weatherFile;
    else
      qWarning() << Q_FUNC_INFO << "is not an Active Sky flight plan weather snapshot file" << weatherFile;
  }
  else
    qInfo() << Q_FUNC_INFO << "file does not exist" << weatherFile;
}

bool WeatherReporter::testUrl(QStringList& result, const QString& url, const QString& airportIcao,
                              const QHash<QString, QString>& headerParameters)
{
  return atools::fs::weather::testUrl(result, url, airportIcao, headerParameters);
}

QString WeatherReporter::getCurrentActiveSkyName() const
{
  if(activeSkyType == WeatherReporter::ASP4)
    return tr("ASP4");
  else if(activeSkyType == WeatherReporter::ASP5)
    return tr("ASP5");
  else if(activeSkyType == WeatherReporter::AS16)
    return tr("AS16");
  else if(activeSkyType == WeatherReporter::ASN)
    return tr("ASN");
  else if(activeSkyType == WeatherReporter::ASXPL11)
    return tr("ASXP11");
  else if(activeSkyType == WeatherReporter::ASXPL12)
    return tr("ASXP12");
  else
    // Manually selected
    return tr("Active Sky");
}

const QString& WeatherReporter::getActiveSkyDepartureIdent()
{
  loadAllActiveSkyFiles();
  return activeSkyDepartureIdent;
}

const QString& WeatherReporter::getActiveSkyDestinationIdent()
{
  loadAllActiveSkyFiles();
  return activeSkyDestinationIdent;
}

void WeatherReporter::updateAirportWeather()
{
  updateTimeouts();
}

void WeatherReporter::weatherDownloadSslErrors(const QStringList& errors, const QString& downloadUrl)
{
  qWarning() << Q_FUNC_INFO;
  NavApp::closeSplashScreen();

  int result = atools::gui::Dialog(mainWindow).
               showQuestionMsgBox(lnm::ACTIONS_SHOW_SSL_WARNING_WEATHER,
                                  tr("<p>Errors while trying to establish an encrypted "
                                       "connection to download weather information:</p>"
                                       "<p>URL: %1</p>"
                                         "<p>Error messages:<br/>%2</p>"
                                           "<p>Continue?</p>").
                                  arg(downloadUrl).
                                  arg(atools::strJoin(errors, tr("<br/>"))),
                                  tr("Do not &show this again and ignore errors."),
                                  QMessageBox::Cancel | QMessageBox::Yes,
                                  QMessageBox::Cancel, QMessageBox::Yes);

  atools::fs::weather::WeatherDownloadBase *downloader = dynamic_cast<atools::fs::weather::WeatherDownloadBase *>(sender());

  if(downloader != nullptr)
    downloader->setIgnoreSslErrors(result == QMessageBox::Yes);
  else
    qWarning() << Q_FUNC_INFO << "Downloader is null";
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
                                                       "<p>Check weather settings or disable weather downloads.</p>"
                                                         "<p>Suppressing further messages during this session.</p>").
                                                   arg(url).arg(error).arg(errorCode),
                                                   tr("Do not &show this dialog again."));
  }
}

QString WeatherReporter::getActiveSkyMetar(const QString& airportIcao)
{
  // Load on demand
  loadAllActiveSkyFiles();

  if(activeSkyDepartureIdent == airportIcao)
    return activeSkyDepartureMetar;
  else if(activeSkyDestinationIdent == airportIcao)
    return activeSkyDestinationMetar;
  else
    // Return value or empty
    return activeSkyMetars.value(airportIcao, QString());
}

atools::fs::weather::MetarResult WeatherReporter::getXplaneMetar(const QString& station, const atools::geo::Pos& pos)
{
  if(xpWeatherReader->getWeatherPath().isEmpty())
    // Display warnings if path is empty
    checkXplanePaths();
  else if(xpWeatherReader->needsLoading())
  {
    // Check paths and show warning dialog if invalid
    if(checkXplanePaths())
    {
      // Loading takes longer - show wait cursor
      QGuiApplication::setOverrideCursor(Qt::WaitCursor);
      xpWeatherReader->load();
      QGuiApplication::restoreOverrideCursor();
    }
  }

  return xpWeatherReader->getXplaneMetar(station, pos);
}

atools::fs::weather::MetarResult WeatherReporter::getNoaaMetar(const QString& airportIcao, const atools::geo::Pos& pos)
{
  return noaaWeather->getMetar(airportIcao, pos);
}

atools::fs::weather::MetarResult WeatherReporter::getVatsimMetar(const QString& airportIcao, const atools::geo::Pos& pos)
{
  return vatsimWeather->getMetar(airportIcao, pos);
}

atools::fs::weather::MetarResult WeatherReporter::getIvaoMetar(const QString& airportIcao, const atools::geo::Pos& pos)
{
  return ivaoWeather->getMetar(airportIcao, pos);
}

atools::fs::weather::Metar WeatherReporter::getAirportWeather(const map::MapAirport& airport, bool stationOnly)
{
  // Empty position forces station only instead of allowing nearest
  const atools::geo::Pos& pos = stationOnly ? atools::geo::EMPTY_POS : airport.position;
  map::MapWeatherSource source = NavApp::getMapWeatherSource();
  const QString& ident = airport.metarIdent();

  switch(source)
  {
    case map::WEATHER_SOURCE_DISABLED:
      return atools::fs::weather::Metar();

    case map::WEATHER_SOURCE_SIMULATOR:
      if(atools::fs::FsPaths::isAnyXplane(NavApp::getCurrentSimulatorDb()))
        // X-Plane weather file
        return Metar(getXplaneMetar(ident, pos).getMetar(stationOnly));
      else if(NavApp::isConnected() /*&& !NavApp::getConnectClient()->isConnectedNetwork()*/)
      {
        atools::fs::weather::MetarResult res = NavApp::getConnectClient()->requestWeather(ident, pos, true);

        if(res.isValid() && !res.metarForStation.isEmpty())
          // FSX/P3D - Flight simulator fetched weather or network connection
          return Metar(res.metarForStation, res.requestIdent, res.timestamp, true);
      }
      return Metar();

    case map::WEATHER_SOURCE_ACTIVE_SKY:
      return Metar(getActiveSkyMetar(ident));

    case map::WEATHER_SOURCE_NOAA:
      return Metar(getNoaaMetar(ident, pos).getMetar(stationOnly));

    case map::WEATHER_SOURCE_VATSIM:
      return Metar(getVatsimMetar(ident, pos).getMetar(stationOnly));

    case map::WEATHER_SOURCE_IVAO:
      return Metar(getIvaoMetar(ident, pos).getMetar(stationOnly));
  }
  return Metar();
}

void WeatherReporter::getAirportWind(int& windDirectionDeg, float& windSpeedKts, const map::MapAirport& airport, bool stationOnly)
{
  atools::fs::weather::Metar metar = getAirportWeather(airport, stationOnly);
  const atools::fs::weather::MetarParser& parsed = metar.getParsedMetar();
  windDirectionDeg = parsed.getWindDir();
  windSpeedKts = parsed.getWindSpeedKts();
}

void WeatherReporter::getBestRunwaysTextShort(QString& title, QString& runwayNumbers, QString& sourceText, const map::MapAirport& airport)
{
  if(NavApp::getAirportWeatherSource() != map::WEATHER_SOURCE_DISABLED)
  {
    int windDirectionDeg;
    float windSpeedKts;
    getAirportWind(windDirectionDeg, windSpeedKts, airport, false /* stationOnly */);

    // Need wind direction and speed - otherwise all runways are good =======================
    if(windDirectionDeg != -1 && windSpeedKts < atools::fs::weather::INVALID_METAR_VALUE)
    {
      // Sorted by wind and merged for same direction
      maptools::RwVector ends(windSpeedKts, windDirectionDeg);
      NavApp::getInfoQuery()->getRunwayEnds(ends, airport.id);

      if(!ends.isEmpty())
      {
        // Simple runway list for tooltips only with headwind > 1
        QStringList runways = ends.getSortedRunways(2);
        if(!runways.isEmpty())
        {
          title = runways.size() == 1 ? tr("Wind prefers runway:") : tr("Wind prefers runways:");
          runwayNumbers = tr("%1.").arg(atools::strJoin(runways.mid(0, 4), tr(", "), tr(" and ")));
        }
      }

      if(runwayNumbers.isEmpty())
        title = tr("All runways good for takeoff and landing.");
    }
    else
      title = tr("No wind information.");

    sourceText = tr("Using airport weather source \"%1\".").arg(map::mapWeatherSourceString(NavApp::getAirportWeatherSource()));
  }
  else
    sourceText = tr("Airport weather source is disabled in menu \"Weather\" -> \"Airport Weather Source\".");
}

void WeatherReporter::preDatabaseLoad()
{

}

void WeatherReporter::postDatabaseLoad(atools::fs::FsPaths::SimulatorType type)
{
  if(type != simType)
  {
    // Enable warning dialogs about wrong paths again
    xp11WarningPathShown = xp12WarningPathShown = false;

    // Simulator has changed - reload files
    simType = type;
    resetErrorState();
    updateTimeouts();
    initActiveSkyPaths();
    initXplane();
  }
}

void WeatherReporter::optionsChanged()
{
  vatsimWeather->setRequestUrl(OptionData::instance().getWeatherVatsimUrl());
  noaaWeather->setRequestUrl(OptionData::instance().getWeatherNoaaUrl());
  ivaoWeather->setRequestUrl(OptionData::instance().getWeatherIvaoUrl());

  // Enable warning dialogs about wrong paths again
  xp11WarningPathShown = xp12WarningPathShown = false;

  resetErrorState();
  updateTimeouts();
  initActiveSkyPaths();
  initXplane();
}

void WeatherReporter::resetErrorState()
{
  vatsimWeather->setErrorStateTimer(false);
  noaaWeather->setErrorStateTimer(false);
  ivaoWeather->setErrorStateTimer(false);
}

void WeatherReporter::updateTimeouts()
{
  map::MapWeatherSource airportWeatherSource = NavApp::getMapWidgetGui() != nullptr ?
                                               NavApp::getAirportWeatherSource() : map::WEATHER_SOURCE_SIMULATOR;

  // Disable periodic downloads if feature is not needed
  optsw::FlagsWeather flags = OptionData::instance().getFlagsWeather();

  if(atools::fs::FsPaths::isAnyXplane(simType) && (flags & optsw::WEATHER_INFO_FS || flags & optsw::WEATHER_TOOLTIP_FS ||
                                                   airportWeatherSource == map::WEATHER_SOURCE_SIMULATOR))
    initXplane();
  else
    disableXplane();

  if(flags & optsw::WEATHER_INFO_NOAA || flags & optsw::WEATHER_TOOLTIP_NOAA || airportWeatherSource == map::WEATHER_SOURCE_NOAA)
    noaaWeather->setUpdatePeriod(onlineWeatherTimeoutSecs);
  else
    noaaWeather->setUpdatePeriod(-1);

  if(flags & optsw::WEATHER_INFO_IVAO || flags & optsw::WEATHER_TOOLTIP_IVAO || airportWeatherSource == map::WEATHER_SOURCE_IVAO)
    ivaoWeather->setUpdatePeriod(onlineWeatherTimeoutSecs);
  else
    ivaoWeather->setUpdatePeriod(-1);

  if(flags & optsw::WEATHER_INFO_VATSIM || flags & optsw::WEATHER_TOOLTIP_VATSIM || airportWeatherSource == map::WEATHER_SOURCE_VATSIM)
    vatsimWeather->setUpdatePeriod(onlineWeatherTimeoutSecs);
  else
    vatsimWeather->setUpdatePeriod(-1);
}

void WeatherReporter::activeSkyWeatherFilesChanged(const QStringList& paths)
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << "file" << paths << "changed";

  if(asSnapshotPathChecker->isValid())
    loadActiveSkySnapshot(asSnapshotPath);

  if(asFlightplanPathChecker->isValid())
    loadActiveSkyFlightplanSnapshot(asFlightplanPath);

  if(asSnapshotPathChecker->isValid())
  {
    mainWindow->setStatusMessage(tr("Active Sky weather information updated."), true /* addToLog */);
    emit weatherUpdated();
  }
}

void WeatherReporter::xplaneWeatherFileChanged()
{
  mainWindow->setStatusMessage(tr("X-Plane weather information updated."), true /* addToLog */);
  emit weatherUpdated();
}

void WeatherReporter::debugDumpContainerSizes() const
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << "activeSkyMetars.size()" << activeSkyMetars.size();

  if(noaaWeather != nullptr)
    noaaWeather->debugDumpContainerSizes();
  if(vatsimWeather != nullptr)
    vatsimWeather->debugDumpContainerSizes();
  if(ivaoWeather != nullptr)
    ivaoWeather->debugDumpContainerSizes();
  if(xpWeatherReader != nullptr)
    xpWeatherReader->debugDumpContainerSizes();
}
