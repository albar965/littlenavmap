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

#include "profile/profilewidget.h"

#include "atools.h"
#include "navapp.h"
#include "geo/calculations.h"
#include "common/mapcolors.h"
#include "profile/profilescrollarea.h"
#include "profile/profilelabelwidget.h"
#include "ui_mainwindow.h"
#include "common/symbolpainter.h"
#include "route/routecontroller.h"
#include "route/routealtitude.h"
#include "common/aircrafttrack.h"
#include "common/unit.h"
#include "mapgui/mapwidget.h"
#include "options/optiondata.h"
#include "common/elevationprovider.h"
#include "common/vehicleicons.h"

#include <QPainter>
#include <QTimer>
#include <QRubberBand>
#include <QMouseEvent>
#include <QtConcurrent/QtConcurrentRun>

#include <marble/ElevationModel.h>
#include <marble/GeoDataCoordinates.h>
#include <marble/GeoDataLineString.h>

/* Maximum delta values depending on update rate in options */
static QHash<opts::SimUpdateRate, ProfileWidget::SimUpdateDelta> SIM_UPDATE_DELTA_MAP(
{
  {
    opts::FAST, {1, 1.f}
  },
  {
    opts::MEDIUM, {2, 10.f}
  },
  {
    opts::LOW, {4, 100.f}
  }
});

using Marble::GeoDataCoordinates;
using Marble::GeoDataLineString;
using atools::geo::Pos;
using atools::geo::LineString;

ProfileWidget::ProfileWidget(QWidget *parent)
  : QWidget(parent)
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

  setContextMenuPolicy(Qt::DefaultContextMenu);
  setFocusPolicy(Qt::WheelFocus);

  scrollArea = new ProfileScrollArea(this, ui->scrollAreaProfile);
  scrollArea->setProfileLeftOffset(X0);
  scrollArea->setProfileTopOffset(Y0);

  connect(scrollArea, &ProfileScrollArea::showPosAlongFlightplan, this, &ProfileWidget::showPosAlongFlightplan);
  connect(scrollArea, &ProfileScrollArea::hideRubberBand, this, &ProfileWidget::hideRubberBand);

  routeController = NavApp::getRouteController();

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
  updateTimer->stop();
  terminateThread();
  delete scrollArea;
}

void ProfileWidget::aircraftTrackPruned()
{
  if(!widgetVisible)
    return;

  updateScreenCoords();
  update();
}

void ProfileWidget::simDataChanged(const atools::fs::sc::SimConnectData& simulatorData)
{
  if(!widgetVisible || databaseLoadStatus || !simulatorData.getUserAircraftConst().getPosition().isValid())
    return;

  bool updateWidget = false;

  if(!NavApp::getRouteConst().isFlightplanEmpty())
  {
    const Route& route = routeController->getRoute();

    if((showAircraft || showAircraftTrack))
    {
      if(!route.isPassedLastLeg() && !route.isActiveMissed())
      {
        simData = simulatorData;
        if(route.getRouteDistances(&aircraftDistanceFromStart, &aircraftDistanceToDest))
        {
          // Get screen point from last update
          QPoint lastPoint;
          if(lastSimData.getUserAircraftConst().getPosition().isValid())
            lastPoint = QPoint(X0 + static_cast<int>(aircraftDistanceFromStart * horizontalScale),
                               Y0 + static_cast<int>(rect().height() - Y0 -
                                                     lastSimData.getUserAircraftConst().getPosition().getAltitude()
                                                     * verticalScale));

          // Get screen point for current update
          QPoint currentPoint(X0 + static_cast<int>(aircraftDistanceFromStart *horizontalScale),
                              Y0 + static_cast<int>(rect().height() - Y0 -
                                                    simData.getUserAircraftConst().getPosition().getAltitude() *
                                                    verticalScale));

          if(aircraftTrackPoints.isEmpty() || (aircraftTrackPoints.last() - currentPoint).manhattanLength() > 3)
          {
            // Add track point and update widget if delta value between last and current update is large enough
            if(simData.getUserAircraftConst().getPosition().isValid())
            {
              aircraftTrackPoints.append(currentPoint);

              if(aircraftTrackPoints.boundingRect().width() > MIN_AIRCRAFT_TRACK_WIDTH)
                maxTrackAltitudeFt = std::max(maxTrackAltitudeFt,
                                              simData.getUserAircraftConst().getPosition().getAltitude());
              else
                maxTrackAltitudeFt = 0.f;

              // Center aircraft on scroll area
              if(NavApp::getMainUi()->actionProfileCenterAircraft->isChecked())
                scrollArea->centerAircraft(currentPoint);

              updateWidget = true;
            }
          }

          const SimUpdateDelta& deltas = SIM_UPDATE_DELTA_MAP.value(OptionData::instance().getSimUpdateRate());

          using atools::almostNotEqual;
          if(!lastSimData.getUserAircraftConst().getPosition().isValid() ||
             (lastPoint - currentPoint).manhattanLength() > deltas.manhattanLengthDelta ||
             almostNotEqual(lastSimData.getUserAircraftConst().getPosition().getAltitude(),
                            simData.getUserAircraftConst().getPosition().getAltitude(), deltas.altitudeDelta))
          {
            // Aircraft position has changed enough
            lastSimData = simData;
            if(simData.getUserAircraftConst().getPosition().getAltitude() > maxWindowAlt)
              // Scale up to keep the aircraft visible
              updateScreenCoords();
            updateWidget = true;
          }
        }
      }
    }
    else
    {
      // Neither aircraft nor track shown - update simulator data only
      bool valid = simData.getUserAircraftConst().getPosition().isValid();
      simData = atools::fs::sc::SimConnectData();
      if(valid)
        updateWidget = true;
    }
  }
  if(updateWidget)
    update();

  updateLabel();
}

void ProfileWidget::connectedToSimulator()
{
  qDebug() << Q_FUNC_INFO;
  simData = atools::fs::sc::SimConnectData();
  updateScreenCoords();
  update();
  updateLabel();
}

void ProfileWidget::disconnectedFromSimulator()
{
  qDebug() << Q_FUNC_INFO;
  simData = atools::fs::sc::SimConnectData();
  updateScreenCoords();
  update();
  updateLabel();
}

float ProfileWidget::calcGroundBuffer(float maxElevation)
{
  float groundBuffer = Unit::rev(OptionData::instance().getRouteGroundBuffer(), Unit::altFeetF);
  float roundBuffer = Unit::rev(500.f, Unit::altFeetF);

  return std::ceil((maxElevation + groundBuffer) / roundBuffer) * roundBuffer;
}

/* Check if the aircraft track is large enough on the screen to be shown and to alter the profile altitude */
bool ProfileWidget::aircraftTrackValid()
{
  if(!NavApp::getRouteConst().isFlightplanEmpty() && showAircraftTrack)
  {
    // Check if the track size is large enough to alter the maximum height
    int minTrackX = std::numeric_limits<int>::max(), maxTrackX = 0;
    if(!NavApp::getRouteConst().isFlightplanEmpty() && showAircraftTrack)
    {
      for(const at::AircraftTrackPos& trackPos : NavApp::getMapWidget()->getAircraftTrack())
      {
        float distFromStart = legList.route.getDistanceFromStart(trackPos.pos);
        if(distFromStart < map::INVALID_DISTANCE_VALUE)
        {
          int x = X0 + static_cast<int>(distFromStart * horizontalScale);
          minTrackX = std::min(x, minTrackX);
          maxTrackX = std::max(x, maxTrackX);
        }
      }
    }
    return maxTrackX - minTrackX > MIN_AIRCRAFT_TRACK_WIDTH;
  }
  return false;
}

void ProfileWidget::updateScreenCoords()
{
  /* Update all screen coordinates and scale factors */

  MapWidget *mapWidget = NavApp::getMapWidget();
  // Widget drawing region width and height
  int w = rect().width() - X0 * 2, h = rect().height() - Y0;

  // Need scale to determine track length on screen
  horizontalScale = w / legList.totalDistance;

  // Need to check this now to get the maximum elevation
  bool trackValid = aircraftTrackValid();

  if(trackValid)
    maxTrackAltitudeFt = mapWidget->getAircraftTrack().getMaxAltitude();
  else
    maxTrackAltitudeFt = 0.f;

  // Update elevation polygon
  // Add 1000 ft buffer and round up to the next 500 feet
  minSafeAltitudeFt = calcGroundBuffer(legList.maxElevationFt);
  flightplanAltFt = routeController->getRoute().getCruisingAltitudeFeet();
  maxWindowAlt = std::max(minSafeAltitudeFt, flightplanAltFt);

  if(simData.getUserAircraftConst().getPosition().isValid() &&
     (showAircraft || showAircraftTrack) && !NavApp::getRouteConst().isFlightplanEmpty())
    maxWindowAlt = std::max(maxWindowAlt, simData.getUserAircraftConst().getPosition().getAltitude());

  if(showAircraftTrack)
    maxWindowAlt = std::max(maxWindowAlt, maxTrackAltitudeFt);

  scrollArea->routeAltitudeChanged();

  verticalScale = h / maxWindowAlt;

  // Calculate the landmass polygon
  waypointX.clear();
  landPolygon.clear();

  // First point
  landPolygon.append(QPoint(X0, h + Y0));

  for(const ElevationLeg& leg : legList.elevationLegs)
  {
    waypointX.append(X0 + static_cast<int>(leg.distances.first() * horizontalScale));

    QPoint lastPt;
    for(int i = 0; i < leg.elevation.size(); i++)
    {
      float alt = leg.elevation.at(i).getAltitude();
      QPoint pt(X0 + static_cast<int>(leg.distances.at(i) * horizontalScale),
                Y0 + static_cast<int>(h - alt *verticalScale));

      if(lastPt.isNull() || i == leg.elevation.size() - 1 || (lastPt - pt).manhattanLength() > 2)
      {
        landPolygon.append(pt);
        lastPt = pt;
      }
    }
  }

  // Destination point
  waypointX.append(X0 + w);

  // Last point closing polygon
  landPolygon.append(QPoint(X0 + w, h + Y0));

  aircraftTrackPoints.clear();
  if(!NavApp::getRouteConst().isFlightplanEmpty() && showAircraftTrack)
  {
    // Update aircraft track screen coordinates
    const Route& route = legList.route;
    const AircraftTrack& aircraftTrack = mapWidget->getAircraftTrack();

    for(int i = 0; i < aircraftTrack.size(); i++)
    {
      const Pos& aircraftPos = aircraftTrack.at(i).pos;
      float distFromStart = route.getDistanceFromStart(aircraftPos);

      if(distFromStart < map::INVALID_DISTANCE_VALUE)
      {
        QPoint pt(X0 + static_cast<int>(distFromStart *horizontalScale),
                  Y0 + static_cast<int>(rect().height() - Y0 - aircraftPos.getAltitude() * verticalScale));

        if(aircraftTrackPoints.isEmpty() || (aircraftTrackPoints.last() - pt).manhattanLength() > 3)
          aircraftTrackPoints.append(pt);
      }
    }
  }
}

QVector<std::pair<int, int> > ProfileWidget::calcScaleValues()
{
  int h = rect().height() - Y0;
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
  for(float y = Y0 + h, alt = 0; y > Y0; y -= step * tempScale, alt += step)
    steps.append(std::make_pair(y, alt));

  return steps;
}

int ProfileWidget::getMinSafeAltitudeY() const
{
  return altitudeY(minSafeAltitudeFt);
}

int ProfileWidget::getFlightplanAltY() const
{
  return altitudeY(flightplanAltFt);
}

int ProfileWidget::altitudeY(float altitudeFt) const
{
  if(altitudeFt < map::INVALID_DISTANCE_VALUE)
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
  for(const QPointF& pt : leg)
    retval.append(toScreen(pt));
  return retval;
}

int ProfileWidget::distanceX(float distanceNm) const
{
  if(distanceNm < map::INVALID_DISTANCE_VALUE)
    return X0 + atools::roundToInt(distanceNm * horizontalScale);
  else
    return map::INVALID_INDEX_VALUE;
}

void ProfileWidget::showPosAlongFlightplan(int x, bool doubleClick)
{
  // Check range before
  if(x > X0 && x < width() - X0)
    emit showPos(calculatePos(x), 0.f, doubleClick);
}

void ProfileWidget::paintEvent(QPaintEvent *)
{
  // Saved route that was used to create the geometry
  // const Route& route = legList.route;

  // Get a copy of the active route
  Route route = NavApp::getRoute();
  const RouteAltitude& altitudeLegs = route.getAltitudeLegs();

  if(legList.route.size() != route.size() ||
     atools::almostNotEqual(legList.route.getTotalDistance(), route.getTotalDistance()))
    // Do not draw if route is updated to avoid invalid indexes
    return;

  // Keep margin to left, right and top
  int w = rect().width() - X0 * 2, h = rect().height() - Y0;

  bool dark = NavApp::isCurrentGuiStyleNight();

  // Fill background sky blue ====================================================
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  painter.fillRect(X0, 0, rect().width() - X0 * 2, rect().height(),
                   dark ? mapcolors::profileSkyDarkColor : mapcolors::profileSkyColor);

  SymbolPainter symPainter;

  if(!widgetVisible || legList.elevationLegs.isEmpty() || route.size() < 2 || altitudeLegs.size() < 2)
  {
    // Nothing to show label =========================
    symPainter.textBox(&painter, {tr("No Flight Plan")}, QPen(Qt::black),
                       X0 + w / 2, Y0 + h / 2, textatt::BOLD | textatt::CENTER, 255);
    return;
  }

  if(route.getAltitudeLegs().size() != route.size())
    // Do not draw if route altitudes are not updated to avoid invalid indexes
    return;

  // Draw the ground ======================================================
  painter.setBrush(dark ? mapcolors::profileLandDarkColor : mapcolors::profileLandColor);
  painter.setPen(dark ? mapcolors::profileLandOutlineDarkPen : mapcolors::profileLandOutlinePen);
  painter.drawPolygon(landPolygon);

  // Draw grey vertical lines for waypoints
  int flightplanY = getFlightplanAltY();
  int safeAltY = getMinSafeAltitudeY();
  int flightplanTextY = flightplanY + 14;
  painter.setPen(dark ? mapcolors::profileWaypointLineDarkPen : mapcolors::profileWaypointLinePen);
  for(int wpx : waypointX)
    painter.drawLine(wpx, flightplanY, wpx, Y0 + h);

  // Draw elevation scale lines ======================================================
  painter.setPen(dark ? mapcolors::profileElevationScaleDarkPen : mapcolors::profileElevationScalePen);
  for(const std::pair<int, int>& scale : calcScaleValues())
    painter.drawLine(0, scale.first, X0 + static_cast<int>(w), scale.first);

  // Draw one line for the label to the left
  painter.drawLine(0, flightplanY, X0 + static_cast<int>(w), flightplanY);
  painter.drawLine(0, safeAltY, X0, safeAltY);

  // Draw orange minimum safe altitude lines for each segment ======================================================
  painter.setPen(dark ? mapcolors::profileSafeAltLegLineDarkPen : mapcolors::profileSafeAltLegLinePen);
  for(int i = 0; i < legList.elevationLegs.size(); i++)
  {
    if(waypointX.at(i) == waypointX.at(i + 1))
      // Skip zero length segments to avoid dots on the graph
      continue;

    const ElevationLeg& leg = legList.elevationLegs.at(i);
    int lineY = Y0 + static_cast<int>(h - calcGroundBuffer(leg.maxElevation) * verticalScale);
    painter.drawLine(waypointX.at(i), lineY, waypointX.at(i + 1), lineY);
  }

  // Draw the red minimum safe altitude line ======================================================
  painter.setPen(dark ? mapcolors::profileSafeAltLineDarkPen : mapcolors::profileSafeAltLinePen);
  painter.drawLine(X0, safeAltY, X0 + static_cast<int>(w), safeAltY);

  // Get TOD position from active route  ======================================================

  // Calculate line y positions ======================================================
  QVector<QPolygon> altLegs; /* Flight plan waypoint screen coordinates. x = distance and y = altitude */
  for(const RouteAltitudeLeg& routeAlt : altitudeLegs)
    altLegs.append(toScreen(routeAlt.getGeometry()));

  // Collect indexes in reverse (painting) order without missed ================
  QVector<int> indexes;
  for(int i = 0; i < route.size(); i++)
  {
    if(route.at(i).getProcedureLeg().isMissed())
      break;
    else
      indexes.prepend(i);
  }

  // Draw altitude restriction bars ============================
  painter.setBackground(Qt::white);
  painter.setBackgroundMode(Qt::OpaqueMode);
  QBrush diagPatternBrush(Qt::darkGray, Qt::BDiagPattern);
  QPen thinLinePen(Qt::black, 1., Qt::SolidLine, Qt::FlatCap);
  QPen thickLinePen(Qt::black, 4., Qt::SolidLine, Qt::FlatCap);

  for(int routeIndex : indexes)
  {
    const proc::MapAltRestriction& restriction = altitudeLegs.at(routeIndex).getRestriction();

    if(restriction.isValid())
    {
      int rectWidth = 30, rectHeight = 14;

      // Start and end of line
      int wpx = waypointX.at(routeIndex);
      int x11 = wpx - rectWidth / 2;
      int x12 = wpx + rectWidth / 2;
      int y1 = altitudeY(restriction.alt1);

      if(restriction.descriptor)
      {
        // Connect waypoint with restriction with a thin vertical line
        painter.setPen(thinLinePen);
        painter.drawLine(QPoint(wpx, y1), altLegs.at(routeIndex).last());

        if(restriction.descriptor == proc::MapAltRestriction::AT_OR_ABOVE)
          // Draw diagonal pattern rectancle
          painter.fillRect(x11, y1, rectWidth, rectHeight, diagPatternBrush);
        else if(restriction.descriptor == proc::MapAltRestriction::AT_OR_BELOW)
          // Draw diagonal pattern rectancle
          painter.fillRect(x11, y1 - rectHeight, rectWidth, rectHeight, diagPatternBrush);
        else if(restriction.descriptor == proc::MapAltRestriction::BETWEEN)
        {
          // At or above alt2 and at or below alt1

          // Draw diagonal pattern rectancle for below alt1
          painter.fillRect(x11, y1 - rectHeight, rectWidth, rectHeight, diagPatternBrush);

          int x21 = waypointX.at(routeIndex) - rectWidth / 2;
          int x22 = waypointX.at(routeIndex) + rectWidth / 2;
          int y2 = altitudeY(restriction.alt2);
          // Draw diagonal pattern rectancle for above alt2
          painter.fillRect(x21, y2, rectWidth, rectHeight, diagPatternBrush);

          // Connect above and below with a thin line
          painter.drawLine(waypointX.at(routeIndex), y1, waypointX.at(routeIndex), y2);
          painter.setPen(thickLinePen);

          // Draw line for alt2
          painter.drawLine(x21, y2, x22, y2);
        }

        if(restriction.descriptor != proc::MapAltRestriction::NONE)
        {
          // Draw line for alt1 if any restriction
          painter.setPen(thickLinePen);
          painter.drawLine(x11, y1, x12, y1);
        }
      }
    }
  }

  const OptionData& optData = OptionData::instance();

  // Get active route leg
  bool activeValid = route.isActiveValid();
  // Active normally start at 1 - this will consider all legs as not passed
  int activeRouteLeg = activeValid ? route.getActiveLegIndex() : 0;
  int passedRouteLeg = optData.getFlags2() & opts::MAP_ROUTE_DIM_PASSED ? activeRouteLeg : 0;

  // Draw background line ======================================================
  float flightplanOutlineWidth = (optData.getDisplayThicknessFlightplan() / 100.f) * 7;
  float flightplanWidth = (optData.getDisplayThicknessFlightplan() / 100.f) * 4;
  painter.setPen(QPen(mapcolors::routeOutlineColor, flightplanOutlineWidth, Qt::SolidLine, Qt::RoundCap,
                      Qt::RoundJoin));
  for(int i = passedRouteLeg; i < waypointX.size(); i++)
  {
    if(i > 0 && !route.at(i).getProcedureLeg().isCircleToLand())
      painter.drawPolyline(altLegs.at(i));
  }

  // Draw passed ======================================================
  painter.setPen(QPen(optData.getFlightplanPassedSegmentColor(), flightplanWidth,
                      Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  for(int i = 1; i < passedRouteLeg; i++)
    painter.drawPolyline(altLegs.at(i));

  // Draw ahead ======================================================
  QPen flightplanPen(optData.getFlightplanColor(), flightplanWidth,
                     Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  QPen procedurePen(optData.getFlightplanProcedureColor(), flightplanWidth,
                    Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

  painter.setBackgroundMode(Qt::OpaqueMode);
  painter.setBackground(Qt::white);
  painter.setBrush(Qt::NoBrush);

  for(int i = passedRouteLeg + 1; i < waypointX.size(); i++)
  {
    painter.setPen(altitudeLegs.at(i).isAnyProcedure() ? procedurePen : flightplanPen);

    if(route.at(i).getProcedureLeg().isCircleToLand())
      mapcolors::adjustPenForCircleToLand(&painter);

    painter.drawPolyline(altLegs.at(i));
  }

  if(activeRouteLeg > 0)
  {
    // Draw active  ======================================================
    painter.setPen(QPen(optData.getFlightplanActiveSegmentColor(), flightplanWidth,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    if(route.at(activeRouteLeg).getProcedureLeg().isCircleToLand())
      mapcolors::adjustPenForCircleToLand(&painter);

    painter.drawPolyline(altLegs.at(activeRouteLeg));
  }
  // Draw flightplan symbols ======================================================

  // Calculate symbol sizes
  float sizeScaleSymbol = 1.f;
  int waypointSize = atools::roundToInt((optData.getDisplaySymbolSizeNavaid() * sizeScaleSymbol / 100.) * 9.);
  int navaidSize = atools::roundToInt((optData.getDisplaySymbolSizeNavaid() * sizeScaleSymbol / 100.) * 12.);
  int airportSize = atools::roundToInt((optData.getDisplaySymbolSizeAirport() * sizeScaleSymbol / 100.) * 10.);

  // Make default font 20 percent smaller and set to bold
  QFont defaultFont = painter.font();
  defaultFont.setBold(true);
  painter.setFont(defaultFont);
  mapcolors::scaleFont(&painter, 0.8f);
  defaultFont = painter.font();

  painter.setBackgroundMode(Qt::TransparentMode);

  // Show only ident in labels
  textflags::TextFlags flags = textflags::IDENT | textflags::ROUTE_TEXT | textflags::ABS_POS;

  // Draw the most unimportant symbols and texts first ============================
  mapcolors::scaleFont(&painter, optData.getDisplayTextSizeFlightplan() / 100.f, &defaultFont);
  int waypointIndex = waypointX.size();
  for(int routeIndex : indexes)
  {
    const RouteLeg& leg = route.at(routeIndex);
    waypointIndex--;
    if(altLegs.at(waypointIndex).isEmpty())
      continue;

    QPoint symPt(altLegs.at(waypointIndex).last());

    map::MapObjectTypes type = leg.getMapObjectType();

    // Symbols ========================================================
    if(type == map::WAYPOINT || leg.getWaypoint().isValid())
      symPainter.drawWaypointSymbol(&painter, QColor(), symPt.x(), symPt.y(), waypointSize, true, false);
    else if(type == map::USERPOINTROUTE)
      symPainter.drawUserpointSymbol(&painter, symPt.x(), symPt.y(), waypointSize, true, false);
    else if(type == map::INVALID)
      symPainter.drawWaypointSymbol(&painter, mapcolors::routeInvalidPointColor, symPt.x(), symPt.y(), 9, true, false);
    else if(type == map::PROCEDURE)
      // Missed is not included
      symPainter.drawProcedureSymbol(&painter, symPt.x(), symPt.y(), waypointSize, true, false);
    else
      continue;

    if(routeIndex >= activeRouteLeg - 1)
    {
      // Procedure symbols ========================================================
      if(leg.isAnyProcedure())
        symPainter.drawProcedureUnderlay(&painter, symPt.x(), symPt.y(), 6, leg.getProcedureLeg().flyover,
                                         leg.getProcedureLeg().isFinalApproachFix());

      // Labels ========================================================
      int symytxt = std::min(symPt.y() + 14, h);
      if(type == map::WAYPOINT || leg.getWaypoint().isValid())
        symPainter.drawWaypointText(&painter, leg.getWaypoint(), symPt.x() - 5, symytxt, flags, 10, true);
      else if(type == map::USERPOINTROUTE)
        symPainter.textBox(&painter, {atools::elideTextShort(leg.getIdent(), 6)}, mapcolors::routeUserPointColor,
                           symPt.x() - 5, symytxt, textatt::BOLD | textatt::ROUTE_BG_COLOR, 255);
      else if(type == map::INVALID)
        symPainter.textBox(&painter, {atools::elideTextShort(leg.getIdent(), 6)}, mapcolors::routeInvalidPointColor,
                           symPt.x() - 5, symytxt, textatt::BOLD | textatt::ROUTE_BG_COLOR, 255);
    }
  }

  // Draw the more important radio navaids
  waypointIndex = waypointX.size();
  for(int routeIndex : indexes)
  {
    const RouteLeg& leg = route.at(routeIndex);
    waypointIndex--;
    if(altLegs.at(waypointIndex).isEmpty())
      continue;

    QPoint symPt(altLegs.at(waypointIndex).last());

    map::MapObjectTypes type = leg.getMapObjectType();

    // Symbols ========================================================
    if(type == map::NDB || leg.getNdb().isValid())
      symPainter.drawNdbSymbol(&painter, symPt.x(), symPt.y(), navaidSize, true, false);
    else if(type == map::VOR || leg.getVor().isValid())
      symPainter.drawVorSymbol(&painter, leg.getVor(), symPt.x(), symPt.y(), navaidSize, true, false, false);
    else
      continue;

    if(routeIndex >= activeRouteLeg - 1)
    {
      // Procedure symbols ========================================================
      if(leg.isAnyProcedure())
        symPainter.drawProcedureUnderlay(&painter, symPt.x(), symPt.y(), 6, leg.getProcedureLeg().flyover,
                                         leg.getProcedureLeg().isFinalApproachFix());

      // Labels ========================================================
      int symytxt = std::min(symPt.y() + 14, h);
      if(type == map::NDB || leg.getNdb().isValid())
        symPainter.drawNdbText(&painter, leg.getNdb(), symPt.x() - 5, symytxt, flags, 10, true);
      else if(type == map::VOR || leg.getVor().isValid())
        symPainter.drawVorText(&painter, leg.getVor(), symPt.x() - 5, symytxt, flags, 10, true);
    }
  }

  // Draw the most important airport symbols
  waypointIndex = waypointX.size();
  for(int routeIndex : indexes)
  {
    const RouteLeg& leg = route.at(routeIndex);
    waypointIndex--;
    if(altLegs.at(waypointIndex).isEmpty())
      continue;
    QPoint symPt(altLegs.at(waypointIndex).last());

    // Draw all airport except destination and departure
    if(leg.getMapObjectType() == map::AIRPORT && routeIndex > 0 && routeIndex < route.size() - 1)
    {
      symPainter.drawAirportSymbol(&painter, leg.getAirport(), symPt.x(), symPt.y(), airportSize, false, false);

      if(routeIndex >= activeRouteLeg - 1)
      {
        int symytxt = std::min(symPt.y() + 14, h);
        symPainter.drawAirportText(&painter, leg.getAirport(), symPt.x() - 5, symytxt,
                                   optData.getDisplayOptions(), flags, 10, false, 16);
      }
    }
  }

  if(!route.isEmpty())
  {
    // Draw departure always on the left also if there are departure procedures
    const RouteLeg& departureLeg = route.first();
    if(departureLeg.getMapObjectType() == map::AIRPORT)
    {
      int textW = painter.fontMetrics().width(departureLeg.getIdent());
      symPainter.drawAirportSymbol(&painter, departureLeg.getAirport(), X0, flightplanY, airportSize, false, false);
      symPainter.drawAirportText(&painter, departureLeg.getAirport(), X0 - textW / 2, flightplanTextY,
                                 optData.getDisplayOptions(), flags, 10, false, 16);
    }

    // Draw destination always on the right also if there are approach procedures
    const RouteLeg& destinationLeg = route.last();
    if(destinationLeg.getMapObjectType() == map::AIRPORT)
    {
      int textW = painter.fontMetrics().width(destinationLeg.getIdent());
      symPainter.drawAirportSymbol(&painter, destinationLeg.getAirport(), X0 + w, flightplanY, airportSize, false,
                                   false);
      symPainter.drawAirportText(&painter, destinationLeg.getAirport(), X0 + w - textW / 2, flightplanTextY,
                                 optData.getDisplayOptions(), flags, 10, false, 16);
    }
  }

  if(!route.isFlightplanEmpty())
  {
    if(optData.getFlags() & opts::FLIGHT_PLAN_SHOW_TOD)
    {
      float tocDist = altitudeLegs.getTopOfClimbDistance();
      float todDist = altitudeLegs.getTopOfDescentDistance();
      float width = optData.getDisplaySymbolSizeNavaid() * sizeScaleSymbol / 100.f * 3.f;
      int radius = atools::roundToInt(optData.getDisplaySymbolSizeNavaid() * sizeScaleSymbol / 100. * 6.);

      painter.setBackgroundMode(Qt::TransparentMode);
      painter.setPen(QPen(Qt::black, width, Qt::SolidLine, Qt::FlatCap));
      painter.setBrush(Qt::NoBrush);

      if(tocDist > 0.2f)
      {
        int tocX = distanceX(altitudeLegs.getTopOfClimbDistance());
        if(tocX < map::INVALID_INDEX_VALUE)
        {
          // Draw the top of climb point and text =========================================================
          painter.drawEllipse(QPoint(tocX, flightplanY), radius, radius);

          QStringList txt;
          txt.append(tr("TOC"));
          txt.append(Unit::distNm(route.getTopOfClimbFromStart()));

          symPainter.textBox(&painter, txt, QPen(Qt::black),
                             tocX + 8, flightplanY + 8,
                             textatt::ROUTE_BG_COLOR | textatt::BOLD, 255);
        }
      }

      if(todDist < route.getTotalDistance() - 0.2f)
      {
        int todX = distanceX(altitudeLegs.getTopOfDescentDistance());
        if(todX < map::INVALID_INDEX_VALUE)
        {
          // Draw the top of descent point and text =========================================================
          painter.drawEllipse(QPoint(todX, flightplanY), radius, radius);

          QStringList txt;
          txt.append(tr("TOD"));
          txt.append(Unit::distNm(route.getTopOfDescentFromDestination()));

          symPainter.textBox(&painter, txt, QPen(Qt::black),
                             todX + 8, flightplanY + 8,
                             textatt::ROUTE_BG_COLOR | textatt::BOLD, 255);
        }
      }
    }

    // Departure altitude label =========================================================
    QColor labelColor = dark ? mapcolors::profileLabelDarkColor : mapcolors::profileLabelColor;
    float departureAlt = legList.route.first().getPosition().getAltitude();
    int departureAltTextY = Y0 + atools::roundToInt(h - departureAlt * verticalScale);
    departureAltTextY = std::min(departureAltTextY, Y0 + h - painter.fontMetrics().height() / 2);
    QString startAltStr = Unit::altFeet(departureAlt);
    symPainter.textBox(&painter, {startAltStr}, labelColor, X0 - 4, departureAltTextY,
                       textatt::BOLD | textatt::RIGHT, 255);

    // Destination altitude label =========================================================
    float destAlt = route.last().getPosition().getAltitude();
    int destinationAltTextY = Y0 + static_cast<int>(h - destAlt * verticalScale);
    destinationAltTextY = std::min(destinationAltTextY, Y0 + h - painter.fontMetrics().height() / 2);
    QString destAltStr = Unit::altFeet(destAlt);
    symPainter.textBox(&painter, {destAltStr}, labelColor, X0 + w + 4, destinationAltTextY,
                       textatt::BOLD | textatt::LEFT, 255);

    // Draw user aircraft track =========================================================
    if(!aircraftTrackPoints.isEmpty() && showAircraftTrack &&
       aircraftTrackPoints.boundingRect().width() > MIN_AIRCRAFT_TRACK_WIDTH)
    {
      painter.setPen(mapcolors::aircraftTrailPen(optData.getDisplayThicknessTrail() / 100.f * 2.f));
      painter.drawPolyline(aircraftTrackPoints);
    }

    // Draw user aircraft =========================================================
    if(simData.getUserAircraftConst().getPosition().isValid() && showAircraft)
    {
      float acx = X0 + aircraftDistanceFromStart * horizontalScale;
      float acy = Y0 + (h - simData.getUserAircraftConst().getPosition().getAltitude() * verticalScale);

      // Draw aircraft symbol
      int acsize = atools::roundToInt(optData.getDisplaySymbolSizeAircraftUser() / 100. * 32.);
      painter.translate(acx, acy);
      painter.rotate(90);
      painter.scale(0.75, 1.);
      painter.shear(0.0, 0.5);
      painter.drawPixmap(QPointF(-acsize / 2., -acsize / 2.),
                         *NavApp::getVehicleIcons()->pixmapFromCache(simData.getUserAircraftConst(), acsize, 0));
      painter.resetTransform();

      // Draw aircraft label
      mapcolors::scaleFont(&painter, optData.getDisplayTextSizeAircraftUser() / 100.f, &defaultFont);

      int vspeed = atools::roundToInt(simData.getUserAircraftConst().getVerticalSpeedFeetPerMin());
      QString upDown;
      if(vspeed > 100.f)
        upDown = tr(" ▲");
      else if(vspeed < -100.f)
        upDown = tr(" ▼");

      QStringList texts;
      texts.append(Unit::altFeet(simData.getUserAircraftConst().getPosition().getAltitude()));

      if(vspeed > 10.f || vspeed < -10.f)
        texts.append(Unit::speedVertFpm(vspeed) + upDown);

      textatt::TextAttributes att = textatt::BOLD;
      float textx = acx, texty = acy + 20.f;

      QRect rect = symPainter.textBoxSize(&painter, texts, att);
      if(textx + rect.right() > X0 + w)
        // Move text to the left when approaching the right corner
        att |= textatt::RIGHT;

      att |= textatt::ROUTE_BG_COLOR;

      if(texty + rect.bottom() > Y0 + h)
        // Move text down when approaching top boundary
        texty -= rect.bottom() + 20.f;

      symPainter.textBoxF(&painter, texts, QPen(Qt::black), textx, texty, att, 255);
    }
  }

  // Dim the whole map
  if(NavApp::isCurrentGuiStyleNight())
    painter.fillRect(QRect(0, 0, width(), height()),
                     QColor::fromRgb(0, 0, 0, 255 - (255 * optData.getGuiStyleMapDimming() / 100)));

  scrollArea->updateLabelWidget();
}

/* Update signal from Marble elevation model */
void ProfileWidget::elevationUpdateAvailable()
{
  if(!widgetVisible || databaseLoadStatus)
    return;

  // Do not terminate thread here since this can lead to starving updates

  // Start thread after long delay to calculate new data
  updateTimer->start(NavApp::getElevationProvider()->isGlobeOfflineProvider() ?
                     ELEVATION_CHANGE_OFFLINE_UPDATE_TIMEOUT_MS : ELEVATION_CHANGE_UPDATE_TIMEOUT_MS);
}

void ProfileWidget::routeAltitudeChanged(int altitudeFeet)
{
  Q_UNUSED(altitudeFeet);

  if(!widgetVisible || databaseLoadStatus)
    return;

  updateScreenCoords();
  update();
  updateErrorMessages();
  updateLabel();
}

void ProfileWidget::routeChanged(bool geometryChanged, bool newFlightPlan)
{
  if(!widgetVisible || databaseLoadStatus)
    return;

  scrollArea->routeChanged(geometryChanged);

  if(newFlightPlan)
    scrollArea->expandWidget();

  if(geometryChanged)
  {
    // Start thread after short delay to calculate new data
    updateTimer->start(NavApp::getElevationProvider()->isGlobeOfflineProvider() ?
                       ROUTE_CHANGE_OFFLINE_UPDATE_TIMEOUT_MS : ROUTE_CHANGE_UPDATE_TIMEOUT_MS);
  }
  updateErrorMessages();
  updateLabel();
}

/* Called by updateTimer after any route or elevation updates and starts the thread */
void ProfileWidget::updateTimeout()
{
  if(!widgetVisible || databaseLoadStatus)
    return;

  // qDebug() << Q_FUNC_INFO;

  // Terminate and wait for thread
  terminateThread();
  terminateThreadSignal = false;

  // Need a copy of the leg list before starting thread to avoid synchronization problems
  // Start the computation in background
  ElevationLegList legs;
  legs.route = routeController->getRoute();

  // Start thread
  future = QtConcurrent::run(this, &ProfileWidget::fetchRouteElevationsThread, legs);

  // Watcher will call updateThreadFinished when finished
  watcher.setFuture(future);
}

/* Called by watcher when the thread is finished */
void ProfileWidget::updateThreadFinished()
{
  if(!widgetVisible || databaseLoadStatus)
    return;

  // qDebug() << Q_FUNC_INFO;

  if(!terminateThreadSignal)
  {
    // Was not terminated in the middle of calculations - get result from the future
    legList = future.result();
    updateScreenCoords();
    updateErrorMessages();
    updateLabel();
    update();
  }
}

/* Get elevation points between the two points. This returns also correct results if the antimeridian is crossed
 * @return true if not aborted */
bool ProfileWidget::fetchRouteElevations(atools::geo::LineString& elevations,
                                         const atools::geo::LineString& geometry) const
{
  ElevationProvider *elevationProvider = NavApp::getElevationProvider();
  for(int i = 0; i < geometry.size() - 1; i++)
  {
    // Create a line string from the two points and split it at the date line if crossing
    GeoDataLineString coords;
    coords.setTessellate(true);
    coords << GeoDataCoordinates(geometry.at(i).getLonX(), geometry.at(i).getLatY(),
                                 0., GeoDataCoordinates::Degree)
           << GeoDataCoordinates(geometry.at(i + 1).getLonX(), geometry.at(i + 1).getLatY(),
                          0., GeoDataCoordinates::Degree);

    QVector<Marble::GeoDataLineString *> coordsCorrected = coords.toDateLineCorrected();
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
        elevationProvider->getElevations(elevations, atools::geo::Line(p1, p2));
      }
    }
    qDeleteAll(coordsCorrected);
  }

  if(!elevations.isEmpty())
  {
    // Add start or end point if heightProfile omitted these - check only lat lon not alt
    if(!elevations.first().almostEqual(geometry.first()))
      elevations.prepend(Pos(geometry.first().getLonX(), geometry.first().getLatY(), elevations.first().getAltitude()));

    if(!elevations.last().almostEqual(geometry.last()))
      elevations.append(Pos(geometry.last().getLonX(), geometry.last().getLatY(), elevations.last().getAltitude()));
  }

  return true;
}

/* Background thread. Fetches elevation points from Marble elevation model and updates totals. */
ProfileWidget::ElevationLegList ProfileWidget::fetchRouteElevationsThread(ElevationLegList legs) const
{
  QThread::currentThread()->setPriority(QThread::LowestPriority);
  // qDebug() << "priority" << QThread::currentThread()->priority();

  using atools::geo::meterToNm;
  using atools::geo::meterToFeet;

  legs.totalNumPoints = 0;
  legs.totalDistance = 0.f;
  legs.maxElevationFt = 0.f;
  legs.elevationLegs.clear();

  // Loop over all route legs
  for(int i = 1; i < legs.route.size(); i++)
  {
    if(terminateThreadSignal)
      // Return empty result
      return ElevationLegList();

    const RouteLeg& routeLeg = legs.route.at(i);
    if(routeLeg.getProcedureLeg().isMissed())
      break;

    const RouteLeg& lastLeg = legs.route.at(i - 1);
    ElevationLeg leg;

    // Skip for too long segments when using the marble online provider
    if(routeLeg.getDistanceTo() < ELEVATION_MAX_LEG_NM || NavApp::getElevationProvider()->isGlobeOfflineProvider())
    {
      LineString geometry;
      if(routeLeg.isAnyProcedure() && routeLeg.getGeometry().size() > 2)
        geometry = routeLeg.getGeometry();
      else
        geometry << lastLeg.getPosition() << routeLeg.getPosition();

      geometry.removeInvalid();

      LineString elevations;
      if(!fetchRouteElevations(elevations, geometry))
        return ElevationLegList();

      float dist = legs.totalDistance;
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

        leg.elevation.append(coord);
        if(j > 0)
          // Update total distance
          dist += meterToNm(lastPos.distanceMeterTo(coord));

        // Distance to elevation point from departure
        leg.distances.append(dist);

        legs.totalNumPoints++;
        lastPos = coord;
      }

      legs.totalDistance += routeLeg.getDistanceTo();
      leg.elevation.append(lastPos);
      leg.distances.append(legs.totalDistance);

    }
    else
    {
      float dist = meterToNm(lastLeg.getPosition().distanceMeterTo(routeLeg.getPosition()));
      leg.distances.append(legs.totalDistance);
      legs.totalDistance += dist;
      leg.distances.append(legs.totalDistance);
      leg.elevation.append(lastLeg.getPosition());
      leg.elevation.append(routeLeg.getPosition());
    }
    legs.elevationLegs.append(leg);
  }

  return legs;
}

void ProfileWidget::showEvent(QShowEvent *)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  if(size().isNull())
    resize(ui->scrollAreaProfile->viewport()->size());

  widgetVisible = true;
  // Start update immediately
  updateTimer->start(ROUTE_CHANGE_UPDATE_TIMEOUT_MS);
}

void ProfileWidget::hideEvent(QHideEvent *)
{
  // Stop all updates
  widgetVisible = false;
  updateTimer->stop();
  terminateThread();
}

atools::geo::Pos ProfileWidget::calculatePos(int x)
{
  atools::geo::Pos pos;
  int index;
  float distance, distanceToGo, alt, maxElev;
  calculateDistancesAndPos(x, pos, index, distance, distanceToGo, alt, maxElev);
  return pos;
}

void ProfileWidget::calculateDistancesAndPos(int x, atools::geo::Pos& pos, int& routeIndex, float& distance,
                                             float& distanceToGo, float& groundElevation, float& maxElev)
{
  // Get index for leg
  routeIndex = 0;
  QVector<int>::iterator it = std::lower_bound(waypointX.begin(), waypointX.end(), x);
  if(it != waypointX.end())
  {
    routeIndex = static_cast<int>(std::distance(waypointX.begin(), it)) - 1;
    if(routeIndex < 0)
      routeIndex = 0;
  }
  const ElevationLeg& leg = legList.elevationLegs.at(routeIndex);

  // Calculate distance from screen coordinates
  distance = (x - X0) / horizontalScale;
  if(distance < 0.f)
    distance = 0.f;

  distanceToGo = legList.totalDistance - distance;
  if(distanceToGo < 0.f)
    distanceToGo = 0.f;

  // Get distance value index for lower and upper bound at cursor position
  int indexLowDist = 0;
  QVector<float>::const_iterator lowDistIt = std::lower_bound(leg.distances.begin(),
                                                              leg.distances.end(), distance);
  if(lowDistIt != leg.distances.end())
  {
    indexLowDist = static_cast<int>(std::distance(leg.distances.begin(), lowDistIt));
    if(indexLowDist < 0)
      indexLowDist = 0;
  }
  int indexUpperDist = 0;
  QVector<float>::const_iterator upperDistIt = std::upper_bound(leg.distances.begin(),
                                                                leg.distances.end(), distance);
  if(upperDistIt != leg.distances.end())
  {
    indexUpperDist = static_cast<int>(std::distance(leg.distances.begin(), upperDistIt));
    if(indexUpperDist < 0)
      indexUpperDist = 0;
  }

  // Get altitude values before and after cursor and interpolate
  float alt1 = leg.elevation.at(indexLowDist).getAltitude();
  float alt2 = leg.elevation.at(indexUpperDist).getAltitude();
  groundElevation = (alt1 + alt2) / 2.f;

  // Calculate min altitude for this leg
  maxElev = calcGroundBuffer(leg.maxElevation);

  // Get Position for highlight on map
  float legdistpart = distance - leg.distances.first();
  float legdist = leg.distances.last() - leg.distances.first();

  // Calculate position along the flight plan
  pos = leg.elevation.first().interpolate(leg.elevation.last(), legdistpart / legdist);
}

void ProfileWidget::mouseMoveEvent(QMouseEvent *mouseEvent)
{
  if(!widgetVisible || legList.elevationLegs.isEmpty() || legList.route.isEmpty())
    return;

  if(rubberBand == nullptr)
    // Create rubber band line
    rubberBand = new QRubberBand(QRubberBand::Line, this);

  // Limit x to drawing region
  int x = mouseEvent->pos().x();
  x = std::max(x, X0);
  x = std::min(x, rect().width() - X0);

  rubberBand->setGeometry(x - 1, 0, 2, rect().height());
  rubberBand->show();

  atools::geo::Pos pos;
  int index;
  float distance, distanceToGo, groundElevation, maxElev;
  calculateDistancesAndPos(x, pos, index, distance, distanceToGo, groundElevation, maxElev);

  // Get from/to text
  QString from = atools::elideTextShort(legList.route.at(index).getIdent(), 20);
  QString to = atools::elideTextShort(legList.route.at(index + 1).getIdent(), 20);

  // Add text to upper dock window label ==========================
  variableLabelText =
    from + tr(" ► ") + to + tr(", ") +
    Unit::distNm(distance) + tr(" ► ") +
    Unit::distNm(distanceToGo) + tr(", ") +
    tr(" Ground Elevation ") + Unit::altFeet(groundElevation) + tr(", ") +
    tr(" Above Ground Altitude ") + Unit::altFeet(flightplanAltFt - groundElevation) + tr(", ") +
    tr(" Leg Safe Altitude ") + Unit::altFeet(maxElev);

#ifdef DEBUG_INFORMATION
  variableLabelText.append(QString(" [%1]").arg(NavApp::getRoute().getAltitudeForDistance(distanceToGo)));
#endif

  // Allow event to propagate to scroll widget
  mouseEvent->ignore();
  updateLabel();

  // Follow cursor on map if enabled ==========================
  if(NavApp::getMainUi()->actionProfileFollow->isChecked())
    emit showPos(calculatePos(x), map::INVALID_DISTANCE_VALUE, false);

  // Tell map widget to create a rubberband rectangle on the map
  emit highlightProfilePoint(pos);
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

  bool routeEmpty = NavApp::getRoute().isEmpty();

  // Position on the whole map widget
  QPoint mapPoint = mapFromGlobal(globalPoint);

  // Local visible widget position
  QPoint pointArea = scrollArea->getScrollArea()->mapFromGlobal(globalPoint);
  QRect rectArea = scrollArea->getScrollArea()->rect();

  // Local visible widget position
  QPoint pointLabel = scrollArea->getLabelWidget()->mapFromGlobal(globalPoint);
  QRect rectLabel = scrollArea->getLabelWidget()->rect();

  bool hasPosition = mapPoint.x() > X0 && mapPoint.x() < width() - X0 && rectArea.contains(pointArea);

  // Do not show context menu if point is neither on the visible widget and not on the label
  QPoint menuPos = globalPoint;
  if(!rectArea.contains(pointArea) && !rectLabel.contains(pointLabel))
    menuPos = scrollArea->getScrollArea()->mapToGlobal(rectArea.center());

  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->actionProfileShowOnMap->setEnabled(hasPosition && !routeEmpty);

  QMenu menu;
  menu.addAction(ui->actionProfileShowOnMap);
  menu.addSeparator();
  menu.addAction(ui->actionProfileExpand);
  menu.addSeparator();
  menu.addAction(ui->actionProfileCenterAircraft);
  menu.addSeparator();
  menu.addAction(ui->actionProfileFollow);
  menu.addSeparator();
  menu.addAction(ui->actionProfileShowZoom);
  menu.addAction(ui->actionProfileShowLabels);
  menu.addAction(ui->actionProfileShowScrollbars);
  QAction *action = menu.exec(menuPos);

  if(action == ui->actionProfileShowOnMap)
  {
    atools::geo::Pos pos = calculatePos(mapPoint.x());
    if(pos.isValid())
      emit showPos(pos, 0.f, false);
  }
  else if(action == ui->actionProfileCenterAircraft || action == ui->actionProfileFollow)
    scrollArea->update();

  // Other actions are connected to methods or used during updates
  // else if(action == ui->actionProfileFit)
  // else if(action == ui->actionProfileExpand)
  // menu.addAction(ui->actionProfileShowZoom);
  // else if(action == ui->actionProfileShowLabels)
  // else if(action == ui->actionProfileShowScrollbars)
}

void ProfileWidget::updateErrorMessages()
{
  // Collect errors from profile calculation
  const RouteAltitude& altitudeLegs = NavApp::getRoute().getAltitudeLegs();
  if(!altitudeLegs.isEmpty())
  {
    messages.clear();
    if(!altitudeLegs.isEmpty() &&
       !(altitudeLegs.getTopOfDescentDistance() < map::INVALID_DISTANCE_VALUE &&
         altitudeLegs.getTopOfClimbDistance() < map::INVALID_DISTANCE_VALUE))
      messages << tr("Cannot calculate top of climb or top of descent.");

    if(altitudeLegs.altRestrictionsViolated())
      messages << tr("Cannot comply with altitude restrictions.");

    if(!messages.isEmpty())
      messages.prepend(tr("Flight Plan is not valid."));
  }
}

void ProfileWidget::updateLabel()
{
  float distFromStartNm = 0.f, distToDestNm = 0.f;

  if(simData.getUserAircraftConst().getPosition().isValid())
  {
    if(routeController->getRoute().getRouteDistances(&distFromStartNm, &distToDestNm))
    {
      if(routeController->getRoute().isActiveMissed())
        distToDestNm = 0.f;

      if(OptionData::instance().getFlags() & opts::FLIGHT_PLAN_SHOW_TOD)
      {
        float toTod = routeController->getRoute().getTopOfDescentFromStart() - distFromStartNm;

        fixedLabelText = tr("<b>To Destination: %1, to Top of Descent: %2.</b>&nbsp;&nbsp;").
                         arg(Unit::distNm(distToDestNm)).
                         arg(toTod > 0 ? Unit::distNm(toTod) : tr("Passed"));
      }
      else
        fixedLabelText = tr("<b>To Destination: %1.</b>&nbsp;&nbsp;").arg(Unit::distNm(distToDestNm));
    }
  }
  else
    fixedLabelText.clear();

  QString msg = messages.isEmpty() ? QString() :
                ("<span style=\"background-color: #ff0000; color: #ffffff; font-weight:bold\">" +
                 messages.join(" ") +
                 "</span><br/>");

  NavApp::getMainUi()->labelProfileInfo->setText(msg + fixedLabelText + " " + variableLabelText);
}

/* Cursor leaves widget. Stop displaying the rubberband */
void ProfileWidget::leaveEvent(QEvent *)
{
  hideRubberBand();
}

void ProfileWidget::hideRubberBand()
{
  if(!widgetVisible || legList.elevationLegs.isEmpty() || legList.route.isEmpty())
    return;

  delete rubberBand;
  rubberBand = nullptr;

  variableLabelText.clear();
  updateLabel();

  // Tell map widget to erase highlight
  emit highlightProfilePoint(atools::geo::EMPTY_POS);
}

/* Resizing needs an update of the screen coordinates */
void ProfileWidget::resizeEvent(QResizeEvent *)
{
  // Ui::MainWindow *ui = NavApp::getMainUi();
  // qDebug() << Q_FUNC_INFO << size();
  // qDebug() << Q_FUNC_INFO << "sizeHint()" << sizeHint();
  // qDebug() << Q_FUNC_INFO << "viewport()->size()" << ui->scrollAreaProfile->viewport()->size();
  // qDebug() << Q_FUNC_INFO << "viewport()->sizeHint()" << ui->scrollAreaProfile->viewport()->sizeHint();

  updateScreenCoords();
}

/* Deleting aircraft track needs an update of the screen coordinates */
void ProfileWidget::deleteAircraftTrack()
{
  aircraftTrackPoints.clear();
  maxTrackAltitudeFt = 0.f;

  updateScreenCoords();
  update();
}

/* Stop thread */
void ProfileWidget::preDatabaseLoad()
{
  updateTimer->stop();
  terminateThread();
  databaseLoadStatus = true;
}

/* Force new thread creation */
void ProfileWidget::postDatabaseLoad()
{
  databaseLoadStatus = false;
  routeChanged(true /* geometry changed */, true /* new flight plan */);
}

void ProfileWidget::optionsChanged()
{
  updateScreenCoords();
  updateErrorMessages();
  updateLabel();
  update();
}

void ProfileWidget::styleChanged()
{
  scrollArea->styleChanged();
}

void ProfileWidget::saveState()
{
  scrollArea->saveState();
}

void ProfileWidget::restoreState()
{
  scrollArea->restoreState();
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
  update();
}

void ProfileWidget::updateProfileShowFeatures()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  // Compare own values with action values
  bool updateProfile = showAircraft != ui->actionMapShowAircraft->isChecked() ||
                       showAircraftTrack != ui->actionMapShowAircraftTrack->isChecked();

  showAircraft = ui->actionMapShowAircraft->isChecked();
  showAircraftTrack = ui->actionMapShowAircraftTrack->isChecked();

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
