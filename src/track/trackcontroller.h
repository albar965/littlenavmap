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

#ifndef LNM_AIRWAYCONTROLLER_H
#define LNM_AIRWAYCONTROLLER_H

#include "track/tracktypes.h"

#include <QObject>

namespace atools {
namespace track {
class TrackDownloader;
}
}

class MainWindow;
class TrackManager;
class AirwayTrackQuery;
class WaypointTrackQuery;

/*
 * Downloads track systems (NAT, PACOTS and AUSOTS) from public websites, parses the pages and loads the tracks into
 * the track database.
 *
 * Also initializes and updates waypoint and airway queries classes which share track and navaid databases.
 */
class TrackController :
  public QObject
{
  Q_OBJECT

public:
  explicit TrackController(TrackManager *trackManagerParam, MainWindow *mainWindowParam);
  virtual ~TrackController() override;

  TrackController(const TrackController& other) = delete;
  TrackController& operator=(const TrackController& other) = delete;

  /* Read and write widget states, source and airspace selection */
  void restoreState();
  void saveState();

  /* Update on option change */
  void optionsChanged();

  /* Clears and deletes all queries */
  void preDatabaseLoad();

  /* Re-initializes all queries */
  void postDatabaseLoad();

  /* Start download from all enabled sources and load the tracks into the database once done.
   * The raw data is kept so it can be loaded into the database again when switching simulators.*/
  void startDownload();
  void startDownloadStartup();
  void deleteTracks();
  void downloadToggled(bool checked);

  /* Stop download and clear internal track list. */
  void cancelDownload();

  /* True if there are tracks in the database */
  bool hasTracks() const;

  bool hasTracksEnabled() const
  {
    return !enabledTracks().isEmpty();
  }

  /* If true: Do not load tracks that are currently not valid. */
  void setDownloadOnlyValid(bool value)
  {
    downloadOnlyValid = value;
  }

signals:
  /* Signals sent before and after loading tracks into database */
  void preTrackLoad();
  void postTrackLoad();

private:
  /* Slots called by the TrackDownloader after finishing each source */
  void trackDownloadFinished(const atools::track::TrackVectorType& tracks, atools::track::TrackType type);
  void trackDownloadFailed(const QString& error, int errorCode, QString downloadUrl, atools::track::TrackType type);
  void trackDownloadSslErrors(const QStringList& errors, const QString& downloadUrl);
  void tracksLoaded();
  QVector<atools::track::TrackType> enabledTracks() const;
  void startDownloadInternal();
  void trackSelectionChanged(bool);

  /* Avoid multiple error reports. */
  bool errorReported = false;
  bool verbose = false;
  MainWindow *mainWindow;

  atools::track::TrackDownloader *downloader = nullptr;

  /* Wraps the track database */
  TrackManager *trackManager = nullptr;

  /* Filled with all types when starting download. Empty when all types finished downloading */
  QVector<atools::track::TrackType> downloadQueue;

  /* Saved raw track data */
  atools::track::TrackVectorType trackVector;

  /* Do not load tracks that are currently not valid. */
  bool downloadOnlyValid = false;
};

#endif // LNM_AIRWAYCONTROLLER_H
