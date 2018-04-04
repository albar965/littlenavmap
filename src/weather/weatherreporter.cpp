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

#include "weather/weatherreporter.h"

#include "gui/mainwindow.h"
#include "settings/settings.h"
#include "options/optiondata.h"
#include "common/constants.h"
#include "fs/weather/xpweatherreader.h"
#include "navapp.h"
#include "fs/weather/weathernetdownload.h"
#include "fs/sc/simconnecttypes.h"
#include "query/mapquery.h"
#include "query/airportquery.h"
#include "fs/weather/weathernetsingle.h"

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
static const QRegularExpression ASN_VALIDATE_REGEXP("^[A-Z0-9]{3,4}::[A-Z0-9]{3,4} .+$");
static const QRegularExpression ASN_VALIDATE_FLIGHTPLAN_REGEXP("^DepartureMETAR=.+$");
static const QRegularExpression ASN_FLIGHTPLAN_REGEXP("^(DepartureMETAR|DestinationMETAR)=([A-Z0-9]{3,4})?(.*)$");

using atools::fs::FsPaths;
using atools::fs::weather::WeatherNetDownload;
using atools::fs::weather::WeatherNetSingle;

WeatherReporter::WeatherReporter(MainWindow *parentWindow, atools::fs::FsPaths::SimulatorType type)
  : QObject(parentWindow), simType(type),
  mainWindow(parentWindow)
{
  onlineWeatherTimeoutSecs = atools::settings::Settings::instance().valueInt(lnm::OPTIONS_WEATHER_UPDATE, 600);

  xpWeatherReader = new atools::fs::weather::XpWeatherReader(this);

  bool verbose = false;
#ifdef DEBUG_INFORMATION
  verbose = true;
#endif

  noaaWeather = new WeatherNetSingle(parentWindow, onlineWeatherTimeoutSecs, verbose);
  QString noaaUrl(OptionData::instance().getWeatherNoaaUrl());
  noaaWeather->setRequestUrl(noaaUrl);
  noaaWeather->setStationIndexUrl(noaaUrl.left(noaaUrl.lastIndexOf("/")) + "/", noaaIndexParser);
  noaaWeather->setFetchAirportCoords(fetchAirportCoordinates);

  vatsimWeather = new WeatherNetSingle(parentWindow, onlineWeatherTimeoutSecs, verbose);
  vatsimWeather->setRequestUrl(OptionData::instance().getWeatherVatsimUrl());

  ivaoWeather = new WeatherNetDownload(parentWindow, verbose);
  ivaoWeather->setRequestUrl(OptionData::instance().getWeatherIvaoUrl());
  // Set callback so the reader can build an index for nearest airports
  ivaoWeather->setFetchAirportCoords(fetchAirportCoordinates);
  if(OptionData::instance().getFlags2() & opts::WEATHER_INFO_IVAO ||
     OptionData::instance().getFlags2() & opts::WEATHER_TOOLTIP_IVAO)
    ivaoWeather->setUpdatePeriod(onlineWeatherTimeoutSecs);
  else
    ivaoWeather->setUpdatePeriod(-1);

  initActiveSkyNext();

  // Set callback so the reader can build an index for nearest airports
  xpWeatherReader->setFetchAirportCoords(fetchAirportCoordinates);
  initXplane();

  connect(xpWeatherReader, &atools::fs::weather::XpWeatherReader::weatherUpdated,
          this, &WeatherReporter::xplaneWeatherFileChanged);

  // Forward signals from clients
  connect(noaaWeather, &WeatherNetSingle::weatherUpdated, this, &WeatherReporter::weatherUpdated);
  connect(vatsimWeather, &WeatherNetSingle::weatherUpdated, this, &WeatherReporter::weatherUpdated);
  connect(ivaoWeather, &WeatherNetDownload::weatherUpdated, this, &WeatherReporter::weatherUpdated);
}

WeatherReporter::~WeatherReporter()
{
  deleteFsWatcher();
  delete noaaWeather;
  delete vatsimWeather;
  delete ivaoWeather;

  delete xpWeatherReader;
}

atools::geo::Pos WeatherReporter::fetchAirportCoordinates(const QString& airportIdent)
{
  return NavApp::getAirportQuerySim()->getAirportCoordinatesByIdent(airportIdent);
}

void WeatherReporter::noaaIndexParser(QString& icao, QDateTime& lastUpdate, const QString& line)
{
  // Need to use hardcoded English month names since QDate uses localized names
  static const QStringList months({"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"});
  static const QRegularExpression INDEX_LINE_REGEXP(
    ">([A-Z0-9]{3,4}).TXT<.*>(\\d+)-(\\S+)-(\\d+)\\s+(\\d+):(\\d+)\\s*<");

  // <tr><td><a href="AGGM.TXT">AGGM.TXT</a></td><td align="right">09-Feb-2018 03:06  </td><td align="right"> 72 </td></tr>
  // <tr><td><a href="AGTB.TXT">AGTB.TXT</a></td><td align="right">27-Feb-2009 10:31  </td><td align="right"> 88 </td></tr>
  // <tr><td><a href="AK15.TXT">AK15.TXT</a></td><td align="right">13-Jan-2014 13:20  </td><td align="right"> 58 </td></tr>
  // qDebug() << Q_FUNC_INFO << line;

  QRegularExpressionMatch match = INDEX_LINE_REGEXP.match(line);
  if(match.hasMatch())
  {
    icao = match.captured(1).toUpper();

    // 09-Feb-2018 03:06
    int day = match.captured(2).toInt();
    int month = months.indexOf(match.captured(3)) + 1;
    int year = match.captured(4).toInt();
    int hour = match.captured(5).toInt();
    int minute = match.captured(6).toInt();

    lastUpdate = QDateTime(QDate(year, month, day), QTime(hour, minute));

    // qDebug() << icao << lastUpdate;
  }
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

bool WeatherReporter::testUrl(const QString& url, const QString& airportIcao, QString& result)
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
  else
    // Manually selected
    return tr("Active Sky");
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

QString WeatherReporter::getVatsimMetar(const QString& airportIcao)
{
  // VATSIM does not use nearest station
  return vatsimWeather->getMetar(airportIcao, atools::geo::EMPTY_POS).metarForStation;
}

atools::fs::weather::MetarResult WeatherReporter::getIvaoMetar(const QString& airportIcao, const atools::geo::Pos& pos)
{
  return ivaoWeather->getMetar(airportIcao, pos);
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
  vatsimWeather->setRequestUrl(OptionData::instance().getWeatherVatsimUrl());
  noaaWeather->setRequestUrl(OptionData::instance().getWeatherNoaaUrl());
  ivaoWeather->setRequestUrl(OptionData::instance().getWeatherIvaoUrl());

  if(OptionData::instance().getFlags2() & opts::WEATHER_INFO_IVAO ||
     OptionData::instance().getFlags2() & opts::WEATHER_TOOLTIP_IVAO)
    ivaoWeather->setUpdatePeriod(onlineWeatherTimeoutSecs);
  else
    ivaoWeather->setUpdatePeriod(-1);

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
