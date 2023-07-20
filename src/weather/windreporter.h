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

#ifndef LNM_WINDREPORTER_H
#define LNM_WINDREPORTER_H

#include "fs/fspaths.h"
#include "grib/windtypes.h"
#include "query/querytypes.h"

#include <QWidgetAction>

namespace windinternal {
class WindSliderAction;
class WindLabelAction;
}

namespace atools {
namespace geo {
class Rect;
class Line;
class LineString;
}
namespace grib {
class WindQuery;
}
}

class QToolButton;
class QAction;
class QActionGroup;
class QSlider;
class Route;

namespace wind {

/* All levels above 0 are millibar */
enum WindSelection
{
  NONE, /* No wind barbs shown */
  AGL, /* 80 m / 250 ft above ground */
  FLIGHTPLAN, /* Flight plan altitude */
  SELECTED /* Selected in slider */
};

enum WindSource
{
  WIND_SOURCE_DISABLED, /* Disabled */
  WIND_SOURCE_MANUAL, /* Set in fuel report panel */
  WIND_SOURCE_SIMULATOR, /* X-Plane GRIB file */
  WIND_SOURCE_NOAA /* Download from NOAA site */
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

  WindReporter(const WindReporter& other) = delete;
  WindReporter& operator=(const WindReporter& other) = delete;

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

  /* true if wind barbs grid is to be drawn */
  bool isWindShown() const;

  /* true if wind barbs should be shown at flight plan waypoints */
  bool isRouteWindShown() const;

  /* True if online wind available */
  bool hasOnlineWindData() const;

  /* true if action "manual" is selected */
  bool isWindManual() const;

  /* Either manual or online */
  bool hasAnyWindData() const
  {
    return hasOnlineWindData() || isWindManual();
  }

  /* Get currently shown/selected wind bar altitude level in ft. 0. if none is selected.  */
  float getDisplayAltitudeFt() const;

  /* Selecte manual altitude for wind layer */
  float getManualAltitudeFt() const;

  /* Get a list of wind positions for the given rectangle for painting. Does not use manual wind setting.
   * Result is sorted by y and x coordinates. */
  const atools::grib::WindPosList *getWindForRect(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                                  bool lazy, int gridSpacing);

  /* Get (interpolated) wind for given position and altitude */
  atools::grib::WindPos getWindForPos(const atools::geo::Pos& pos, float altFeet);
  atools::grib::WindPos getWindForPos(const atools::geo::Pos& pos);

  /* Get (interpolated) wind for given position and altitude. Use manual wind setting if checkbox is set. */
  atools::grib::Wind getWindForPosRoute(const atools::geo::Pos& pos);

  /* Get interpolated winds for lines. Use manual wind setting if checkbox is set. */
  atools::grib::Wind getWindForLineRoute(const atools::geo::Pos& pos1, const atools::geo::Pos& pos2);
  atools::grib::Wind getWindForLineRoute(const atools::geo::Line& line);
  atools::grib::Wind getWindForLineStringRoute(const atools::geo::LineString& line);

  /* Get a list of winds for the given position at all given altitudes. Returns only not interpolated levels.
   * Altitiude field in resulting pos contains the altitude. */
  atools::grib::WindPosList getWindStackForPos(const atools::geo::Pos& pos, const atools::grib::WindPos *additionalWind = nullptr) const;

  /* Updates the query class */
  void updateManualRouteWinds();

  /* Update toolbar button and menu items */
  void updateToolButtonState();

  /* AGL, flight plan or altitude */
  QString getLevelText() const;

  /* Manual, NOAA, Simulator, ... */
  QString getSourceText() const;
  wind::WindSource getSource() const;

#ifdef DEBUG_INFORMATION
  QString getDebug(const atools::geo::Pos& pos);

#endif

  void resetSettingsToDefault();

  /* Print the size of all container classes to detect overflow or memory leak conditions */
  void debugDumpContainerSizes() const;

signals:
  /* Emitted when NOAA or X-Plane wind file changes or a request to weather was fullfilled */
  void windUpdated();

  /* Emitted on display changes like wind altitude in slider */
  void windDisplayUpdated();

private:
  /* Get a list of winds for the given position at all given altitudes. Altitiude field in resulting pos contains the altitude.
   * Adds flight plan altitude if needed and selected in GUI. Does not use manual wind setting.*/
  atools::grib::WindPosList windStackForPosInternal(const atools::geo::Pos& pos, QVector<int> altitudesFt) const;

  /* One of the toolbar dropdown menu items of main menu items was triggered */
  void toolbarActionTriggered();
  void toolbarActionFlightplanTriggered();
  void updateDataSource();

  /* Download successfully finished. Only for void init(). */
  void windDownloadFinished();

  /* Download failed.  Only for void init(). */
  void windDownloadFailed(const QString& error, int errorCode);
  void windDownloadSslErrors(const QStringList& errors, const QString& downloadUrl);
  void windDownloadProgress(qint64 bytesReceived, qint64 bytesTotal, QString downloadUrl);

  /* Copy GUI action state to fields and vice versa */
  void actionToValues();
  void valuesToAction();

  void sourceActionTriggered();

  void altSliderChanged();

  /* Update altitude label from slider values */
  void updateSliderLabel();

  atools::grib::WindQuery *currentWindQuery() const
  {
    return isWindManual() ? windQueryManual : windQueryOnline;
  }

  /* GRIB wind data query for downloading files and monitoring files- Manual wind if for user setting. */
  atools::grib::WindQuery *windQueryOnline = nullptr, *windQueryManual = nullptr;

  /* Toolbar button */
  QToolButton *windlevelToolButton = nullptr;

  /* Special actions */
  QAction *actionNone = nullptr, *actionFlightplanWaypoints = nullptr, *actionFlightplan = nullptr, *actionAgl = nullptr,
          *actionSelected = nullptr;

  /* Group for mutual exclusion */
  bool verbose = false;
  atools::fs::FsPaths::SimulatorType simType = atools::fs::FsPaths::NONE;

  QActionGroup *actionGroup = nullptr;

  /* Levels for the tooltip */
  QVector<int> levelsTooltipFt = {0 /* interpreted at AGL */, 1000, 2000, 5000, 10000, 15000, 20000, 25000, 30000, 35000, 40000, 45000};

  /* Currently displayed altitude or one of SpecialLevels */
  wind::WindSelection currentWindSelection = wind::NONE;

  wind::WindSource currentSource = wind::WIND_SOURCE_NOAA;
  bool showFlightplanWaypoints = false;

  /* Avoid action signals when updating GUI elements */
  bool ignoreUpdates = false;

  /* Wind positions as a result of querying the rectangle for caching */
  query::SimpleRectCache<atools::grib::WindPos> windPosCache;
  int cachedLevel = wind::NONE;

  windinternal::WindSliderAction *sliderActionAltitude = nullptr;
  windinternal::WindLabelAction *labelActionWindAltitude = nullptr;

  bool downloadErrorReported = false;
};

namespace windinternal {
/*
 * Wraps a slider into an action allowing to add it to a menu.
 */
class WindSliderAction
  : public QWidgetAction
{
  Q_OBJECT

public:
  explicit WindSliderAction(QObject *parent);

  int getAltitudeFt() const;

  /* Adjusts sliders but does not send signals */
  void setAltitudeFt(int altitude);

  /* Minimum step in feet */
  const static int WIND_SLIDER_STEP_ALT_FT = 500;
  const static int MIN_WIND_ALT = 1000;
  const static int MAX_WIND_ALT = 45000;

signals:
  void valueChanged(int value);
  void sliderReleased();

protected:
  /* Create and delete widget for more than one menu (tearout and normal) */
  virtual QWidget *createWidget(QWidget *parent) override;
  virtual void deleteWidget(QWidget *widget) override;

  /* minmum and maximum values in ft */
  int minValue() const;
  int maxValue() const;
  void setSliderValue(int value);

  /* List of created/registered slider widgets */
  QVector<QSlider *> sliders;

  /* Altitude in feet / WIND_SLIDER_STEP_ALT_FT */
  int sliderValue = 0;
};
}

#endif // LNM_WINDREPORTER_H
