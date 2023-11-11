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

#include "profile/profilewidget.h"

#include "atools.h"
#include "app/navapp.h"
#include "geo/calculations.h"
#include "common/mapcolors.h"
#include "profile/profilescrollarea.h"
#include "profile/profilelabelwidgetvert.h"
#include "profile/profilelabelwidgethoriz.h"
#include "profile/profileoptions.h"
#include "ui_mainwindow.h"
#include "common/symbolpainter.h"
#include "util/htmlbuilder.h"
#include "route/route.h"
#include "route/routealtitude.h"
#include "common/unit.h"
#include "common/formatter.h"
#include "settings/settings.h"
#include "common/constants.h"
#include "mapgui/mapwidget.h"
#include "options/optiondata.h"
#include "common/elevationprovider.h"
#include "common/vehicleicons.h"
#include "common/jumpback.h"
#include "weather/windreporter.h"
#include "options/optiondata.h"
#include "perf/aircraftperfcontroller.h"

#include <QPainter>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>
#include <QStringBuilder>

#include <marble/ElevationModel.h>
#include <marble/GeoDataCoordinates.h>
#include <marble/GeoDataLineString.h>

/* Scale levels to test for display */
static const int NUM_SCALE_STEPS = 5;
static const int SCALE_STEPS[NUM_SCALE_STEPS] = {500, 1000, 2000, 5000, 10000};
/* Scales should be at least this amount of pixels apart */
static const int MIN_SCALE_SCREEN_DISTANCE = 25;
const int TOP = 16; /* Top margin inside widget */

/* Thread will start after this delay if route was changed */
static const int ROUTE_CHANGE_UPDATE_TIMEOUT_MS = 200;
static const int ROUTE_CHANGE_OFFLINE_UPDATE_TIMEOUT_MS = 100;

/* Thread will start after this delay if an elevation update arrives */
static const int ELEVATION_CHANGE_ONLINE_UPDATE_TIMEOUT_MS = 5000;
static const int ELEVATION_CHANGE_OFFLINE_UPDATE_TIMEOUT_MS = 100;

// Defines a rectangle where five points are sampled and the maximum altitude is used.
// Results in a sample rectangle with ELEVATION_SAMPLE_RADIUS_NM * ELEVATION_SAMPLE_RADIUS_NM size
static const float ELEVATION_SAMPLE_RADIUS_NM = 0.1f;

/* Do not calculate a profile for legs longer than this value */
static const int ELEVATION_MAX_LEG_NM = 2000;

/* Zoom to aircraft + 100 NM or to aircraft to destination */
static const float ZOOM_DESTINATION_MAX_AHEAD = 100.f;

/* Maximum delta values depending on update rate in options */
// int manhattanLengthDelta;
// float altitudeDelta;
static QHash<opts::SimUpdateRate, ProfileWidget::SimUpdateDelta> SIM_UPDATE_DELTA_MAP(
{
  {
    opts::FAST, {1, 1.f}
  },
  {
    opts::MEDIUM, {2, 10.f}
  },
  {
    opts::LOW, {4, 20.f}
  }
});

/* Text label position and attributes for navaids */
struct Label
{
  Label(const QPoint& symPtParam, const QColor& colorParam, bool procSymbolParam, const QStringList& textsParam,
        const map::MapTypes& typeParam)
    : symPt(symPtParam), color(colorParam), procSymbol(procSymbolParam), texts(textsParam), type(typeParam)
  {
  }

  bool operator<(const Label& other) const
  {
    // Sort first by type and then by position
    if(priority() == other.priority())
      return symPt.x() > other.symPt.x();
    else
      return priority() < other.priority();
  }

  int priority() const
  {
    if(type == map::PROCEDURE)
      return 0;
    else if(type == map::VOR)
      return 1;
    else if(type == map::NDB)
      return 2;
    else if(type == map::WAYPOINT)
      return 3;
    else if(type == map::USERPOINTROUTE)
      return 4;

    return 5;
  }

  QPoint symPt;
  QColor color;
  bool procSymbol;
  QStringList texts;
  map::MapTypes type;
};

using Marble::GeoDataCoordinates;
using Marble::GeoDataLineString;
using atools::geo::Pos;
using atools::geo::LineString;
using atools::roundToInt;

// =======================================================================================

/* Route leg storing all elevation points */
struct ElevationLeg
{
  QString ident;
  atools::geo::LineString elevation, /* Ground elevation (Pos.altitude) and position */
                          geometry; /* Route geometry. Normally only start and endpoint.
                                     * More complex for procedures. */
  QVector<double> distances; /* Distances along the route for each elevation point.
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

// =======================================================================================

ProfileWidget::ProfileWidget(QWidget *parent)
  : QWidget(parent)
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

  setContextMenuPolicy(Qt::DefaultContextMenu);
  setFocusPolicy(Qt::WheelFocus);

  profileOptions = new ProfileOptions(this);
  legList = new ElevationLegList;

  scrollArea = new ProfileScrollArea(this, ui->scrollAreaProfile);
  scrollArea->setProfileLeftOffset(left);
  scrollArea->setProfileTopOffset(TOP);

  jumpBack = new JumpBack(this, atools::settings::Settings::instance().getAndStoreValue(lnm::OPTIONS_PROFILE_JUMP_BACK_DEBUG,
                                                                                        false).toBool());
  connect(jumpBack, &JumpBack::jumpBack, this, &ProfileWidget::jumpBackToAircraftTimeout);

  connect(scrollArea, &ProfileScrollArea::showPosAlongFlightplan, this, &ProfileWidget::showPosAlongFlightplan);
  connect(scrollArea, &ProfileScrollArea::hideRubberBand, this, &ProfileWidget::hideRubberBand);
  connect(scrollArea, &ProfileScrollArea::jumpBackToAircraftStart, this, &ProfileWidget::jumpBackToAircraftStart);
  connect(scrollArea, &ProfileScrollArea::jumpBackToAircraftCancel, this, &ProfileWidget::jumpBackToAircraftCancel);
  connect(ui->actionProfileCenterAircraft, &QAction::toggled, this, &ProfileWidget::jumpBackToAircraftCancel);
  connect(ui->actionProfileZoomAircraft, &QAction::toggled, this, &ProfileWidget::jumpBackToAircraftCancel);

  connect(ui->actionProfileDisplayOptions, &QAction::triggered, this, &ProfileWidget::showDisplayOptions);
  connect(ui->pushButtonProfileSettings, &QPushButton::clicked, this, &ProfileWidget::showDisplayOptions);

  // Create single shot timer that will restart the thread after a delay
  updateTimer = new QTimer(this);
  updateTimer->setSingleShot(true);
  connect(updateTimer, &QTimer::timeout, this, &ProfileWidget::updateTimeout);

  // Marble will let us know when updates are available
  connect(NavApp::getElevationProvider(), &ElevationProvider::updateAvailable,
          this, &ProfileWidget::elevationUpdateAvailable);

  // Notification from thread that it has finished and we can get the result from the future
  connect(&watcher, &QFutureWatcher<ElevationLegList>::finished, this, &ProfileWidget::updateThreadFinished);

  // Want mouse events even when no button is pressed
  setMouseTracking(true);
}

ProfileWidget::~ProfileWidget()
{
  ATOOLS_DELETE_LOG(jumpBack);

  updateTimer->stop();
  updateTimer->deleteLater();
  terminateThread();

  ATOOLS_DELETE_LOG(scrollArea);
  ATOOLS_DELETE_LOG(legList);
  ATOOLS_DELETE_LOG(profileOptions);
}

void ProfileWidget::aircraftTrailPruned()
{
  if(!widgetVisible)
    return;

  updateScreenCoords();
  update();
}

float ProfileWidget::aircraftAlt(const atools::fs::sc::SimConnectUserAircraft& aircraft)
{
  return aircraft.getIndicatedAltitudeFt() < atools::fs::sc::SC_INVALID_FLOAT ?
         aircraft.getIndicatedAltitudeFt() : aircraft.getActualAltitudeFt();
}

void ProfileWidget::simDataChanged(const atools::fs::sc::SimConnectData& simulatorData)
{
  if(databaseLoadStatus || !simulatorData.getUserAircraftConst().isValid())
    return;

  bool updateWidget = false;

  // Need route with update active leg and aircraft position
  const Route& route = NavApp::getRouteConst();

  // Do not update for single airport plans
  if(route.getSizeWithoutAlternates() > 1)
  {
    simData = simulatorData;

    bool lastPosValid = lastSimData.getUserAircraftConst().isValid();
    bool simPosValid = simData.getUserAircraftConst().isValid();

    float lastAlt = aircraftAlt(lastSimData.getUserAircraftConst());
    float simAlt = aircraftAlt(simData.getUserAircraftConst());

    aircraftDistanceFromStart = route.getProjectionDistance();

    // Update if new or last distance is/was invalid
    updateWidget = (aircraftDistanceFromStart < map::INVALID_DISTANCE_VALUE) !=
                   (lastAircraftDistanceFromStart < map::INVALID_DISTANCE_VALUE) && widgetVisible;

    if(aircraftDistanceFromStart < map::INVALID_DISTANCE_VALUE)
    {
#ifdef DEBUG_INFORMATION_PROFILE_SIMDATA
      if(simData.getUserAircraft().isDebug())
        qDebug() << Q_FUNC_INFO << aircraftDistanceFromStart;
#endif

      // Get point (NM/ft) for current update
      QPointF currentPoint(aircraftDistanceFromStart, simAlt);
      QPoint currentScreenPoint = toScreen(currentPoint);

      // Add track point if delta value between last and current update is large enough
      if(simPosValid)
      {
        if(aircraftTrailPoints.isEmpty())
          aircraftTrailPoints.append(currentPoint);
        else
        {
          QLineF delta(aircraftTrailPoints.constLast(), currentPoint);

          if(std::abs(delta.dx()) > 0.1 /* NM */ || std::abs(delta.dy()) > 50. /* ft */)
            aircraftTrailPoints.append(currentPoint);

          if(aircraftTrailPoints.size() > OptionData::instance().getAircraftTrailMaxPoints())
            aircraftTrailPoints.removeFirst();
        }
      }

      if(widgetVisible)
      {
        const SimUpdateDelta deltas = SIM_UPDATE_DELTA_MAP.value(OptionData::instance().getSimUpdateRate());

        using atools::almostNotEqual;
        // Get point (NM/ft) from last update
        QPointF lastPoint;
        if(lastPosValid)
          lastPoint = QPointF(lastAircraftDistanceFromStart, lastAlt);
        // Update widget if delta value between last and current update is large enough
        if(!lastPosValid || // No last position
           !scrollArea->isPointVisible(currentScreenPoint) ||
           (toScreen(lastPoint) - currentScreenPoint).manhattanLength() >= deltas.manhattanLengthDelta || // Position change on screen
           almostNotEqual(lastAlt, simAlt, deltas.altitudeDelta) // Altitude change
           )
        {
          movingBackwards = (lastAircraftDistanceFromStart < map::INVALID_DISTANCE_VALUE) &&
                            (lastAircraftDistanceFromStart > aircraftDistanceFromStart);

          lastSimData = simData;
          lastAircraftDistanceFromStart = aircraftDistanceFromStart;

          if(simAlt > maxWindowAlt)
            // Scale up to keep the aircraft visible
            updateScreenCoords();

          if(!jumpBack->isActive())
            centerAircraft();

          // Aircraft position has changed enough
          updateWidget = true;
        } // if(!lastPosValid ...
      } // if(widgetVisible)
    } // if(aircraftDistanceFromStart < map::INVALID_DISTANCE_VALUE)
  } // if(!NavApp::getRouteConst().isFlightplanEmpty())

  if(updateWidget)
    update();

  if(widgetVisible)
    updateHeaderLabel();
}

void ProfileWidget::centerAircraft()
{
  // Need route with update active leg and aircraft position
  Ui::MainWindow *ui = NavApp::getMainUi();

  // Probably center aircraft on scroll area
  if(ui->actionProfileCenterAircraft->isChecked())
  {
    QPoint currentScreenPoint = toScreen(QPointF(aircraftDistanceFromStart, aircraftAlt(simData.getUserAircraftConst())));
    bool destUsed;
    QPoint zoomScreenPoint = destinationAirportScreenPos(destUsed, ZOOM_DESTINATION_MAX_AHEAD);
    if(ui->actionProfileZoomAircraft->isChecked() && destUsed)
    {
      // Vertical zoom only in descent phase
      const Route& route = NavApp::getRouteConst();
      bool zoomVertically = route.getTopOfDescentDistance() < map::INVALID_DISTANCE_VALUE &&
                            aircraftDistanceFromStart > route.getTopOfDescentDistance();

      // Destination in range - zoom to aircraft and destination rectangle
      scrollArea->centerRect(currentScreenPoint, zoomScreenPoint, zoomVertically, false /* force */);
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << "destUsed" << destUsed << "zoomVertically" << zoomVertically;
#endif
    }
    else
      // Destination not in range - zoom normally, keep aircraft visible and keep zoom value
      scrollArea->centerAircraft(currentScreenPoint, simData.getUserAircraftConst().getVerticalSpeedFeetPerMin(), false /* force */);
  }
}

void ProfileWidget::connectedToSimulator()
{
  qDebug() << Q_FUNC_INFO;
  jumpBack->cancel();
  simData = atools::fs::sc::SimConnectData();
  updateScreenCoords();
  update();
  updateHeaderLabel();
}

void ProfileWidget::disconnectedFromSimulator()
{
  qDebug() << Q_FUNC_INFO;
  jumpBack->cancel();
  simData = atools::fs::sc::SimConnectData();
  updateScreenCoords();
  update();
  updateHeaderLabel();
  scrollArea->hideTooltip();
}

float ProfileWidget::getGroundBufferForLegFt(int legIndex)
{
  if(atools::inRange(legList->elevationLegs, legIndex))
    return calcGroundBufferFt(legList->elevationLegs.value(legIndex).maxElevation);
  else
    return map::INVALID_ALTITUDE_VALUE;
}

void ProfileWidget::showIlsChanged()
{
  NavApp::getRoute().updateApproachIls();
  legList->route.updateApproachIls();
  update();
}

float ProfileWidget::calcGroundBufferFt(float maxElevationFt)
{
  if(maxElevationFt < map::INVALID_ALTITUDE_VALUE)
  {
    // Revert local units to feet
    float groundBufferFt = Unit::rev(OptionData::instance().getRouteGroundBuffer(), Unit::altFeetF);
    float roundBufferFt = Unit::rev(500.f, Unit::altFeetF);

    return std::ceil((maxElevationFt + groundBufferFt) / roundBufferFt) * roundBufferFt;
  }
  else
    return map::INVALID_ALTITUDE_VALUE;
}

void ProfileWidget::updateScreenCoords()
{
  /* Update all screen coordinates and scale factors */

  calcLeftMargin();

  // Widget drawing region width and height
  int w = rect().width() - left * 2, h = rect().height() - TOP;

  // Need scale to determine track length on screen
  horizontalScale = w / legList->totalDistance;

  // Update elevation polygon
  // Add 1000 ft buffer and round up to the next 500 feet
  minSafeAltitudeFt = calcGroundBufferFt(legList->maxElevationFt);

  if(profileOptions->getDisplayOptions().testFlag(optsp::PROFILE_SAFE_ALTITUDE) && minSafeAltitudeFt < map::INVALID_ALTITUDE_VALUE)
    maxWindowAlt = std::max(minSafeAltitudeFt, legList->route.getCruiseAltitudeFt());
  else
    maxWindowAlt = legList->route.getCruiseAltitudeFt();

  if(simData.getUserAircraftConst().isValid() && (showAircraft || showAircraftTrail) && !NavApp::getRouteConst().isFlightplanEmpty())
    maxWindowAlt = std::max(maxWindowAlt, aircraftAlt(simData.getUserAircraftConst()));

  // if(showAircraftTrack)
  // maxWindowAlt = std::max(maxWindowAlt, maxTrackAltitudeFt);

  scrollArea->routeAltitudeChanged();

  verticalScale = h / maxWindowAlt;

  // Calculate the landmass polygon
  waypointX.clear();
  landPolygon.clear();

  if(!legList->elevationLegs.isEmpty())
  {
    // First point
    landPolygon.append(QPoint(left, h + TOP));

#ifdef DEBUG_INFORMATION_PROFILE
    qDebug() << Q_FUNC_INFO << "==========================================================================";
#endif

#ifdef DEBUG_INFORMATION_PROFILE
    int num = 0;
#endif
    for(const ElevationLeg& leg : qAsConst(legList->elevationLegs))
    {
      if(leg.distances.isEmpty() || leg.elevation.isEmpty())
        continue;

#ifdef DEBUG_INFORMATION_PROFILE
      qDebug() << Q_FUNC_INFO << num << leg.ident << "leg.distances" << leg.distances;
      qDebug() << Q_FUNC_INFO << num << leg.ident << "leg.geometry" << leg.geometry;
      qDebug() << Q_FUNC_INFO << num << leg.ident << "leg.elevation" << leg.elevation;
#endif

      waypointX.append(left + static_cast<int>(leg.distances.constFirst() * horizontalScale));

      QPoint lastPt;
      for(int i = 0; i < leg.elevation.size(); i++)
      {
        float alt = leg.elevation.at(i).getAltitude();
        QPoint pt(left + static_cast<int>(leg.distances.at(i) * horizontalScale), TOP + static_cast<int>(h - alt * verticalScale));

        if(lastPt.isNull() || i == leg.elevation.size() - 1 || (lastPt - pt).manhattanLength() > 2)
        {
          landPolygon.append(pt);
          lastPt = pt;
        }
      }
#ifdef DEBUG_INFORMATION_PROFILE
      num++;
#endif
    }

    // Destination point
    if(!waypointX.isEmpty())
      waypointX.append(left + w);

#ifdef DEBUG_INFORMATION_PROFILE
    qDebug() << Q_FUNC_INFO << "waypointX" << waypointX;
    qDebug() << Q_FUNC_INFO << "==========================================================================";
#endif

    // Last point closing polygon
    if(!landPolygon.isEmpty())
      landPolygon.append(QPoint(left + w, h + TOP));
  }
}

const QVector<std::pair<int, int> > ProfileWidget::calcScaleValues()
{
  int h = rect().height() - TOP;
  // Create a temporary scale based on current units
  float tempScale = Unit::rev(verticalScale, Unit::altFeetF);

  // Find best scale for elevation lines
  float step = 10000.f;
  for(int stp : SCALE_STEPS)
  {
    // Loop through all scale steps and check which one fits best
    if(stp * tempScale > MIN_SCALE_SCREEN_DISTANCE)
    {
      step = stp;
      break;
    }
  }

  // Now collect all scale positions by iterating step-wise from bottom to top
  QVector<std::pair<int, int> > steps;
  for(float y = TOP + h, alt = 0; y > TOP; y -= step * tempScale, alt += step)
    steps.append(std::make_pair(y, alt));

  return steps;
}

int ProfileWidget::getMinSafeAltitudeY() const
{
  return altitudeY(minSafeAltitudeFt);
}

int ProfileWidget::getFlightplanAltY() const
{
  return altitudeY(legList->route.getCruiseAltitudeFt());
}

bool ProfileWidget::hasValidRouteForDisplay() const
{
  const Route& route = NavApp::getRouteConst();
  return !legList->elevationLegs.isEmpty() && route.getSizeWithoutAlternates() >= 2 &&
         route.getAltitudeLegs().size() >= 2 &&
         route.size() == route.getAltitudeLegs().size();
}

int ProfileWidget::altitudeY(float altitudeFt) const
{
  if(altitudeFt < map::INVALID_ALTITUDE_VALUE)
    return static_cast<int>(rect().height() - altitudeFt * verticalScale);
  else
    return map::INVALID_INDEX_VALUE;
}

QPoint ProfileWidget::toScreen(const QPointF& pt) const
{
  return QPoint(distanceX(static_cast<float>(pt.x())), altitudeY(static_cast<float>(pt.y())));
}

QPolygon ProfileWidget::toScreen(const QPolygonF& leg) const
{
  QPolygon retval;
  for(const QPointF& point : leg)
  {
    QPoint pt = toScreen(point);
    if(retval.isEmpty())
      retval.append(pt);
    else if((retval.constLast() - point).manhattanLength() > 3)
      retval.append(pt);
  }
  return retval;
}

QPoint ProfileWidget::destinationAirportScreenPos(bool& destUsed, float distanceAhead) const
{
  const Route& route = NavApp::getRouteConst();

  int destIdx = route.getDestinationAirportLegIndex();
  if(destIdx != map::INVALID_INDEX_VALUE)
  {
    float dist = map::INVALID_DISTANCE_VALUE, alt = map::INVALID_ALTITUDE_VALUE;
    destUsed = aircraftDistanceFromStart + distanceAhead > route.getAltitudeLegAt(destIdx).getDistanceFromStart();
    alt = route.getAltitudeLegAt(destIdx).getWaypointAltitude();
    dist = route.getAltitudeLegAt(destIdx).getDistanceFromStart();

    if(alt < map::INVALID_ALTITUDE_VALUE && dist < map::INVALID_DISTANCE_VALUE)
      return toScreen(QPointF(dist, alt));
  }
  return QPoint();
}

int ProfileWidget::distanceX(float distanceNm) const
{
  if(distanceNm < map::INVALID_DISTANCE_VALUE)
    return left + roundToInt(distanceNm * horizontalScale);
  else
    return map::INVALID_INDEX_VALUE;
}

void ProfileWidget::showPosAlongFlightplan(int x, bool doubleClick)
{
  // Check range before
  if(x > left && x < width() - left)
    emit showPos(calculatePos(x), 0.f, doubleClick);
}

void ProfileWidget::paintVerticalPath(QPainter& painter, const Route& route)
{
  static const float LINE_LENGTH_NM = 20.f;

  float aircraftAltitude = aircraftAlt(simData.getUserAircraftConst());
  int acx = distanceX(aircraftDistanceFromStart);
  int acy = altitudeY(aircraftAltitude);

  if(acx < map::INVALID_INDEX_VALUE && acy < map::INVALID_INDEX_VALUE && aircraftDistanceFromStart < route.getTotalDistance())
  {

    painter.setBrush(Qt::NoBrush);
    painter.setPen(mapcolors::adjustWidth(mapcolors::markSelectedAltitudeRangePen,
                                          (OptionData::instance().getDisplayThicknessFlightplanProfile() / 100.f) * 2.f));
    painter.setBackgroundMode(Qt::OpaqueMode);
    painter.setBackground(Qt::transparent);

    float verticalSpeedFeetPerMin = simData.getUserAircraftConst().getVerticalSpeedFeetPerMin();
    float groundSpeedFeetPerMin = atools::geo::nmToFeet(simData.getUserAircraftConst().getGroundSpeedKts()) / 60.f;

    float lineLenNm = std::min(std::min(route.getTotalDistance() - aircraftDistanceFromStart, LINE_LENGTH_NM),
                               scrollArea->getViewport()->width() / 3.f / horizontalScale);
    float travelTimeForLineMin = atools::geo::nmToFeet(lineLenNm) / groundSpeedFeetPerMin;
    float feetForLine = travelTimeForLineMin * verticalSpeedFeetPerMin;

    painter.drawLine(toScreen(QPointF(aircraftDistanceFromStart, aircraftAltitude)),
                     toScreen(QPointF(aircraftDistanceFromStart + lineLenNm, aircraftAltitude + feetForLine)));
  }
}

void ProfileWidget::paintIls(QPainter& painter, const Route& route)
{
  if(!NavApp::getMainUi()->actionProfileShowIls->isChecked())
    return;

  const QVector<map::MapIls>& ilsVector = route.getDestRunwayIlsProfile();
  if(!ilsVector.isEmpty())
  {
    const RouteAltitude& altitudeLegs = route.getAltitudeLegs();

    // Get origin
    int x = distanceX(altitudeLegs.getDestinationDistance());
    int y = altitudeY(altitudeLegs.getDestinationAltitude());

    if(x < map::INVALID_INDEX_VALUE && y < map::INVALID_INDEX_VALUE)
    {
      // Draw all ILS
      for(const map::MapIls& ils : ilsVector)
      {
        bool isIls = !ils.isAnyGlsRnp();

        QColor fillColor = isIls ? mapcolors::ilsFillColor : mapcolors::glsFillColor;
        QColor symColor = isIls ? mapcolors::ilsSymbolColor : mapcolors::glsSymbolColor;
        QColor textColor = isIls ? mapcolors::ilsTextColor : mapcolors::glsTextColor;
        QPen centerPen = isIls ? mapcolors::ilsCenterPen : mapcolors::glsCenterPen;

        painter.setBrush(fillColor);
        painter.setPen(QPen(symColor, 2, Qt::SolidLine, Qt::FlatCap));
        painter.setBackgroundMode(Qt::OpaqueMode);
        painter.setBackground(Qt::transparent);

        // ILS feather length is 9 NM
        float featherLen = atools::geo::nmToFeet(9.f);

        // Calculate altitude at end of feather
        // tan a = GK / AK; // tan a * AK = GK;
        float ydiff1 = atools::geo::tanDeg(ils.slope) * featherLen;

        // Calculate screen points for end of feather
        int y2 = altitudeY(altitudeLegs.getDestinationAltitude() + ydiff1);
        int x2 = distanceX(altitudeLegs.getDestinationDistance() - 9.f);

        // Screen difference for +/- 0.35°
        // float ydiffUpper = std::tan(atools::geo::toRadians(ils.slope + 1.f)) * featherLen - ydiff1;

        // Construct geometry
        QLineF centerLine(x, y, x2, y2);
        QLineF lowerLine(x, y, x2, y2 + 25 /*(ydiffUpper *verticalScale)*/);
        QLineF upperLine(x, y, x2, y2 - 25 /*(ydiffUpper *verticalScale)*/);

        // Make all the same length
        lowerLine.setLength(centerLine.length());
        upperLine.setLength(centerLine.length());

        // Shorten the center line
        centerLine.setLength(centerLine.length() - QLineF(lowerLine.p2(), upperLine.p2()).length() / 2.);

        // Draw feather
        painter.drawPolygon(QPolygonF({lowerLine.p1(), lowerLine.p2(), upperLine.p2(), upperLine.p1()}));

        // Draw small indicator for ILS
        painter.drawPolyline(QPolygonF({lowerLine.p2(), centerLine.p2(), upperLine.p2()}));

        // Dashed center line
        painter.setPen(centerPen);
        painter.drawLine(centerLine);

        // Calculate text ==================
        painter.setPen(textColor);
        if(OptionData::instance().getFlags2() & opts2::MAP_NAVAID_TEXT_BACKGROUND)
        {
          painter.setBackground(Qt::white);
          painter.setBackgroundMode(Qt::OpaqueMode);
        }
        else
          painter.setBackgroundMode(Qt::TransparentMode);

        // Place near p2 at end of feather
        double angle = atools::geo::angleFromQt(upperLine.angle());
        painter.translate(upperLine.p2());
        painter.rotate(angle + 90.);
        painter.drawText(10, -painter.fontMetrics().descent(), map::ilsText(ils) + tr(" ►"));
        painter.resetTransform();
      }
    }
  }
}

void ProfileWidget::paintVasi(QPainter& painter, const Route& route)
{
  if(!NavApp::getMainUi()->actionProfileShowVasi->isChecked())
    return;

  const RouteAltitude& altitudeLegs = route.getAltitudeLegs();
  const map::MapRunwayEnd& runwayEnd = route.getDestRunwayEnd();

  if(runwayEnd.isValid())
  {
    const RouteLeg& leg = route.getDestinationLeg();
    const proc::MapProcedureLeg& procLeg = leg.getProcedureLeg();

    // Do not show VASI if approach leg has a large offset compared to the runway
    if(procLeg.isValid() && procLeg.calculatedTrueCourse < map::INVALID_COURSE_VALUE &&
       atools::geo::angleAbsDiff(procLeg.calculatedTrueCourse, runwayEnd.heading) > 45.f)
      return;

    // Get origin on screen
    int x = distanceX(altitudeLegs.getDestinationDistance());
    int y = altitudeY(altitudeLegs.getDestinationAltitude());

    // Collect left and right VASI
    QVector<std::pair<float, QString> > vasiList;

    if(runwayEnd.hasRightVasi())
      vasiList.append(std::make_pair(runwayEnd.rightVasiPitch, runwayEnd.rightVasiTypeStr()));

    if(runwayEnd.hasLeftVasi())
      vasiList.append(std::make_pair(runwayEnd.leftVasiPitch, runwayEnd.leftVasiTypeStr()));

    if(vasiList.isEmpty())
      return;

    if(vasiList.size() == 2)
    {
      // VASI on both sides
      if(atools::almostEqual(vasiList.at(0).first, vasiList.at(1).first))
      {
        if(vasiList.at(0).second != vasiList.at(1).second)
          // Merge type if the angle is the same but type is different
          vasiList[0].second = tr("%1 / %2").arg(vasiList.at(0).second).arg(vasiList.at(1).second);

        // Remove duplicate
        vasiList.removeLast();
      }
    }

    // Draw all VASI =================================================
    for(const std::pair<float, QString>& vasi : qAsConst(vasiList))
    {
      // VASI has shorted visibility than ILS range
      float featherLen = atools::geo::nmToFeet(6.f);

      // Calculate altitude at end of guide
      // tan a = GK / AK; // tan a * AK = GK;
      float ydiff1 = atools::geo::tanDeg(vasi.first) * featherLen;

      // Calculate screen points for end of guide
      int yUpper = altitudeY(altitudeLegs.getDestinationAltitude() + ydiff1);
      int xUpper = distanceX(altitudeLegs.getDestinationDistance() - 6.f);

      if(xUpper < map::INVALID_INDEX_VALUE && yUpper < map::INVALID_INDEX_VALUE)
      {
        // Screen difference for +/- one degree
        // float ydiffUpper = std::tan(atools::geo::toRadians(vasi.first + 1.5f)) * featherLen - ydiff1;

        // Build geometry
        QLineF center(x, y, xUpper, yUpper);
        QLineF lower(x, y, xUpper, yUpper + 30 /*(ydiffUpper *verticalScale)*/);
        QLineF upper(x, y, xUpper, yUpper - 30 /*(ydiffUpper *verticalScale)*/);

        lower.setLength(center.length());
        upper.setLength(center.length());

        // Draw lower red guide
        painter.setPen(Qt::NoPen);
        painter.setBrush(mapcolors::profileVasiBelowColor);
        painter.drawPolygon(QPolygonF({lower.p1(), lower.p2(), center.p2(), center.p1()}));

        // Draw above white guide
        painter.setBrush(mapcolors::profileVasiAboveColor);
        painter.drawPolygon(QPolygonF({upper.p1(), upper.p2(), center.p2(), center.p1()}));

        // Draw dashed center line
        painter.setPen(mapcolors::profileVasiCenterPen);
        painter.drawLine(center);

        painter.setPen(Qt::black);
        if(OptionData::instance().getFlags2() & opts2::MAP_NAVAID_TEXT_BACKGROUND)
        {
          painter.setBackground(Qt::white);
          painter.setBackgroundMode(Qt::OpaqueMode);
        }
        else
          painter.setBackgroundMode(Qt::TransparentMode);

        // Draw VASI text ========================
        double angle = atools::geo::angleFromQt(upper.angle());
        painter.translate(upper.p2());
        painter.rotate(angle + 90.);

        QString txt;
        if(vasi.second.isEmpty())
          txt = tr("%1° ►").arg(QLocale().toString(vasi.first, 'f', 1));
        else
          txt = tr("%1° / %2 ►").arg(QLocale().toString(vasi.first, 'f', 1)).arg(vasi.second);
        painter.drawText(10, -painter.fontMetrics().descent(), txt);
        painter.resetTransform();
      }
    }
  }
}

void ProfileWidget::calcLeftMargin()
{
  QFontMetrics metrics(OptionData::instance().getMapFont());
  left = 30;
  if(!legList->route.isEmpty())
  {
    // Calculate departure altitude text size
    float departAlt = legList->route.getDepartureAirportLeg().getAltitude();
    if(departAlt < map::INVALID_ALTITUDE_VALUE / 2.f)
      left = std::max(metrics.horizontalAdvance(Unit::altFeet(departAlt)), left);

    // Calculate destination altitude text size
    float destAlt = legList->route.getDestinationAirportLeg().getAltitude();
    if(destAlt < map::INVALID_ALTITUDE_VALUE / 2.f)
      left = std::max(metrics.horizontalAdvance(Unit::altFeet(destAlt)), left);
    left += 8;
    left = std::max(left, 30);
  }
}

void ProfileWidget::paintEvent(QPaintEvent *)
{
  // Show only ident in labels
  static const textflags::TextFlags TEXTFLAGS = textflags::IDENT | textflags::ROUTE_TEXT | textflags::ABS_POS;

  if(!active)
    return;

  // Saved route that was used to create the geometry
  const Route& route = legList->route;

  const RouteAltitude& altitudeLegs = route.getAltitudeLegs();
  const OptionData& optionData = OptionData::instance();

  // Keep margin to left, right and top
  int w = rect().width() - left * 2, h = rect().height() - TOP;

  SymbolPainter symPainter;
  QPainter painter(this);

  // Nothing to show label =========================
  if(NavApp::getRouteConst().isEmpty())
  {
    setFont(optionData.getGuiFont());
    painter.fillRect(rect(), QApplication::palette().color(QPalette::Base));
    symPainter.textBox(&painter, {tr("No Flight Plan")}, QApplication::palette().color(QPalette::PlaceholderText),
                       4, painter.fontMetrics().ascent(), textatt::RIGHT, 0);
    scrollArea->updateLabelWidgets();
    return;
  }
  else if(!hasValidRouteForDisplay())
  {
    QFont font = optionData.getGuiFont();
    font.setBold(true);
    setFont(font);
    painter.fillRect(rect(), QApplication::palette().color(QPalette::Base));
    symPainter.textBox(&painter, {tr("Flight Plan not valid")}, atools::util::HtmlBuilder::COLOR_FOREGROUND_WARNING,
                       4, painter.fontMetrics().ascent(), textatt::RIGHT, 0);
    scrollArea->updateLabelWidgets();
    return;
  }

  if(legList->route.size() != route.size() || atools::almostNotEqual(legList->route.getTotalDistance(), route.getTotalDistance()))
    // Do not draw if route is updated to avoid invalid indexes
    return;

  if(altitudeLegs.size() != route.size())
  {
    // Do not draw if route altitudes are not updated to avoid invalid indexes
    qWarning() << Q_FUNC_INFO << "Route altitudes not updated";
    return;
  }

  // Cruise altitude in screen coordinates
  int flightplanY = getFlightplanAltY();
  int safeAltY = getMinSafeAltitudeY();

  if(flightplanY == map::INVALID_INDEX_VALUE || safeAltY == map::INVALID_INDEX_VALUE)
  {
    qWarning() << Q_FUNC_INFO << "No flight plan elevation";
    return;
  }

  setFont(optionData.getMapFont());

  optsp::DisplayOptionsProfile displayOptions = profileOptions->getDisplayOptions();
  map::MapDisplayTypes mapFeaturesDisplay = NavApp::getMapWidgetGui()->getShownMapDisplayTypes();

  // Fill background sky blue ====================================================
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  painter.fillRect(left, 0, rect().width() - left * 2, rect().height(), mapcolors::profileSkyColor);

  // Draw the ground ======================================================
  if(displayOptions.testFlag(optsp::PROFILE_GROUND))
  {
    painter.setBrush(mapcolors::profileLandColor);
    painter.setPen(mapcolors::profileLandOutlinePen);
    painter.drawPolygon(landPolygon);
  }

  int flightplanTextY = flightplanY + 14;
  painter.setPen(mapcolors::profileWaypointLinePen);
  for(int wpx : qAsConst(waypointX))
    painter.drawLine(wpx, 0, wpx, TOP + h);

  // Draw elevation scale lines ======================================================
  painter.setPen(mapcolors::profileElevationScalePen);
  for(const std::pair<int, int>& scale : calcScaleValues())
    painter.drawLine(0, scale.first, left + static_cast<int>(w), scale.first);

  // Draw one line for the label to the left
  painter.drawLine(0, flightplanY, left + static_cast<int>(w), flightplanY);

  // Draw orange minimum safe altitude lines for each segment ======================================================
  if(displayOptions.testFlag(optsp::PROFILE_LEG_SAFE_ALTITUDE))
  {
    painter.setPen(mapcolors::profileSafeAltLegLinePen);
    for(int i = 0; i < legList->elevationLegs.size(); i++)
    {
      if(waypointX.value(i, 0) == waypointX.value(i + 1, 0))
        // Skip zero length segments to avoid dots on the graph
        continue;

      float groundBuffer = getGroundBufferForLegFt(i);
      if(groundBuffer < map::INVALID_ALTITUDE_VALUE)
      {
        int lineY = TOP + static_cast<int>(h - groundBuffer * verticalScale);
        painter.drawLine(waypointX.value(i, 0), lineY, waypointX.value(i + 1, 0), lineY);
      }
    }
  }

  // Draw the red minimum safe altitude line ======================================================
  if(displayOptions.testFlag(optsp::PROFILE_SAFE_ALTITUDE))
  {
    painter.setPen(mapcolors::profileSafeAltLinePen);
    painter.drawLine(0, safeAltY, left, safeAltY);
    painter.drawLine(left, safeAltY, left + static_cast<int>(w), safeAltY);
  }

  // Calculate line y positions ======================================================
  // Flight plan waypoint screen coordinates. x = distance and y = altitude  =======================
  QVector<QPolygon> altLegs;
  bool showTodToc = mapFeaturesDisplay.testFlag(map::FLIGHTPLAN_TOC_TOD);

  for(int i = 0; i < altitudeLegs.size(); i++)
  {
    const RouteAltitudeLeg& routeAlt = altitudeLegs.value(i);
    if(routeAlt.isMissed() || routeAlt.isAlternate())
      break;

    QPolygonF geo = routeAlt.getGeometry();
    if(!showTodToc)
    {
      // Set all points to flight plan cruise altitude if no TOD and TOC wanted
      for(QPointF& pt : geo)
        pt.setY(legList->route.getCruiseAltitudeFt());
    }
    altLegs.append(toScreen(geo));
  }

  // Collect indexes in reverse (painting) order without missed ================
  QVector<int> indexes;
  for(int i = 0; i <= route.getDestinationLegIndex(); i++)
  {
    if(route.value(i).getProcedureLeg().isMissed() || route.value(i).isAlternate())
      break;
    indexes.prepend(i);
  }

  // Draw altitude restriction bars ============================================
  if(mapFeaturesDisplay.testFlag(map::FLIGHTPLAN) && displayOptions.testFlag(optsp::PROFILE_FP_ALT_RESTRICTION_BLOCK))
  {
    painter.setBackground(mapcolors::profileAltRestrictionFill);
    painter.setBackgroundMode(Qt::OpaqueMode);
    QBrush diagPatternBrush(mapcolors::profileAltRestrictionOutline, Qt::BDiagPattern);
    QPen thinLinePen(mapcolors::profileAltRestrictionOutline, 1., Qt::SolidLine, Qt::FlatCap);
    QPen thickLinePen(mapcolors::profileAltRestrictionOutline, 4., Qt::SolidLine, Qt::FlatCap);

    for(int routeIndex : indexes)
    {
      const RouteLeg& leg = route.value(routeIndex);

      if(leg.isAnyProcedure() && leg.getRunwayEnd().isValid())
        continue;

      const proc::MapAltRestriction& restriction = leg.getProcedureLeg().altRestriction;

      if(restriction.isValid() && restriction.descriptor != proc::MapAltRestriction::ILS_AT &&
         restriction.descriptor != proc::MapAltRestriction::ILS_AT_OR_ABOVE)
      {
        // Use 5 NM width and minimum of 10 pix and maximum of 40 pix
        int rectWidth = roundToInt(std::min(std::max(5.f * horizontalScale, 10.f), 20.f));
        int rectHeight = 16;

        // Start and end of line
        int wpx = waypointX.value(routeIndex, 0);
        int x11 = wpx - rectWidth / 2;
        int x12 = wpx + rectWidth / 2;
        int y1 = altitudeY(restriction.alt1);

        if(restriction.isValid())
        {
          proc::MapAltRestriction::Descriptor descr = restriction.descriptor;

          // Connect waypoint with restriction with a thin vertical line
          painter.setPen(thinLinePen);
          painter.drawLine(QPoint(wpx, y1), altLegs.at(routeIndex).constLast());

          if(descr == proc::MapAltRestriction::AT_OR_ABOVE ||
             descr == proc::MapAltRestriction::AT)
            // Draw diagonal pattern rectangle above
            painter.fillRect(x11, y1, rectWidth, rectHeight, diagPatternBrush);

          if(descr == proc::MapAltRestriction::AT_OR_BELOW ||
             descr == proc::MapAltRestriction::AT)
            // Draw diagonal pattern rectangle below
            painter.fillRect(x11, y1 - rectHeight, rectWidth, rectHeight, diagPatternBrush);

          if(descr == proc::MapAltRestriction::BETWEEN)
          {
            // At or above alt2 and at or below alt1

            // Draw diagonal pattern rectangle for below alt1
            painter.fillRect(x11, y1 - rectHeight, rectWidth, rectHeight, diagPatternBrush);

            int x21 = waypointX.value(routeIndex, 0) - rectWidth / 2;
            int x22 = waypointX.value(routeIndex, 0) + rectWidth / 2;
            int y2 = altitudeY(restriction.alt2);
            // Draw diagonal pattern rectangle for above alt2
            painter.fillRect(x21, y2, rectWidth, rectHeight, diagPatternBrush);

            // Connect above and below with a thin line
            painter.drawLine(waypointX.value(routeIndex, 0), y1, waypointX.value(routeIndex, 0), y2);
            painter.setPen(thickLinePen);

            // Draw line for alt2
            painter.drawLine(x21, y2, x22, y2);
          }

          if(descr != proc::MapAltRestriction::NO_ALT_RESTR)
          {
            // Draw line for alt1 if any restriction
            painter.setPen(thickLinePen);
            painter.drawLine(x11, y1, x12, y1);
          }
        }
      } // if(restriction.isValid() && restriction.descriptor != proc::MapAltRestriction::ILS_AT && ...
    } // for(int routeIndex : indexes)
  } // if(mapFeaturesDisplay.testFlag(map::FLIGHTPLAN))

  // Draw ILS or VASI guidance ============================
  mapcolors::scaleFont(&painter, 0.95f);

  paintVasi(painter, route);
  paintIls(painter, route);

  // Get active route leg but ignore alternate legs
  const Route& curRoute = NavApp::getRouteConst();
  bool activeValid = curRoute.isActiveValid();

  // Active normally start at 1 - this will consider all legs as not passed
  int activeRouteLeg = activeValid ? atools::minmax(0, waypointX.size() - 1, curRoute.getActiveLegIndex()) : 0;
  int passedRouteLeg = optionData.getFlags2().testFlag(opts2::MAP_ROUTE_DIM_PASSED) ? activeRouteLeg : 0;

  if(curRoute.isActiveAlternate())
  {
    // Disable active leg and show all legs as passed if an alternate is enabled
    activeRouteLeg = 0;
    passedRouteLeg = optionData.getFlags2().testFlag(opts2::MAP_ROUTE_DIM_PASSED) ? std::min(passedRouteLeg + 1, waypointX.size()) : 0;
  }

  // Draw flight plan =============================================================================
  setFont(optionData.getMapFont());
  mapcolors::scaleFont(&painter, optionData.getDisplayTextSizeFlightplanProfile() / 100.f, &painter.font());

  if(mapFeaturesDisplay.testFlag(map::FLIGHTPLAN))
  {
    // Draw background line ======================================================
    float flightplanOutlineWidth = (optionData.getDisplayThicknessFlightplanProfile() / 100.f) * 7;
    float flightplanWidth = (optionData.getDisplayThicknessFlightplanProfile() / 100.f) * 4;
    painter.setPen(QPen(Qt::black, flightplanOutlineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    for(int i = passedRouteLeg; i < waypointX.size(); i++)
    {
      const proc::MapProcedureLeg& leg = route.value(i).getProcedureLeg();
      if(i > 0 && !leg.isCircleToLand() && !leg.isStraightIn() && !leg.isVectors() && !leg.isManual())
      {
        // Draw line ========================================
        painter.drawPolyline(altLegs.at(i));
      }
    } // for(int i = passedRouteLeg; i < waypointX.size(); i++)

    // Draw passed ======================================================
    painter.setPen(QPen(optionData.getFlightplanPassedSegmentColor(), flightplanWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if(passedRouteLeg < map::INVALID_INDEX_VALUE)
    {
      for(int i = 1; i < passedRouteLeg; i++)
        painter.drawPolyline(altLegs.at(i));
    }
    else
      qWarning() << Q_FUNC_INFO;

    // Draw ahead ======================================================
    QPen flightplanPen(optionData.getFlightplanColor(), flightplanWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen procedurePen(optionData.getFlightplanProcedureColor(), flightplanWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

    painter.setBackgroundMode(Qt::OpaqueMode);
    painter.setBackground(Qt::white);
    painter.setBrush(Qt::NoBrush);

    if(passedRouteLeg < map::INVALID_INDEX_VALUE)
    {
      for(int i = passedRouteLeg + 1; i < waypointX.size(); i++)
      {
        painter.setPen(altitudeLegs.value(i).isAnyProcedure() ? procedurePen : flightplanPen);
        const proc::MapProcedureLeg& leg = route.value(i).getProcedureLeg();
        if(leg.isCircleToLand() || leg.isStraightIn())
          mapcolors::adjustPenForCircleToLand(&painter);
        else if(leg.isVectors())
          mapcolors::adjustPenForVectors(&painter);
        else if(leg.isManual())
          mapcolors::adjustPenForManual(&painter);

        painter.drawPolyline(altLegs.at(i));
      }
    }
    else
      qWarning() << Q_FUNC_INFO;

    if(activeRouteLeg > 0 && activeRouteLeg < route.size())
    {
      // Draw active  ======================================================
      const RouteLeg& activeLeg = route.value(activeRouteLeg);
      QColor activeColor;

      // Choose color depending on highlight option for active leg and procedure or en-route
      if(optionData.getFlags2().testFlag(opts2::MAP_ROUTE_HIGHLIGHT_ACTIVE))
        activeColor = optionData.getFlightplanActiveSegmentColor();
      else
      {
        if(activeLeg.isAnyProcedure())
          activeColor = optionData.getFlightplanProcedureColor();
        else
          activeColor = optionData.getFlightplanColor();
      }

      painter.setPen(QPen(activeColor, flightplanWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

      const proc::MapProcedureLeg& actProcLeg = activeLeg.getProcedureLeg();
      if(actProcLeg.isCircleToLand() || actProcLeg.isStraightIn())
        mapcolors::adjustPenForCircleToLand(&painter);
      else if(actProcLeg.isVectors())
        mapcolors::adjustPenForVectors(&painter);
      else if(actProcLeg.isManual())
        mapcolors::adjustPenForManual(&painter);

      painter.drawPolyline(altLegs.at(activeRouteLeg));
    }

    // =============================================================================
    // Draw flightplan symbols and labels ======================================================
    bool distOpt = displayOptions.testFlag(optsp::PROFILE_FP_DIST);
    bool magCrsOpt = displayOptions.testFlag(optsp::PROFILE_FP_MAG_COURSE);
    bool trueCrsOpt = displayOptions.testFlag(optsp::PROFILE_FP_TRUE_COURSE);
    bool angleOpt = displayOptions.testFlag(optsp::PROFILE_FP_VERTICAL_ANGLE);

    // Labels along line ========================================================================================
    if(displayOptions & optsp::PROFILE_FP_ANY)
    {
      QFontMetricsF fontMetrics(painter.font());
      for(int i = passedRouteLeg; i < waypointX.size(); i++)
      {
        const RouteLeg& routeLeg = route.value(i);
        const proc::MapProcedureLeg& leg = routeLeg.getProcedureLeg();
        const RouteAltitudeLeg& altLeg = altitudeLegs.value(i);

        // Get screen geometry
        QPolygonF geometry = altLegs.value(i);

        // Draw vertical angle =============================================================================
        // Flight path angle label only for descent ==========================
        QVector<float> angles;
        bool requiredByProcedure = false;

        // Avoid display for wrongly required vertical angle if line is horizontal due to procedure inconsitencies
        if(angleOpt && !altLeg.getGeometry().isEmpty() && altLeg.getGeometry().constFirst().y() > altLeg.getGeometry().constLast().y())
        {
          if(i > 0 && !leg.isCircleToLand() && !leg.isStraightIn() && !leg.isVectors() && !leg.isManual())
          {
            if(altLeg.isVerticalProcAngleValid() && geometry.size() == 2)
            {
              // A required vertical angle given by procedure
              angles.append(altLeg.getVerticalProcAngle());
              requiredByProcedure = true;
            }
            else
              // Calculated list of angles by aircraft performance
              angles = altLeg.getVerticalGeoAngles();
          }
        }

        painter.setBackgroundMode(Qt::OpaqueMode);
        painter.setPen(Qt::black);

        // Iterate over vertical geometry for this route leg ==========================================
        for(int j = 1; j < geometry.size(); j++)
        {
          // Build angle text ===============================
          float pathAngle = angles.value(j - 1, map::INVALID_ANGLE_VALUE);
          QString angleText = pathAngle < -0.5f ? tr(" %1° ► ").arg(pathAngle, 0, 'g', requiredByProcedure ? 3 : 2) : QString();

          QString separator(tr(" /"));
          QLineF line(geometry.at(j - 1), geometry.at(j));
          double textWidthAngle = fontMetrics.horizontalAdvance(angleText);
          double textWidthSep = fontMetrics.horizontalAdvance(separator);
          double textHeight = fontMetrics.height();
          float legDist = static_cast<float>(altLeg.getGeometry().value(j).x() - altLeg.getGeometry().value(j - 1).x());

          QString courseDistText = routeLeg.buildLegText(false, magCrsOpt, trueCrsOpt, true /* narrow */).join(tr(" / "));
          if(distOpt)
            // Prepend distance if selected
            courseDistText = Unit::distNm(legDist, true, 20, true) % tr(" / ") % courseDistText;

          // Transform painter
          painter.translate(line.center());
          painter.rotate(atools::geo::angleFromQt(line.angle()) - 90.); // Rotate for display angle

          // Elide fist part of text before angle ===============================
          double textWidthCourseDist = 0.;
          if(!angleText.isEmpty())
            textWidthCourseDist = line.length() - textWidthSep - textWidthAngle - textHeight;
          else
            textWidthCourseDist = line.length() - textHeight;

          courseDistText = fontMetrics.elidedText(courseDistText, Qt::ElideRight, textWidthCourseDist).simplified();

          if(!angleText.isEmpty() && !courseDistText.isEmpty())
            courseDistText += separator;

          textWidthCourseDist = fontMetrics.horizontalAdvance(courseDistText);

          double textX = (textWidthAngle + textWidthCourseDist + textWidthSep) / 2.;

          painter.setBackground(mapcolors::routeTextBackgroundColor);
          if(requiredByProcedure)
          {
            // Split drawing of angle and other texts for differnt background color
            painter.drawText(roundToInt(-textX), roundToInt(textHeight / 2. - fontMetrics.descent()), courseDistText);

            painter.setBackground(mapcolors::profileAltRestrictionFill);
            painter.drawText(roundToInt(-textX + textWidthCourseDist),
                             roundToInt(textHeight / 2. - fontMetrics.descent()), angleText);
          }
          else
            // Draw all texts with same style
            painter.drawText(roundToInt(-textX), roundToInt(textHeight / 2. - fontMetrics.descent()),
                             courseDistText % angleText);

          painter.resetTransform();
        }
      } // for(int i = passedRouteLeg; i < waypointX.size(); i++)
    } // if(optionData.getDisplayOptionsProfile() & optsd::PROFILE_FP_ANY)

    // ========================================================================================
    // Calculate symbol sizes
    float sizeScaleSymbol = 1.f;
    int waypointSize = roundToInt((optionData.getDisplayTextSizeFlightplanProfile() * sizeScaleSymbol / 100.) * 8.);
    int procPointSize = roundToInt((optionData.getDisplayTextSizeFlightplanProfile() * sizeScaleSymbol / 100.) * 8.) + 3;
    int navaidSize = roundToInt((optionData.getDisplayTextSizeFlightplanProfile() * sizeScaleSymbol / 100.) * 12.);
    int airportSize = roundToInt((optionData.getDisplayTextSizeFlightplanProfile() * sizeScaleSymbol / 100.) * 10.);

    painter.setBackgroundMode(Qt::TransparentMode);
    setFont(optionData.getMapFont());
    mapcolors::scaleFont(&painter, optionData.getDisplayTextSizeFlightplanProfile() / 100.f, &painter.font());

    // Draw symbols and texts =====================================================================================
    // Draw the most unimportant symbols and texts first - userpoints, invalid and procedure points ============================
    int waypointIndex = waypointX.size();
    for(int routeIndex : indexes)
    {
      const RouteLeg& leg = route.value(routeIndex);
      const proc::MapProcedureLeg& procedureLeg = leg.getProcedureLeg();

      waypointIndex--;
      if(altLegs.at(waypointIndex).isEmpty())
        continue;

      int legScreenWidth = calcLegScreenWidth(altLegs, waypointIndex) - 10;

      QColor color;
      bool procSymbol = false;
      // Do not get display text for intercept texts if any since these will be drawn separately
      QStringList texts = textsAndColorForLeg(color, procSymbol, leg,
                                              !(leg.isAnyProcedure() && procedureLeg.interceptPos.isValid()), legScreenWidth);

      // Check for an intercept position which can be drawn separately within the leg
      if(routeIndex >= activeRouteLeg - 1 && leg.isAnyProcedure() && procedureLeg.interceptPos.isValid())
      {
        float distanceFromStart = route.getDistanceFromStart(procedureLeg.interceptPos);
        int interceptX = distanceX(distanceFromStart);
        int interceptY = altitudeY(route.getAltitudeForDistance(route.getTotalDistance() - distanceFromStart));

        // Draw symbol and label for intercept position
        symPainter.drawProcedureSymbol(&painter, interceptX, interceptY, procPointSize, true);
        QStringList displayText = atools::elidedTexts(painter.fontMetrics(), procedureLeg.displayText, Qt::ElideRight,
                                                      legScreenWidth); // TODO legScreenWidth is not correct due to offset
        symPainter.textBox(&painter, displayText, color, interceptX + 5, std::min(interceptY + 14, h), textatt::ROUTE_BG_COLOR, 255);
      }

      // Symbols ========================================================
      QPoint symPt(altLegs.at(waypointIndex).constLast());
      if(!procSymbol)
      {
        // Draw all except airport, waypoint, VOR and NDB
        map::MapTypes type = leg.getMapType();
        if(type == map::AIRPORT || leg.isAirport() ||
           type == map::WAYPOINT || leg.getWaypoint().isValid() ||
           type == map::VOR || leg.getVor().isValid() ||
           type == map::NDB || leg.getNdb().isValid())
          continue;
        else if(type == map::USERPOINTROUTE)
          symPainter.drawUserpointSymbol(&painter, symPt.x(), symPt.y(), waypointSize, true);
        else if(type == map::INVALID)
          symPainter.drawWaypointSymbol(&painter, mapcolors::routeInvalidPointColor, symPt.x(), symPt.y(), 9, true);
        else if(type == map::PROCEDURE)
          // Missed is not included
          symPainter.drawProcedureSymbol(&painter, symPt.x(), symPt.y(), procPointSize, true);
      }
      else
        symPainter.drawProcedureSymbol(&painter, symPt.x(), symPt.y(), procPointSize, true);

      // Procedure symbols ========================================================
      if(routeIndex >= activeRouteLeg - 1 && leg.isAnyProcedure())
        symPainter.drawProcedureUnderlay(&painter, symPt.x(), symPt.y(), 6, procedureLeg.flyover, procedureLeg.malteseCross);
    } // for(int routeIndex : indexes)

    // Draw waypoints below radio navaids ============================
    waypointIndex = waypointX.size();
    for(int routeIndex : indexes)
    {
      const RouteLeg& leg = route.value(routeIndex);
      const proc::MapProcedureLeg& procedureLeg = leg.getProcedureLeg();

      waypointIndex--;
      if(altLegs.at(waypointIndex).isEmpty())
        continue;

      int legScreenWidth = calcLegScreenWidth(altLegs, waypointIndex) - 10;

      QColor color;
      bool procSymbol = false;
      QStringList texts = textsAndColorForLeg(color, procSymbol, leg, !(leg.isAnyProcedure() && procedureLeg.interceptPos.isValid()),
                                              legScreenWidth);

      // Symbols ========================================================
      QPoint symPt(altLegs.at(waypointIndex).constLast());
      if(!procSymbol)
      {
        // Draw all except airport, VOR, NDB and userpoint
        map::MapTypes type = leg.getMapType();
        if(type == map::AIRPORT || leg.isAirport() ||
           type == map::VOR || leg.getVor().isValid() ||
           type == map::NDB || leg.getNdb().isValid() ||
           type == map::USERPOINTROUTE || type == map::INVALID)
          continue;
        else if(type == map::WAYPOINT || leg.getWaypoint().isValid())
          symPainter.drawWaypointSymbol(&painter, QColor(), symPt.x(), symPt.y(), waypointSize, true);
      }
      // else procedure symbols drawn before

      // Procedure symbols ========================================================
      if(routeIndex >= activeRouteLeg - 1 && leg.isAnyProcedure())
        symPainter.drawProcedureUnderlay(&painter, symPt.x(), symPt.y(), 6, procedureLeg.flyover, procedureLeg.malteseCross);
    } // for(int routeIndex : indexes)

    // Draw the more important radio navaids =======================================================
    waypointIndex = waypointX.size();
    for(int routeIndex : indexes)
    {
      waypointIndex--;
      if(altLegs.at(waypointIndex).isEmpty())
        continue;

      const RouteLeg& leg = route.value(routeIndex);
      const proc::MapProcedureLeg& procedureLeg = leg.getProcedureLeg();

      int legScreenWidth = calcLegScreenWidth(altLegs, waypointIndex) - 10;

      QColor color;
      bool procSymbol = false;
      QStringList texts = textsAndColorForLeg(color, procSymbol, leg, !(leg.isAnyProcedure() && procedureLeg.interceptPos.isValid()),
                                              legScreenWidth);

      // Symbols ========================================================
      QPoint symPt(altLegs.at(waypointIndex).constLast());
      if(!procSymbol)
      {
        // Draw all except airport, waypoint and userpoint
        map::MapTypes type = leg.getMapType();
        if(type == map::AIRPORT || leg.isAirport() ||
           type == map::WAYPOINT || leg.getWaypoint().isValid() ||
           type == map::USERPOINTROUTE || type == map::INVALID)
          continue;
        else if(type == map::NDB || leg.getNdb().isValid())
          symPainter.drawNdbSymbol(&painter, symPt.x(), symPt.y(), navaidSize, true, false);
        else if(type == map::VOR || leg.getVor().isValid())
          symPainter.drawVorSymbol(&painter, leg.getVor(), symPt.x(), symPt.y(), navaidSize, 0.f, true /* routeFill */, false /* fast */);
      }

      // Procedure symbols ========================================================
      if(routeIndex >= activeRouteLeg - 1 && leg.isAnyProcedure())
        symPainter.drawProcedureUnderlay(&painter, symPt.x(), symPt.y(), 6, procedureLeg.flyover, procedureLeg.malteseCross);
    } // for(int routeIndex : indexes)

    // ===============================================================================================
    // Waypoint, procedure, VOR and NDB labels - collect and merge texts
    QVector<Label> labels;
    waypointIndex = waypointX.size();
    for(int routeIndex : indexes)
    {
      waypointIndex--;

      if(altLegs.at(waypointIndex).isEmpty())
        continue;

      if(routeIndex < activeRouteLeg - 1)
        continue;

      const RouteLeg& leg = route.value(routeIndex);

      // Airports are drawn separately and need no merge for texts
      if(leg.getMapType() == map::AIRPORT)
        continue;

      int legScreenWidth = calcLegScreenWidth(altLegs, waypointIndex) - 10;

      // Get text and attributes
      QColor color;
      bool procSymbol = false;
      QStringList texts = textsAndColorForLeg(color, procSymbol, leg,
                                              !(leg.isAnyProcedure() && leg.getProcedureLeg().interceptPos.isValid()),
                                              legScreenWidth);

      // Check if position is the same and merge texts (only for procedures)
      Label *last = labels.isEmpty() ? nullptr : &labels.last();
      QPoint symPt(altLegs.at(waypointIndex).constLast());
      if(last != nullptr && last->symPt == symPt && last->color == color && last->procSymbol == procSymbol)
      {
        last->texts.append(texts);
        last->texts.removeDuplicates();
      }
      else
        labels.append(Label(symPt, color, procSymbol, texts, leg.getMapType()));
    }

    // Sort by type and position (right to left)
    std::sort(labels.begin(), labels.end());

    // Draw Labels ========================
    for(const Label& label : labels)
      symPainter.textBox(&painter, label.texts, label.color, label.symPt.x() + 5,
                         std::min(label.symPt.y() + 14, h), textatt::ROUTE_BG_COLOR, 255);

    // ===============================================================================================
    // Draw the most important airport symbols on top ============================================
    waypointIndex = waypointX.size();
    for(int routeIndex : indexes)
    {
      const RouteLeg& leg = route.value(routeIndex);
      waypointIndex--;
      if(altLegs.at(waypointIndex).isEmpty())
        continue;
      QPoint symPt(altLegs.at(waypointIndex).constLast());

      // Draw all airport except destination and departure
      if(leg.getMapType() == map::AIRPORT && routeIndex > 0 && routeIndex < route.getDestinationAirportLegIndex())
      {
        symPainter.drawAirportSymbol(&painter, leg.getAirport(), symPt.x(), symPt.y(), airportSize, false, false, false);

        // Labels ========================
        if(routeIndex >= activeRouteLeg - 1)
          symPainter.drawAirportText(&painter, leg.getAirport(), symPt.x() - 5, std::min(symPt.y() + 14, h),
                                     optsd::AIRPORT_NONE, TEXTFLAGS, 10, false, 16);
      }
    } // for(int routeIndex : indexes)

    if(!route.isEmpty())
    {
      // Draw departure always on the left also if there are departure procedures
      const RouteLeg& departureLeg = route.getDepartureAirportLeg();
      if(departureLeg.getMapType() == map::AIRPORT)
      {
        int textW = painter.fontMetrics().horizontalAdvance(departureLeg.getDisplayIdent());
        symPainter.drawAirportSymbol(&painter, departureLeg.getAirport(), left, flightplanY, airportSize, false, false, false);
        symPainter.drawAirportText(&painter, departureLeg.getAirport(), left - textW / 2, flightplanTextY,
                                   optsd::AIRPORT_NONE, TEXTFLAGS, 10, false, 16);
      }

      // Draw destination always on the right also if there are approach procedures
      const RouteLeg& destinationLeg = route.getDestinationAirportLeg();
      if(destinationLeg.getMapType() == map::AIRPORT)
      {
        int textW = painter.fontMetrics().horizontalAdvance(destinationLeg.getDisplayIdent());
        symPainter.drawAirportSymbol(&painter, destinationLeg.getAirport(), left + w, flightplanY, airportSize, false, false, false);
        symPainter.drawAirportText(&painter, destinationLeg.getAirport(), left + w - textW / 2, flightplanTextY,
                                   optsd::AIRPORT_NONE, TEXTFLAGS, 10, false, 16);
      }
    }

    if(!route.isFlightplanEmpty())
    {
      if(mapFeaturesDisplay.testFlag(map::FLIGHTPLAN_TOC_TOD))
      {
        float tocDist = altitudeLegs.getTopOfClimbDistance();
        float todDist = altitudeLegs.getTopOfDescentDistance();
        float width = optionData.getDisplayTextSizeFlightplanProfile() * sizeScaleSymbol / 100.f * 3.f;
        int radius = roundToInt(optionData.getDisplayTextSizeFlightplanProfile() * sizeScaleSymbol / 100. * 6.);

        painter.setBackgroundMode(Qt::TransparentMode);
        painter.setPen(QPen(Qt::black, width, Qt::SolidLine, Qt::FlatCap));
        painter.setBrush(Qt::NoBrush);

        if(!optionData.getFlags2().testFlag(opts2::MAP_ROUTE_DIM_PASSED) ||
           activeRouteLeg == map::INVALID_INDEX_VALUE || route.getTopOfClimbLegIndex() > activeRouteLeg - 1)
        {
          if(tocDist > 0.f)
          {
            int tocX = distanceX(altitudeLegs.getTopOfClimbDistance());
            if(tocX < map::INVALID_INDEX_VALUE)
            {
              // Draw the top of climb point and text =========================================================
              painter.drawEllipse(QPoint(tocX, flightplanY), radius, radius);

              symPainter.textBox(&painter, {tr("TOC %1").arg(Unit::distNm(route.getTopOfClimbDistance()))},
                                 QPen(Qt::black), tocX - radius * 2, flightplanY - 6, textatt::ROUTE_BG_COLOR | textatt::LEFT, 255);
            }
          }
        }

        if(!optionData.getFlags2().testFlag(opts2::MAP_ROUTE_DIM_PASSED) ||
           activeRouteLeg == map::INVALID_INDEX_VALUE || route.getTopOfDescentLegIndex() > activeRouteLeg - 1)
        {
          if(todDist < route.getTotalDistance())
          {
            int todX = distanceX(altitudeLegs.getTopOfDescentDistance());
            if(todX < map::INVALID_INDEX_VALUE)
            {
              // Draw the top of descent point and text =========================================================
              painter.drawEllipse(QPoint(todX, flightplanY), radius, radius);

              symPainter.textBox(&painter, {tr("TOD %1").arg(Unit::distNm(route.getTopOfDescentFromDestination()))},
                                 QPen(Qt::black), todX + radius * 2, flightplanY - 6, textatt::ROUTE_BG_COLOR, 255);
            }
          }
        }
      }
    } // if(!route.isFlightplanEmpty())

    // Departure altitude label =========================================================
    QColor labelColor = mapcolors::profileLabelColor;
    float departureAlt = legList->route.getDepartureAirportLeg().getAltitude();
    int departureAltTextY = TOP + roundToInt(h - departureAlt * verticalScale);
    departureAltTextY = std::min(departureAltTextY, TOP + h - painter.fontMetrics().height() / 2);
    QString startAltStr = Unit::altFeet(departureAlt);
    symPainter.textBox(&painter, {startAltStr}, labelColor, left - 4, departureAltTextY, textatt::BOLD | textatt::LEFT, 255);

    // Destination altitude label =========================================================
    float destAlt = route.getDestinationAirportLeg().getAltitude();
    int destinationAltTextY = TOP + static_cast<int>(h - destAlt * verticalScale);
    destinationAltTextY = std::min(destinationAltTextY, TOP + h - painter.fontMetrics().height() / 2);
    QString destAltStr = Unit::altFeet(destAlt);
    symPainter.textBox(&painter, {destAltStr}, labelColor, left + w + 4, destinationAltTextY, textatt::BOLD | textatt::RIGHT, 255);
  } // if(NavApp::getMapWidget()->getShownMapFeatures() & map::FLIGHTPLAN)

  // Draw user aircraft trail =========================================================
  if(!aircraftTrailPoints.isEmpty() && showAircraftTrail)
  {
    painter.setPen(mapcolors::aircraftTrailPenProfile(optionData.getDisplayThicknessFlightplanProfile() / 100.f * 2.f));
    painter.drawPolyline(toScreen(aircraftTrailPoints));
  }

  // Draw user aircraft =========================================================
  const atools::fs::sc::SimConnectUserAircraft& userAircraft = simData.getUserAircraftConst();
  if(userAircraft.isValid() && showAircraft && aircraftDistanceFromStart < map::INVALID_DISTANCE_VALUE && !curRoute.isActiveMissed() &&
     !curRoute.isActiveAlternate())
  {
    // Draw path line ===================
    if(NavApp::getMainUi()->actionProfileShowVerticalTrack->isChecked())
      paintVerticalPath(painter, route);

    float acx = distanceX(aircraftDistanceFromStart);
    float acy = altitudeY(aircraftAlt(userAircraft));

    // Draw aircraft symbol =======================
    int acsize = roundToInt(optionData.getDisplayTextSizeFlightplanProfile() / 100. * 40.);
    painter.translate(acx, acy);
    painter.rotate(90);
    painter.scale(0.6, 1.);
    painter.shear(0.0, 0.5);

    // Turn aircraft if distance shrinks
    if(movingBackwards)
      // Reflection is a special case of scaling matrix
      painter.scale(1., -1.);

    const QPixmap *pixmap = NavApp::getVehicleIcons()->pixmapFromCache(userAircraft, acsize, 0);
    painter.drawPixmap(QPointF(-acsize / 2., -acsize / 2.), *pixmap);
    painter.resetTransform();

    // Draw aircraft label
    mapcolors::scaleFont(&painter, optionData.getDisplayTextSizeFlightplanProfile() / 100.f, &painter.font());

    // Draw optional aircraft labels =======================
    QStringList texts;

    // Actual altitude
    if(displayOptions.testFlag(optsp::PROFILE_AIRCRAFT_ALTITUDE))
      texts.append(Unit::altFeet(aircraftAlt(userAircraft)));

    // Actual vertical speed
    if(displayOptions.testFlag(optsp::PROFILE_AIRCRAFT_VERT_SPEED))
    {
      int vspeed = roundToInt(userAircraft.getVerticalSpeedFeetPerMin());
      if(vspeed > 10.f || vspeed < -10.f)
      {
        QString upDown;
        if(vspeed > 100.f)
          upDown = tr(" ▲");
        else if(vspeed < -100.f)
          upDown = tr(" ▼");
        texts.append(Unit::speedVertFpm(vspeed) % upDown);
      }
    }

    // Needed vertical speed to catch next calculated altitude
    if(displayOptions.testFlag(optsp::PROFILE_AIRCRAFT_VERT_ANGLE_NEXT))
    {
      const Route& origRoute = NavApp::getRouteConst();
      // The corrected leg will point to an approach leg if we head to the start of a procedure
      int activeLegIdx = origRoute.getActiveLegIndexCorrected();
      float nextLegDistance = 0.f;

      if(activeLegIdx != map::INVALID_INDEX_VALUE && origRoute.getRouteDistances(nullptr, nullptr, &nextLegDistance, nullptr))
      {
        float vertAngleToNext = origRoute.getVerticalAngleToNext(nextLegDistance);
        if(vertAngleToNext < map::INVALID_ANGLE_VALUE)
          texts.append(Unit::speedVertFpm(-atools::geo::descentSpeedForPathAngle(userAircraft.getGroundSpeedKts(),
                                                                                 vertAngleToNext)) % tr(" ▼ N"));
      }
    }

    textatt::TextAttributes att = textatt::NONE;
    float textx = acx, texty = acy + 20.f;

    QRectF rect = symPainter.textBoxSize(&painter, texts, att);
    if(textx + rect.right() > left + w)
      // Move text to the left when approaching the right corner
      att |= textatt::LEFT;

    att |= textatt::ROUTE_BG_COLOR;

    if(acy - rect.height() > scrollArea->getOffset().y() + TOP)
      texty -= static_cast<float>(rect.bottom() + 20.); // Text at top

    symPainter.textBoxF(&painter, texts, QPen(Qt::black), textx, texty, att, 255);
  }

  // Dim the map by drawing a semi-transparent black rectangle
  mapcolors::darkenPainterRect(painter);

  scrollArea->updateLabelWidgets();
}

int ProfileWidget::calcLegScreenWidth(const QVector<QPolygon>& altLegs, int waypointIndex)
{
  QPolygon legWidth = altLegs.value(waypointIndex + 1);
  int legScreenWidth = legWidth.isEmpty() ? 0 : legWidth.constLast().x() - legWidth.constFirst().x();

  // Check if this is a zero-length FAF or other leg
  if(legScreenWidth == 0)
  {
    legWidth = altLegs.value(waypointIndex + 2);
    legScreenWidth = legWidth.isEmpty() ? 200 : legWidth.constLast().x() - legWidth.constFirst().x();
  }
  return legScreenWidth;
}

QStringList ProfileWidget::textsAndColorForLeg(QColor& color, bool& procSymbol, const RouteLeg& leg, bool procedureDisplayText,
                                               int legWidth)
{
  map::MapTypes type = leg.getMapType();
  QString ident;
  color = mapcolors::routeUserPointColor;

  if(type == map::WAYPOINT || leg.getWaypoint().isValid())
  {
    ident = leg.getIdent();
    color = mapcolors::waypointSymbolColor;
  }
  else if(type == map::VOR || leg.getVor().isValid())
  {
    ident = leg.getIdent();
    color = mapcolors::vorSymbolColor;
  }
  else if(type == map::NDB || leg.getNdb().isValid())
  {
    ident = leg.getIdent();
    color = mapcolors::ndbSymbolColor;
  }
  else if(type == map::USERPOINTROUTE)
  {
    ident = leg.getIdent();
    color = mapcolors::routeUserPointColor;
  }
  else if(type == map::INVALID)
    ident = leg.getIdent();
  else if(type == map::PROCEDURE && !leg.getProcedureLeg().fixIdent.isEmpty())
    // Custom approach or departure
    ident = leg.getIdent();

  if(leg.isAnyProcedure())
  {
    if(leg.getProcedureLeg().noIdentDisplay() || !proc::procedureLegDrawIdent(leg.getProcedureLegType()))
      color = mapcolors::routeUserPointColor;

    procSymbol = !proc::procedureLegDrawIdent(leg.getProcedureLegType());
  }
  else
    procSymbol = false;

  // Start with ident ===============
  QStringList texts;
  if(!leg.isAnyProcedure() || (!leg.getProcedureLeg().noIdentDisplay() && proc::procedureLegDrawIdent(leg.getProcedureLegType())))
    texts.append(atools::elideTextShort(ident, 10));

  // Display texts ===============
  if(leg.getProcedureLegType() != proc::START_OF_PROCEDURE && procedureDisplayText)
    texts.append(leg.getProcedureLeg().displayText);

  if(leg.isAnyProcedure() && !leg.getRunwayEnd().isValid())
  {
    // Add constrains if enabled in options ======================
    const optsp::DisplayOptionsProfile opts = profileOptions->getDisplayOptions();

    // Do not add altitude for runways since this is given for departure and destination airport already
    if(!leg.getProcedureLegAltRestr().isIls() && opts.testFlag(optsp::PROFILE_FP_ALT_RESTRICTION))
      texts.append(proc::altRestrictionTextNarrow(leg.getProcedureLegAltRestr()));

    if(opts.testFlag(optsp::PROFILE_FP_SPEED_RESTRICTION))
      texts.append(proc::speedRestrictionTextNarrow(leg.getProcedureLegSpeedRestr()));
  }

  // Elide all texts in the list
  texts = atools::elidedTexts(fontMetrics(), texts, Qt::ElideRight, legWidth);
  texts.removeAll(QString());
  texts.removeDuplicates();

  return texts;
}

/* Update signal from Marble elevation model */
void ProfileWidget::elevationUpdateAvailable()
{
  if(databaseLoadStatus)
    return;

  // Do not terminate thread here since this can lead to starving updates

  // Start thread after long delay to calculate new data
  // Calls ProfileWidget::updateTimeout()
  updateTimer->start(NavApp::isGlobeOfflineProvider() ?
                     ELEVATION_CHANGE_OFFLINE_UPDATE_TIMEOUT_MS : ELEVATION_CHANGE_ONLINE_UPDATE_TIMEOUT_MS);
}

void ProfileWidget::routeAltitudeChanged(int altitudeFeet)
{
  Q_UNUSED(altitudeFeet)

  if(databaseLoadStatus)
    return;

  routeChanged(true, false);
}

void ProfileWidget::aircraftPerformanceChanged(const atools::fs::perf::AircraftPerf *)
{
  routeChanged(true, false);
}

const Route& ProfileWidget::getRoute() const
{
  return legList->route;
}

void ProfileWidget::windUpdated()
{
  routeChanged(true, false);
}

void ProfileWidget::routeChanged(bool geometryChanged, bool newFlightPlan)
{
  if(databaseLoadStatus)
    return;

  showIlsChanged();
  scrollArea->routeChanged(geometryChanged);

  if(newFlightPlan)
    scrollArea->expandWidget();

  if(geometryChanged)
  {
    // Start thread after short delay to calculate new data
    // Calls ProfileWidget::updateTimeout()
    updateTimer->start(NavApp::isGlobeOfflineProvider() ? ROUTE_CHANGE_OFFLINE_UPDATE_TIMEOUT_MS : ROUTE_CHANGE_UPDATE_TIMEOUT_MS);
  }
  else
    update();

  updateErrorLabel();
  updateHeaderLabel();
}

/* Called by updateTimer after any route or elevation updates and starts the thread */
void ProfileWidget::updateTimeout()
{
  if(databaseLoadStatus)
    return;

  // Terminate and wait for thread
  terminateThread();
  terminateThreadSignal = false;

  // Need a copy of the leg list before starting thread to avoid synchronization problems
  // Start the computation in background
  ElevationLegList legs;
  legs.route = NavApp::getRouteConst();
  legs.route.updateApproachIls();

  // Start thread
  future = QtConcurrent::run(this, &ProfileWidget::fetchRouteElevationsThread, legs);

  // Watcher will call ProfileWidget::updateThreadFinished() when finished
  watcher.setFuture(future);
}

/* Called by watcher when the thread is finished */
void ProfileWidget::updateThreadFinished()
{
  if(databaseLoadStatus)
    return;

  if(!terminateThreadSignal)
  {
    // Was not terminated in the middle of calculations - get result from the future
    *legList = future.result();
    updateScreenCoords();
    updateErrorLabel();
    updateHeaderLabel();
    update();
    updateTooltip();

    // Update scroll bars
    scrollArea->routeChanged(true);

    // Update flight plan table and others
    emit profileAltCalculationFinished();
  }
}

/* Get elevation points between the two points. This returns also correct results if the antimeridian is crossed
 * @return true if not aborted */
bool ProfileWidget::fetchRouteElevations(atools::geo::LineString& elevations, const atools::geo::LineString& geometry) const
{
  ElevationProvider *elevationProvider = NavApp::getElevationProvider();

  if(elevationProvider->isValid())
  {
    for(int i = 0; i < geometry.size() - 1; i++)
    {
      // Create a line string from the two points and split it at the date line if crossing
      GeoDataLineString coords;
      coords.setTessellate(true);
      coords << GeoDataCoordinates(geometry.at(i).getLonX(), geometry.at(i).getLatY(), 0., GeoDataCoordinates::Degree)
             << GeoDataCoordinates(geometry.at(i + 1).getLonX(), geometry.at(i + 1).getLatY(), 0., GeoDataCoordinates::Degree);

      const QVector<Marble::GeoDataLineString *> coordsCorrected = coords.toDateLineCorrected();
      for(const Marble::GeoDataLineString *ls : coordsCorrected)
      {
        for(int j = 1; j < ls->size(); j++)
        {
          if(terminateThreadSignal)
            return false;

          const Marble::GeoDataCoordinates& c1 = ls->at(j - 1);
          const Marble::GeoDataCoordinates& c2 = ls->at(j);
          Pos p1(c1.longitude(), c1.latitude());
          Pos p2(c2.longitude(), c2.latitude());

          p1.toDeg();
          p2.toDeg();
          elevationProvider->getElevations(elevations, atools::geo::Line(p1, p2), atools::geo::nmToMeter(ELEVATION_SAMPLE_RADIUS_NM));
        }
      }
      qDeleteAll(coordsCorrected);
    }

    if(!elevations.isEmpty())
    {
      // Add start or end point if heightProfile omitted these - check only lat lon not alt
      if(!elevations.constFirst().almostEqual(geometry.constFirst()))
        elevations.prepend(geometry.constFirst().alt(elevations.constFirst().getAltitude()));

      if(!elevations.constLast().almostEqual(geometry.constLast()))
        elevations.append(geometry.constLast().alt(elevations.constLast().getAltitude()));
    }
  }

  // Add two null elevation dummy points if provider does not return any values
  if(elevations.isEmpty())
  {
    elevations.append(geometry.constFirst().alt(0.f));
    elevations.append(geometry.constLast().alt(0.f));
  }

  return true;
}

/* Background thread. Fetches elevation points from Marble elevation model and updates totals. */
ElevationLegList ProfileWidget::fetchRouteElevationsThread(ElevationLegList legs) const
{
  QThread::currentThread()->setPriority(QThread::LowestPriority);
  // qDebug() << "priority" << QThread::currentThread()->priority();

  using atools::geo::meterToNm;
  using atools::geo::nmToMeter;
  using atools::geo::meterToFeet;

  legs.totalNumPoints = 0;
  legs.totalDistance = 0.f;
  legs.maxElevationFt = 0.f;
  legs.elevationLegs.clear();

  if(legs.route.getSizeWithoutAlternates() <= 1)
    // Return empty result
    return ElevationLegList();

  if(legs.route.getAltitudeLegs().isEmpty())
    // Return empty result
    return ElevationLegList();

  // Total calculated distance across all legs
  double totalDistanceNm = 0.;

  // Loop over all route legs - first is departure airport point
  for(int i = 1; i <= legs.route.getDestinationLegIndex(); i++)
  {
    if(terminateThreadSignal)
      // Return empty result
      return ElevationLegList();

    const RouteAltitudeLeg& altLeg = legs.route.getAltitudeLegAt(i);
    if(altLeg.isMissed() || altLeg.isAlternate())
      break;

    ElevationLeg leg;
    leg.ident = altLeg.getIdent();

    // Used to adapt distances of all legs to total distance due to inaccuracies
    double scale = 1.;

    // Skip for too long segments when using the marble online provider
    if(altLeg.getDistanceTo() < ELEVATION_MAX_LEG_NM || NavApp::isGlobeOfflineProvider())
    {
      LineString geometry = altLeg.getGeoLineString();

      geometry.removeInvalid();
      if(geometry.size() == 1)
        geometry.append(geometry.constFirst());

      // Includes first and last point
      LineString elevations;
      if(!fetchRouteElevations(elevations, geometry))
        return ElevationLegList();

      if(elevations.isEmpty())
        return ElevationLegList();

      // elevations.removeDuplicates();
#ifdef DEBUG_INFORMATION_PROFILE
      qDebug() << Q_FUNC_INFO << "elevations" << elevations << atools::geo::meterToNm(elevations.lengthMeter());
      qDebug() << Q_FUNC_INFO << "geometry" << geometry << atools::geo::meterToNm(geometry.lengthMeter());
#endif
      leg.geometry = geometry;

      double distNm = totalDistanceNm;
      // Loop over all elevation points for the current leg
      Pos lastPos;
      for(int j = 0; j < elevations.size(); j++)
      {
        if(terminateThreadSignal)
          return ElevationLegList();

        Pos& coord = elevations[j];
        float altFeet = meterToFeet(coord.getAltitude());
        coord.setAltitude(altFeet);

        // Adjust maximum
        if(altFeet > leg.maxElevation)
          leg.maxElevation = altFeet;
        if(altFeet > legs.maxElevationFt)
          legs.maxElevationFt = altFeet;

        if(j > 0)
          // Update total distance
          distNm += meterToNm(lastPos.distanceMeterToDouble(coord));

        // Coordinate with elevation
        leg.elevation.append(coord);

        // Distance to elevation point from departure
        leg.distances.append(distNm);

        legs.totalNumPoints++;
        lastPos = coord;
      }

      // float distanceTo = atools::geo::meterToNm(geometry.lengthMeter());
      float distanceTo = altLeg.getDistanceTo();
      totalDistanceNm += distanceTo;

      if(!leg.distances.isEmpty())
      {
        double lastDist = leg.distances.constLast();
        if(atools::almostNotEqual(lastDist, totalDistanceNm))
          // Accumulated distance is different from route total distance - adjust
          // This can happen with the online elevation provider which does not return all points exactly on the leg
          scale = totalDistanceNm / leg.distances.constLast();
      }
    }
    else
    {
      // Skip long segment
      leg.distances.append(totalDistanceNm);
      totalDistanceNm += atools::geo::meterToNm(altLeg.getGeoLineString().lengthMeter());
      leg.distances.append(totalDistanceNm);
      leg.elevation = altLeg.getGeoLineString();
      leg.elevation.setAltitude(0.f);
      leg.geometry = altLeg.getGeoLineString();
    }

    // Apply correction starting with factor 1 for first and "scale" for last =====================
    if(!leg.distances.isEmpty() && atools::almostNotEqual(scale, 1.))
    {
      double firstDist = leg.distances.constFirst();
      double lastDist = leg.distances.constLast();

      for(double& curDist : leg.distances)
        curDist *= atools::interpolate(1., scale, firstDist, lastDist, curDist);
    }

    legs.elevationLegs.append(leg);
  }

  legs.totalDistance = static_cast<float>(totalDistanceNm);
  return legs;
}

void ProfileWidget::showEvent(QShowEvent *)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  if(size().isNull())
    resize(ui->scrollAreaProfile->viewport()->size());

  widgetVisible = true;
}

void ProfileWidget::hideEvent(QHideEvent *)
{
  // Stop all updates
  widgetVisible = false;
}

atools::geo::Pos ProfileWidget::calculatePos(int x)
{
  atools::geo::Pos pos;
  int indexDummy;
  float distDummy, distToGoDummy, altDummy, maxElevDummy;
  calculateDistancesAndPos(x, pos, indexDummy, distDummy, distToGoDummy, altDummy, maxElevDummy);
  return pos;
}

void ProfileWidget::calculateDistancesAndPos(int x, atools::geo::Pos& pos, int& routeIndex, float& distance,
                                             float& distanceToGo, float& groundElevation, float& maxElev)
{
  // Get index for leg
  routeIndex = 0;

  if(x > waypointX.constLast())
    x = waypointX.constLast();

#ifdef DEBUG_INFORMATION_PROFILE
  qDebug() << Q_FUNC_INFO << waypointX;
#endif

  // Returns an iterator pointing to the first element in the range [first, last) that
  // is not less than (i.e. greater or equal to) value, or last if no such element is found.
  auto it = std::lower_bound(waypointX.constBegin(), waypointX.constEnd(), x);
  if(it != waypointX.constEnd())
  {
    routeIndex = static_cast<int>(std::distance(waypointX.constBegin(), it)) - 1;
    if(routeIndex < 0)
      routeIndex = 0;
  }
  const ElevationLeg& leg = legList->elevationLegs.at(routeIndex);

  // Calculate distance from screen coordinates
  distance = (x - left) / horizontalScale;
  distance = atools::minmax(0.f, legList->totalDistance, distance);
  distanceToGo = legList->totalDistance - distance;

  // Get distance value index for lower and upper bound at cursor position
  int indexLowDist = 0;
  QVector<double>::const_iterator lowDistIt = std::lower_bound(leg.distances.begin(), leg.distances.end(), distance);
  if(lowDistIt != leg.distances.end())
  {
    indexLowDist = static_cast<int>(std::distance(leg.distances.begin(), lowDistIt));
    if(indexLowDist < 0)
      indexLowDist = 0;
  }
  int indexUpperDist = 0;
  QVector<double>::const_iterator upperDistIt = std::upper_bound(leg.distances.begin(), leg.distances.end(), distance);
  if(upperDistIt != leg.distances.end())
  {
    indexUpperDist = static_cast<int>(std::distance(leg.distances.begin(), upperDistIt));
    if(indexUpperDist < 0)
      indexUpperDist = 0;
  }

  // Get altitude values before and after cursor and interpolate - gives 0 if index is not valid
  float alt1 = leg.elevation.value(indexLowDist, atools::geo::EMPTY_POS).getAltitude();
  float alt2 = leg.elevation.value(indexUpperDist, atools::geo::EMPTY_POS).getAltitude();
  groundElevation = (alt1 + alt2) / 2.f;

  // Calculate min altitude for this leg
  maxElev = calcGroundBufferFt(leg.maxElevation);

  // Get Position for highlight on map
  float legdistpart = distance - static_cast<float>(leg.distances.constFirst());
  float legdist = static_cast<float>(leg.distances.constLast() - leg.distances.constFirst());

  // Calculate position along the flight plan
  pos = leg.geometry.interpolate(legdistpart / legdist);
}

void ProfileWidget::mouseMoveEvent(QMouseEvent *mouseEvent)
{
  if(!widgetVisible || legList->elevationLegs.isEmpty() || legList->route.isEmpty())
    return;

  if(rubberBand == nullptr)
    // Create rubber band line
    rubberBand = new QRubberBand(QRubberBand::Line, this);

  // Limit x to drawing region
  int x = mouseEvent->pos().x();
  x = std::max(x, left);
  x = std::min(x, rect().width() - left);

  rubberBand->setGeometry(x - 1, 0, 2, rect().height());
  rubberBand->show();

  buildTooltip(x, false /* force */);
  lastTooltipScreenPos = mouseEvent->globalPos();
  // Show tooltip
  if(!lastTooltipString.isEmpty())
    scrollArea->showTooltip(lastTooltipScreenPos, lastTooltipString);

  // Allow event to propagate to scroll widget
  mouseEvent->ignore();

  // Follow cursor on map if enabled ==========================
  if(NavApp::getMainUi()->actionProfileFollow->isChecked())
  {
    atools::geo::Pos pos = calculatePos(x);
    if(pos.isValid())
      emit showPos(pos, map::INVALID_DISTANCE_VALUE, false);
  }

  // Tell map widget to create a highlight on the map
  if(profileOptions->getDisplayOptions().testFlag(optsp::PROFILE_HIGHLIGHT))
    emit highlightProfilePoint(lastTooltipPos);
}

void ProfileWidget::updateTooltip()
{
  if(scrollArea->isTooltipVisible())
  {
    buildTooltip(lastTooltipX, true /* force */);
    if(!lastTooltipString.isEmpty())
      scrollArea->showTooltip(lastTooltipScreenPos, lastTooltipString);
  }
}

void ProfileWidget::buildTooltip(int x, bool force)
{
  // Nothing to show label =========================
  if(!hasValidRouteForDisplay())
    return;

  optsp::DisplayOptionsProfile displayOpts = profileOptions->getDisplayOptions();

  if(!displayOpts.testFlag(optsp::PROFILE_TOOLTIP) && !displayOpts.testFlag(optsp::PROFILE_HIGHLIGHT))
    // Neither tooltip nor highlight selected
    return;

  if(atools::almostEqual(lastTooltipX, x, 3) && !force)
    // Almost same position - do not update except if forced
    return;
  else
    lastTooltipX = x;

  // Get from/to text
  // QString fromWaypoint = atools::elideTextShort(  legList->route.value(index).getIdent(), 20);

  QString fromTo(tr("to"));

  // Fetch all parameters =======================
  int index;
  float distance, distanceToGo, groundElevation, maxElev;
  calculateDistancesAndPos(x, lastTooltipPos, index, distance, distanceToGo, groundElevation, maxElev);

  if(!displayOpts.testFlag(optsp::PROFILE_TOOLTIP))
    // No tooltip but highlight selected. hightlight needs calculateDistancesAndPos() and lastTooltipPos
    return;

#ifdef DEBUG_INFORMATION_PROFILE
  qDebug() << Q_FUNC_INFO << "x" << x << "lastTooltipPos" << lastTooltipPos << "index" << index << "distance" << distance
           << "distanceToGo" << distanceToGo << "groundElevation" << groundElevation << "maxElev" << maxElev;
#endif

  const RouteLeg& routeLeg = legList->route.value(index + 1);
  if(routeLeg.isAnyProcedure() && proc::procedureLegFrom(routeLeg.getProcedureLegType()))
    fromTo = tr("from");

  QString toWaypoint = atools::elideTextShort(routeLeg.getDisplayIdent(), 20);

  // Create text for tooltip ==========================
  atools::util::HtmlBuilder html;

  // Header from and to distance, altitude and next waypoint ============
  float altitude = legList->route.getAltitudeForDistance(distanceToGo);
  const RouteLeg *leg = index < legList->route.size() - 1 ? &legList->route.value(index + 1) : nullptr;
  WindReporter *windReporter = NavApp::getWindReporter();
  atools::grib::Wind wind;
  if(windReporter->hasAnyWindData() && leg != nullptr)
    wind = windReporter->getWindForPosRoute(lastTooltipPos.alt(altitude));

  html.p(atools::util::html::NOBR_WHITESPACE);
  html.b(Unit::distNm(distance, false) + tr(" ► ") + Unit::distNm(distanceToGo));
#ifdef DEBUG_INFORMATION_PROFILE
  html.br().b("[" + QString::number(distance) + tr(" ► ") + QString::number(distanceToGo) + "]");
#endif
  if(altitude < map::INVALID_ALTITUDE_VALUE)
    html.b(tr(", %1, %2 %3").arg(Unit::altFeet(altitude)).arg(fromTo).arg(toWaypoint));

  // Course ========================================
  if(routeLeg.getCourseStartMag() < map::INVALID_COURSE_VALUE)
  {
    html.br().b(tr("Course: ")).
    text(formatter::courseTextFromMag(routeLeg.getCourseStartMag(), routeLeg.getMagvarStart(), false /* magBold */),
         atools::util::html::NO_ENTITIES);

    // Crab angle / heading ========================================
    if(wind.isValid() && !wind.isNull())
    {
      float tas = legList->route.getSpeedForDistance(distanceToGo);
      if(tas < map::INVALID_SPEED_VALUE)
      {
#ifdef DEBUG_INFORMATION_PROFILE
        html.brText(QString("[TAS %1 kts, %2°T %3 kts]").
                    arg(tas, 0, 'f', 0).arg(wind.dir, 0, 'f', 0).arg(wind.speed, 0, 'f', 0));
#endif

        float headingTrue = atools::geo::windCorrectedHeading(wind.speed, wind.dir, routeLeg.getCourseEndTrue(), tas);
        if(headingTrue < map::INVALID_COURSE_VALUE && atools::almostNotEqual(headingTrue, routeLeg.getCourseEndTrue(), 1.f))
          html.br().b(tr("Heading: ", "Aircraft heading")).
          text(formatter::courseTextFromTrue(headingTrue, routeLeg.getMagvarEnd()), atools::util::html::NO_ENTITIES);
      }
    }
  }

  // Vertical angle ===============================================================
  bool required = false;
  float verticalAngle = legList->route.getVerticalAngleAtDistance(distanceToGo, &required);

  if(verticalAngle < -0.5f)
    html.br().b(required ? tr("Required Flight Path Angle: ") : tr("Flight path angle: ")).
    text(tr("%L1 °").arg(verticalAngle, 0, 'g', required ? 3 : 2));

  // Ground ===============================================================
  QStringList groundText;
  if(groundElevation < map::INVALID_ALTITUDE_VALUE)
    groundText.append(Unit::altFeet(groundElevation));

  if(altitude < map::INVALID_ALTITUDE_VALUE && groundElevation < map::INVALID_ALTITUDE_VALUE)
  {
    float aboveGround = map::INVALID_ALTITUDE_VALUE;
    aboveGround = std::max(0.f, altitude - groundElevation);
    if(aboveGround < map::INVALID_ALTITUDE_VALUE)
      groundText.append(tr("%1 above").arg(Unit::altFeet(aboveGround)));
  }

  if(!groundText.isEmpty())
    html.br().b(tr("Ground: ")).text(atools::strJoin(groundText, tr(", ")));

  // Safe altitude ===============================================================
  if(maxElev < map::INVALID_ALTITUDE_VALUE)
    html.br().b(tr("Leg Safe Altitude: ")).text(Unit::altFeet(maxElev));

  // Show wind at altitude ===============================================
  if(wind.isValid() && !wind.isNull())
  {
    float headWind = 0.f, crossWind = 0.f;
    atools::geo::windForCourse(headWind, crossWind, wind.speed, wind.dir, leg->getCourseEndTrue());

    float magVar = NavApp::getMagVar(lastTooltipPos);

    QString windText = tr("%1°M, %2").
                       arg(atools::geo::normalizeCourse(wind.dir - magVar), 0, 'f', 0).
                       arg(Unit::speedKts(wind.speed));
    html.br().b(tr("%1Wind: ").arg(windReporter->isWindManual() ? tr("Manual ") : QString())).text(windText);

    // Add tail/headwind if sufficient =======================================
    if(std::abs(headWind) >= 1.f)
    {
      QString windPtr;
      if(headWind >= 1.f)
        windPtr = tr("▼");
      else if(headWind <= -1.f)
        windPtr = tr("▲");
      html.text(tr(", %1 %2").arg(windPtr).arg(Unit::speedKts(std::abs(headWind))));
    }
  }

#ifdef DEBUG_INFORMATION
  html.br().b(required ? "[required angle" : "[angle").text(tr(" %L1 °]").arg(std::abs(verticalAngle), 0, 'g', 6));
#endif

#ifdef DEBUG_INFORMATION_PROFILE
  using namespace formatter;
  using namespace map;

  FuelTimeResult result;
  NavApp::getAircraftPerfController()->calculateFuelAndTimeTo(result, distanceToGo, INVALID_DISTANCE_VALUE,
                                                              index + 1);

  html.append(QString("<br/><code>[alt %1,idx %2, crs %3, "
                        "fuel dest %4/%5, fuel TOD %6/%7, "
                        "time dest %8 (%9), time TOD %10 (%11)]</code>").
              arg(NavApp::getRoute().getAltitudeForDistance(distanceToGo)).
              arg(index).
              arg(leg != nullptr ? QString::number(leg->getCourseToTrue()) : "-").
              arg(result.fuelLbsToDest, 0, 'f', 2).
              arg(result.fuelGalToDest, 0, 'f', 2).
              arg(result.fuelLbsToTod < INVALID_WEIGHT_VALUE ? result.fuelLbsToTod : -1., 0, 'f', 2).
              arg(result.fuelGalToTod < INVALID_VOLUME_VALUE ? result.fuelGalToTod : -1., 0, 'f', 2).
              arg(result.timeToDest, 0, 'f', 2).
              arg(formatMinutesHours(result.timeToDest)).
              arg(result.timeToTod < map::INVALID_TIME_VALUE ? result.timeToTod : -1., 0, 'f', 2).
              arg(result.timeToTod < INVALID_TIME_VALUE ? formatMinutesHours(result.timeToTod) : "-1"));
#endif
  html.pEnd(); // html.p(atools::util::html::NOBR_WHITESPACE);
  lastTooltipString = html.getHtml();
}

void ProfileWidget::contextMenuEvent(QContextMenuEvent *event)
{
  QPoint globalpoint;
  if(event->reason() == QContextMenuEvent::Keyboard)
    // Event does not contain position is triggered by keyboard
    globalpoint = QCursor::pos();
  else
    globalpoint = event->globalPos();

  showContextMenu(globalpoint);
}

void ProfileWidget::showContextMenu(const QPoint& globalPoint)
{
  qDebug() << Q_FUNC_INFO << globalPoint;
  contextMenuActive = true;

  // Position on the whole map widget
  QPoint mapPoint = mapFromGlobal(globalPoint);

  // Local visible widget position
  QPoint pointArea = scrollArea->getScrollArea()->mapFromGlobal(globalPoint);
  QRect rectArea = scrollArea->getScrollArea()->rect();

  // Local visible widget position
  QPoint pointLabelVert = scrollArea->getLabelWidgetVert()->mapFromGlobal(globalPoint);
  QRect rectLabelVert = scrollArea->getLabelWidgetVert()->rect();
  QPoint pointLabelHoriz = scrollArea->getLabelWidgetHoriz()->mapFromGlobal(globalPoint);
  QRect rectLabelHoriz = scrollArea->getLabelWidgetHoriz()->rect();

  bool hasPosition = mapPoint.x() > left && mapPoint.x() < width() - left && rectArea.contains(pointArea);

  // Do not show context menu if point is neither on the visible widget and not on the label
  QPoint menuPos = globalPoint;
  if(!rectArea.contains(pointArea) && !rectLabelVert.contains(pointLabelVert) && !rectLabelHoriz.contains(pointLabelHoriz))
    menuPos = scrollArea->getScrollArea()->mapToGlobal(rectArea.center());

  // Move menu position off the cursor to avoid accidental selection on touchpads
  menuPos += QPoint(3, 3);

  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->actionProfileShowOnMap->setEnabled(hasPosition && hasValidRouteForDisplay());
  ui->actionProfileExpand->setEnabled(hasValidRouteForDisplay());

  ui->actionProfileCenterAircraft->setEnabled(NavApp::isConnectedAndAircraft());
  // Zoom to aircraft and destination is only enabled if center is checked
  ui->actionProfileZoomAircraft->setEnabled(NavApp::isConnectedAndAircraft() && ui->actionProfileCenterAircraft->isChecked());
  ui->actionProfileDeleteAircraftTrack->setEnabled(hasTrailPoints());

  ui->actionProfileShowVasi->setEnabled(hasValidRouteForDisplay());
  ui->actionProfileShowIls->setEnabled(hasValidRouteForDisplay());
  ui->actionProfileShowVerticalTrack->setEnabled(hasValidRouteForDisplay());

  ui->actionProfileFollow->setEnabled(hasValidRouteForDisplay());

  QMenu menu;
  menu.setToolTipsVisible(NavApp::isMenuToolTipsVisible());
  menu.addAction(ui->actionProfileShowOnMap);
  menu.addSeparator();
  menu.addAction(ui->actionProfileExpand);
  menu.addSeparator();
  menu.addAction(ui->actionProfileCenterAircraft);
  menu.addAction(ui->actionProfileZoomAircraft);
  menu.addAction(ui->actionProfileDeleteAircraftTrack);
  menu.addSeparator();
  menu.addAction(ui->actionProfileShowVasi);
  menu.addAction(ui->actionProfileShowIls);
  menu.addAction(ui->actionMapShowTocTod); // Global option also used in other menus
  menu.addAction(ui->actionProfileShowVerticalTrack);
  menu.addSeparator();
  menu.addAction(ui->actionProfileFollow);
  menu.addSeparator();
  menu.addAction(ui->actionProfileShowZoom);
  menu.addAction(ui->actionProfileShowScrollbars);
  menu.addSeparator();
  menu.addAction(ui->actionProfileDisplayOptions);

  QAction *action = menu.exec(menuPos);

  if(action == ui->actionProfileShowOnMap)
  {
    atools::geo::Pos pos = calculatePos(mapPoint.x());
    if(pos.isValid())
      emit showPos(pos, 0.f, false);
  }
  else if(action == ui->actionProfileCenterAircraft || action == ui->actionProfileZoomAircraft || action == ui->actionProfileFollow)
    scrollArea->update();
  else if(action == ui->actionProfileShowIls || action == ui->actionProfileShowVasi || action == ui->actionMapShowTocTod ||
          action == ui->actionProfileShowVerticalTrack)
    update();
  else if(action == ui->actionProfileDeleteAircraftTrack)
    deleteAircraftTrail();

  // Other actions are connected to methods or used during updates
  // else if(action == ui->actionProfileFit)
  // else if(action == ui->actionProfileExpand)
  // menu.addAction(ui->actionProfileShowZoom);
  // else if(action == ui->actionProfileShowLabels)
  // else if(action == ui->actionProfileShowScrollbars)

  contextMenuActive = false;
}

void ProfileWidget::updateHeaderLabel()
{
  optsp::DisplayOptionsProfile options = profileOptions->getDisplayOptions();
  QStringList text;

  if(options & optsp::PROFILE_HEADER_ANY)
  {
    float distFromStartNm = 0.f, distToDestNm = 0.f, nearestLegDistance = 0.f;
    const Route& route = NavApp::getRouteConst();
    if(simData.getUserAircraftConst().isValid())
    {
      bool timeToDestOpt = options.testFlag(optsp::PROFILE_HEADER_DIST_TIME_TO_DEST);

      if(route.getRouteDistances(&distFromStartNm, &distToDestNm, &nearestLegDistance))
      {
        if(route.isActiveMissed())
          distToDestNm = 0.f;

        if(route.isActiveAlternate())
        {
          // Show only alternate distance ==========================================
          if(timeToDestOpt)
            // Use distance to alternate instead of destination
            text.append(tr("<b>Alternate:</b> %1").arg(Unit::distNm(nearestLegDistance)));
        }
        else
        {
          bool vertDeviationOpt = options.testFlag(optsp::PROFILE_HEADER_DESCENT_PATH_DEVIATION);
          bool descentAngleOpt = options.testFlag(optsp::PROFILE_HEADER_DESCENT_PATH_ANGLE);
          bool timeToTodOpt = options.testFlag(optsp::PROFILE_HEADER_DIST_TIME_TO_TOD);

          if(NavApp::getMapWidgetGui()->getShownMapDisplayTypes().testFlag(map::FLIGHTPLAN_TOC_TOD) &&
             route.getTopOfDescentDistance() < map::INVALID_DISTANCE_VALUE)
          {
            // Fuel and time calculated or estimated
            FuelTimeResult fuelTime;
            NavApp::getAircraftPerfController()->calculateFuelAndTimeTo(fuelTime, distToDestNm, nearestLegDistance,
                                                                        route.getActiveLegIndex());

            // Time and distance to destination ==========================================
            if(timeToDestOpt)
              text.append(tr("<b>Destination:</b> %1 (%2)").
                          arg(Unit::distNm(distToDestNm)).
                          arg(formatter::formatMinutesHoursLong(fuelTime.timeToDest)));

            float toTod = route.getTopOfDescentDistance() - distFromStartNm;
            bool todAhead = toTod > 0.f;

            // Time and distance to TOD ==========================================
            if(timeToTodOpt && todAhead)
              text.append(tr("<b>Top of Descent:</b> %1%2").
                          arg(todAhead ? Unit::distNm(toTod) : tr("Passed")).
                          arg(todAhead ? tr(" (%1)").arg(formatter::formatMinutesHoursLong(fuelTime.timeToTod)) : QString()));

            // Descent angle and speed after TOD ==========================================
            if((vertDeviationOpt || descentAngleOpt) && !todAhead)
            {
              QString descentDeviationText, verticalAngleText;
              bool verticalRequired;
              route.getVerticalPathDeviationTexts(&descentDeviationText, &verticalAngleText, &verticalRequired, nullptr);

              if(vertDeviationOpt && !descentDeviationText.isEmpty())
                text.append(tr("<b>Vert. Path Deviation:</b> %1").arg(descentDeviationText));

              if(descentAngleOpt && !verticalAngleText.isEmpty())
              {
                QString vertText = verticalRequired ? tr("<b>Required Angle and Speed:</b> %1") : tr("<b>Angle and Speed:</b> %1");
                text.append(vertText.arg(verticalAngleText));
              }
            }
          }
          else if(timeToDestOpt)
            text.append(tr("<b>Destination:</b> %1").arg(Unit::distNm(distToDestNm)));
        }
      }
    }
  }

  NavApp::getMainUi()->labelProfileInfo->setVisible(!text.isEmpty());

  if(!text.isEmpty())
    NavApp::getMainUi()->labelProfileInfo->setText(text.join(tr(",&nbsp;&nbsp;&nbsp;", "Separator for profile header")));
}

/* Cursor leaves widget. Stop displaying the rubberband */
void ProfileWidget::leaveEvent(QEvent *)
{
  hideRubberBand();
  scrollArea->hideTooltip();
}

void ProfileWidget::hideRubberBand()
{
  if(!widgetVisible || legList->elevationLegs.isEmpty() || legList->route.isEmpty())
    return;

  delete rubberBand;
  rubberBand = nullptr;

  updateHeaderLabel();

  // Tell map widget to erase highlight
  emit highlightProfilePoint(atools::geo::EMPTY_POS);
}

/* Resizing needs an update of the screen coordinates */
void ProfileWidget::resizeEvent(QResizeEvent *)
{
  if(!insideResizeEvent)
  {
    // Recursive call might happen through:
    // ProfileWidget::updateScreenCoords
    // ProfileScrollArea::routeAltitudeChanged
    // ProfileScrollArea::updateWidgets
    // ProfileScrollArea::verticalZoomSliderValueChanged
    // ProfileScrollArea::scaleView

    insideResizeEvent = true;
    updateScreenCoords();
    insideResizeEvent = false;
  }
  else
    qWarning() << Q_FUNC_INFO << "Recursion";
}

/* Deleting aircraft track needs an update of the screen coordinates */
void ProfileWidget::deleteAircraftTrail()
{
  aircraftTrailPoints.clear();

  updateScreenCoords();
  update();
}

/* Stop thread */
void ProfileWidget::preDatabaseLoad()
{
  jumpBack->cancel();
  updateTimer->stop();
  scrollArea->hideTooltip();
  terminateThread();
  databaseLoadStatus = true;
}

/* Force new thread creation */
void ProfileWidget::postDatabaseLoad()
{
  databaseLoadStatus = false;
  scrollArea->hideTooltip();
  routeChanged(true /* geometry changed */, true /* new flight plan */);
}

void ProfileWidget::optionsChanged()
{
  jumpBack->cancel();
  scrollArea->optionsChanged();

  updateScreenCoords();
  updateErrorLabel();
  updateHeaderLabel();
  update();
}

void ProfileWidget::styleChanged()
{
  scrollArea->styleChanged();
}

void ProfileWidget::saveState()
{
  scrollArea->saveState();
  profileOptions->saveState();

  saveAircraftTrail();
}

void ProfileWidget::restoreState()
{
  profileOptions->restoreState();
  scrollArea->restoreState();

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_TRAIL && !NavApp::isSafeMode())
    loadAircraftTrail();
}

void ProfileWidget::restoreSplitter()
{
  scrollArea->restoreSplitter();
}

void ProfileWidget::preRouteCalc()
{
  updateTimer->stop();
  terminateThread();
}

void ProfileWidget::mainWindowShown()
{
  updateScreenCoords();
  scrollArea->routeChanged(true);
  scrollArea->expandWidget();
  active = true;
  update();
}

void ProfileWidget::updateProfileShowFeatures()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  // Compare own values with action values
  bool updateProfile = showAircraft != ui->actionMapShowAircraft->isChecked() ||
                       showAircraftTrail != ui->actionMapShowAircraftTrack->isChecked();

  showAircraft = ui->actionMapShowAircraft->isChecked();
  showAircraftTrail = ui->actionMapShowAircraftTrack->isChecked();

  if(updateProfile)
  {
    updateScreenCoords();
    update();
  }
}

void ProfileWidget::terminateThread()
{
  if(future.isRunning() || future.isStarted())
  {
    terminateThreadSignal = true;
    future.waitForFinished();
  }
}

void ProfileWidget::jumpBackToAircraftStart()
{
  // Zoom only enabled with center
  if(NavApp::getMainUi()->actionProfileCenterAircraft->isChecked())
    jumpBack->start();
}

void ProfileWidget::jumpBackToAircraftCancel()
{
  jumpBack->cancel();
}

void ProfileWidget::jumpBackToAircraftTimeout()
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  Ui::MainWindow *ui = NavApp::getMainUi();
  if(ui->actionProfileCenterAircraft->isChecked() && NavApp::isConnectedAndAircraft() &&
     OptionData::instance().getFlags2() & opts2::ROUTE_NO_FOLLOW_ON_MOVE && aircraftDistanceFromStart < map::INVALID_DISTANCE_VALUE)
  {
    if(QApplication::mouseButtons() & Qt::LeftButton || contextMenuActive)
      // Restart as long as menu is active or user is dragging around
      jumpBack->restart();
    else
    {
      jumpBack->cancel();

      if(ui->actionProfileCenterAircraft->isChecked())
        centerAircraft();
    }
  }
  else
    jumpBack->cancel();
}

void ProfileWidget::updateErrorLabel()
{
  NavApp::updateErrorLabel();
}

void ProfileWidget::saveAircraftTrail()
{
  QFile trailFile(atools::settings::Settings::getConfigFilename(lnm::PROFILE_TRACK_SUFFIX));

  if(trailFile.open(QIODevice::WriteOnly))
  {
    QDataStream out(&trailFile);
    out.setVersion(QDataStream::Qt_5_5);
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);
    out << FILE_MAGIC_NUMBER << FILE_VERSION << aircraftTrailPoints;
    trailFile.close();
  }
  else
    qWarning() << "Cannot write track" << trailFile.fileName() << ":" << trailFile.errorString();
}

void ProfileWidget::loadAircraftTrail()
{
  QFile trailFile(atools::settings::Settings::getConfigFilename(lnm::PROFILE_TRACK_SUFFIX));
  if(trailFile.exists())
  {
    if(trailFile.open(QIODevice::ReadOnly))
    {
      quint32 magic;
      quint16 version;
      QDataStream in(&trailFile);
      in.setVersion(QDataStream::Qt_5_5);
      in.setFloatingPointPrecision(QDataStream::SinglePrecision);
      in >> magic;

      if(magic == FILE_MAGIC_NUMBER)
      {
        in >> version;
        if(version == FILE_VERSION)
          in >> aircraftTrailPoints;
        else
          qWarning() << "Cannot read track" << trailFile.fileName() << ". Invalid version number:" << version;
      }
      else
        qWarning() << "Cannot read track" << trailFile.fileName() << ". Invalid magic number:" << magic;
      trailFile.close();
    }
    else
      qWarning() << "Cannot read track" << trailFile.fileName() << ":" << trailFile.errorString();
  }
}

void ProfileWidget::showDisplayOptions()
{
  if(profileOptions->showOptions())
    optionsChanged();
}
