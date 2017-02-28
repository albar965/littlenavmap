/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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
#include "gui/mainwindow.h"
#include "geo/calculations.h"
#include "common/mapcolors.h"
#include "ui_mainwindow.h"
#include "common/symbolpainter.h"
#include "route/routecontroller.h"
#include "common/aircrafttrack.h"
#include "common/unit.h"
#include "mapgui/mapwidget.h"
#include "options/optiondata.h"

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

ProfileWidget::ProfileWidget(MainWindow *parent)
  : QWidget(parent), mainWindow(parent)
{
  setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
  setMinimumSize(QSize(50, 40));

  elevationModel = mainWindow->getElevationModel();
  routeController = mainWindow->getRouteController();

  // Create single shot timer that will restart the thread after a delay
  updateTimer = new QTimer(this);
  updateTimer->setSingleShot(true);
  connect(updateTimer, &QTimer::timeout, this, &ProfileWidget::updateTimeout);

  // Marble will let us know when updates are available
  connect(elevationModel, &Marble::ElevationModel::updateAvailable, this,
          &ProfileWidget::elevationUpdateAvailable);

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

  if(!routeController->isFlightplanEmpty())
  {
    const RouteMapObjectList& rmoList = routeController->getRouteApprMapObjects();

    if((showAircraft || showAircraftTrack))
    {
      if(!rmoList.isPassedLastLeg())
      {
        simData = simulatorData;
        if(rmoList.getRouteDistances(&aircraftDistanceFromStart, &aircraftDistanceToDest))
        {
          // Get screen point from last update
          QPoint lastPoint;
          if(lastSimData.getUserAircraft().getPosition().isValid())
            lastPoint = QPoint(X0 + static_cast<int>(aircraftDistanceFromStart * horizontalScale),
                               Y0 + static_cast<int>(rect().height() - Y0 -
                                                     lastSimData.getUserAircraft().getPosition().getAltitude()
                                                     * verticalScale));

          // Get screen point for current update
          QPoint currentPoint(X0 + static_cast<int>(aircraftDistanceFromStart * horizontalScale),
                              Y0 + static_cast<int>(rect().height() - Y0 -
                                                    simData.getUserAircraft().getPosition().getAltitude() *
                                                    verticalScale));

          if(aircraftTrackPoints.isEmpty() || (aircraftTrackPoints.last() - currentPoint).manhattanLength() > 3)
          {
            // Add track point and update widget if delta value between last and current update is large enough
            if(simData.getUserAircraft().getPosition().isValid())
            {
              aircraftTrackPoints.append(currentPoint);
              maxTrackAltitudeFt = std::max(maxTrackAltitudeFt,
                                            simData.getUserAircraft().getPosition().getAltitude());

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

/* Update all screen coordinates and scale factors */
void ProfileWidget::updateScreenCoords()
{
  MapWidget *mapWidget = mainWindow->getMapWidget();

  // Widget drawing region width and height
  int w = rect().width() - X0 * 2, h = rect().height() - Y0;

  if(!routeController->isFlightplanEmpty() && showAircraftTrack)
    maxTrackAltitudeFt = mapWidget->getAircraftTrack().getMaxAltitude();
  else
    maxTrackAltitudeFt = 0.f;

  // Update elevation polygon
  // Add 1000 ft buffer and round up to the next 500 feet
  minSafeAltitudeFt = calcGroundBuffer(legList.maxElevationFt);
  flightplanAltFt =
    static_cast<float>(Unit::rev(
                         routeController->getRouteApprMapObjects().getFlightplan().getCruisingAltitude(),
                         Unit::altFeetF));
  maxWindowAlt = std::max(minSafeAltitudeFt, flightplanAltFt);

  if(simData.getUserAircraft().getPosition().isValid() &&
     (showAircraft || showAircraftTrack) && !routeController->isFlightplanEmpty())
    maxWindowAlt = std::max(maxWindowAlt, simData.getUserAircraft().getPosition().getAltitude());

  if(showAircraftTrack)
    maxWindowAlt = std::max(maxWindowAlt, maxTrackAltitudeFt);

  verticalScale = h / maxWindowAlt;
  horizontalScale = w / legList.totalDistance;

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
                Y0 + static_cast<int>(h - alt * verticalScale));

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
  if(!routeController->isFlightplanEmpty() && showAircraftTrack)
  {
    // Update aircraft track screen coordinates
    const RouteMapObjectList& rmoList = legList.routeApprMapObjects;
    const AircraftTrack& aircraftTrack = mapWidget->getAircraftTrack();

    for(int i = 0; i < aircraftTrack.size(); i++)
    {
      const Pos& aircraftPos = aircraftTrack.at(i).pos;
      float distFromStart = 0.f;
      if(rmoList.getRouteDistances(&distFromStart, nullptr))
      {
        QPoint pt(X0 + static_cast<int>(distFromStart * horizontalScale),
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

  if(!widgetVisible || legList.elevationLegs.isEmpty() || legList.routeApprMapObjects.isEmpty())
  {
    symPainter.textBox(&painter, {tr("No Flight Plan loaded.")}, QPen(Qt::black),
                       X0 + w / 4, Y0 + h / 2, textatt::BOLD, 255);
    return;
  }

  // Draw the mountains
  painter.setBrush(mapcolors::profileLandColor);
  painter.setPen(mapcolors::profileLandOutlineColor);
  painter.drawPolygon(landPolygon);

  // Draw grey vertical lines for waypoints
  int flightplanY = Y0 + static_cast<int>(h - flightplanAltFt * verticalScale);
  painter.setPen(mapcolors::profileWaypointLinePen);
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
  painter.setPen(mapcolors::profleElevationScalePen);
  for(int i = Y0 + h, alt = 0; i > Y0; i -= step * tempScale, alt += step)
  {
    painter.drawLine(X0, i, X0 + static_cast<int>(w), i);

    symPainter.textBox(&painter, {QString::number(alt, 'f', 0)},
                       mapcolors::profleElevationScalePen, X0 - 8, i, textatt::BOLD | textatt::RIGHT, 0);

    symPainter.textBox(&painter, {QString::number(alt, 'f', 0)},
                       mapcolors::profleElevationScalePen, X0 + w + 4, i, textatt::BOLD | textatt::LEFT, 0);
  }

  const RouteMapObjectList& route = routeController->getRouteApprMapObjects();

  // Draw orange minimum safe altitude lines for each segment
  painter.setPen(mapcolors::profileSafeAltLegLinePen);
  for(int i = 0; i < legList.elevationLegs.size(); i++)
  {
    const ElevationLeg& leg = legList.elevationLegs.at(i);
    int lineY = Y0 + static_cast<int>(h - calcGroundBuffer(leg.maxElevation) * verticalScale);
    painter.drawLine(waypointX.at(i), lineY, waypointX.at(i + 1), lineY);
  }

  // Draw the red minimum safe altitude line
  painter.setPen(mapcolors::profileSafeAltLinePen);
  int maxAltY = Y0 + static_cast<int>(h - minSafeAltitudeFt * verticalScale);
  painter.drawLine(X0, maxAltY, X0 + static_cast<int>(w), maxAltY);

  int todX;
  if(route.getTopOfDescentFromStart() > 0.f)
    todX = X0 + atools::roundToInt(route.getTopOfDescentFromStart() * horizontalScale);
  else
    todX = X0;

  // Draw the flightplan line
  float destAlt = legList.routeApprMapObjects.last().getPosition().getAltitude();
  QPolygon line;
  line << QPoint(X0, flightplanY);
  if(todX < X0 + w)
  {
    line << QPoint(todX, flightplanY);
    line << QPoint(X0 + w, Y0 + static_cast<int>(h - destAlt * verticalScale));
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

  // Draw the most unimportant symbols and texts first
  for(int i = legList.routeApprMapObjects.size() - 1; i >= 0; i--)
  {
    const RouteMapObject& rmo = legList.routeApprMapObjects.at(i);
    if(!rmo.isMissed())
    {
      int symx = waypointX.at(i);

      maptypes::MapObjectTypes type = rmo.getMapObjectType();

      if(type == maptypes::WAYPOINT || rmo.getWaypoint().isValid())
      {
        symPainter.drawWaypointSymbol(&painter, QColor(), symx, flightplanY, 8, true, false);
        symPainter.drawWaypointText(&painter, rmo.getWaypoint(), symx - 5, flightplanY + 18, flags, 10, true);
      }
      else if(type == maptypes::USER)
      {
        symPainter.drawUserpointSymbol(&painter, symx, flightplanY, 8, true, false);
        symPainter.textBox(&painter, {rmo.getIdent()}, mapcolors::routeUserPointColor,
                           symx - 5, flightplanY + 18, textatt::BOLD | textatt::ROUTE_BG_COLOR, 255);
      }
      else if(type == maptypes::INVALID)
      {
        symPainter.drawWaypointSymbol(&painter, mapcolors::routeInvalidPointColor, symx, flightplanY, 8, true, false);
        symPainter.textBox(&painter, {rmo.getIdent()}, mapcolors::routeInvalidPointColor,
                           symx - 5, flightplanY + 18, textatt::BOLD | textatt::ROUTE_BG_COLOR, 255);
      }
      else if(type == maptypes::APPROACH || type == maptypes::APPROACH_TRANSITION)
        symPainter.drawApproachSymbol(&painter, symx, flightplanY, 6, true, false);
    }
  }

  // Draw the more important radio navaids
  for(int i = legList.routeApprMapObjects.size() - 1; i >= 0; i--)
  {
    const RouteMapObject& rmo = legList.routeApprMapObjects.at(i);
    if(!rmo.isMissed())
    {
      int symx = waypointX.at(i);

      maptypes::MapObjectTypes type = rmo.getMapObjectType();

      if(type == maptypes::NDB || rmo.getNdb().isValid())
      {
        symPainter.drawNdbSymbol(&painter, symx, flightplanY, 12, true, false);
        symPainter.drawNdbText(&painter, rmo.getNdb(), symx - 5, flightplanY + 18, flags, 10, true);
      }
      else if(type == maptypes::VOR || rmo.getVor().isValid())
      {
        symPainter.drawVorSymbol(&painter, rmo.getVor(), symx, flightplanY, 12, true, false, false);
        symPainter.drawVorText(&painter, rmo.getVor(), symx - 5, flightplanY + 18, flags, 10, true);
      }
    }
  }

  // Draw the most important airport symbols
  font.setBold(true);
  font.setPointSizeF(defaultFontSize);
  painter.setFont(font);
  for(int i = legList.routeApprMapObjects.size() - 1; i >= 0; i--)
  {
    const RouteMapObject& rmo = legList.routeApprMapObjects.at(i);
    if(!rmo.isMissed())
    {
      int symx = waypointX.at(i);

      int symy = flightplanY;
      // if(i == legList.routeMapObjects.size() - 1)
      // symy = Y0 + static_cast<int>(h - destAlt * verticalScale);

      if(rmo.getMapObjectType() == maptypes::AIRPORT)
      {
        symPainter.drawAirportSymbol(&painter, rmo.getAirport(), symx, symy, 10, false, false);
        symPainter.drawAirportText(&painter, rmo.getAirport(), symx - 5, symy + 22,
                                   OptionData::instance().getDisplayOptions(), flags, 10, false);
      }
    }
  }

  if(legList.routeApprMapObjects.isConnectedToApproach())
  {
    const RouteMapObject& rmo = legList.routeMapObjects.last();
    if(rmo.getMapObjectType() == maptypes::AIRPORT)
    {
      int symx = waypointX.last();
      int symy = flightplanY;
      symPainter.drawAirportSymbol(&painter, rmo.getAirport(), symx, symy, 10, false, false);
      symPainter.drawAirportText(&painter, rmo.getAirport(), symx - 5, symy + 22,
                                 OptionData::instance().getDisplayOptions(), flags, 10, false);
    }
  }

  // Draw text labels
  // Departure altitude label
  float startAlt = legList.routeApprMapObjects.first().getPosition().getAltitude();
  QString startAltStr = Unit::altFeet(startAlt);
  symPainter.textBox(&painter, {startAltStr},
                     QPen(Qt::black), X0 - 8,
                     Y0 + static_cast<int>(h - startAlt * verticalScale),
                     textatt::BOLD | textatt::RIGHT, 255);

  // Destination altitude label
  QString destAltStr = Unit::altFeet(destAlt);
  symPainter.textBox(&painter, {destAltStr},
                     QPen(Qt::black), X0 + w + 4,
                     Y0 + static_cast<int>(h - destAlt * verticalScale),
                     textatt::BOLD | textatt::LEFT, 255);

  // Safe altitude label
  symPainter.textBox(&painter, {Unit::altFeet(minSafeAltitudeFt)},
                     QPen(Qt::red), X0 - 8, maxAltY + 5, textatt::BOLD | textatt::RIGHT, 255);

  // Route cruise altitude
  float routeAlt = route.getFlightplan().getCruisingAltitude();

  symPainter.textBox(&painter, {QLocale().toString(routeAlt, 'f', 0)},
                     QPen(Qt::black), X0 - 8, flightplanY + 5, textatt::BOLD | textatt::RIGHT, 255);

  if(!routeController->isFlightplanEmpty())
  {
    if(todX < X0 + w)
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
    if(!aircraftTrackPoints.isEmpty() && showAircraftTrack)
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
  updateTimer->start(ELEVATION_CHANGE_UPDATE_TIMEOUT_MS);
}

void ProfileWidget::routeChanged(bool geometryChanged)
{
  if(!widgetVisible || databaseLoadStatus)
    return;

  if(geometryChanged)
  {
    // // Terminate and wait for thread
    // terminateThread();
    // terminateThreadSignal = false;

    // Start thread after short delay to calculate new data
    updateTimer->start(ROUTE_CHANGE_UPDATE_TIMEOUT_MS);
  }
  else
  {
    // Only update screen
    updateScreenCoords();
    update();
  }

  updateLabel();
}

/* Called by updateTimer after any route or elevation updates and starts the thread */
void ProfileWidget::updateTimeout()
{
  if(!widgetVisible || databaseLoadStatus)
    return;

  qDebug() << "Profile update elevation timeout";

  // Terminate and wait for thread
  terminateThread();
  terminateThreadSignal = false;

  // Need a copy of the leg list before starting thread to avoid synchronization problems
  // Start the computation in background
  ElevationLegList legs;
  legs.routeApprMapObjects = routeController->getRouteApprMapObjects();
  legs.routeMapObjects = routeController->getRouteMapObjects();

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

  qDebug() << "Profile update finished";

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

        // Get altitude points for the line segment
        // The might not be complete and will be more complete on further iterations when we get a signal
        // from the elevation model
        QVector<GeoDataCoordinates> temp = elevationModel->heightProfile(
          c1.longitude(GeoDataCoordinates::Degree), c1.latitude(GeoDataCoordinates::Degree),
          c2.longitude(GeoDataCoordinates::Degree), c2.latitude(GeoDataCoordinates::Degree));

        for(const GeoDataCoordinates& c : temp)
        {
          if(terminateThreadSignal)
            return false;

          elevations.append(Pos(c.longitude(), c.latitude(), c.altitude()).toDeg());
        }

        if(elevations.isEmpty())
        {
          // Workaround for invalid geometry data - add void
          elevations.append(Pos(c1.longitude(), c1.latitude(), c1.altitude()).toDeg());
          elevations.append(Pos(c2.longitude(), c2.latitude(), c2.altitude()).toDeg());
        }
      }
    }
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
  for(int i = 1; i < legs.routeApprMapObjects.size(); i++)
  {
    if(terminateThreadSignal)
      // Return empty result
      return ElevationLegList();

    const RouteMapObject& rmo = legs.routeApprMapObjects.at(i);
    if(rmo.isMissed())
      break;

    const RouteMapObject& lastRmo = legs.routeApprMapObjects.at(i - 1);
    ElevationLeg leg;

    if(rmo.getDistanceTo() < ELEVATION_MAX_LEG_NM)
    {
      LineString geometry;
      if(rmo.isAnyApproach() && rmo.getGeometry().size() > 2)
        geometry = rmo.getGeometry();
      else
        geometry << lastRmo.getPosition() << rmo.getPosition();

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
        // Limit ground altitude to 30000 feet
        float altFeet = std::min(meterToFeet(coord.getAltitude()), ALTITUDE_LIMIT_FT);
        coord.setAltitude(altFeet);

        // Drop points with similar altitude except the first and last one on a segment
        if(geometry.size() == 2 && lastPos.isValid() && j != 0 && j != elevations.size() - 1 &&
           legs.elevationLegs.size() > 2 && atools::almostEqual(altFeet, lastPos.getAltitude(), 10.f))
          continue;

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

      legs.totalDistance += rmo.getDistanceTo();
      leg.elevation.append(lastPos);
      leg.distances.append(legs.totalDistance);

    }
    else
    {
      float dist = meterToNm(lastRmo.getPosition().distanceMeterTo(rmo.getPosition()));
      leg.distances.append(legs.totalDistance);
      legs.totalDistance += dist;
      leg.distances.append(legs.totalDistance);
      leg.elevation.append(lastRmo.getPosition());
      leg.elevation.append(rmo.getPosition());
    }
    legs.elevationLegs.append(leg);
  }

  return legs;
}

void ProfileWidget::showEvent(QShowEvent *)
{
  widgetVisible = true;
  // Start update immediately
  updateTimer->start(0);
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
  if(!widgetVisible || legList.elevationLegs.isEmpty() || legList.routeApprMapObjects.isEmpty())
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
  QString from = legList.routeApprMapObjects.at(index).getIdent();
  QString to = legList.routeApprMapObjects.at(index + 1).getIdent();

  // Add text to upper dock window label
  variableLabelText =
    from + tr(" ► ") + to + tr(", ") +
    Unit::distNm(distance) + tr(" ► ") +
    Unit::distNm(distanceToGo) + tr(", ") +
    tr(" Ground Elevation ") + Unit::altFeet(alt) +
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
    if(routeController->getRouteApprMapObjects().getRouteDistances(&distFromStartNm, &distToDestNm))
    {
      float toTod = routeController->getRouteApprMapObjects().getTopOfDescentFromStart() - distFromStartNm;

      fixedLabelText = tr("<b>To Destination %1, to Top of Descent %2.</b>&nbsp;&nbsp;").
                       arg(Unit::distNm(distToDestNm)).
                       arg(toTod > 0 ? Unit::distNm(toTod) : tr("Passed"));
    }
  }
  else
    fixedLabelText.clear();

  mainWindow->getUi()->labelElevationInfo->setText(fixedLabelText + " " + variableLabelText);
}

/* Cursor leaves widget. Stop displaying the rubberband */
void ProfileWidget::leaveEvent(QEvent *)
{
  if(!widgetVisible || legList.elevationLegs.isEmpty() || legList.routeApprMapObjects.isEmpty())
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
  Ui::MainWindow *ui = mainWindow->getUi();

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
