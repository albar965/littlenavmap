/*****************************************************************************
* Copyright 2015-2022 Alexander Barthel alex@littlenavmap.org
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
#include "common/maptools.h"
#include "common/constants.h"
#include "options/optiondata.h"
#include "zip/gzip.h"
#include "gui/dialog.h"
#include "geo/calculations.h"
#include "sql/sqlquery.h"
#include "sql/sqlrecord.h"
#include "mapgui/maplayer.h"
#include "app/navapp.h"
#include "settings/settings.h"
#include "fs/sc/simconnectdata.h"

#include <QDebug>
#include <QMessageBox>
#include <QTextCodec>
#include <QCoreApplication>

static const int MIN_SERVER_DOWNLOAD_INTERVAL_MIN = 15;
static const int MIN_TRANSCEIVER_DOWNLOAD_INTERVAL_MIN = 5;

// Minimum reload time for whazzup files (JSON or txt)
static const int MIN_RELOAD_TIME_SECONDS = 15;

using atools::fs::online::OnlinedataManager;
using atools::util::HttpDownloader;
using atools::geo::LineString;
using atools::geo::Pos;
using atools::fs::online::OnlineAircraft;
using atools::fs::sc::SimConnectAircraft;

atools::fs::online::Format convertFormat(opts::OnlineFormat format)
{
  switch(format)
  {
    case opts::ONLINE_FORMAT_VATSIM:
      return atools::fs::online::VATSIM;

    case opts::ONLINE_FORMAT_VATSIM_JSON:
      return atools::fs::online::VATSIM_JSON3;

    case opts::ONLINE_FORMAT_IVAO:
      return atools::fs::online::IVAO;

    case opts::ONLINE_FORMAT_IVAO_JSON:
      return atools::fs::online::IVAO_JSON2;
  }
  return atools::fs::online::UNKNOWN;
}

OnlinedataController::OnlinedataController(atools::fs::online::OnlinedataManager *onlineManager, MainWindow *parent)
  : manager(onlineManager), mainWindow(parent), aircraftCache()
{
  // Files use Windows code with embedded UTF-8 for ATIS text
  codec = QTextCodec::codecForName("Windows-1252");
  if(codec == nullptr)
    codec = QTextCodec::codecForLocale();

  atools::settings::Settings& settings = atools::settings::Settings::instance();
  verbose = settings.getAndStoreValue(lnm::OPTIONS_ONLINE_NETWORK_DEBUG, false).toBool();

  // Load criteria used to detect shadow aircraft right after download finished
  maxShadowDistanceNm = settings.getAndStoreValue(lnm::OPTIONS_ONLINE_NETWORK_MAX_SHADOW_DIST_NM, 2.0).toFloat();
  maxShadowAltDiffFt = settings.getAndStoreValue(lnm::OPTIONS_ONLINE_NETWORK_MAX_SHADOW_ALT_DIFF_FT, 1000.).toFloat();
  maxShadowGsDiffKts = settings.getAndStoreValue(lnm::OPTIONS_ONLINE_NETWORK_MAX_SHADOW_GS_DIFF_KTS, 50.).toFloat();
  maxShadowHdgDiffDeg = settings.getAndStoreValue(lnm::OPTIONS_ONLINE_NETWORK_MAX_SHADOW_HDG_DIFF_DEG, 90.).toFloat();

  downloader = new atools::util::HttpDownloader(mainWindow, verbose);

  // Request gzipped content if possible
  downloader->setAcceptEncoding("gzip");

  updateAtcSizes();

  connect(downloader, &HttpDownloader::downloadFinished, this, &OnlinedataController::downloadFinished);
  connect(downloader, &HttpDownloader::downloadFailed, this, &OnlinedataController::downloadFailed);
  connect(downloader, &HttpDownloader::downloadSslErrors, this, &OnlinedataController::downloadSslErrors);

  // Recurring downloads
  connect(&downloadTimer, &QTimer::timeout, this, &OnlinedataController::startDownloadInternal);

  using namespace std::placeholders;
  manager->setGeometryCallback(std::bind(&OnlinedataController::airspaceGeometryCallback, this, _1, _2));

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
  if(verbose)
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
  bool whazzupGzipped = false, whazzupJson = false;
  whazzupUrlFromStatus = manager->getWhazzupUrlFromStatus(whazzupGzipped, whazzupJson);

  if(currentState == NONE) // Happens if the timeout is triggered - not in a download chain
  {
    // Check for timeout of transceivers and start download before whazzup JSON
    int transceiverReload = OptionData::instance().getOnlineVatsimTransceiverReload();
    if(transceiverReload == -1)
      transceiverReload = MIN_TRANSCEIVER_DOWNLOAD_INTERVAL_MIN * 60;

    QString url;
    if(convertFormat(OptionData::instance().getOnlineFormat()) == atools::fs::online::VATSIM_JSON3 &&
       lastUpdateTimeTransceivers.isValid() && // Initial download already happened
       lastUpdateTimeTransceivers < QDateTime::currentDateTime().addSecs(-transceiverReload)) // Check age of download
    {
      // Download transceivers since data is too old - whazzup is downloaded right after this
      url = OptionData::instance().getOnlineTransceiverUrl();
      currentState = DOWNLOADING_TRANSCEIVERS;
    }
    else
    {
      // Create a default user agent if not disabled for debugging
      if(!atools::settings::Settings::instance().valueBool(lnm::OPTIONS_NO_USER_AGENT))
        downloader->setDefaultUserAgentShort(QString(" Config/%1").arg(getNetwork()));

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
    }

    if(!url.isEmpty())
    {
      // Trigger the download chain
      downloader->setUrl(url);
      startDownloader();
    }
  }
}

atools::sql::SqlDatabase *OnlinedataController::getDatabase()
{
  return manager->getDatabase();
}

QString OnlinedataController::uncompress(const QByteArray& data, const QString& func, bool utf8)
{
  QByteArray textData = atools::zip::gzipDecompressIf(data, func);

  if(utf8)
    return QString(textData);
  else
    // Convert from encoding to UTF-8. Some formats use windows encoding
    return codec->toUnicode(textData);
}

void OnlinedataController::downloadFinished(const QByteArray& data, QString url)
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << "url" << url << "data size" << data.size() << "state" << stateAsStr(currentState);

  const QDateTime now = QDateTime::currentDateTime();
  if(currentState == DOWNLOADING_STATUS)
  {
    // status.txt downloaded ============================================
    if(verbose)
      qDebug() << Q_FUNC_INFO << "DOWNLOADING_STATUS";

    // Parse status file
    manager->readFromStatus(uncompress(data, Q_FUNC_INFO, false /* utf8 */));

    // Get URL from status file
    bool whazzupGzipped = false, whazzupJson = false;
    whazzupUrlFromStatus = manager->getWhazzupUrlFromStatus(whazzupGzipped, whazzupJson);

    if(!manager->getMessageFromStatus().isEmpty())
      // Call later in the event loop
      QTimer::singleShot(0, this, &OnlinedataController::showMessageDialog);

    if(whazzupJson)
    {
      // Next in chain is transceivers JSON
      currentState = DOWNLOADING_TRANSCEIVERS;
      downloader->setUrl(OptionData::instance().getOnlineTransceiverUrl());
      startDownloader();
    }
    else if(!whazzupUrlFromStatus.isEmpty())
    {
      // Next in chain is whazzup.txt
      currentState = DOWNLOADING_WHAZZUP;
      downloader->setUrl(whazzupUrlFromStatus);
      startDownloader();
    }
    else
    {
      // Done after downloading status.txt - start timer for next session
      // Should never happen
      startDownloadTimer();
      currentState = NONE;
      lastUpdateTime = now;
    }
  }
  else if(currentState == DOWNLOADING_TRANSCEIVERS)
  {
    if(verbose)
      qDebug() << Q_FUNC_INFO << "DOWNLOADING_TRANSCEIVERS";

    // transceivers.json downloaded ============================================
    manager->readFromTransceivers(uncompress(data, Q_FUNC_INFO, true /* utf8 */));

    // Next in chain after transceivers is JSON
    currentState = DOWNLOADING_WHAZZUP;
    lastUpdateTimeTransceivers = now;
    downloader->setUrl(whazzupUrlFromStatus);
    startDownloader();
  }
  else if(currentState == DOWNLOADING_WHAZZUP)
  {
    if(verbose)
      qDebug() << Q_FUNC_INFO << "DOWNLOADING_WHAZZUP";

    // whazzup.txt or JSON downloaded ============================================
    atools::fs::online::Format format = convertFormat(OptionData::instance().getOnlineFormat());

    // Contains servers and does not need an extra download
    bool vatsimJson = format == atools::fs::online::VATSIM_JSON3;
    bool ivaoJson = format == atools::fs::online::IVAO_JSON2;

    if(manager->readFromWhazzup(uncompress(data, Q_FUNC_INFO, ivaoJson || vatsimJson /* utf8 */),
                                format, manager->getLastUpdateTimeFromWhazzup()))
    {
      QString whazzupVoiceUrlFromStatus = manager->getWhazzupVoiceUrlFromStatus();
      if(!vatsimJson && !ivaoJson && !whazzupVoiceUrlFromStatus.isEmpty() &&
         lastServerDownload < now.addSecs(-MIN_SERVER_DOWNLOAD_INTERVAL_MIN * 60))
      {
        // Next in chain is server file
        currentState = DOWNLOADING_WHAZZUP_SERVERS;
        downloader->setUrl(whazzupVoiceUrlFromStatus);
        startDownloader();
      }
      else
      {
        // Done after downloading whazzup.txt - start timer for next session
        startDownloadTimer();
        currentState = NONE;
        lastUpdateTime = now;

        // Clear map display cache and update spatial index to match simulator shadow aircraft
        aircraftCache.clear();
        updateShadowIndex();

        // Message for search tabs, map widget and info
        emit onlineServersUpdated(true /* load all */, true /* keep selection */, true /* force */);
        emit onlineClientAndAtcUpdated(true /* load all */, true /* keep selection */, true /* force */);
        statusBarMessage();
      }
    }
    else
    {
      if(verbose)
        qInfo() << Q_FUNC_INFO << "whazzup.txt is not recent";

      // Done after old update - try again later
      startDownloadTimer();
      currentState = NONE;
      lastUpdateTime = now;
    }
  }
  else if(currentState == DOWNLOADING_WHAZZUP_SERVERS)
  {
    if(verbose)
      qDebug() << Q_FUNC_INFO << "DOWNLOADING_WHAZZUP_SERVERS";

    manager->readServersFromWhazzup(uncompress(data, Q_FUNC_INFO, false /* utf8 */),
                                    convertFormat(OptionData::instance().getOnlineFormat()),
                                    manager->getLastUpdateTimeFromWhazzup());
    lastServerDownload = now;

    // Done after downloading server.txt - start timer for next session
    startDownloadTimer();
    currentState = NONE;
    lastUpdateTime = now;

    // Message for search tabs, map widget and info
    emit onlineClientAndAtcUpdated(true /* load all */, true /* keep selection */, true /* force */);
    emit onlineServersUpdated(true /* load all */, true /* keep selection */, true /* force */);
    statusBarMessage();
  }
}

void OnlinedataController::startDownloader()
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << downloader->getUrl();

  // Call later in the event loop to avoid recursion
  QTimer::singleShot(0, downloader, &HttpDownloader::startDownload);
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
  qWarning() << Q_FUNC_INFO;
  NavApp::closeSplashScreen();

  int result = atools::gui::Dialog(mainWindow).
               showQuestionMsgBox(lnm::ACTIONS_SHOW_SSL_WARNING_ONLINE,
                                  tr("<p>Errors while trying to establish an encrypted connection "
                                       "to download online network data:</p>"
                                       "<p>URL: %1</p>"
                                         "<p>Error messages:<br/>%2</p>"
                                           "<p>Continue?</p>").
                                  arg(downloadUrl).
                                  arg(atools::strJoin(errors, tr("<br/>"))),
                                  tr("Do not &show this again and ignore the errors"),
                                  QMessageBox::Cancel | QMessageBox::Yes,
                                  QMessageBox::Cancel, QMessageBox::Yes);
  downloader->setIgnoreSslErrors(result == QMessageBox::Yes);
}

void OnlinedataController::statusBarMessage()
{
  QString net = getNetworkTranslated();
  if(!net.isEmpty())
    mainWindow->setOnlineConnectionStatusMessageText(QString(), tr("Connected to %1.").arg(net));
  else
    mainWindow->setOnlineConnectionStatusMessageText(QString(), QString());
}

void OnlinedataController::stopAllProcesses()
{
  downloader->cancelDownload();
  downloadTimer.stop();
  currentState = NONE;
  // clientCallsignAndPosMap.clear(); // Do not clear these until the download is finished
}

void OnlinedataController::showMessageDialog()
{
  QMessageBox::information(mainWindow, QApplication::applicationName(),
                           tr("Message from downloaded status file:\n\n%2\n").arg(manager->getMessageFromStatus()));
}

const LineString *OnlinedataController::airspaceGeometryCallback(const QString& callsign, atools::fs::online::fac::FacilityType type)
{
  opts2::Flags2 flags2 = OptionData::instance().getFlags2();

  const LineString *lineString = nullptr;

  // Try to get airspace boundary by name vs. callsign if set in options
  if(flags2 & opts2::ONLINE_AIRSPACE_BY_NAME)
    lineString = NavApp::getAirspaceController()->getOnlineAirspaceGeoByName(callsign, atools::fs::online::facilityTypeToDb(type));

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

  // Remove all from the database
  manager->clearData();
  aircraftCache.clear();
  onlineAircraftSpatialIndex.clear();
  aircraftIdSimToOnline.clear();
  aircraftIdOnlineToSim.clear();
  currentDataPacketMap.clear();

  updateAtcSizes();

  emit onlineClientAndAtcUpdated(true /* load all */, true /* keep selection */, true /* force */);
  emit onlineServersUpdated(true /* load all */, true /* keep selection */, true /* force */);
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

/* Get aircraft within bounding rectangle. Objects are cached. */
const QList<atools::fs::sc::SimConnectAircraft> *OnlinedataController::getAircraft(const Marble::GeoDataLatLonBox& rect,
                                                                                   const MapLayer *mapLayer, bool lazy,
                                                                                   bool& overflow)
{
  static const double queryRectInflationFactor = 0.2;
  static const double queryRectInflationIncrement = 0.1;
  static const int queryMaxRows = 5000;

  aircraftCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                            [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
  {
    return curLayer->hasSameQueryParametersAircraft(newLayer);
  });

  if((aircraftCache.list.isEmpty() && !lazy))
  {
    for(const Marble::GeoDataLatLonBox& r : query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      query::bindRect(r, aircraftByRectQuery);
      aircraftByRectQuery->exec();
      while(aircraftByRectQuery->next())
      {
        SimConnectAircraft onlineAircraft;
        OnlinedataManager::fillFromClient(onlineAircraft, aircraftByRectQuery->record(),
                                          getShadowSimAircraft(aircraftByRectQuery->valueInt("client_id")));

        if(!aircraftIdOnlineToSim.contains(onlineAircraft.getId()))
          // Avoid duplicates with simulator shadow aircraft - sim aircraft are drawn in another context
          aircraftCache.list.append(onlineAircraft);
      }
    }
  }
  overflow = aircraftCache.validate(queryMaxRows);
  return &aircraftCache.list;
}

const atools::fs::sc::SimConnectAircraft& OnlinedataController::getShadowSimAircraft(int onlineId)
{
  const static atools::fs::sc::SimConnectAircraft EMPTY_SIM_AIRCRAFT;

  int simId = aircraftIdOnlineToSim.value(onlineId, -1);
  if(simId != -1 && !currentDataPacketMap.isEmpty())
  {
    // Get latest data packet
    const atools::fs::sc::SimConnectData& last = currentDataPacketMap.last();
    const atools::fs::sc::SimConnectUserAircraft& userAircraft = last.getUserAircraftConst();
    if(userAircraft.isValid() && userAircraft.getId() == simId)
      // Is user aircraft
      return userAircraft;
    else
    {
      // Look for AI aircraft
      const atools::fs::sc::SimConnectAircraft *simAircraft = last.getAiAircraftConstById(simId);
      if(simAircraft != nullptr)
        return *simAircraft;
    }
  }

  return EMPTY_SIM_AIRCRAFT;
}

/* Get an online network aircraft for given simulator shadow aircraft with updated position */
atools::fs::sc::SimConnectAircraft OnlinedataController::getShadowedOnlineAircraft(const atools::fs::sc::SimConnectAircraft& simAircraft)
{
  atools::fs::sc::SimConnectAircraft onlineAircraft;
  if(isShadowAircraft(simAircraft))
  {
    atools::sql::SqlRecord client = manager->getClientRecordById(aircraftIdSimToOnline.value(simAircraft.getId()));

    if(!client.isEmpty())
    {
      OnlinedataManager::fillFromClient(onlineAircraft, client, simAircraft);

      // Update to real simulator position including altitude for shadows
      onlineAircraft.getPosition() = simAircraft.getPosition();
    }
    else
      qWarning() << Q_FUNC_INFO << "No client found for" << simAircraft.getAirplaneRegistration();
  }
  return onlineAircraft;
}

bool OnlinedataController::isShadowAircraft(const atools::fs::sc::SimConnectAircraft& simAircraft)
{
  return aircraftIdSimToOnline.contains(simAircraft.getId());
}

// Called by ConnectClient after each simulator data package
void OnlinedataController::updateAircraftShadowState(atools::fs::sc::SimConnectData& dataPacket)
{
  // Check if connected online to avoid overflow of currentDataPacketMap which
  // is cleared in updateShadowIndex() after each online download
  if(isNetworkActive())
  {
    // Modify AI aircraft and set shadow flag if a online network aircraft is registered as shadowed in the index
    if(!dataPacket.isEmptyReply() && dataPacket.isUserAircraftValid())
    {
      atools::fs::sc::SimConnectUserAircraft& userAircraft = dataPacket.getUserAircraft();
      userAircraft.setFlag(atools::fs::sc::SIM_ONLINE_SHADOW, isShadowAircraft(userAircraft));

      for(SimConnectAircraft& aiAircraft : dataPacket.getAiAircraft())
        aiAircraft.setFlag(atools::fs::sc::SIM_ONLINE_SHADOW, isShadowAircraft(aiAircraft));

      currentDataPacketMap.insert(QDateTime::currentDateTimeUtc(), dataPacket);
    }
  }
  else
    currentDataPacketMap.clear();
}

/* Return online aircraft for simulator aircraft based on distance and other parameter similarity */
OnlineAircraft OnlinedataController::shadowAircraftInternal(const atools::fs::sc::SimConnectAircraft& simAircraft)
{
  const static OnlineAircraft EMPTY_ONLINE_AIRCRAFT;

#ifdef DEBUG_INFORMATION_USER_ONLINE_DISABLED
  if(simAircraft.isUser())
  {
    qDebug() << Q_FUNC_INFO << simAircraft.getAirplaneRegistrationKey() << simAircraft.getPosition();

    if(onlineAircraftRegKeyIndex.contains(simAircraft.getAirplaneRegistrationKey()))
    {
      const atools::geo::Pos pos = onlineAircraftRegKeyIndex.value(simAircraft.getAirplaneRegistrationKey()).getPosition();
      qDebug() << Q_FUNC_INFO << "online" << pos << "sim" << simAircraft.getPosition();

      qDebug() << Q_FUNC_INFO << atools::geo::meterToNm(pos.distanceMeterTo3d(simAircraft.getPosition()));
    }
  }
#endif

  if(!onlineAircraftSpatialIndex.isEmpty())
  {
    // First get all nearest aircraft from spatial index ======================================
    QVector<OnlineAircraft> nearest;
    onlineAircraftSpatialIndex.getRadius(nearest, simAircraft.getPosition(), atools::geo::nmToMeter(maxShadowDistanceNm));

    if(verbose && simAircraft.isUser())
      qDebug() << Q_FUNC_INFO << "nearest.size()" << nearest.size();

    // Filter out all which do not match more non-spatial criteria =================================
    nearest.erase(std::remove_if(nearest.begin(), nearest.end(),
                                 [&simAircraft, this](const OnlineAircraft& aircraft) -> bool {
      bool altOk = true, gsOk = true, hdgOk = true;

      if(atools::inRange(-1000.f, map::INVALID_ALTITUDE_VALUE / 4.f, simAircraft.getActualAltitudeFt()) &&
         atools::inRange(-1000.f, map::INVALID_ALTITUDE_VALUE / 4.f, aircraft.pos.getAltitude()))
        altOk = atools::almostEqual(simAircraft.getActualAltitudeFt(), aircraft.pos.getAltitude(), maxShadowAltDiffFt);

      if(atools::inRange(0.f, map::INVALID_SPEED_VALUE / 4.f, simAircraft.getGroundSpeedKts()) &&
         atools::inRange(0.f, map::INVALID_SPEED_VALUE / 4.f, aircraft.groundSpeedKts))
        gsOk = atools::almostEqual(simAircraft.getGroundSpeedKts(), aircraft.groundSpeedKts, maxShadowGsDiffKts);

      if(atools::inRange(0.f, map::INVALID_HEADING_VALUE / 4.f, simAircraft.getHeadingDegTrue()) &&
         atools::inRange(0.f, map::INVALID_HEADING_VALUE / 4.f, aircraft.headingTrue))
        hdgOk = atools::geo::angleAbsDiff(simAircraft.getHeadingDegTrue(), aircraft.headingTrue) < maxShadowHdgDiffDeg;

      return !(altOk && gsOk && hdgOk);
    }), nearest.end());

    if(verbose && simAircraft.isUser())
      qDebug() << Q_FUNC_INFO << "nearest.size() after filter" << nearest.size();

    if(!nearest.isEmpty())
    {
      // Sort to get closest by coordinates and altitude to start of list
      maptools::sortByDistanceAndAltitude(nearest, simAircraft.getPosition());

      if(verbose && simAircraft.isUser())
        qDebug() << Q_FUNC_INFO << "Found" << nearest.first().registrationKey;

      return nearest.constFirst();
    }
  } // if(!onlineAircraftSpatialIndex.isEmpty())
  return EMPTY_ONLINE_AIRCRAFT;
}

void OnlinedataController::clearShadowIndexes()
{
  onlineAircraftSpatialIndex.clear();
  aircraftIdSimToOnline.clear();
  aircraftIdOnlineToSim.clear();
}

// Called after each download
void OnlinedataController::updateShadowIndex()
{
  if(verbose)
    qDebug() << Q_FUNC_INFO << "===========================";

  // Clear the id maps and the spatial index
  clearShadowIndexes();

  if(OptionData::instance().getFlags().testFlag(opts::ONLINE_REMOVE_SHADOW) && !currentDataPacketMap.isEmpty())
  {
    const QDateTime lastUpdateTimeWhazzup = manager->getLastUpdateTimeFromWhazzup();
    const auto upper = currentDataPacketMap.upperBound(lastUpdateTimeWhazzup);
    const auto lower = currentDataPacketMap.lowerBound(lastUpdateTimeWhazzup);
    QMap<QDateTime, atools::fs::sc::SimConnectData>::iterator entry = currentDataPacketMap.end();

    if(upper != currentDataPacketMap.end() && lower != currentDataPacketMap.end())
    {
      qint64 lastUpdateWhazzupSecs = lastUpdateTimeWhazzup.toSecsSinceEpoch();
      if(std::abs(upper.key().toSecsSinceEpoch() - lastUpdateWhazzupSecs) <
         std::abs(lower.key().toSecsSinceEpoch() - lastUpdateWhazzupSecs))
        entry = upper;
      else
        entry = lower;
    }
    else if(upper != currentDataPacketMap.end())
      entry = upper;
    else if(lower != currentDataPacketMap.end())
      entry = lower;

    if(verbose)
    {
      qDebug() << Q_FUNC_INFO << currentDataPacketMap.firstKey() << currentDataPacketMap.lastKey();
      qDebug() << Q_FUNC_INFO << "lastUpdateTimeWhazzup" << lastUpdateTimeWhazzup
               << "lower" << lower.key() << "upper" << upper.key();

      if(entry != currentDataPacketMap.end())
        qDebug() << Q_FUNC_INFO << "entry.key()" << entry.key();
    }

    if(entry != currentDataPacketMap.end())
    {
      int removed = 0;
      while(currentDataPacketMap.begin() != entry && !currentDataPacketMap.isEmpty())
      {
        currentDataPacketMap.erase(currentDataPacketMap.begin());
        removed++;
      }

      if(verbose)
        qDebug() << Q_FUNC_INFO << "removed" << removed << "currentDataPackets.size()" << currentDataPacketMap.size();

      atools::fs::sc::SimConnectData currentDataPacket = entry.value();
      if(currentDataPacket.isUserAircraftValid())
      {
        // Fill and update spatial index =================================
        onlineAircraftSpatialIndex.append(manager->getClientCallsignAndPosMap());
        onlineAircraftSpatialIndex.updateIndex();

        const atools::fs::sc::SimConnectUserAircraft& simUserAircraft = currentDataPacket.getUserAircraftConst();
        if(!simUserAircraft.isAnyBoat())
        {
          const OnlineAircraft onlineUserAircraft = shadowAircraftInternal(simUserAircraft);
          if(onlineUserAircraft.isValid())
          {
            int simId = simUserAircraft.getId();
            int onlineId = onlineUserAircraft.id;
            aircraftIdSimToOnline.insert(simId, onlineId);
            aircraftIdOnlineToSim.insert(onlineId, simId);

            if(verbose)
              qDebug() << Q_FUNC_INFO << "User sim" << simId << simUserAircraft.getAirplaneRegistration() << simUserAircraft.getPosition()
                       << "online" << onlineId << onlineUserAircraft.registration << onlineUserAircraft.pos;
          }
        }

        // update an index for currentDataPacket
        for(const SimConnectAircraft& simAircraft : currentDataPacket.getAiAircraftConst())
        {
          if(!simAircraft.isAnyBoat())
          {
            OnlineAircraft onlineAircraft = shadowAircraftInternal(simAircraft);

            if(onlineAircraft.isValid())
            {
              int simId = simAircraft.getId();
              int onlineId = onlineAircraft.id;
              aircraftIdSimToOnline.insert(simId, onlineId);
              aircraftIdOnlineToSim.insert(onlineId, simId);

              if(verbose)
                qDebug() << Q_FUNC_INFO << "Sim" << simId << simAircraft.getAirplaneRegistration() << simAircraft.getPosition()
                         << "online" << onlineId << onlineAircraft.registration << onlineAircraft.pos;
            }
          }
        }
      }
    }
  }
}

atools::fs::sc::SimConnectAircraft OnlinedataController::getClientAircraftById(int id)
{
  return manager->getClientAircraftById(id);
}

void OnlinedataController::fillAircraftFromClient(atools::fs::sc::SimConnectAircraft& ac, const atools::sql::SqlRecord& record)
{
  OnlinedataManager::fillFromClient(ac, record, getShadowSimAircraft(record.valueInt("client_id")));
}

/* Removes the online aircraft from "onlineAircraft" which also have a simulator shadow in "simAircraft" */
void OnlinedataController::removeOnlineShadowedAircraft(QList<map::MapOnlineAircraft>& onlineAircraftList,
                                                        const QList<map::MapAiAircraft>& simAircraftList)
{
  // Collect simulator shadow aircraft IDs from index
  QSet<int> onlineAircraftIds;
  for(const map::MapAiAircraft& simAircraft : simAircraftList)
  {
    if(aircraftIdSimToOnline.contains(simAircraft.getId()))
      onlineAircraftIds.insert(aircraftIdSimToOnline.value(simAircraft.getId()));
  }

  // Remove the shadowed aircraft from the online list which have a copy in simulator
  onlineAircraftList.erase(std::remove_if(onlineAircraftList.begin(), onlineAircraftList.end(),
                                          [onlineAircraftIds](const map::MapOnlineAircraft& onlineAircraft) -> bool
  {
    return onlineAircraftIds.contains(onlineAircraft.getAircraft().getId());
  }), onlineAircraftList.end());
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
  aircraftByRectQuery->prepare("select * from client where lonx between :leftx and :rightx and laty between :bottomy and :topy");
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
  int intervalSeconds = OptionData::instance().getOnlineReload(onlineNetwork);

  if(onlineNetwork == opts::ONLINE_CUSTOM || onlineNetwork == opts::ONLINE_CUSTOM_STATUS)
    // Use options for custom network - ignore reload in whazzup.txt
    source = "options";
  else
  {
    if(intervalSeconds == -1)
    {
      // Use time from whazzup.txt - mode auto
      intervalSeconds = std::max(manager->getReloadMinutesFromWhazzup() * 60, 60);
      source = "whazzup";
    }
    else
    {
      intervalSeconds = std::max(intervalSeconds, MIN_RELOAD_TIME_SECONDS);
      source = "networks.cfg";
    }
  }

  if(verbose)
    qDebug() << Q_FUNC_INFO << "timer set to" << intervalSeconds << "from" << source;

#ifdef DEBUG_ONLINE_DOWNLOAD
  downloadTimer.setInterval(2000);
#else
  downloadTimer.setInterval(intervalSeconds * 1000);
#endif
  downloadTimer.start();
}

QString OnlinedataController::stateAsStr(OnlinedataController::State state)
{
  switch(state)
  {
    case OnlinedataController::NONE:
      return "None";

    case OnlinedataController::DOWNLOADING_STATUS:
      return "Downloading Status";

    case OnlinedataController::DOWNLOADING_WHAZZUP:
      return "Downloading Whazzup";

    case OnlinedataController::DOWNLOADING_TRANSCEIVERS:
      return "Downloading Transceivers";

    case OnlinedataController::DOWNLOADING_WHAZZUP_SERVERS:
      return "Downloading Servers";
  }
  return QString();
}

void OnlinedataController::debugDumpContainerSizes() const
{
  if(downloader != nullptr)
    downloader->debugDumpContainerSizes();
  qDebug() << Q_FUNC_INFO << "currentDataPacketMap.size()" << currentDataPacketMap.size();
  qDebug() << Q_FUNC_INFO << "onlineAircraftSpatialIndex.size()" << onlineAircraftSpatialIndex.size();
  qDebug() << Q_FUNC_INFO << "aircraftIdSimToOnline.size()" << aircraftIdSimToOnline.size();
  qDebug() << Q_FUNC_INFO << "aircraftIdOnlineToSim.size()" << aircraftIdOnlineToSim.size();
  qDebug() << Q_FUNC_INFO << "aircraftCache.list.size()" << aircraftCache.list.size();

}
