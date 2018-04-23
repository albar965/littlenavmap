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

#include "online/onlinedatacontroller.h"

#include "fs/online/onlinedatamanager.h"
#include "util/httpdownloader.h"
#include "gui/mainwindow.h"
#include "common/constants.h"
#include "settings/settings.h"
#include "options/optiondata.h"
#include "zip/gzip.h"
#include "sql/sqlquery.h"
#include "mapgui/maplayer.h"
#include "fs/sc/simconnectaircraft.h"

#include <QDebug>
#include <QMessageBox>
#include <QTextCodec>
#include <QApplication>

// #define DEBUG_ONLINE_DOWNLOAD 1

static const int MIN_SERVER_DOWNLOAD_INTERVAL_MIN = 15;

using atools::fs::online::OnlinedataManager;
using atools::util::HttpDownloader;

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

  connect(downloader, &HttpDownloader::downloadFinished, this, &OnlinedataController::downloadFinished);
  connect(downloader, &HttpDownloader::downloadFailed, this, &OnlinedataController::downloadFailed);

  // Recurring downloads
  connect(&downloadTimer, &QTimer::timeout, this, &OnlinedataController::startDownloadInternal);

#ifdef DEBUG_ONLINE_DOWNLOAD
  downloader->enableCache(60);
#endif
}

OnlinedataController::~OnlinedataController()
{
  deInitQueries();

  delete downloader;

  // Remove all from the database to avoid confusion on startup
  manager->clearData();
}

void OnlinedataController::startProcessing()
{
  startDownloadInternal();
}

void OnlinedataController::startDownloadInternal()
{
  qDebug() << Q_FUNC_INFO;
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
        // Message for search tabs, map widget and info
        emit onlineClientAndAtcUpdated(true /* load all */, true /* keep selection */);
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
    // Message for search tabs, map widget and info
    emit onlineClientAndAtcUpdated(true /* load all */, true /* keep selection */);
    emit onlineServersUpdated(true /* load all */, true /* keep selection */);
  }
}

void OnlinedataController::downloadFailed(const QString& error, QString url)
{
  qWarning() << Q_FUNC_INFO << "Failed" << error << url;
  stopAllProcesses();
  QMessageBox::warning(mainWindow, QApplication::applicationName(),
                       tr("Download from\n\n\"%1\"\n\nfailed. Reason:\n\n%2\n\nPress OK to retry.").
                       arg(url).arg(error));
  startProcessing();
}

void OnlinedataController::stopAllProcesses()
{
  downloader->cancelDownload();
  downloadTimer.stop();
  currentState = NONE;
}

void OnlinedataController::showMessageDialog()
{
  QMessageBox::information(mainWindow, QApplication::applicationName(),
                           tr("Message from downloaded status file:\n\n%2\n").arg(manager->getMessageFromStatus()));
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

  if(OptionData::instance().getOnlineNetwork() == opts::ONLINE_NONE)
  {
    emit onlineClientAndAtcUpdated(true /* load all */, true /* keep selection */);
    emit onlineServersUpdated(true /* load all */, true /* keep selection */);
    emit onlineNetworkChanged();
  }
  else
  {
    lastUpdateTime = QDateTime::fromSecsSinceEpoch(0);
    lastServerDownload = QDateTime::fromSecsSinceEpoch(0);

    emit onlineNetworkChanged();

    startDownloadInternal();
  }
}

bool OnlinedataController::hasData() const
{
  return manager->hasData();
}

QString OnlinedataController::getNetwork() const
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

    case opts::ONLINE_CUSTOM_STATUS:
    case opts::ONLINE_CUSTOM:
      return tr("Custom Network");

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

  if(aircraftCache.list.isEmpty() && !lazy)
  {
    for(const Marble::GeoDataLatLonBox& r :
        query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      query::bindCoordinatePointInRect(r, aircraftByRectQuery);
      aircraftByRectQuery->exec();
      while(aircraftByRectQuery->next())
      {
        atools::fs::sc::SimConnectAircraft ac;
        fillAircraftFromClient(ac, aircraftByRectQuery->record());
        aircraftCache.list.append(ac);
      }
    }
  }
  aircraftCache.validate(queryMaxRows);
  return &aircraftCache.list;
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
