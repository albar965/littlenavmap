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

#ifndef LITTLENAVMAP_PROFILEWIDGET_H
#define LITTLENAVMAP_PROFILEWIDGET_H

#include "route/route.h"
#include "fs/sc/simconnectdata.h"

#include <QFuture>
#include <QFutureWatcher>
#include <QWidget>

namespace atools {
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

/*
 * Loads and displays the flight plan elevation profile. The elevation data is
 * calculated in a background thread that is triggered when new elevation data
 * arrives from the Marble widget.
 */
class ProfileWidget :
  public QWidget
{
  Q_OBJECT

public:
  ProfileWidget(QWidget *parent);
  virtual ~ProfileWidget() override;

  /* If geometry has changed the elevation calculation is started after a short delay */
  void routeChanged(bool geometryChanged, bool newFlightPlan);
  void routeAltitudeChanged(int altitudeFeet);

  /* Update user aircraft on profile display */
  void simDataChanged(const atools::fs::sc::SimConnectData& simulatorData);

  /* Track was shortened and needs a full update */
  void aircraftTrackPruned();

  void simulatorStatusChanged();

  /* Deletes track */
  /* Stops showing the user aircraft */
  void connectedToSimulator();
  void disconnectedFromSimulator();

  /* Disables or enables aircraft and/or track display */
  void updateProfileShowFeatures();

  /* Notification after track deletion */
  void deleteAircraftTrack();

  /* Stops thread and disables all udpates */
  void preDatabaseLoad();

  /* Restarts updates */
  void postDatabaseLoad();

  /* Delta settings for option */
  struct SimUpdateDelta
  {
    int manhattanLengthDelta;
    float altitudeDelta;
  };

  void optionsChanged();
  void styleChanged();

  void saveState();
  void restoreState();

  void preRouteCalc();

  void mainWindowShown();

  /* Pair of screen y and altitude in feet to display and label the scale */
  QVector<std::pair<int, int> > calcScaleValues();

  float getMinSafeAltitudeFt() const
  {
    return minSafeAltitudeFt;
  }

  /* Widget coordinates for red line */
  int getMinSafeAltitudeY() const;

  float getFlightplanAltFt() const
  {
    return flightplanAltFt;
  }

  /* Widget coordinates for flight plan line */
  int getFlightplanAltY() const;

  float getMaxWindowAlt() const
  {
    return maxWindowAlt;
  }

  bool hasValidRouteForDisplay(const Route& route) const;

  /* Call by this and profile label widget class. Point in screen coordinates. */
  void showContextMenu(const QPoint& globalPoint);

  /* From center button */
  void jumpBackToAircraftCancel();

  void aircraftPerformanceChanged(const atools::fs::perf::AircraftPerf* perf);

signals:
  /* Emitted when the mouse cursor hovers over the map profile.
   * @param pos Position on the map display.
   */
  void highlightProfilePoint(const atools::geo::Pos& pos);

  /* Show flight plan waypoint or user position on map */
  void showPos(const atools::geo::Pos& pos, float zoom, bool doubleClick);

private:
  /* Route leg storing all elevation points */
  struct ElevationLeg
  {
    atools::geo::LineString elevation; /* Ground elevation (Pos.altitude) and position */
    QVector<float> distances; /* Distances along the route for each elevation point.
                               *  Measured from departure point. Nautical miles. */
    float maxElevation = 0.f; /* Max ground altitude for this leg */
  };

  struct ElevationLegList
  {
    Route route; /* Copy from route controller.
                  * Need a copy to avoid thread synchronization problems. */
    QList<ElevationLeg> elevationLegs; /* Elevation data for each route leg */
    float maxElevationFt = 0.f /* Maximum ground elevation for the route */,
          totalDistance = 0.f /* Total route distance in nautical miles */;
    int totalNumPoints = 0; /* Number of elevation points in whole flight plan */
  };

  /* Show position at x ordinate on profile on the map */
  void showPosAlongFlightplan(int x, bool doubleClick);

  virtual void paintEvent(QPaintEvent *) override;
  virtual void showEvent(QShowEvent *) override;
  virtual void hideEvent(QHideEvent *) override;
  virtual void resizeEvent(QResizeEvent *) override;
  virtual void leaveEvent(QEvent *) override;

  /* Mouse events*/
  virtual void mouseMoveEvent(QMouseEvent *mouseEvent) override;
  virtual void contextMenuEvent(QContextMenuEvent *event) override;

  bool fetchRouteElevations(atools::geo::LineString& elevations, const atools::geo::LineString& geometry) const;
  ElevationLegList fetchRouteElevationsThread(ElevationLegList legs) const;
  void elevationUpdateAvailable();
  void updateTimeout();
  void updateThreadFinished();
  void updateScreenCoords();
  void terminateThread();
  float calcGroundBuffer(float maxElevation);
  bool aircraftTrackValid();

  void updateLabel();
  void updateErrorMessages();

  /* Calculate map position on flight plan for x screen/widget position on profile.
   *  Additionally gives index into route, distances from/to and altitude at x. maxElev is minimum elevation for leg */
  void calculateDistancesAndPos(int x, atools::geo::Pos& pos, int& routeIndex, float& distance, float& distanceToGo,
                                float& groundElevation, float& maxElev);

  /* Calculate map position on flight plan for x screen/widget position on profile. */
  atools::geo::Pos calculatePos(int x);

  /* Calulate screen x and y position on map */
  int distanceX(float distanceNm) const;
  int altitudeY(float altitudeFt) const;

  /* Convert points (x = distance and y = altitude) to screen coordinates x/y */
  QPoint toScreen(const QPointF& pt) const;
  QPolygon toScreen(const QPolygonF& leg) const;

  void hideRubberBand();

  /* Paint slopes at destination if an approach is selected. */
  void paintIls(QPainter& painter, const Route& route);
  void paintVasi(QPainter& painter, const Route& route);

  /* Scale levels to test for display */
  static Q_DECL_CONSTEXPR int NUM_SCALE_STEPS = 5;
  const int SCALE_STEPS[NUM_SCALE_STEPS] = {500, 1000, 2000, 5000, 10000};
  /* Scales should be at least this amount of pixels apart */
  static Q_DECL_CONSTEXPR int MIN_SCALE_SCREEN_DISTANCE = 25;
  const int X0 = 52; // 65; /* Left margin inside widget */
  const int Y0 = 16; // 14; /* Top margin inside widget */

  /* Thread will start after this delay if route was changed */
  static Q_DECL_CONSTEXPR int ROUTE_CHANGE_UPDATE_TIMEOUT_MS = 1000;
  static Q_DECL_CONSTEXPR int ROUTE_CHANGE_OFFLINE_UPDATE_TIMEOUT_MS = 200;

  /* Thread will start after this delay if an elevation update arrives */
  static Q_DECL_CONSTEXPR int ELEVATION_CHANGE_UPDATE_TIMEOUT_MS = 5000;
  static Q_DECL_CONSTEXPR int ELEVATION_CHANGE_OFFLINE_UPDATE_TIMEOUT_MS = 200;

  /* Do not calculate a profile for legs longer than this value */
  static Q_DECL_CONSTEXPR int ELEVATION_MAX_LEG_NM = 2000;

  /* Minimum screen size of the aircraft track on the screen to be shown and to alter the profile altitude */
  static Q_DECL_CONSTEXPR int MIN_AIRCRAFT_TRACK_WIDTH = 10;

  /* User aircraft data */
  atools::fs::sc::SimConnectData simData, lastSimData;
  QPolygon aircraftTrackPoints;
  float maxTrackAltitudeFt = 0.f;

  float aircraftDistanceFromStart, aircraftDistanceToDest;
  float lastAircraftDistanceFromStart;
  ElevationLegList legList;

  RouteController *routeController = nullptr;
  JumpBack *jumpBack = nullptr;
  bool contextMenuActive = false;

  /* Calls updateTimeout which will start the update thread in background */
  QTimer *updateTimer = nullptr;

  /* Used to fetch result from thread */
  QFuture<ElevationLegList> future;
  /* Sends signal once thread is finished */
  QFutureWatcher<ElevationLegList> watcher;
  bool terminateThreadSignal = false;

  bool databaseLoadStatus = false;

  QRubberBand *rubberBand = nullptr;

  QString fixedLabelText, variableLabelText;

  bool widgetVisible = false, showAircraft = false, showAircraftTrack = false;
  QVector<int> waypointX; /* Flight plan waypoint screen coordinates - does contain the dummy
                           * from airport to runway but not missed legs */
  QPolygon landPolygon; /* Green landmass polygon */
  float minSafeAltitudeFt = 0.f, /* Red line */
        flightplanAltFt = 0.f, /* Cruise altitude */
        maxWindowAlt = 1.f; /* Maximum altitude at top of widget */

  ProfileScrollArea *scrollArea = nullptr;

  float verticalScale = 1.f /* Factor to convert altitude in feet to screen coordinates*/,
        horizontalScale = 1.f /* Factor to convert distance along flight plan in nautical miles to screen coordinates*/;

  /* Error messages from profile altitude calculation */
  QStringList messages;
  void jumpBackToAircraftStart();
  void jumpBackToAircraftTimeout();

};

Q_DECLARE_TYPEINFO(ProfileWidget::SimUpdateDelta, Q_PRIMITIVE_TYPE);

#endif // LITTLENAVMAP_PROFILEWIDGET_H
