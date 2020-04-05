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

#include <QDebug>

using atools::track::TrackDownloader;

TrackController::TrackController(TrackManager *trackManagerParam, MainWindow *mainWindowParam) :
  QObject(mainWindowParam), mainWindow(mainWindowParam), trackManager(trackManagerParam)
{
  verbose = atools::settings::Settings::instance().getAndStoreValue(lnm::OPTIONS_TRACK_DEBUG, false).toBool();

  // Set up airway queries =====================
  airwayTrackQuery = new AirwayTrackQuery(new AirwayQuery(NavApp::getDatabaseNav(), false),
                                          new AirwayQuery(NavApp::getDatabaseTrack(), true));
  airwayTrackQuery->initQueries();

  // Set up waypoint queries =====================
  waypointTrackQuery = new WaypointTrackQuery(new WaypointQuery(NavApp::getDatabaseNav(), false),
                                              new WaypointQuery(NavApp::getDatabaseTrack(), true));
  waypointTrackQuery->initQueries();

  downloader = new TrackDownloader(this, verbose);
  connect(downloader, &TrackDownloader::downloadFinished, this, &TrackController::downloadFinished);
  connect(downloader, &TrackDownloader::downloadFailed, this, &TrackController::downloadFailed);

#ifdef DEBUG_TRACK_TEST
  downloader->setUrl(atools::track::NAT, "/home/alex/Temp/tracks/NAT.html");
  downloader->setUrl(atools::track::PACOTS, "/home/alex/Temp/tracks/PACOTS.html");
  downloader->setUrl(atools::track::AUSOTS, "/home/alex/Temp/tracks/AUSOTS.html");
#endif
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

}

void TrackController::saveState()
{

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
  airwayTrackQuery->initQueries();
  waypointTrackQuery->initQueries();

  // Reload track into database to catch changed waypoint ids
  trackManager->setVerbose(verbose);
  trackManager->loadTracks(trackVector);
}

void TrackController::startDownload()
{
  qDebug() << Q_FUNC_INFO;

  // Append all to queue and start
  downloadQueue.append(atools::track::ALL_TRACK_TYPES);
  downloader->startAllDownloads();
}

void TrackController::cancelDownload()
{
  qDebug() << Q_FUNC_INFO;
  downloader->cancelAllDownloads();
  downloadQueue.clear();
  trackVector.clear();
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
    trackManager->setVerbose(verbose);
    trackManager->loadTracks(trackVector);

    // Update queries - clear cache
    airwayTrackQuery->postTrackLoad();
    waypointTrackQuery->postTrackLoad();
    emit postTrackLoad();
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
    airwayTrackQuery->postTrackLoad();
    waypointTrackQuery->postTrackLoad();
    emit postTrackLoad();
  }
}
