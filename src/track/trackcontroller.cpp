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

#include "track/trackcontroller.h"

#include "atools.h"
#include "common/constants.h"
#include "gui/dialog.h"
#include "gui/mainwindow.h"
#include "gui/widgetstate.h"
#include "app/navapp.h"
#include "settings/settings.h"
#include "settings/settings.h"
#include "track/trackdownloader.h"
#include "track/trackmanager.h"
#include "ui_mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>

using atools::track::TrackDownloader;
using atools::settings::Settings;

TrackController::TrackController(TrackManager *trackManagerParam, MainWindow *mainWindowParam) :
  QObject(mainWindowParam), mainWindow(mainWindowParam), trackManager(trackManagerParam)
{
  verbose = atools::settings::Settings::instance().getAndStoreValue(lnm::OPTIONS_TRACK_DEBUG, false).toBool();
  trackManager->setVerbose(verbose);

  downloader = new TrackDownloader(this, verbose);
#ifdef DEBUG_TRACK_TEST
  downloader->setUrl(atools::track::NAT, "/tmp/NAT.txt");
  downloader->setUrl(atools::track::PACOTS, "/tmp/PACOTS.txt");
  downloader->setUrl(atools::track::AUSOTS, "/tmp/AUSOTS.txt");
#else
  namespace t = atools::track;
  atools::settings::Settings& settings = Settings::instance();
  downloader->setUrl(t::NAT,
                     settings.getAndStoreValue(lnm::OPTIONS_TRACK_NAT_URL, TrackDownloader::URL.value(t::NAT)).toString(),
                     settings.getAndStoreValue(lnm::OPTIONS_TRACK_NAT_PARAM, TrackDownloader::PARAM.value(t::NAT)).toStringList());

  downloader->setUrl(t::PACOTS,
                     settings.getAndStoreValue(lnm::OPTIONS_TRACK_PACOTS_URL, TrackDownloader::URL.value(t::PACOTS)).toString(),
                     settings.getAndStoreValue(lnm::OPTIONS_TRACK_PACOTS_PARAM, TrackDownloader::PARAM.value(t::PACOTS)).toStringList());

  downloader->setUrl(t::AUSOTS,
                     settings.getAndStoreValue(lnm::OPTIONS_TRACK_AUSOTS_URL, TrackDownloader::URL.value(t::AUSOTS)).toString(),
                     settings.getAndStoreValue(lnm::OPTIONS_TRACK_AUSOTS_PARAM, TrackDownloader::PARAM.value(t::AUSOTS)).toStringList());
#endif

  connect(downloader, &TrackDownloader::trackDownloadFinished, this, &TrackController::trackDownloadFinished);
  connect(downloader, &TrackDownloader::trackDownloadFailed, this, &TrackController::trackDownloadFailed);
  connect(downloader, &TrackDownloader::trackDownloadSslErrors, this, &TrackController::trackDownloadSslErrors);

  Ui::MainWindow *ui = NavApp::getMainUi();
  connect(ui->actionTrackSourcesNat, &QAction::toggled, this, &TrackController::trackSelectionChanged);
  connect(ui->actionTrackSourcesPacots, &QAction::toggled, this, &TrackController::trackSelectionChanged);
  connect(ui->actionTrackSourcesAusots, &QAction::toggled, this, &TrackController::trackSelectionChanged);
}

TrackController::~TrackController()
{
}

void TrackController::restoreState()
{
  atools::gui::WidgetState state(lnm::AIRSPACE_CONTROLLER_WIDGETS, false /* visibility */, true /* block signals */);

  Ui::MainWindow *ui = NavApp::getMainUi();
  state.restore({ui->actionTrackSourcesNat, ui->actionTrackSourcesPacots, ui->actionTrackSourcesAusots, ui->actionRouteDownloadTracks});
}

void TrackController::saveState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  atools::gui::WidgetState state(lnm::AIRSPACE_CONTROLLER_WIDGETS);

  state.save({ui->actionTrackSourcesNat, ui->actionTrackSourcesPacots, ui->actionTrackSourcesAusots, ui->actionRouteDownloadTracks});
}

void TrackController::optionsChanged()
{

}

void TrackController::preDatabaseLoad()
{
  downloader->cancelAllDownloads();
}

void TrackController::postDatabaseLoad()
{
  if(!trackVector.isEmpty())
  {
    emit preTrackLoad();
    trackManager->loadTracks(trackVector, downloadOnlyValid);
    emit postTrackLoad();
  }
}

void TrackController::startDownload()
{
  NavApp::getMainUi()->actionMapShowTracks->setChecked(true);
  startDownloadInternal();
}

void TrackController::startDownloadStartup()
{
  startDownloadInternal();
}

void TrackController::startDownloadInternal()
{
  qDebug() << Q_FUNC_INFO;

  deleteTracks();

  // Append all to queue and start
  QVector<atools::track::TrackType> trackTypes = enabledTracks();
  downloadQueue.append(trackTypes);

  for(atools::track::TrackType trackType: trackTypes)
    downloader->startDownload(trackType);

  NavApp::setStatusMessage(tr("Track download started."));
}

void TrackController::deleteTracks()
{
  cancelDownload();
  downloadQueue.clear();
  trackVector.clear();

  emit preTrackLoad();
  trackManager->clearTracks();
  emit postTrackLoad();

  NavApp::setStatusMessage(tr("Tracks deleted."));
}

void TrackController::downloadToggled(bool checked)
{
  if(checked && !hasTracks())
  {
    startDownload();
    NavApp::getMainUi()->actionMapShowTracks->setChecked(true);
  }
}

void TrackController::cancelDownload()
{
  qDebug() << Q_FUNC_INFO;
  downloader->cancelAllDownloads();
  downloadQueue.clear();
  trackVector.clear();
}

bool TrackController::hasTracks() const
{
  return trackManager->hasData();
}

void TrackController::trackDownloadFinished(const atools::track::TrackVectorType& tracks, atools::track::TrackType type)
{
  qDebug() << Q_FUNC_INFO << static_cast<int>(type) << "size" << tracks.size();

  // Remove finished type from queue and append to vector
  downloadQueue.removeAll(type);
  trackVector.append(tracks);

  if(downloadQueue.isEmpty())
  {
    // Finished downloading all types - load into database ================
    qDebug() << Q_FUNC_INFO << "Download queue empty";

    // notify before changing database
    emit preTrackLoad();

    // Load tracks but keep raw data in vector
    trackManager->loadTracks(trackVector, downloadOnlyValid);
    tracksLoaded();
    emit postTrackLoad();
  }
}

void TrackController::trackDownloadSslErrors(const QStringList& errors, const QString& downloadUrl)
{
  qWarning() << Q_FUNC_INFO;
  NavApp::closeSplashScreen();

  int result = atools::gui::Dialog(mainWindow).
               showQuestionMsgBox(lnm::ACTIONS_SHOW_SSL_WARNING_TRACK,
                                  tr("<p>Errors while trying to establish an encrypted "
                                       "connection to download track information:</p>"
                                       "<p>URL: %1</p>"
                                         "<p>Error messages:<br/>%2</p>"
                                           "<p>Continue?</p>").
                                  arg(downloadUrl).
                                  arg(atools::strJoin(errors, tr("<br/>"))),
                                  tr("Do not &show this again and ignore errors."),
                                  QMessageBox::Cancel | QMessageBox::Yes,
                                  QMessageBox::Cancel, QMessageBox::Yes);

  downloader->setIgnoreSslErrors(result == QMessageBox::Yes);
}

void TrackController::trackDownloadFailed(const QString& error, int errorCode, QString downloadUrl,
                                          atools::track::TrackType type)
{
  qDebug() << Q_FUNC_INFO << error << errorCode << downloadUrl << static_cast<int>(type);

  if(!errorReported)
  {
    // Show an error dialog for any failed downloads but only once per session
    errorReported = true;
    QString typeStr = atools::track::typeToString(type);

    atools::gui::Dialog(mainWindow).showWarnMsgBox(lnm::ACTIONS_SHOW_TRACK_DOWNLOAD_FAIL,
                                                   tr("<p>Download of %1 track information from<br/>\"%2\"<br/>"
                                                      "failed.</p><p>Error: %3 (%4)</p>"
                                                      "<p>Check track settings or disable track downloads.</p>"
                                                        "<p>Suppressing further messages during this session.</p>").
                                                   arg(typeStr).
                                                   arg(downloadUrl).arg(error).arg(errorCode),
                                                   tr("Do not &show this dialog again."));
  }

  // Clear all on error
  downloadQueue.removeAll(type);
  trackVector.clear();

  if(downloadQueue.isEmpty())
  {
    qDebug() << Q_FUNC_INFO << "Download queue empty";
    emit postTrackLoad();

    NavApp::setStatusMessage(tr("Track download failed."), true /* addToLog */);
  }
}

void TrackController::tracksLoaded()
{
  QMap<atools::track::TrackType, int> numTracks = trackManager->getNumTracks();

  const QStringList& errorMessages = trackManager->getErrorMessages();
  if(!errorMessages.isEmpty())
  {
    QStringList str;
    QString err;
    for(auto it = numTracks.constBegin(); it != numTracks.constEnd(); ++it)
    {
      atools::track::TrackType type = it.key();
      int num = numTracks.value(type);
      str.append(tr("<li>%1: %2 tracks.</li>").
                 arg(atools::track::typeToString(type)).
                 arg(num == 0 ? tr("No") : QString::number(num)));

    }

    err += tr("<p>Errors reading tracks.<br/>Make sure to use the latest navdata.</p><ul><li>");
    err += errorMessages.mid(0, 5).join("</li><li>");

    if(errorMessages.size() > 5)
      err += tr("</li><li>More ...");

    err += tr("</li></ul>");
    QString boxMessage = tr("<p>Tracks downloaded.</p><ul>%1</ul>%2").arg(str.join("")).arg(err);

    NavApp::closeSplashScreen();
    QMessageBox::warning(mainWindow, QApplication::applicationName(), boxMessage);
    NavApp::setStatusMessage(tr("Track download finished with errors."), true /* addToLog */);
  }
  else
  {
    QStringList msg;
    for(auto it = numTracks.constBegin(); it != numTracks.constEnd(); ++it)
    {
      atools::track::TrackType type = it.key();
      int num = numTracks.value(type);
      msg.append(tr("%1: %2 tracks").arg(atools::track::typeToString(type)).arg(num == 0 ? tr("no") : QString::number(num)));
    }

    NavApp::setStatusMessage(tr("Tracks downloaded: %1.").arg(msg.join(tr(", "))), true /* addToLog */);
  }
}

QVector<atools::track::TrackType> TrackController::enabledTracks() const
{
  QVector<atools::track::TrackType> retval;
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(ui->actionTrackSourcesNat->isChecked())
    retval.append(atools::track::NAT);
  if(ui->actionTrackSourcesPacots->isChecked())
    retval.append(atools::track::PACOTS);
  if(ui->actionTrackSourcesAusots->isChecked())
    retval.append(atools::track::AUSOTS);
  return retval;
}

void TrackController::trackSelectionChanged(bool)
{
  if(hasTracksEnabled())
  {
    cancelDownload();
    startDownload();
  }
  else
    deleteTracks();
}
