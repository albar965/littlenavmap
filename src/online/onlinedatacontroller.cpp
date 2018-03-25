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
#include "options/optiondata.h"
#include "zip/gzip.h"

#include <QDebug>
#include <QTextCodec>

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
  codec = QTextCodec::codecForName("Windows-1252");
  if(codec == nullptr)
    codec = QTextCodec::codecForLocale();

  downloader = new atools::util::HttpDownloader(mainWindow, false /* verbose */);

  connect(downloader, &HttpDownloader::downloadFinished, this, &OnlinedataController::downloadFinished);
  connect(downloader, &HttpDownloader::downloadFailed, this, &OnlinedataController::downloadFailed);

  // Start downloads when event queue is active
  startDownloadAsyncTimer.setSingleShot(true);
  startDownloadAsyncTimer.setInterval(0);
  connect(&startDownloadAsyncTimer, &QTimer::timeout, downloader, &HttpDownloader::startDownload);

  // Recurring downloads
  downloadTimer.setInterval(OptionData::instance().getOnlineReloadTimeSeconds() * 1000);
  connect(&downloadTimer, &QTimer::timeout, this, &OnlinedataController::startDownloadInternal);
}

OnlinedataController::~OnlinedataController()
{
  delete downloader;
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
      startDownloadAsyncTimer.start();
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
    if(!whazzupUrlFromStatus.isEmpty())
    {
      // Next in chain is whazzup.txt
      currentState = DOWNLOADING_WHAZZUP;
      downloader->setUrl(whazzupUrlFromStatus);
      startDownloadAsyncTimer.start();
    }
    else
    {
      // Done here - start timer
      startDownloadTimer();
      currentState = NONE;
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

    manager->readFromWhazzup(codec->toUnicode(whazzupData), convertFormat(OptionData::instance().getOnlineFormat()));

    QString whazzupVoiceUrlFromStatus = manager->getWhazzupVoiceUrlFromStatus();
    if(!whazzupVoiceUrlFromStatus.isEmpty() &&
       lastServerDownload < QDateTime::currentDateTime().addSecs(-MIN_SERVER_DOWNLOAD_INTERVAL_MIN * 60))
    {
      // Next in chain is server file
      currentState = DOWNLOADING_WHAZZUP_SERVERS;
      downloader->setUrl(whazzupVoiceUrlFromStatus);
      startDownloadAsyncTimer.start();
    }
    else
    {
      // Done here - start timer
      startDownloadTimer();
      currentState = NONE;
    }
  }
  else if(currentState == DOWNLOADING_WHAZZUP_SERVERS)
  {
    manager->readServersFromWhazzup(codec->toUnicode(data), convertFormat(OptionData::instance().getOnlineFormat()));
    lastServerDownload = QDateTime::currentDateTime();

    // Done here - start timer for next session
    startDownloadTimer();
    currentState = NONE;
  }
}

void OnlinedataController::downloadFailed(const QString& error, QString url)
{
  qDebug() << Q_FUNC_INFO << "Failed" << error << url;
}

void OnlinedataController::preDatabaseLoad()
{
  qDebug() << Q_FUNC_INFO;
}

void OnlinedataController::postDatabaseLoad()
{
  qDebug() << Q_FUNC_INFO;
}

void OnlinedataController::stopAllProcesses()
{
  downloader->cancelDownload();
  downloadTimer.stop();
  startDownloadAsyncTimer.stop();
  currentState = NONE;
}

void OnlinedataController::optionsChanged()
{
  qDebug() << Q_FUNC_INFO;

  // Clear all URL from status.txt too
  manager->reset();

  stopAllProcesses();

  startDownloadInternal();
}

void OnlinedataController::startDownloadTimer()
{
  downloadTimer.stop();

  int reloadFromOptions = OptionData::instance().getOnlineReloadTimeSeconds();
  int reloadFromWhazzup = manager->getReloadMinutesFromWhazzup() * 60;

  // Use three minutes as default it nothing is given
  int intervalSeconds = 3 * 60;
  if(reloadFromOptions > 15)
  {
    // Use time from dialog
    intervalSeconds = reloadFromOptions;
    qDebug() << Q_FUNC_INFO << "timer set to" << intervalSeconds << "from options";
  }
  else if(reloadFromWhazzup > 15)
  {
    // Use time from whazzup.txt
    intervalSeconds = reloadFromWhazzup;
    qDebug() << Q_FUNC_INFO << "timer set to" << intervalSeconds << "from whazzup";
  }

  downloadTimer.setInterval(intervalSeconds * 1000);
  downloadTimer.start();
}
