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
#include "ui_mainwindow.h"
#include "common/symbolpainter.h"
#include "route/routecontroller.h"
#include "common/aircrafttrack.h"
#include "common/unit.h"
#include "mapgui/mapwidget.h"
#include "options/optiondata.h"
#include "common/elevationprovider.h"

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

ProfileWidget::ProfileWidget(QMainWindow *parent)
  : QWidget(parent), mainWindow(parent)
{
  setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
  setMinimumSize(QSize(50, 40));

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
  if(!widgetVisible || databaseLoadStatus || !simulatorData.getUserAircraft().getPosition().isValid())
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
          if(lastSimData.getUserAircraft().getPosition().isValid())
            lastPoint = QPoint(X0 + static_cast<int>(aircraftDistanceFromStart * horizontalScale),
                               Y0 + static_cast<int>(rect().height() - Y0 -
                                                     lastSimData.getUserAircraft().getPosition().getAltitude()
                                                     * verticalScale));

          // Get screen point for current update
          QPoint currentPoint(X0 + static_cast<int>(aircraftDistanceFromStart *horizontalScale),
                              Y0 + static_cast<int>(rect().height() - Y0 -
                                                    simData.getUserAircraft().getPosition().getAltitude() *
                                                    verticalScale));

          if(aircraftTrackPoints.isEmpty() || (aircraftTrackPoints.last() - currentPoint).manhattanLength() > 3)
          {
            // Add track point and update widget if delta value between last and current update is large enough
            if(simData.getUserAircraft().getPosition().isValid())
            {
              aircraftTrackPoints.append(currentPoint);

              if(aircraftTrackPoints.boundingRect().width() > MIN_AIRCRAFT_TRACK_WIDTH)
                maxTrackAltitudeFt = std::max(maxTrackAltitudeFt,
                                              simData.getUserAircraft().getPosition().getAltitude());
              else
                maxTrackAltitudeFt = 0.f;

              updateWidget = true;
            }
          }

          const SimUpdateDelta& deltas = SIM_UPDATE_DELTA_MAP.value(OptionData::instance().getSimUpdateRate());

          using atools::almostNotEqual;
          if(!lastSimData.getUserAircraft().getPosition().isValid() ||
             (lastPoint - currentPoint).manhattanLength() > deltas.manhattanLengthDelta ||
             almostNotEqual(lastSimData.getUserAircraft().getPosition().getAltitude(),
                            simData.getUserAircraft().getPosition().getAltitude(), deltas.altitudeDelta))
          {
            // Aircraft position has changed enough
            lastSimData = simData;
            if(simData.getUserAircraft().getPosition().getAltitude() > maxWindowAlt)
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
      bool valid = simData.getUserAircraft().getPosition().isValid();
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

  if(simData.getUserAircraft().getPosition().isValid() &&
     (showAircraft || showAircraftTrack) && !NavApp::getRouteConst().isFlightplanEmpty())
    maxWindowAlt = std::max(maxWindowAlt, simData.getUserAircraft().getPosition().getAltitude());

  if(showAircraftTrack)
    maxWindowAlt = std::max(maxWindowAlt, maxTrackAltitudeFt);

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

void ProfileWidget::paintEvent(QPaintEvent *)
{
  int w = rect().width() - X0 * 2, h = rect().height() - Y0;

  bool darkStyle = OptionData::instance().isGuiStyleDark();

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), darkStyle ? mapcolors::profileBackgroundDarkColor : mapcolors::profileBackgroundColor);
  painter.fillRect(X0, 0, w, h + Y0, darkStyle ? mapcolors::profileSkyDarkColor : mapcolors::profileSkyColor);

  SymbolPainter symPainter;

  if(!widgetVisible || legList.elevationLegs.isEmpty() || legList.route.isEmpty())
  {
    symPainter.textBox(&painter, {tr("No Flight Plan")}, QPen(Qt::black),
                       X0 + w / 4, Y0 + h / 2, textatt::BOLD, 255);
    return;
  }

  // Draw the mountains
  painter.setBrush(darkStyle ? mapcolors::profileLandDarkColor : mapcolors::profileLandColor);
  painter.setPen(darkStyle ? mapcolors::profileLandOutlineDarkPen : mapcolors::profileLandOutlinePen);
  painter.drawPolygon(landPolygon);

  // Draw grey vertical lines for waypoints
  int flightplanY = Y0 + static_cast<int>(h - flightplanAltFt * verticalScale);
  int flightplanTextY = flightplanY + 14;
  painter.setPen(darkStyle ? mapcolors::profileWaypointLineDarkPen : mapcolors::profileWaypointLinePen);
  for(int wpx : waypointX)
    painter.drawLine(wpx, flightplanY, wpx, Y0 + h);

  // Create a temporary scale based on current units
  float tempScale = Unit::rev(verticalScale, Unit::altFeetF);

  // Find best scale for elevation lines
  int step = 10000;
  for(int s : SCALE_STEPS)
  {
    // Loop through all scale steps and check which one fits best
    if(s * tempScale > MIN_SCALE_SCREEN_DISTANCE)
    {
      step = s;
      break;
    }
  }

  // Draw elevation scale line and texts
  QPen scalePen = darkStyle ? mapcolors::profileElevationScaleDarkPen : mapcolors::profileElevationScalePen;
  painter.setPen(scalePen);
  for(int y = Y0 + h, alt = 0; y > Y0; y -= step * tempScale, alt += step)
  {
    painter.drawLine(X0, y, X0 + static_cast<int>(w), y);

    int altTextY = std::min(y, Y0 + h - painter.fontMetrics().height() / 2);
    symPainter.textBox(&painter, {QString::number(alt, 'f', 0)},
                       scalePen, X0 - 8, altTextY, textatt::BOLD | textatt::RIGHT, 0);

    symPainter.textBox(&painter, {QString::number(alt, 'f', 0)},
                       scalePen, X0 + w + 4, altTextY, textatt::BOLD | textatt::LEFT, 0);
  }

  const Route& route = routeController->getRoute();

  // Draw orange minimum safe altitude lines for each segment
  painter.setPen(darkStyle ? mapcolors::profileSafeAltLegLineDarkPen : mapcolors::profileSafeAltLegLinePen);
  for(int i = 0; i < legList.elevationLegs.size(); i++)
  {
    if(waypointX.at(i) == waypointX.at(i + 1))
      // Skip zero length segments to avoid dots on the graph
      continue;

    const ElevationLeg& leg = legList.elevationLegs.at(i);
    int lineY = Y0 + static_cast<int>(h - calcGroundBuffer(leg.maxElevation) * verticalScale);
    painter.drawLine(waypointX.at(i), lineY, waypointX.at(i + 1), lineY);
  }

  // Draw the red minimum safe altitude line
  painter.setPen(darkStyle ? mapcolors::profileSafeAltLineDarkPen : mapcolors::profileSafeAltLinePen);
  int maxAltY = Y0 + static_cast<int>(h - minSafeAltitudeFt * verticalScale);
  painter.drawLine(X0, maxAltY, X0 + static_cast<int>(w), maxAltY);

  int todX = 0;
  QPolygon line;
  line << QPoint(X0, flightplanY);
  float destAlt = legList.route.last().getPosition().getAltitude();
  if(OptionData::instance().getFlags() & opts::FLIGHT_PLAN_SHOW_TOD)
  {
    if(route.getTopOfDescentFromStart() > 0.f)
      todX = X0 + atools::roundToInt(route.getTopOfDescentFromStart() * horizontalScale);
    else
      todX = X0;

    // Draw the flightplan line
    if(todX < X0 + w)
    {
      line << QPoint(todX, flightplanY);
      line << QPoint(X0 + w, Y0 + static_cast<int>(h - destAlt * verticalScale));
    }
    else
      line << QPoint(X0 + w, flightplanY);
  }
  else
    line << QPoint(X0 + w, flightplanY);

  painter.setPen(QPen(mapcolors::routeOutlineColor, 4, Qt::SolidLine));
  painter.drawPolyline(line);

  painter.setPen(QPen(OptionData::instance().getFlightplanColor(), 2, Qt::SolidLine));
  painter.drawPolyline(line);

  // Draw flightplan symbols
  // Set default font to bold and reduce size
  QFont font = painter.font();
  float defaultFontSize = static_cast<float>(font.pointSizeF());
  font.setBold(true);
  font.setPointSizeF(defaultFontSize * 0.8f);
  painter.setFont(font);

  painter.setBackgroundMode(Qt::TransparentMode);

  textflags::TextFlags flags = textflags::IDENT | textflags::ROUTE_TEXT | textflags::ABS_POS;

  // Collect indexes in reverse (painting) order without missed
  QVector<int> indexes;
  for(int i = 0; i < legList.route.size(); i++)
  {
    if(legList.route.at(i).getProcedureLeg().isMissed())
      break;
    else
      indexes.prepend(i);
  }

  // Draw the most unimportant symbols and texts first
  int waypointIndex = waypointX.size();
  for(int routeIndex : indexes)
  {
    const RouteLeg& leg = legList.route.at(routeIndex);
    int symx = waypointX.at(--waypointIndex);

    map::MapObjectTypes type = leg.getMapObjectType();

    if(type == map::WAYPOINT || leg.getWaypoint().isValid())
    {
      symPainter.drawWaypointSymbol(&painter, QColor(), symx, flightplanY, 8, true, false);
      symPainter.drawWaypointText(&painter, leg.getWaypoint(), symx - 5, flightplanY + 14, flags, 10, true);
    }
    else if(type == map::USERPOINTROUTE)
    {
      symPainter.drawUserpointSymbol(&painter, symx, flightplanY, 8, true, false);
      symPainter.textBox(&painter, {atools::elideTextShort(leg.getIdent(), 6)}, mapcolors::routeUserPointColor,
                         symx - 5, flightplanTextY, textatt::BOLD | textatt::ROUTE_BG_COLOR, 255);
    }
    else if(type == map::INVALID)
    {
      symPainter.drawWaypointSymbol(&painter, mapcolors::routeInvalidPointColor, symx, flightplanY, 8, true, false);
      symPainter.textBox(&painter, {atools::elideTextShort(leg.getIdent(), 6)}, mapcolors::routeInvalidPointColor,
                         symx - 5, flightplanTextY, textatt::BOLD | textatt::ROUTE_BG_COLOR, 255);
    }
    else if(type == map::PROCEDURE)
      // Missed is not included
      symPainter.drawProcedureSymbol(&painter, symx, flightplanY, 9, true, false);
  }

  // Draw the more important radio navaids
  waypointIndex = waypointX.size();
  for(int routeIndex : indexes)
  {
    const RouteLeg& leg = legList.route.at(routeIndex);
    int symx = waypointX.at(--waypointIndex);

    map::MapObjectTypes type = leg.getMapObjectType();

    if(type == map::NDB || leg.getNdb().isValid())
    {
      symPainter.drawNdbSymbol(&painter, symx, flightplanY, 12, true, false);
      symPainter.drawNdbText(&painter, leg.getNdb(), symx - 5, flightplanTextY, flags, 10, true);
    }
    else if(type == map::VOR || leg.getVor().isValid())
    {
      symPainter.drawVorSymbol(&painter, leg.getVor(), symx, flightplanY, 12, true, false, false);
      symPainter.drawVorText(&painter, leg.getVor(), symx - 5, flightplanTextY, flags, 10, true);
    }
  }

  // Draw the most important airport symbols
  font.setBold(true);
  font.setPointSizeF(defaultFontSize);
  painter.setFont(font);
  waypointIndex = waypointX.size();
  for(int routeIndex : indexes)
  {
    const RouteLeg& leg = legList.route.at(routeIndex);
    int symx = waypointX.at(--waypointIndex);

    // Draw all airport except destination and departure
    if(leg.getMapObjectType() == map::AIRPORT && routeIndex > 0 && routeIndex < legList.route.size())
    {
      symPainter.drawAirportSymbol(&painter, leg.getAirport(), symx, flightplanY, 10, false, false);
      symPainter.drawAirportText(&painter, leg.getAirport(), symx - 5, flightplanTextY,
                                 OptionData::instance().getDisplayOptions(), flags, 10, false, 16);
    }
  }

  if(!legList.route.isEmpty())
  {
    // Draw departure always on the left also if there are departure procedures
    const RouteLeg& departureLeg = legList.route.first();
    if(departureLeg.getMapObjectType() == map::AIRPORT)
    {
      symPainter.drawAirportSymbol(&painter, departureLeg.getAirport(), X0, flightplanY, 10, false, false);
      symPainter.drawAirportText(&painter, departureLeg.getAirport(), X0 - 5, flightplanTextY,
                                 OptionData::instance().getDisplayOptions(), flags, 10, false, 16);
    }

    // Draw destination always on the right also if there are approach procedures
    const RouteLeg& destinationLeg = legList.route.last();
    if(destinationLeg.getMapObjectType() == map::AIRPORT)
    {
      symPainter.drawAirportSymbol(&painter, destinationLeg.getAirport(), X0 + w, flightplanY, 10, false, false);
      symPainter.drawAirportText(&painter, destinationLeg.getAirport(), X0 + w - 5, flightplanTextY,
                                 OptionData::instance().getDisplayOptions(), flags, 10, false, 16);
    }
  }

  // Draw text labels ========================================================
  // Safe altitude label
  symPainter.textBox(&painter, {Unit::altFeet(minSafeAltitudeFt)},
                     darkStyle ? mapcolors::profileSafeAltLineDarkPen : mapcolors::profileSafeAltLinePen,
                     X0 - 8, maxAltY, textatt::BOLD | textatt::RIGHT, 255);

  // Departure altitude label
  QColor labelColor = darkStyle ? mapcolors::profileLabelDarkColor : mapcolors::profileLabelColor;
  float departureAlt = legList.route.first().getPosition().getAltitude();
  int departureAltTextY = Y0 + atools::roundToInt(h - departureAlt * verticalScale);
  departureAltTextY = std::min(departureAltTextY, Y0 + h - painter.fontMetrics().height() / 2);
  QString startAltStr = Unit::altFeet(departureAlt);
  symPainter.textBox(&painter, {startAltStr}, labelColor, X0 - 8, departureAltTextY,
                     textatt::BOLD | textatt::RIGHT, 255);

  // Destination altitude label
  int destinationAltTextY = Y0 + static_cast<int>(h - destAlt * verticalScale);
  destinationAltTextY = std::min(destinationAltTextY, Y0 + h - painter.fontMetrics().height() / 2);
  QString destAltStr = Unit::altFeet(destAlt);
  symPainter.textBox(&painter, {destAltStr}, labelColor, X0 + w + 4, destinationAltTextY,
                     textatt::BOLD | textatt::LEFT, 255);

  // Route cruise altitude
  float routeAlt = route.getFlightplan().getCruisingAltitude();
  symPainter.textBox(&painter, {QLocale().toString(routeAlt, 'f', 0)},
                     labelColor, X0 - 8, flightplanY, textatt::BOLD | textatt::RIGHT, 255);

  if(!NavApp::getRouteConst().isFlightplanEmpty())
  {
    if(todX < X0 + w && OptionData::instance().getFlags() & opts::FLIGHT_PLAN_SHOW_TOD)
    {
      // Draw the top of descent point and text
      painter.setBackgroundMode(Qt::TransparentMode);
      painter.setPen(QPen(Qt::black, 3, Qt::SolidLine, Qt::FlatCap));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(QPoint(todX, flightplanY), 6, 6);

      QStringList tod;
      tod.append(tr("TOD"));
      tod.append(Unit::distNm(route.getTopOfDescentFromDestination()));

      symPainter.textBox(&painter, tod, QPen(Qt::black),
                         todX + 8, flightplanY + 8,
                         textatt::ROUTE_BG_COLOR | textatt::BOLD, 255);
    }

    // Draw user aircraft track
    if(!aircraftTrackPoints.isEmpty() && showAircraftTrack &&
       aircraftTrackPoints.boundingRect().width() > MIN_AIRCRAFT_TRACK_WIDTH)
    {
      painter.setPen(mapcolors::aircraftTrailPen(2.f));
      painter.drawPolyline(aircraftTrackPoints);
    }

    // Draw user aircraft
    if(simData.getUserAircraft().getPosition().isValid() && showAircraft)
    {
      float acx = X0 + aircraftDistanceFromStart * horizontalScale;
      float acy = Y0 + (h - simData.getUserAircraft().getPosition().getAltitude() * verticalScale);

      // Draw aircraft symbol
      painter.translate(acx, acy);
      painter.rotate(90);
      symPainter.drawAircraftSymbol(&painter, 0, 0, 16, simData.getUserAircraft().isOnGround());
      painter.resetTransform();

      // Draw aircraft label
      font.setPointSizeF(defaultFontSize);
      painter.setFont(font);

      int vspeed = atools::roundToInt(simData.getUserAircraft().getVerticalSpeedFeetPerMin());
      QString upDown;
      if(vspeed > 100.f)
        upDown = tr(" ▲");
      else if(vspeed < -100.f)
        upDown = tr(" ▼");

      QStringList texts;
      texts.append(Unit::altFeet(simData.getUserAircraft().getPosition().getAltitude()));

      if(vspeed > 10.f || vspeed < -10.f)
        texts.append(Unit::speedVertFpm(vspeed) + upDown);

      // texts.append(Unit::distNm(aircraftDistanceFromStart) + tr(" ► ") +
      // Unit::distNm(aircraftDistanceToDest));

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
  if(OptionData::instance().isGuiStyleDark())
  {
    int dim = OptionData::instance().getGuiStyleMapDimming();
    QColor col = QColor::fromRgb(0, 0, 0, 255 - (255 * dim / 100));
    painter.fillRect(QRect(0, 0, width(), height()), col);
  }

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
  updateLabel();
}

void ProfileWidget::routeChanged(bool geometryChanged)
{
  if(!widgetVisible || databaseLoadStatus)
    return;

  if(geometryChanged)
  {
    // Start thread after short delay to calculate new data
    updateTimer->start(NavApp::getElevationProvider()->isGlobeOfflineProvider() ?
                       ROUTE_CHANGE_OFFLINE_UPDATE_TIMEOUT_MS : ROUTE_CHANGE_UPDATE_TIMEOUT_MS);
  }
  // else
  // {
  //// Only update screen
  // updateScreenCoords();
  // update();
  // }

  // updateLabel();
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

  // Get index for leg
  int index = 0;
  QVector<int>::iterator it = std::lower_bound(waypointX.begin(), waypointX.end(), x);
  if(it != waypointX.end())
  {
    index = static_cast<int>(std::distance(waypointX.begin(), it)) - 1;
    if(index < 0)
      index = 0;
  }
  const ElevationLeg& leg = legList.elevationLegs.at(index);

  // Calculate distance from screen coordinates
  float distance = (x - X0) / horizontalScale;
  if(distance < 0.f)
    distance = 0.f;

  float distanceToGo = legList.totalDistance - distance;
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
  float alt = (alt1 + alt2) / 2.f;

  // Get Position for highlight on map
  float legdistpart = distance - leg.distances.first();
  float legdist = leg.distances.last() - leg.distances.first();

  // Calculate position along the flight plan
  const atools::geo::Pos& pos = leg.elevation.first().interpolate(leg.elevation.last(), legdistpart / legdist);

  // Calculate min altitude for this leg
  float maxElev = calcGroundBuffer(leg.maxElevation);

  // Get from/to text
  QString from = atools::elideTextShort(legList.route.at(index).getIdent(), 20);
  QString to = atools::elideTextShort(legList.route.at(index + 1).getIdent(), 20);

  // Add text to upper dock window label
  variableLabelText =
    from + tr(" ► ") + to + tr(", ") +
    Unit::distNm(distance) + tr(" ► ") +
    Unit::distNm(distanceToGo) + tr(", ") +
    tr(" Ground Elevation ") + Unit::altFeet(alt) + tr(", ") +
    tr(" Above Ground Altitude ") + Unit::altFeet(flightplanAltFt - alt) + tr(", ") +
    tr(" Leg Safe Altitude ") + Unit::altFeet(maxElev);

  mouseEvent->accept();
  updateLabel();

  // Tell map widget to create a rubberband rectangle on the map
  emit highlightProfilePoint(pos);
}

void ProfileWidget::updateLabel()
{
  float distFromStartNm = 0.f, distToDestNm = 0.f;

  if(simData.getUserAircraft().getPosition().isValid())
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

  NavApp::getMainUi()->labelElevationInfo->setText(fixedLabelText + " " + variableLabelText);
}

/* Cursor leaves widget. Stop displaying the rubberband */
void ProfileWidget::leaveEvent(QEvent *)
{
  if(!widgetVisible || legList.elevationLegs.isEmpty() || legList.route.isEmpty())
    return;

  delete rubberBand;
  rubberBand = nullptr;

  variableLabelText.clear();
  updateLabel();

  // Tell map widget to erase rubberband
  emit highlightProfilePoint(atools::geo::EMPTY_POS);
}

/* Resizing needs an update of the screen coordinates */
void ProfileWidget::resizeEvent(QResizeEvent *)
{
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
  routeChanged(true);
}

void ProfileWidget::optionsChanged()
{
  updateScreenCoords();
  update();
}

void ProfileWidget::preRouteCalc()
{
  updateTimer->stop();
  terminateThread();
}

void ProfileWidget::mainWindowShown()
{
  updateScreenCoords();
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
