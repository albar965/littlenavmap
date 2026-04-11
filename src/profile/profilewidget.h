/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#ifndef LITTLENAVMAP_PROFILEWIDGET_H
#define LITTLENAVMAP_PROFILEWIDGET_H

#include "fs/sc/simconnectdata.h"

#include <QWidget>

class RouteAltitudeLeg;
namespace atools {
namespace geo {
class LineString;
}
namespace fs {
namespace perf {
class AircraftPerf;
}
}
}

namespace Marble {
class ElevationModel;
class GeoDataLineString;
}

class RouteController;
class QTimer;
class QRubberBand;
class ProfileScrollArea;
class JumpBack;
class Route;
class RouteLeg;
class ProfileOptions;
struct ElevationLegList;
struct ElevationLeg;
struct ElevationLegListFuturePrivate;

/*
 * Loads and displays the flight plan elevation profile. The elevation data is
 * calculated in a background thread that is triggered when new elevation data
 * arrives from the Marble widget.
 *
 * This widget is the full drawing area that is covered by the scroll view.
 */
class ProfileWidget :
  public QWidget
{
  Q_OBJECT

public:
  explicit ProfileWidget(QWidget *parent);
  virtual ~ProfileWidget() override;

  ProfileWidget(const ProfileWidget& other) = delete;
  ProfileWidget& operator=(const ProfileWidget& other) = delete;

  /* If geometry has changed the elevation calculation is started after a short delay */
  void windUpdated();
  void routeChanged(bool geometryChanged, bool newFlightplan);
  void routeAltitudeChanged(int altitudeFeet);

  /* Update user aircraft on profile display */
  void simDataChanged(const atools::fs::sc::SimConnectData& simulatorData);

  void simulatorStatusChanged();

  /* Deletes track */
  /* Stops showing the user aircraft */
  void connectedToSimulator();
  void disconnectedFromSimulator();

  /* Disables or enables aircraft and/or track display */
  void updateProfileShowFeatures();

  /* Deleting aircraft track needs an update of the screen coordinates.
   * Called on manual delete and on takeoff detection. */
  void deleteAircraftTrailPoints();

  /* Stops thread and disables all udpates */
  void preDatabaseLoad();

  /* Restarts updates. Forces new thread creation */
  void postDatabaseLoad();

  /* Delta settings for option */
  struct SimUpdateDelta
  {
    int manhattanLengthDelta;
    float altitudeDelta;
  };

  void optionsChanged();
  void styleChanged();
  void fontChanged(const QFont& font);

  void saveState() const;
  void restoreState();

  /* Bring splitter to a resonable size after first start */
  void restoreSplitter();

  void preRouteCalc();

  void mainWindowShown();

  /* Pair of screen y and altitude in feet to display and label the scale */
  const QList<std::pair<int, int> > calcScaleValues();

  float getMinSafeAltitudeFt() const
  {
    return minSafeAltitudeFt;
  }

  /* Widget coordinates for red line */
  int getMinSafeAltitudeY() const;

  /* Widget coordinates for flight plan line */
  int getFlightplanAltY() const;

  float getMaxWindowAlt() const
  {
    return maxWindowAlt;
  }

  /* true if converted route is valid and can be shown on map. This also includes routes without valid TOC and TOD */
  bool hasValidRouteForDisplay() const;

  bool hasTrailPoints() const
  {
    return !aircraftTrailPoints.isEmpty();
  }

  /* Call by this and profile label widget class. Point in screen coordinates. */
  void showContextMenu(const QPoint& globalPoint);

  /* From center button */
  void jumpBackToAircraftCancel();

  void aircraftPerformanceChanged(const atools::fs::perf::AircraftPerf *);

  const QList<int>& getWaypointX() const
  {
    return waypointX;
  }

  const Route& getRoute() const;

  /* Multiply with NM to get distance on screen in pixel for given distance. Divide pixel by this value to get distance in NM. */
  float getVerticalScale() const
  {
    return verticalScale;
  }

  /* Multiply with feet to get altitude on screen in pixel for altitude. Divide pixel by this value to get altitude in feet. */
  float getHorizontalScale() const
  {
    return horizontalScale;
  }

  ProfileOptions *getProfileOptions() const
  {
    return profileOptions;
  }

  float getGroundBufferForLegFt(int legIndex);

  void showIlsChanged();

  /* Stop jump back timer on shutdown */
  void cancelJumpBack();

signals:
  /* Emitted when the mouse cursor hovers over the map profile.
   * @param pos Position on the map display.
   */
  void highlightProfilePoint(const atools::geo::Pos& pos);

  /* Show flight plan waypoint or user position on map */
  void showPos(const atools::geo::Pos& pos, float zoom, bool doubleClick);

  /* Background calculations finished */
  void profileAltCalculationFinished();

private:
  /* Show position at x ordinate on profile on the map */
  void showPosAlongFlightplan(int x, bool doubleClick);

  virtual void paintEvent(QPaintEvent *) override;
  virtual void showEvent(QShowEvent *) override;
  virtual void hideEvent(QHideEvent *) override;

  /* Resizing needs an update of the screen coordinates */
  virtual void resizeEvent(QResizeEvent *) override;

  /* Cursor leaves widget. Stop displaying the rubberband */
  virtual void leaveEvent(QEvent *) override;

  /* Mouse events*/
  virtual void mouseMoveEvent(QMouseEvent *mouseEvent) override;
  virtual void contextMenuEvent(QContextMenuEvent *event) override;

  /* Get elevation points between the two points. This returns also correct results if the antimeridian is crossed
   * @return true if not aborted */
  bool fetchRouteElevations(atools::geo::LineString& elevations, const atools::geo::LineString& geometry, float sampleRadiusNm,
                            bool precision) const;

  /* Background thread. Fetches elevation points from Marble elevation model and updates totals. */
  ElevationLegList fetchRouteElevationsThread(ElevationLegList legs) const;

  /* Update signal from Marble elevation model */
  void elevationUpdateAvailable();

  /* Called by updateTimer after any route or elevation updates and starts the thread */
  void updateTimeout();

  /* Called by watcher when the thread is finished */
  void fetchRouteElevationsThreadFinished();

  /* Update all screen coordinates and scale factors */
  void updateScreenCoords();

  /* Calculate the left and right margin based on font size and airport elevation text */
  void calculateProfileMargin();

  float calcGroundBufferFt(float maxElevationFt);

  void updateHeaderLabel();

  /* Calculate map position on flight plan for x screen/widget position on profile.
   * Additionally gives index into route, distances from/to and altitude at x. maxElev is minimum elevation for leg.
   * Distances in NM and altitudes in feet. */
  void calculateDistancesAndPos(int x, atools::geo::Pos& pos, int *routeIndex = nullptr, float *distance = nullptr,
                                float *distanceToGo = nullptr, float *groundElevationInner = nullptr, float *groundElevationOuter = nullptr,
                                float *maxElev = nullptr);

  /* Calculate map position on flight plan for x screen/widget position on profile. */
  atools::geo::Pos calculatePos(int x);

  /* Calulate screen x and y position on map */
  int distanceX(float distanceNm) const;
  int altitudeY(float altitudeFt) const;

  /* Convert points (x = distance and y = altitude) to screen coordinates x/y.
   * Coordinate does not relate to the left buffer space but window rectangle. */
  QPoint toScreen(const QPointF& pt) const;
  QPolygon toScreen(const QPolygonF& leg) const;

  /* Screen position of the destination airport if valid or aircraft distance plus ahead */
  QPoint destinationAirportScreenPos(bool& destUsed, float distanceAhead) const;

  void hideRubberBand();

  /* Paint slopes at destination if an approach is selected. */
  void paintIls(QPainter& painter, const Route& route);
  void paintVasi(QPainter& painter, const Route& route);

  /* Paint status messages into top left corner */
  void paintCalculatingMessage(QPainter& painter);
  void paintNoRouteMessage(QPainter& painter);
  void paintInvalidRouteMessage(QPainter& painter);
  void paintInitMessage(QPainter& painter);

  /* Draw a vertical track/path line extending from user aircraft */
  void paintVerticalPath(QPainter& painter, const Route& route);

  void jumpBackToAircraftStart();
  void jumpBackToAircraftTimeout();

  /* Update error label in flight plan window */
  void updateErrorLabel();

  /* Load and save track separately */
  void saveAircraftTrail() const;
  void loadAircraftTrail();

  void updateTooltip();
  void buildTooltipText(int x, bool force);

  /* Get either indicated or real */
  float aircraftAlt(const atools::fs::sc::SimConnectUserAircraft& aircraft);

  /* Screen pixel width of this leg. Considers zero-length IAF legs */
  int calcLegScreenWidth(const QList<QPolygon>& altLegs, int waypointIndex);

  /* Get text and text color for a leg. procSymbol is true if only the generic procedure waypoint should be drawn */
  QStringList textsAndColorForLeg(QColor& color, bool& procSymbol, const RouteLeg& leg, bool procedureDisplayText, int legWidth);

  /* Shows the display options dialog */
  void showDisplayOptions();

  /* Center aircraft or zoom to aircraft and destination depending on distance to destination. */
  void centerAircraft();

  /* Calculate landmass polygon in screen coordinates. width and height are window dimensions */
  void calculateLandmass(QPolygon& landPolygon, const QList<ElevationLeg>& elevationLegs, int width, int height) const;

  /* Fill leg, leg list distances, altitudes and maximum altitude for leg and plan */
  bool fillElevationLeg(ElevationLeg& leg, ElevationLegList& legs, double& totalDistanceNm, const atools::geo::LineString& elevations,
                        const RouteAltitudeLeg& altitudeLeg) const;

  /* Fill dummy values for a too long leg */
  void fillLongElevationLeg(ElevationLeg& leg, ElevationLegList& legs, double& totalDistanceNm, const RouteAltitudeLeg& altitudeLeg) const;

  /* Adjust a correction for all distances in the leg.
   * This can happen with the online elevation provider which does not return all points exactly on the leg */
  void applyCorrection(ElevationLeg& leg, double scale) const;

  /* Get ground elevation for give route leg and distance */
  const ElevationLeg& legFromPos(float& groundElevation, const QList<ElevationLeg>& elevationLegs, int routeIndex, float distance);

  /* true if fetchRouteElevationsThread() is running */
  bool isCalculating() const;

  /* Start thread after short delay to calculate new data. Calls ProfileWidget::updateTimeout() */
  void startUpdateTimer();

  /* User aircraft data */
  atools::fs::sc::SimConnectData simData, lastSimData;

  /* Track x = distance from start in NM and y = altitude in feet */
  QPolygonF aircraftTrailPoints;

  float aircraftDistanceFromStart; /* NM */
  float lastAircraftDistanceFromStart;

  /* Aircraft is moving backwards */
  bool movingBackwards = false;

  /* Contains a copy of the flight plan and all calculated elevation positions */
  ElevationLegList *legList;

  JumpBack *jumpBack = nullptr;
  bool contextMenuActive = false;

  /* Calls updateTimeout which will start the update thread in background */
  QTimer *updateTimer = nullptr;

  ElevationLegListFuturePrivate *futurePrivate;

  bool databaseLoadStatus = false;
  bool insideResizeEvent = false; // Avoid recursion when resize is called by ProfileScrollArea::scaleView

  QRubberBand *rubberBand = nullptr;

  int lastTooltipX = -999;
  QString lastTooltipString;
  atools::geo::Pos lastTooltipPos;
  QPointF lastTooltipScreenPos;

  bool widgetVisible = false, showAircraft = false, showAircraftTrail = false;
  QList<int> waypointX; /* Flight plan waypoint screen coordinates - does contain the dummy
                           from airport to runway but not missed legs */

  QPolygon landPolygonInner, landPolygonOuter; /* Green landmass polygon */

  float minSafeAltitudeFt = 0.f, /* Red line */
        maxWindowAlt = 1.f; /* Maximum altitude at top of widget */

  ProfileScrollArea *scrollArea = nullptr;
  ProfileOptions *profileOptions = nullptr;

  float verticalScale = 1.f /* Factor to convert altitude in feet to screen coordinates*/,
        horizontalScale = 1.f /* Factor to convert distance along flight plan in nautical miles to screen coordinates*/;

  /* Left margin inside widget - calculated depending on font and text size in paint. Gray area left and right of profile. */
  int paintMargin = 30;

  /* Margins for messages and warnings */
  int textX = 10, textY = 10;

  /* Widget is shown */
  bool active = false;

  /* Display "initializing ..." on first startup until fetch is finished. The fetch thread is always run, even for invalid routes */
  bool fetchRouteElevationsFinished = false;

  /* Numbers for aircraft track */
  const static quint32 FILE_MAGIC_NUMBER = 0x6B7C2A3C;
  const static quint16 FILE_VERSION = 1;
};

Q_DECLARE_TYPEINFO(ProfileWidget::SimUpdateDelta, Q_PRIMITIVE_TYPE);

#endif // LITTLENAVMAP_PROFILEWIDGET_H
