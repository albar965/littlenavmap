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

  /* Read and write widget states, source and airspace selection */
  void restoreState();
  void saveState();

  /* Update on option change */
  void optionsChanged();

  /* Clears and deletes all queries */
  void preDatabaseLoad();

  /* Re-initializes all queries */
  void postDatabaseLoad();

  /* Start download from all three sources and load the tracks into the database once done.
   * The raw data is kept so it can be loaded into the database again when switching simulators.*/
  void startDownload();
  void deleteTracks();
  void downloadToggled(bool checked);

  /* Stop download and clear internal track list. */
  void cancelDownload();

  /* True if there are tracks in the database */
  bool hasTracks() const;

  AirwayTrackQuery *getAirwayTrackQuery() const
  {
    return airwayTrackQuery;
  }

  WaypointTrackQuery *getWaypointTrackQuery() const
  {
    return waypointTrackQuery;
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
  void downloadFinished(const atools::track::TrackVectorType& tracks, atools::track::TrackType type);
  void downloadFailed(const QString& error, int errorCode, QString downloadUrl, atools::track::TrackType type);
  void tracksLoaded();

  /* Avoid multiple error reports. */
  bool errorReported = false;
  bool verbose = false;
  MainWindow *mainWindow;

  atools::track::TrackDownloader *downloader = nullptr;

  /* Wraps the track database */
  TrackManager *trackManager = nullptr;

  /* Filled with all types when starting download. Empty when all types finished downloading */
  QVector<atools::track::TrackType> downloadQueue;
  AirwayTrackQuery *airwayTrackQuery = nullptr;
  WaypointTrackQuery *waypointTrackQuery = nullptr;

  /* Saved raw track data */
  atools::track::TrackVectorType trackVector;

  /* Do not load tracks that are currently not valid. */
  bool downloadOnlyValid = false;
};

#endif // LNM_AIRWAYCONTROLLER_H
