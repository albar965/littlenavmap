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

#include "track/trackcontroller.h"

#include "track/trackdownloader.h"
#include "settings/settings.h"
#include "gui/mainwindow.h"
#include "common/constants.h"
#include "track/trackmanager.h"
#include "query/airwaytrackquery.h"
#include "navapp.h"
#include "gui/dialog.h"
#include "query/airwayquery.h"
#include "query/waypointtrackquery.h"
#include "query/waypointquery.h"
#include "ui_mainwindow.h"
#include "gui/widgetstate.h"
#include "settings/settings.h"

#include <QDebug>

using atools::track::TrackDownloader;
using atools::settings::Settings;

TrackController::TrackController(TrackManager *trackManagerParam, MainWindow *mainWindowParam) :
  QObject(mainWindowParam), mainWindow(mainWindowParam), trackManager(trackManagerParam)
{
  verbose = atools::settings::Settings::instance().getAndStoreValue(lnm::OPTIONS_TRACK_DEBUG, false).toBool();
  trackManager->setVerbose(verbose);

  // Set up airway queries =====================
  airwayTrackQuery = new AirwayTrackQuery(new AirwayQuery(NavApp::getDatabaseNav(), false),
                                          new AirwayQuery(NavApp::getDatabaseTrack(), true));
  airwayTrackQuery->initQueries();

  // Set up waypoint queries =====================
  waypointTrackQuery = new WaypointTrackQuery(new WaypointQuery(NavApp::getDatabaseNav(), false),
                                              new WaypointQuery(NavApp::getDatabaseTrack(), true));
  waypointTrackQuery->initQueries();

  downloader = new TrackDownloader(this, verbose);
#ifdef DEBUG_TRACK_TEST
  downloader->setUrl(atools::track::NAT, "/home/alex/Temp/tracks/NAT.html");
  downloader->setUrl(atools::track::PACOTS, "/home/alex/Temp/tracks/PACOTS.html");
  downloader->setUrl(atools::track::AUSOTS, "/home/alex/Temp/tracks/AUSOTS.html");
#else
  namespace t = atools::track;
  atools::settings::Settings& settings = Settings::instance();
  downloader->setUrl(t::NAT,
                     settings.getAndStoreValue(lnm::OPTIONS_TRACK_NAT_URL,
                                               TrackDownloader::URL.value(t::NAT)).toString(),
                     settings.getAndStoreValue(lnm::OPTIONS_TRACK_NAT_PARAM,
                                               TrackDownloader::PARAM.value(t::NAT)).toStringList());

  downloader->setUrl(t::PACOTS,
                     settings.getAndStoreValue(lnm::OPTIONS_TRACK_PACOTS_URL,
                                               TrackDownloader::URL.value(t::PACOTS)).toString(),
                     settings.getAndStoreValue(lnm::OPTIONS_TRACK_PACOTS_PARAM,
                                               TrackDownloader::PARAM.value(t::PACOTS)).toStringList());

  downloader->setUrl(t::AUSOTS,
                     settings.getAndStoreValue(lnm::OPTIONS_TRACK_AUSOTS_URL,
                                               TrackDownloader::URL.value(t::AUSOTS)).toString(),
                     settings.getAndStoreValue(lnm::OPTIONS_TRACK_AUSOTS_PARAM,
                                               TrackDownloader::PARAM.value(t::AUSOTS)).toStringList());
#endif

  connect(downloader, &TrackDownloader::downloadFinished, this, &TrackController::downloadFinished);
  connect(downloader, &TrackDownloader::downloadFailed, this, &TrackController::downloadFailed);

  connect(this, &TrackController::postTrackLoad, [ = ](void) -> void {
    waypointTrackQuery->postTrackLoad();
    airwayTrackQuery->postTrackLoad();
  });

}

TrackController::~TrackController()
{
  // Have to delete manually since classes can be copied and does not delete in destructor
  airwayTrackQuery->deleteChildren();
  delete airwayTrackQuery;

  waypointTrackQuery->deleteChildren();
  delete waypointTrackQuery;
}

void TrackController::restoreState()
{
  atools::gui::WidgetState state(lnm::AIRSPACE_CONTROLLER_WIDGETS, false /* visibility */, true /* block signals */);
  state.restore(NavApp::getMainUi()->actionRouteDownloadTracks);
}

void TrackController::saveState()
{
  atools::gui::WidgetState(lnm::AIRSPACE_CONTROLLER_WIDGETS).save(NavApp::getMainUi()->actionRouteDownloadTracks);
}

void TrackController::optionsChanged()
{

}

void TrackController::preDatabaseLoad()
{
  downloader->cancelAllDownloads();
  airwayTrackQuery->deInitQueries();
  waypointTrackQuery->deInitQueries();
}

void TrackController::postDatabaseLoad()
{
  // Reload track into database to catch changed waypoint ids
  airwayTrackQuery->initQueries();
  waypointTrackQuery->initQueries();

  emit preTrackLoad();
  trackManager->loadTracks(trackVector, downloadOnlyValid);
  emit postTrackLoad();
}

void TrackController::startDownload()
{
  qDebug() << Q_FUNC_INFO;

  cancelDownload();

  // Append all to queue and start
  downloadQueue.append(atools::track::ALL_TRACK_TYPES);
  downloader->startAllDownloads();

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
  if(checked)
    startDownload();
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

void TrackController::downloadFinished(const atools::track::TrackVectorType& tracks, atools::track::TrackType type)
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

    NavApp::setStatusMessage(tr("Track download finished."));
  }
}

void TrackController::downloadFailed(const QString& error, int errorCode, QString downloadUrl,
                                     atools::track::TrackType type)
{
  qDebug() << Q_FUNC_INFO << error << errorCode << downloadUrl << static_cast<int>(type);

  if(!errorReported)
  {
    // Show an error dialog for any failed downloads but only once per session
    errorReported = true;
    atools::gui::Dialog(mainWindow).showWarnMsgBox(lnm::ACTIONS_SHOW_TRACK_DOWNLOAD_FAIL,
                                                   tr("<p>Download of track information from<br/>\"%1\"<br/>"
                                                      "failed.</p><p>Error: %2 (%3)</p>"
                                                      "<p>Check your track settings or disable track downloads.</p>"
                                                        "<p>Suppressing further messages during this session.</p>").
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

    NavApp::setStatusMessage(tr("Track download failed."));
  }
}

void TrackController::tracksLoaded()
{
  NavApp::setStatusMessage(tr("Track download finished."));

  QMap<atools::track::TrackType, int> numTracks = trackManager->getNumTracks();

  QStringList str;
  for(atools::track::TrackType type : numTracks.keys())
  {
    int num = numTracks.value(type);
    str.append(tr("<li>%1: %2 tracks.</li>").
               arg(atools::track::typeToString(type)).
               arg(num == 0 ? tr("No") : QString::number(num)));

  }

  QString err;
  const QStringList& errorMessages = trackManager->getErrorMessages();
  if(!errorMessages.isEmpty())
  {
    err += tr("<p>Errors reading tracks.<br/>Make sure to use the latest navdata.</p><ul><li>");
    err += errorMessages.mid(0, 5).join("</li><li>");

    if(errorMessages.size() > 5)
      err += tr("</li><li>More ...");

    err += tr("</li></ul>");
  }

  QString boxMessage = tr("<p>Tracks downloaded.</p><ul>%1</ul>%2").arg(str.join("")).arg(err);

  if(!errorMessages.isEmpty())
    // Force display of dialog in case of errors
    QMessageBox::warning(mainWindow, QApplication::applicationName(), boxMessage);
  else
    atools::gui::Dialog(mainWindow).showInfoMsgBox(lnm::ACTIONS_SHOW_TRACK_DOWNLOAD_SUCCESS, boxMessage,
                                                   tr("Do not &show this dialog again."));
}
