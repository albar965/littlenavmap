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

#include "online/onlinedatacontroller.h"

#include "fs/online/onlinedatamanager.h"
#include "airspace/airspacecontroller.h"
#include "util/httpdownloader.h"
#include "gui/mainwindow.h"
#include "common/constants.h"
#include "settings/settings.h"
#include "options/optiondata.h"
#include "zip/gzip.h"
#include "gui/dialog.h"
#include "geo/calculations.h"
#include "sql/sqlquery.h"
#include "sql/sqlrecord.h"
#include "mapgui/maplayer.h"
#include "fs/sc/simconnectuseraircraft.h"
#include "navapp.h"

#include <QDebug>
#include <QMessageBox>
#include <QTextCodec>
#include <QApplication>

// #define DEBUG_ONLINE_DOWNLOAD 1

static const int MIN_SERVER_DOWNLOAD_INTERVAL_MIN = 15;

// Remove if duplicates with same registration if they are this close (500 kts for 3 min)
#ifdef DEBUG_INFORMATION
static const int MIN_DISTANCE_DUPLICATE_M = atools::geo::nmToMeter(900);
#else
static const int MIN_DISTANCE_DUPLICATE_M = atools::geo::nmToMeter(30);
#endif

using atools::fs::sc::SimConnectAircraft;
using atools::fs::online::OnlinedataManager;
using atools::util::HttpDownloader;
using atools::geo::LineString;
using atools::geo::Pos;

atools::fs::online::Format convertFormat(opts::OnlineFormat format)
{
  switch(format)
  {
    case opts::ONLINE_FORMAT_VATSIM:
      return atools::fs::online::VATSIM;

    case opts::ONLINE_FORMAT_IVAO:
      return atools::fs::online::IVAO;
  }
  return atools::fs::online::UNKNOWN;
}

OnlinedataController::OnlinedataController(atools::fs::online::OnlinedataManager *onlineManager, MainWindow *parent)
  : manager(onlineManager), mainWindow(parent)
{
  // Files use Windows code with emebedded UTF-8 for ATIS text
  codec = QTextCodec::codecForName("Windows-1252");
  if(codec == nullptr)
    codec = QTextCodec::codecForLocale();

  downloader = new atools::util::HttpDownloader(mainWindow, false /* verbose */);

  updateAtcSizes();

  connect(downloader, &HttpDownloader::downloadFinished, this, &OnlinedataController::downloadFinished);
  connect(downloader, &HttpDownloader::downloadFailed, this, &OnlinedataController::downloadFailed);
  connect(downloader, &HttpDownloader::downloadSslErrors, this, &OnlinedataController::downloadSslErrors);

  // Recurring downloads
  connect(&downloadTimer, &QTimer::timeout, this, &OnlinedataController::startDownloadInternal);

  using namespace std::placeholders;
  manager->setGeometryCallback(std::bind(&OnlinedataController::geometryCallback, this, _1, _2));

#ifdef DEBUG_ONLINE_DOWNLOAD
  downloader->enableCache(60);
#endif
}

OnlinedataController::~OnlinedataController()
{
  manager->setGeometryCallback(atools::fs::online::GeoCallbackType(nullptr));

  deInitQueries();

  delete downloader;

  // Remove all from the database to avoid confusion on startup
#ifndef DEBUG_INFORMATION
  manager->clearData();
#endif
}

void OnlinedataController::updateAtcSizes()
{
  // Override default circle radius for certain ATC center types
  const OptionData& opts = OptionData::instance();

  QHash<atools::fs::online::fac::FacilityType, int> sizeMap;
  for(atools::fs::online::fac::FacilityType type : atools::fs::online::allFacilityTypes())
  {
    int diameter = -1;
    switch(type)
    {
      case atools::fs::online::fac::UNKNOWN:
        break;
      case atools::fs::online::fac::OBSERVER:
        diameter = opts.getDisplayOnlineObserver();
        break;
      case atools::fs::online::fac::FLIGHT_INFORMATION:
        diameter = opts.getDisplayOnlineFir();
        break;
      case atools::fs::online::fac::DELIVERY:
        diameter = opts.getDisplayOnlineClearance();
        break;
      case atools::fs::online::fac::GROUND:
        diameter = opts.getDisplayOnlineGround();
        break;
      case atools::fs::online::fac::TOWER:
        diameter = opts.getDisplayOnlineTower();
        break;
      case atools::fs::online::fac::APPROACH:
        diameter = opts.getDisplayOnlineApproach();
        break;
      case atools::fs::online::fac::ACC:
        diameter = opts.getDisplayOnlineArea();
        break;
      case atools::fs::online::fac::DEPARTURE:
        diameter = opts.getDisplayOnlineDeparture();
        break;
    }

    sizeMap.insert(type, diameter != -1 ? std::max(1, diameter / 2) : -1);
  }
  manager->setAtcSize(sizeMap);
}

void OnlinedataController::startProcessing()
{
  startDownloadInternal();
}

void OnlinedataController::startDownloadInternal()
{
  qDebug() << Q_FUNC_INFO;

  if(downloader->isDownloading() || currentState != NONE)
  {
    qWarning() << Q_FUNC_INFO;
    return;
  }

  stopAllProcesses();

  const OptionData& od = OptionData::instance();
  opts::OnlineNetwork onlineNetwork = od.getOnlineNetwork();
  if(onlineNetwork == opts::ONLINE_NONE)
    // No online functionality set in options
    return;

  // Get URLs from configuration which are already set accoding to selected network
  QString onlineStatusUrl = od.getOnlineStatusUrl();
  QString onlineWhazzupUrl = od.getOnlineWhazzupUrl();
  QString whazzupUrlFromStatus = manager->getWhazzupUrlFromStatus(whazzupGzipped);

  if(currentState == NONE)
  {
    // Create a default user agent if not disabled for debugging
    if(!atools::settings::Settings::instance().valueBool(lnm::OPTIONS_NO_USER_AGENT))
      downloader->setDefaultUserAgentShort(QString(" Config/%1").arg(getNetwork()));

    QString url;
    if(whazzupUrlFromStatus.isEmpty() && // Status not downloaded yet
       !onlineStatusUrl.isEmpty()) // Need  status.txt by configuration
    {
      // Start status.txt and whazzup.txt download cycle
      url = onlineStatusUrl;
      currentState = DOWNLOADING_STATUS;
    }
    else if(!onlineWhazzupUrl.isEmpty() || !whazzupUrlFromStatus.isEmpty())
    // Have whazzup.txt url either from config or status
    {
      // Start whazzup.txt and servers.txt download cycle
      url = whazzupUrlFromStatus.isEmpty() ? onlineWhazzupUrl : whazzupUrlFromStatus;
      currentState = DOWNLOADING_WHAZZUP;
    }

    if(!url.isEmpty())
    {
      // Trigger the download chain
      downloader->setUrl(url);

      // Call later in the event loop to avoid recursion
      QTimer::singleShot(0, downloader, &HttpDownloader::startDownload);
    }
  }
  // opts::OnlineFormat onlineFormat = od.getOnlineFormat();
  // int onlineReloadTimeSeconds = od.getOnlineReloadTimeSeconds();
  // QString onlineStatusUrl = od.getOnlineStatusUrl();
  // QString onlineWhazzupUrl = od.getOnlineWhazzupUrl();
}

atools::sql::SqlDatabase *OnlinedataController::getDatabase()
{
  return manager->getDatabase();
}

void OnlinedataController::downloadFinished(const QByteArray& data, QString url)
{
  qDebug() << Q_FUNC_INFO << "url" << url << "data size" << data.size();

  if(currentState == DOWNLOADING_STATUS)
  {
    // Parse status file
    manager->readFromStatus(codec->toUnicode(data));

    // Get URL from status file
    QString whazzupUrlFromStatus = manager->getWhazzupUrlFromStatus(whazzupGzipped);

    if(!manager->getMessageFromStatus().isEmpty())
      // Call later in the event loop
      QTimer::singleShot(0, this, &OnlinedataController::showMessageDialog);

    if(!whazzupUrlFromStatus.isEmpty())
    {
      // Next in chain is whazzup.txt
      currentState = DOWNLOADING_WHAZZUP;
      downloader->setUrl(whazzupUrlFromStatus);

      // Call later in the event loop to avoid recursion
      QTimer::singleShot(0, downloader, &HttpDownloader::startDownload);
    }
    else
    {
      // Done after downloading status.txt - start timer for next session
      startDownloadTimer();
      currentState = NONE;
      lastUpdateTime = QDateTime::currentDateTime();
    }
  }
  else if(currentState == DOWNLOADING_WHAZZUP)
  {
    QByteArray whazzupData;

    if(whazzupGzipped)
    {
      if(!atools::zip::gzipDecompress(data, whazzupData))
        qWarning() << Q_FUNC_INFO << "Error unzipping data";
    }
    else
      whazzupData = data;

    if(manager->readFromWhazzup(codec->toUnicode(whazzupData),
                                convertFormat(OptionData::instance().getOnlineFormat()),
                                manager->getLastUpdateTimeFromWhazzup()))
    {
      // Get all callsigns and positions from online list to allow deduplication
      clientCallsignAndPosMap.clear();
      manager->getClientCallsignAndPosMap(clientCallsignAndPosMap);
      qDebug() << Q_FUNC_INFO << clientCallsignAndPosMap.size();

      QString whazzupVoiceUrlFromStatus = manager->getWhazzupVoiceUrlFromStatus();
      if(!whazzupVoiceUrlFromStatus.isEmpty() &&
         lastServerDownload < QDateTime::currentDateTime().addSecs(-MIN_SERVER_DOWNLOAD_INTERVAL_MIN * 60))
      {
        // Next in chain is server file
        currentState = DOWNLOADING_WHAZZUP_SERVERS;
        downloader->setUrl(whazzupVoiceUrlFromStatus);

        // Call later in the event loop to avoid recursion
        QTimer::singleShot(0, downloader, &HttpDownloader::startDownload);
      }
      else
      {
        // Done after downloading whazzup.txt - start timer for next session
        startDownloadTimer();
        currentState = NONE;
        lastUpdateTime = QDateTime::currentDateTime();

        aircraftCache.clear();
        simulatorAiRegistrations.clear();

        // Message for search tabs, map widget and info
        emit onlineClientAndAtcUpdated(true /* load all */, true /* keep selection */);
        statusBarMessage();
      }
    }
    else
    {
      qInfo() << Q_FUNC_INFO << "whazzup.txt is not recent";

      // Done after old update - try again later
      startDownloadTimer();
      currentState = NONE;
      lastUpdateTime = QDateTime::currentDateTime();
    }
  }
  else if(currentState == DOWNLOADING_WHAZZUP_SERVERS)
  {
    manager->readServersFromWhazzup(codec->toUnicode(data),
                                    convertFormat(OptionData::instance().getOnlineFormat()),
                                    manager->getLastUpdateTimeFromWhazzup());
    lastServerDownload = QDateTime::currentDateTime();

    // Done after downloading server.txt - start timer for next session
    startDownloadTimer();
    currentState = NONE;
    lastUpdateTime = QDateTime::currentDateTime();

    aircraftCache.clear();
    simulatorAiRegistrations.clear();

    // Message for search tabs, map widget and info
    emit onlineClientAndAtcUpdated(true /* load all */, true /* keep selection */);
    emit onlineServersUpdated(true /* load all */, true /* keep selection */);
    statusBarMessage();
  }
}

void OnlinedataController::downloadFailed(const QString& error, int errorCode, QString url)
{
  qWarning() << Q_FUNC_INFO << "Failed" << error << errorCode << url;
  stopAllProcesses();

  mainWindow->setOnlineConnectionStatusMessageText(tr("Online Network Failed"),
                                                   tr("Download from\n\"%1\"\nfailed. "
                                                      "Reason:\n%2\nRetrying again in three minutes.").
                                                   arg(url).arg(error));

  // Delay next download for three minutes to give the user a chance to correct the URLs
  QTimer::singleShot(180 * 1000, this, &OnlinedataController::startProcessing);
}

void OnlinedataController::downloadSslErrors(const QStringList& errors, const QString& downloadUrl)
{
  int result = atools::gui::Dialog(mainWindow).
               showQuestionMsgBox(lnm::ACTIONS_SHOW_SSL_WARNING_ONLINE,
                                  tr("<p>Errors while trying to establish an encrypted connection "
                                       "to download online network data:</p>"
                                       "<p>URL: %1</p>"
                                         "<p>Error messages:<br/>%2</p>"
                                           "<p>Continue?</p>").
                                  arg(downloadUrl).
                                  arg(atools::strJoin(errors, tr("<br/>"))),
                                  tr("Do not show this again and ignore errors in the future"),
                                  QMessageBox::Cancel | QMessageBox::Yes,
                                  QMessageBox::Cancel, QMessageBox::Yes);
  downloader->setIgnoreSslErrors(result == QMessageBox::Yes);
}

void OnlinedataController::statusBarMessage()
{
  mainWindow->setOnlineConnectionStatusMessageText(QString(), tr("Connected to %1.").arg(getNetworkTranslated()));
}

void OnlinedataController::stopAllProcesses()
{
  downloader->cancelDownload();
  downloadTimer.stop();
  currentState = NONE;
  simulatorAiRegistrations.clear();
  // clientCallsignAndPosMap.clear(); // Do not clear these until the download is finished
}

void OnlinedataController::showMessageDialog()
{
  QMessageBox::information(mainWindow, QApplication::applicationName(),
                           tr("Message from downloaded status file:\n\n%2\n").arg(manager->getMessageFromStatus()));
}

LineString *OnlinedataController::geometryCallback(const QString& callsign, atools::fs::online::fac::FacilityType type)
{
  opts2::Flags2 flags2 = OptionData::instance().getFlags2();

  LineString *lineString = nullptr;

  // Try to get airspace boundary by name vs. callsign if set in options
  if(flags2 & opts2::ONLINE_AIRSPACE_BY_NAME)
    lineString = NavApp::getAirspaceController()->
                 getOnlineAirspaceGeoByName(callsign, atools::fs::online::facilityTypeText(type));

  // Try to get airspace boundary by file name vs. callsign if set in options
  if(flags2 & opts2::ONLINE_AIRSPACE_BY_FILE)
  {
    if(lineString == nullptr)
      lineString = NavApp::getAirspaceController()->getOnlineAirspaceGeoByFile(callsign);
  }

  return lineString;
}

void OnlinedataController::optionsChanged()
{
  qDebug() << Q_FUNC_INFO;

  // Clear all URL from status.txt too
  manager->resetForNewOptions();
  stopAllProcesses();
  whazzupGzipped = false;

  // Remove all from the database
  manager->clearData();
  aircraftCache.clear();
  simulatorAiRegistrations.clear();
  clientCallsignAndPosMap.clear();

  updateAtcSizes();

  emit onlineClientAndAtcUpdated(true /* load all */, true /* keep selection */);
  emit onlineServersUpdated(true /* load all */, true /* keep selection */);
  emit onlineNetworkChanged();
  statusBarMessage();

  lastUpdateTime = QDateTime::fromSecsSinceEpoch(0);
  lastServerDownload = QDateTime::fromSecsSinceEpoch(0);

  startDownloadInternal();
}

void OnlinedataController::userAirspacesUpdated()
{
  optionsChanged();
}

bool OnlinedataController::hasData() const
{
  return manager->hasData();
}

QString OnlinedataController::getNetworkTranslated() const
{
  opts::OnlineNetwork onlineNetwork = OptionData::instance().getOnlineNetwork();
  switch(onlineNetwork)
  {
    case opts::ONLINE_NONE:
      return QString();

    case opts::ONLINE_VATSIM:
      return tr("VATSIM");

    case opts::ONLINE_IVAO:
      return tr("IVAO");

    case opts::ONLINE_PILOTEDGE:
      return tr("PilotEdge");

    case opts::ONLINE_CUSTOM_STATUS:
    case opts::ONLINE_CUSTOM:
      return tr("Custom Network");
  }
  return QString();
}

QString OnlinedataController::getNetwork() const
{
  opts::OnlineNetwork onlineNetwork = OptionData::instance().getOnlineNetwork();
  switch(onlineNetwork)
  {
    case opts::ONLINE_NONE:
      return QString();

    case opts::ONLINE_VATSIM:
      return "VATSIM";

    case opts::ONLINE_IVAO:
      return "IVAO";

    case opts::ONLINE_PILOTEDGE:
      return "PilotEdge";

    case opts::ONLINE_CUSTOM_STATUS:
    case opts::ONLINE_CUSTOM:
      return "Custom Network";
  }
  return QString();
}

bool OnlinedataController::isNetworkActive() const
{
  return OptionData::instance().getOnlineNetwork() != opts::ONLINE_NONE;
}

const QList<atools::fs::sc::SimConnectAircraft> *OnlinedataController::getAircraftFromCache()
{
  return &aircraftCache.list;
}

const QList<atools::fs::sc::SimConnectAircraft> *OnlinedataController::getAircraft(const Marble::GeoDataLatLonBox& rect,
                                                                                   const MapLayer *mapLayer, bool lazy)
{
  static const double queryRectInflationFactor = 0.2;
  static const double queryRectInflationIncrement = 0.1;
  static const int queryMaxRows = 5000;

  aircraftCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                            [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
  {
    return curLayer->hasSameQueryParametersWaypoint(newLayer);
  });

  // Remember user aircraft registration aircraft for disambiguation
  const atools::fs::sc::SimConnectUserAircraft& userAircraft = NavApp::getUserAircraft();
  QHash<QString, atools::geo::Pos> curRegistrations;
  curRegistrations.insert(userAircraft.getAirplaneRegistration(), userAircraft.getPosition());

  // Remember valid registrations from simulator aircraft for disambiguation
  if(NavApp::isConnected() || userAircraft.isDebug())
  {
    for(const atools::fs::sc::SimConnectAircraft& aircraft : NavApp::getAiAircraft())
      curRegistrations.insert(aircraft.getAirplaneRegistration(), aircraft.getPosition());
  }
  curRegistrations.remove(QString());

  if(simulatorAiRegistrations.keys() != curRegistrations.keys())
    // List of registrations has changed - clear cache and reload
    aircraftCache.clear();

  if((aircraftCache.list.isEmpty() && !lazy))
  {
    for(const Marble::GeoDataLatLonBox& r :
        query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      query::bindRect(r, aircraftByRectQuery);
      aircraftByRectQuery->exec();
      while(aircraftByRectQuery->next())
      {
        atools::fs::sc::SimConnectAircraft aircraft;
        fillAircraftFromClient(aircraft, aircraftByRectQuery->record());

        if(!curRegistrations.contains(aircraft.getAirplaneRegistration()) ||
           aircraft.getPosition().distanceMeterTo(curRegistrations.value(aircraft.getAirplaneRegistration())) >
           MIN_DISTANCE_DUPLICATE_M)
          // Avoid duplicates with simulator aircraft that are close by
          aircraftCache.list.append(aircraft);
      }
    }
    simulatorAiRegistrations = curRegistrations;
  }
  aircraftCache.validate(queryMaxRows);
  return &aircraftCache.list;
}

bool OnlinedataController::getShadowAircraft(atools::fs::sc::SimConnectAircraft& onlineClient,
                                             const atools::fs::sc::SimConnectAircraft& simAircraft)
{
  if(isShadowAircraft(simAircraft))
  {
    atools::sql::SqlRecordVector clients = manager->getClientRecordsByCallsign(simAircraft.getAirplaneRegistration());

    if(!clients.isEmpty())
    {
      fillAircraftFromClient(onlineClient, clients.first());

      // Update to real simulator position including altitude for shadows
      onlineClient.getPosition() = simAircraft.getPosition();
      return true;
    }
    else
      qWarning() << Q_FUNC_INFO << "No client found for" << simAircraft.getAirplaneRegistration();
  }
  return false;
}

bool OnlinedataController::isShadowAircraft(const atools::fs::sc::SimConnectAircraft& simAircraft)
{
  const atools::geo::Pos pos = clientCallsignAndPosMap.value(simAircraft.getAirplaneRegistration());
  return simAircraft.isOnlineShadow() ||
         (pos.isValid() && pos.distanceMeterTo(simAircraft.getPosition()) < MIN_DISTANCE_DUPLICATE_M);
}

void OnlinedataController::getClientAircraftById(atools::fs::sc::SimConnectAircraft& aircraft, int id)
{
  manager->getClientAircraftById(aircraft, id);
}

void OnlinedataController::fillAircraftFromClient(atools::fs::sc::SimConnectAircraft& ac,
                                                  const atools::sql::SqlRecord& record)
{
  OnlinedataManager::fillFromClient(ac, record);
}

void OnlinedataController::filterOnlineShadowAircraft(QList<map::MapOnlineAircraft>& onlineAircraft,
                                                      const QList<map::MapAiAircraft>& simAircraft)
{
  // Collect simulator shadow aircraft
  QHash<QString, Pos> registrations;
  for(const map::MapAiAircraft& ac : simAircraft)
  {
    if(ac.getAircraft().isOnlineShadow() && !ac.getAircraft().getAirplaneRegistration().isEmpty() &&
       simulatorAiRegistrations.contains(ac.getAircraft().getAirplaneRegistration()))
      registrations.insert(ac.getAircraft().getAirplaneRegistration(), ac.getPosition());
  }

  // Remove the shadowed aircraft from the online list which have a copy in simulator
  auto it = std::remove_if(onlineAircraft.begin(), onlineAircraft.end(), [registrations](
                             const map::MapOnlineAircraft& aircraft) -> bool
  {
    return registrations.contains(aircraft.getAircraft().getAirplaneRegistration()) &&
    aircraft.getPosition().distanceMeterTo(registrations.value(aircraft.getAircraft().getAirplaneRegistration())) <=
    MIN_DISTANCE_DUPLICATE_M;
  });

  if(it != onlineAircraft.end())
    onlineAircraft.erase(it, onlineAircraft.end());
}

atools::sql::SqlRecord OnlinedataController::getClientRecordById(int clientId)
{
  return manager->getClientRecordById(clientId);
}

void OnlinedataController::initQueries()
{
  deInitQueries();

  manager->initQueries();

  aircraftByRectQuery = new atools::sql::SqlQuery(getDatabase());
  aircraftByRectQuery->prepare("select * from client "
                               "where lonx between :leftx and :rightx and "
                               "laty between :bottomy and :topy");
}

void OnlinedataController::deInitQueries()
{
  aircraftCache.clear();

  manager->deInitQueries();

  delete aircraftByRectQuery;
  aircraftByRectQuery = nullptr;
}

int OnlinedataController::getNumClients() const
{
  return manager->getNumClients();
}

void OnlinedataController::startDownloadTimer()
{
  downloadTimer.stop();

  opts::OnlineNetwork onlineNetwork = OptionData::instance().getOnlineNetwork();
  QString source;

  // Use three minutes as default if nothing is given
  int intervalSeconds;

  if(onlineNetwork == opts::ONLINE_CUSTOM || onlineNetwork == opts::ONLINE_CUSTOM_STATUS)
  {
    // Use options for custom network - ignore reload in whazzup.txt
    intervalSeconds = OptionData::instance().getOnlineReloadTimeSeconds();
    source = "options";
  }
  else
  {
    int reloadFromCfg = OptionData::instance().getOnlineReloadTimeSecondsConfig();
    if(reloadFromCfg == -1)
    {
      // Use time from whazzup.txt - mode auto
      intervalSeconds = std::max(manager->getReloadMinutesFromWhazzup() * 60, 60);
      source = "whazzup";
    }
    else
    {
      intervalSeconds = std::max(reloadFromCfg, 60);
      source = "networks.cfg";
    }
  }

  qDebug() << Q_FUNC_INFO << "timer set to" << intervalSeconds << "from" << source;

#ifdef DEBUG_ONLINE_DOWNLOAD
  downloadTimer.setInterval(2000);
#else
  downloadTimer.setInterval(intervalSeconds * 1000);
#endif
  downloadTimer.start();
}
