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

#ifndef LNM_WINDREPORTER_H
#define LNM_WINDREPORTER_H

#include "fs/fspaths.h"

#include "query/querytypes.h"

namespace atools {
namespace geo {
class Rect;
class Pos;
class Line;
class LineString;
}
namespace grib {
class WindQuery;
struct WindPos;
struct Wind;

typedef QList<WindPos> WindPosList;
typedef QVector<WindPos> WindPosVector;
}
}

class QToolButton;
class QAction;
class QActionGroup;
class Route;

namespace wind {

/* All levels above 0 are millibar */
enum SpecialLevels
{
  NONE = -10, /* No wind barbs shown */
  AGL = -11, /* 80 m / 250 ft above ground */
  FLIGHTPLAN = -12 /* Flight plan altitude */
};

enum WindSource
{
  NO_SOURCE, /* Disabled */
  SIMULATOR, /* X-Plane GRIB file */
  NOAA /* Download from NOAA site */
};

}

/*
 * Takes care of upper wind management, queries, configuration and GUI elements.
 */
class WindReporter :
  public QObject
{
  Q_OBJECT

public:
  explicit WindReporter(QObject *parent, atools::fs::FsPaths::SimulatorType type);
  virtual ~WindReporter() override;

  /* Does nothing currently */
  void preDatabaseLoad();

  /* Reloads weather if simulator has changed. */
  void postDatabaseLoad(atools::fs::FsPaths::SimulatorType type);

  /* Options dialog changed settings. Updates paths and URLs from GUI and options and then reloads GRIB files. */
  void optionsChanged();

  /* Save and reload current wind level selection and source to options */
  void saveState();
  void restoreState();

  /* Adds the wind button to the toolbar including all actions. Also populates main menu */
  void addToolbarButton();

  /* Get currently selected level. One of wr::SpecialLevels or millibar */
  int getCurrentLevel() const
  {
    return currentLevel;
  }

  /* true if wind barbs grid is to be drawn */
  bool isWindShown() const;

  /* true if wind barbs should be shown at flight plan waypoints */
  bool isRouteWindShown() const;

  /* True if wind available */
  bool hasWindData() const;

  /* true if checkbox "manual" is checked */
  bool isWindManual() const;

  /* true if X-Plane or NOAA wind is enabled. */
  bool isWindSourceEnabled() const;

  /* Get currently shown/selected wind bar altitude level in ft. 0. if none is selected.  */
  float getAltitude() const;

  /* Get a list of wind positions for the given rectangle for painting. Does not use manual wind setting. */
  const atools::grib::WindPosList *getWindForRect(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                                  bool lazy);

  /* Get (interpolated) wind for given position and altitude */
  atools::grib::WindPos getWindForPos(const atools::geo::Pos& pos, float altFeet);
  atools::grib::WindPos getWindForPos(const atools::geo::Pos& pos);

  /* Get (interpolated) wind for given position and altitude. Use manual wind setting if checkbox is set. */
  atools::grib::Wind getWindForPosRoute(const atools::geo::Pos& pos);

  /* Get interpolated winds for lines. Use manual wind setting if checkbox is set. */
  atools::grib::Wind getWindForLineRoute(const atools::geo::Pos& pos1, const atools::geo::Pos& pos2);
  atools::grib::Wind getWindForLineRoute(const atools::geo::Line& line);
  atools::grib::Wind getWindForLineStringRoute(const atools::geo::LineString& line);

  /* Get a list of winds for the given position at all given altitudes. Altitiude field in pos contains the altitude.
   * Adds flight plan altitude if needed and selected in GUI. Does not use manual wind setting.*/
  atools::grib::WindPosVector getWindStackForPos(const atools::geo::Pos& pos, QVector<int> altitudesFt);

  /* Get a list of winds for the given position at all given altitudes. Returns only not interpolated levels.
   * Altitiude field in pos contains the altitude.
   * Adds flight plan altitude if needed and selected in GUI. Does not use manual wind setting. */
  atools::grib::WindPosVector getWindStackForPos(const atools::geo::Pos& pos);

  /* Updates the query class */
  void updateManualRouteWinds();

  /* Update toolbar button and menu items */
  void updateToolButtonState();

  QString getLevelText() const;
  QString getSourceText() const;

#ifdef DEBUG_INFORMATION
  QString getDebug(const atools::geo::Pos& pos);

#endif

  void resetSettingsToDefault();

signals:
  /* Emitted when NOAA or X-Plane wind file changes or a request to weather was fullfilled */
  void windUpdated();

private:
  /* One of the toolbar dropdown menu items of main menu items was triggered */
  void toolbarActionTriggered();
  void toolbarActionFlightplanTriggered();
  void updateDataSource();

  /* Download successfully finished. Only for void init(). */
  void windDownloadFinished();

  /* Download failed.  Only for void init(). */
  void windDownloadFailed(const QString& error, int errorCode);

  /* Copy GUI action state to fields and vice versa */
  void actionToValues();
  void valuesToAction();

  void sourceActionTriggered();

  /* GRIB wind data query for downloading files and monitoring files- Manual wind if for user setting. */
  atools::grib::WindQuery *windQuery = nullptr, *windQueryManual = nullptr;

  /* Toolbar button */
  QToolButton *windlevelToolButton = nullptr;

  /* Special actions */
  QAction *actionNone = nullptr, *actionFlightplanWaypoints = nullptr, *actionFlightplan = nullptr,
          *actionAgl = nullptr;

  /* Actions for levels */
  QVector<QAction *> actionLevelVector;

  /* Group for mutual exclusion */
  bool verbose = false;
  atools::fs::FsPaths::SimulatorType simType = atools::fs::FsPaths::UNKNOWN;

  QActionGroup *actionGroup = nullptr;

  /* Levels for the dropdown menu in feet */
  QVector<int> levels = {wind::AGL, wind::FLIGHTPLAN, 5000, 10000, 15000, 20000, 25000, 30000, 35000, 40000, 45000};

  /* Levels for the tooltip */
  QVector<int> levelsTooltip = {wind::AGL, 5000, 10000, 15000, 20000, 25000, 30000, 35000, 40000, 45000};

  /* Currently displayed altitude or one of SpecialLevels */
  int currentLevel = wind::NONE;
  wind::WindSource currentSource = wind::NOAA;
  bool showFlightplanWaypoints = false;

  /* Avoid action signals when updating GUI elements */
  bool ignoreUpdates = false;

  /* Wind positions as a result of querying the rectangle for caching */
  query::SimpleRectCache<atools::grib::WindPos> windPosCache;
  int cachedLevel = wind::NONE;

  bool downloadErrorReported = false;
};

#endif // LNM_WINDREPORTER_H
