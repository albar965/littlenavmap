/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

ProfileWidget::ProfileWidget(MainWindow *parent)
  : QWidget(parent), mainWindow(parent)
{
  setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
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
  if(databaseLoadStatus)
    return;

  bool updateWidget = false;

  if(!routeController->isFlightplanEmpty())
  {
    const RouteMapObjectList& rmoList = legList.routeMapObjects;

    if(showAircraft || showAircraftTrack)
    {
      simData = simulatorData;
      if(rmoList.getRouteDistances(simData.getPosition(), &aircraftDistanceFromStart, &aircraftDistanceToDest))
      {
        // Get screen point from last update
        QPoint lastPoint;
        if(lastSimData.getPosition().isValid())
          lastPoint = QPoint(X0 + static_cast<int>(aircraftDistanceFromStart * horizontalScale),
                             Y0 + static_cast<int>(rect().height() - Y0 -
                                                   lastSimData.getPosition().getAltitude() * verticalScale));

        // Get screen point for current update
        QPoint currentPoint(X0 + static_cast<int>(aircraftDistanceFromStart * horizontalScale),
                            Y0 + static_cast<int>(rect().height() - Y0 -
                                                  simData.getPosition().getAltitude() * verticalScale));

        if(aircraftTrackPoints.isEmpty() || (aircraftTrackPoints.last() - currentPoint).manhattanLength() > 3)
        {
          // Add track point and update widget if delta value between last and current update is large enough
          if(simData.getPosition().isValid())
          {
            aircraftTrackPoints.append(currentPoint);
            updateWidget = true;
          }
        }

        const SimUpdateDelta& deltas = SIM_UPDATE_DELTA_MAP.value(OptionData::instance().getSimUpdateRate());

        using atools::almostNotEqual;
        if(!lastSimData.getPosition().isValid() ||
           (lastPoint - currentPoint).manhattanLength() > deltas.manhattanLengthDelta ||
           almostNotEqual(lastSimData.getPosition().getAltitude(),
                          simData.getPosition().getAltitude(), deltas.altitudeDelta))
        {
          // Aircraft position has changed enough
          lastSimData = simData;
          if(simData.getPosition().getAltitude() > maxWindowAlt)
            // Scale up to keep the aircraft visible
            updateScreenCoords();
          updateWidget = true;
        }
      }
    }
    else
    {
      // Neither aircraft nor track shown - update simulator data only
      bool valid = simData.getPosition().isValid();
      simData = atools::fs::sc::SimConnectData();
      if(valid)
        updateWidget = true;
    }
  }
  if(updateWidget)
    update();
}

void ProfileWidget::connectedToSimulator()
{
  disconnectedFromSimulator();
}

void ProfileWidget::disconnectedFromSimulator()
{
  simData = atools::fs::sc::SimConnectData();
  updateScreenCoords();
  update();
}

/* Update all screen coordinates and scale factors */
void ProfileWidget::updateScreenCoords()
{
  MapWidget *mapWidget = mainWindow->getMapWidget();

  // Widget drawing region width and height
  int w = rect().width() - X0 * 2, h = rect().height() - Y0;

  // Update elevation polygon
  // Add 1000 ft buffer and round up to the next 500 feet
  minSafeAltitudeFt = std::ceil((legList.maxElevationFt +
                                 OptionData::instance().getRouteGroundBuffer()) / 500.f) * 500.f;
  flightplanAltFt =
    static_cast<float>(routeController->getRouteMapObjects().getFlightplan().getCruisingAltitude());
  maxWindowAlt = std::max(minSafeAltitudeFt, flightplanAltFt);

  if(simData.getPosition().isValid() &&
     (showAircraft || showAircraftTrack) && !routeController->isFlightplanEmpty())
    maxWindowAlt = std::max(maxWindowAlt, simData.getPosition().getAltitude());

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
    const RouteMapObjectList& rmoList = legList.routeMapObjects;
    const AircraftTrack& aircraftTrack = mapWidget->getAircraftTrack();

    for(int i = 0; i < aircraftTrack.size(); i++)
    {
      const Pos& p = aircraftTrack.at(i).pos;
      float distFromStart = 0.f;
      if(rmoList.getRouteDistances(p, &distFromStart, nullptr))
      {
        QPoint pt(X0 + static_cast<int>(distFromStart * horizontalScale),
                  Y0 + static_cast<int>(rect().height() - Y0 - p.getAltitude() * verticalScale));

        if(aircraftTrackPoints.isEmpty() || (aircraftTrackPoints.last() - pt).manhattanLength() > 3)
          aircraftTrackPoints.append(pt);
      }
    }
  }
}

void ProfileWidget::paintEvent(QPaintEvent *)
{
  int w = rect().width() - X0 * 2, h = rect().height() - Y0;

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), mapcolors::profileBackgroundColor);
  painter.fillRect(X0, 0, w, h + Y0, mapcolors::profileSkyColor);

  SymbolPainter symPainter;

  if(!widgetVisible || legList.elevationLegs.isEmpty() || legList.routeMapObjects.isEmpty())
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

  // Find best scale for elevation lines
  int step = 10000;
  for(int s : SCALE_STEPS)
  {
    // Loop through all scale steps and check which one fits best
    if(s * verticalScale > MIN_SCALE_SCREEN_DISTANCE)
    {
      step = s;
      break;
    }
  }

  // Draw elevation scale line and texts
  painter.setPen(mapcolors::profleElevationScalePen);
  for(int i = Y0 + h, alt = 0; i > Y0; i -= step * verticalScale, alt += step)
  {
    painter.drawLine(X0, i, X0 + static_cast<int>(w), i);

    symPainter.textBox(&painter, {QLocale().toString(alt)},
                       mapcolors::profleElevationScalePen, X0 - 8, i, textatt::BOLD | textatt::RIGHT, 0);

    symPainter.textBox(&painter, {QLocale().toString(alt)},
                       mapcolors::profleElevationScalePen, X0 + w + 4, i, textatt::BOLD | textatt::LEFT, 0);
  }

  // Draw the red maximum elevation line
  painter.setPen(mapcolors::profileSafeAltLinePen);
  int maxAltY = Y0 + static_cast<int>(h - minSafeAltitudeFt * verticalScale);
  painter.drawLine(X0, maxAltY, X0 + static_cast<int>(w), maxAltY);

  // Draw the flightplan line
  painter.setPen(QPen(mapcolors::routeOutlineColor, 4, Qt::SolidLine));
  painter.drawLine(X0, flightplanY, X0 + static_cast<int>(w), flightplanY);

  painter.setPen(QPen(mapcolors::routeColor, 2, Qt::SolidLine));
  painter.drawLine(X0, flightplanY, X0 + w, flightplanY);

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
  for(int i = legList.routeMapObjects.size() - 1; i >= 0; i--)
  {
    const RouteMapObject& rmo = legList.routeMapObjects.at(i);
    int symx = waypointX.at(i);

    switch(rmo.getMapObjectType())
    {
      case maptypes::WAYPOINT:
        symPainter.drawWaypointSymbol(&painter, QColor(), symx, flightplanY, 8, true, false);
        symPainter.drawWaypointText(&painter, rmo.getWaypoint(), symx - 5, flightplanY + 18, flags, 10, true);
        break;
      case maptypes::USER:
        symPainter.drawUserpointSymbol(&painter, symx, flightplanY, 8, true, false);
        symPainter.textBox(&painter, {rmo.getIdent()}, mapcolors::routeUserPointColor,
                           symx - 5, flightplanY + 18, textatt::BOLD | textatt::ROUTE_BG_COLOR, 255);
        break;
      case maptypes::INVALID:
        symPainter.drawWaypointSymbol(&painter, mapcolors::routeInvalidPointColor, symx, flightplanY, 8, true,
                                      false);
        symPainter.textBox(&painter, {rmo.getIdent()}, mapcolors::routeInvalidPointColor,
                           symx - 5, flightplanY + 18, textatt::BOLD | textatt::ROUTE_BG_COLOR, 255);
        break;
    }
  }

  // Draw the more important radio navaids
  for(int i = legList.routeMapObjects.size() - 1; i >= 0; i--)
  {
    const RouteMapObject& rmo = legList.routeMapObjects.at(i);
    int symx = waypointX.at(i);

    switch(rmo.getMapObjectType())
    {
      case maptypes::NDB:
        symPainter.drawNdbSymbol(&painter, symx, flightplanY, 12, true, false);
        symPainter.drawNdbText(&painter, rmo.getNdb(), symx - 5, flightplanY + 18,
                               flags, 10, true);
        break;
      case maptypes::VOR:
        symPainter.drawVorSymbol(&painter, rmo.getVor(), symx, flightplanY, 12, true, false, false);
        symPainter.drawVorText(&painter, rmo.getVor(), symx - 5, flightplanY + 18,
                               flags, 10, true);
        break;
    }
  }

  // Draw the most important airport symbols
  font.setBold(true);
  font.setPointSizeF(defaultFontSize);
  painter.setFont(font);
  for(int i = legList.routeMapObjects.size() - 1; i >= 0; i--)
  {
    const RouteMapObject& rmo = legList.routeMapObjects.at(i);
    int symx = waypointX.at(i);

    if(rmo.getMapObjectType() == maptypes::AIRPORT)
    {
      symPainter.drawAirportSymbol(&painter, rmo.getAirport(), symx, flightplanY, 10, false, false);
      symPainter.drawAirportText(&painter, rmo.getAirport(), symx - 5, flightplanY + 22, flags, 10, false);
    }
  }

  // Draw text lables
  // Departure altitude label
  float startAlt = legList.routeMapObjects.first().getPosition().getAltitude();
  QString startAltStr = QLocale().toString(startAlt, 'f', 0) + tr(" ft");
  symPainter.textBox(&painter, {startAltStr},
                     QPen(Qt::black), X0 - 8,
                     Y0 + static_cast<int>(h - startAlt * verticalScale),
                     textatt::BOLD | textatt::RIGHT, 255);

  // Destination altitude label
  float destAlt = legList.routeMapObjects.last().getPosition().getAltitude();
  QString destAltStr = QLocale().toString(destAlt, 'f', 0) + tr(" ft");
  symPainter.textBox(&painter, {destAltStr},
                     QPen(Qt::black), X0 + w + 4,
                     Y0 + static_cast<int>(h - destAlt * verticalScale),
                     textatt::BOLD | textatt::LEFT, 255);

  // Safe altitude label
  symPainter.textBox(&painter, {QLocale().toString(minSafeAltitudeFt, 'f', 0) + tr(" ft")},
                     QPen(Qt::red), X0 - 8, maxAltY + 5, textatt::BOLD | textatt::RIGHT, 255);

  // Route cruise altitude
  QString routeAlt = QLocale().toString(
    routeController->getRouteMapObjects().getFlightplan().getCruisingAltitude()) + tr(" ft");
  symPainter.textBox(&painter, {routeAlt},
                     QPen(Qt::black), X0 - 8, flightplanY + 5, textatt::BOLD | textatt::RIGHT, 255);

  if(!routeController->isFlightplanEmpty())
  {
    // Draw user aircraft track
    if(!aircraftTrackPoints.isEmpty() && showAircraftTrack)
    {
      painter.setPen(mapcolors::aircraftTrackPen);
      painter.drawPolyline(aircraftTrackPoints);
    }

    // Draw user aircraft
    if(simData.getPosition().isValid() && showAircraft)
    {
      int acx = X0 + static_cast<int>(aircraftDistanceFromStart * horizontalScale);
      int acy = Y0 + static_cast<int>(h - simData.getPosition().getAltitude() * verticalScale);

      // Draw aircraft symbol
      painter.translate(acx, acy);
      painter.rotate(90);
      symPainter.drawAircraftSymbol(&painter, 0, 0, 20, simData.getFlags() & atools::fs::sc::ON_GROUND);
      painter.resetTransform();

      // Draw aircraft label
      font.setPointSizeF(defaultFontSize);
      painter.setFont(font);

      QString upDown;
      if(simData.getVerticalSpeedFeetPerMin() > 100.f)
        upDown = tr(" ▲");
      else if(simData.getVerticalSpeedFeetPerMin() < -100.f)
        upDown = tr(" ▼");

      QStringList texts;
      texts.append(QLocale().toString(simData.getPosition().getAltitude(), 'f', 0) + tr(" ft") + upDown);
      texts.append(QLocale().toString(aircraftDistanceFromStart, 'f', 0) + tr(" nm ► ") +
                   QLocale().toString(aircraftDistanceToDest, 'f', 0) + tr(" nm"));

      textatt::TextAttributes att = textatt::BOLD;
      int textx = acx, texty = acy + 20;

      QRect rect = symPainter.textBoxSize(&painter, texts, att);
      if(textx + rect.right() > X0 + w)
        // Move text to the left when approaching the right corner
        att |= textatt::RIGHT;

      if(texty + rect.bottom() > Y0 + h)
        // Move text down when approaching top boundary
        texty -= rect.bottom() + 20;

      symPainter.textBox(&painter, texts, QPen(Qt::black), textx, texty, att, 255);
    }
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
    // mainWindow->statusMessage(tr("Elevation data updated."));
  }
}

/* Get elevation points between the two points. This returns also correct results if the antimeridian is crossed
 * @return true if not aborted */
bool ProfileWidget::fetchRouteElevations(Marble::GeoDataLineString& elevations,
                                         const atools::geo::Pos& lastPos,
                                         const atools::geo::Pos& curPos) const
{
  // Create a line string from the two points and split it at the date line if crossing
  GeoDataLineString coords;
  coords.setTessellate(true);
  coords << GeoDataCoordinates(lastPos.getLonX(), lastPos.getLatY(), 0., GeoDataCoordinates::Degree)
         << GeoDataCoordinates(curPos.getLonX(), curPos.getLatY(), 0., GeoDataCoordinates::Degree);

  QVector<Marble::GeoDataLineString *> coordsCorrected = coords.toDateLineCorrected();
  for(const Marble::GeoDataLineString *ls : coordsCorrected)
  {
    for(int j = 1; j < ls->size(); j++)
    {
      if(terminateThreadSignal)
        return false;

      const Marble::GeoDataCoordinates& c1 = ls->at(j - 1);
      const Marble::GeoDataCoordinates& c2 = ls->at(j);

      // qDebug() << c1.toString(GeoDataCoordinates::Decimal)
      // << c2.toString(GeoDataCoordinates::Decimal);

      // Get altitude points for the line segment
      // The might not be complete and will be more complete on further iterations when we get a signal
      // from the elevation model
      QVector<GeoDataCoordinates> temp = elevationModel->heightProfile(
        c1.longitude(GeoDataCoordinates::Degree),
        c1.latitude(GeoDataCoordinates::Degree),
        c2.longitude(GeoDataCoordinates::Degree),
        c2.latitude(GeoDataCoordinates::Degree));

      // qDebug() << temp.first().toString(GeoDataCoordinates::Decimal)
      // << temp.first().altitude();
      // qDebug() << temp.at(temp.size() / 4).toString(GeoDataCoordinates::Decimal)
      // << temp.at(temp.size() / 4).altitude();
      // qDebug() << temp.at(temp.size() / 2).toString(GeoDataCoordinates::Decimal)
      // << temp.at(temp.size() / 2).altitude();
      // qDebug() << temp.at(temp.size() / 4 * 3).toString(GeoDataCoordinates::Decimal)
      // << temp.at(temp.size() / 4 * 3).altitude();
      // qDebug() << temp.last().toString(GeoDataCoordinates::Decimal)
      // << temp.last().altitude();

      for(const GeoDataCoordinates& c : temp)
      {
        if(terminateThreadSignal)
          return false;

        elevations.append(c);
      }

      if(elevations.isEmpty())
      {
        // Workaround for invalid geometry data - add void
        elevations.append(c1);
        elevations.append(c2);
      }
    }
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
  for(int i = 1; i < legs.routeMapObjects.size(); i++)
  {
    if(terminateThreadSignal)
      // Return empty result
      return ElevationLegList();

    const RouteMapObject& lastRmo = legs.routeMapObjects.at(i - 1);
    const RouteMapObject& rmo = legs.routeMapObjects.at(i);

    GeoDataLineString elevations;
    elevations.setTessellate(true);
    if(!fetchRouteElevations(elevations, lastRmo.getPosition(), rmo.getPosition()))
      return ElevationLegList();

    // Loop over all elevation points for the current leg
    ElevationLeg leg;
    Pos lastPos;
    for(int j = 0; j < elevations.size(); j++)
    {
      if(terminateThreadSignal)
        return ElevationLegList();

      const GeoDataCoordinates& coord = elevations.at(j);
      double altFeet = meterToFeet(coord.altitude());

      // Limit ground altitude to 30000 feet
      altFeet = std::min(altFeet, 30000.);

      Pos pos(coord.longitude(GeoDataCoordinates::Degree), coord.latitude(GeoDataCoordinates::Degree),
              altFeet);

      // Drop points with similar altitude except the first and last one on a segment
      if(lastPos.isValid() && j != 0 && j != elevations.size() - 1 &&
         legs.elevationLegs.size() > 2 &&
         atools::almostEqual(pos.getAltitude(), lastPos.getAltitude(), 10.f))
        continue;

      float alt = pos.getAltitude();

      // Adjust maximum
      if(alt > leg.maxElevation)
        leg.maxElevation = alt;
      if(alt > legs.maxElevationFt)
        legs.maxElevationFt = alt;

      leg.elevation.append(pos);
      if(j > 0)
      {
        // Update total distance
        float dist = meterToNm(lastPos.distanceMeterTo(pos));
        legs.totalDistance += dist;
      }
      // Distance to elevation point from departure
      leg.distances.append(legs.totalDistance);

      legs.totalNumPoints++;
      lastPos = pos;
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
  if(!widgetVisible || legList.elevationLegs.isEmpty() || legList.routeMapObjects.isEmpty())
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
  float maxElev =
    std::ceil((leg.maxElevation + OptionData::instance().getRouteGroundBuffer()) / 500.f) * 500.f;

  // Get from/to text
  QString from = legList.routeMapObjects.at(index).getIdent();
  QString to = legList.routeMapObjects.at(index + 1).getIdent();

  // Add text to upper dock window label
  mainWindow->getUi()->labelElevationInfo->setText(
    tr("<b>") + from + tr(" ► ") + to + tr("</b>, ") +
    QLocale().toString(distance, 'f', distance < 100.f ? 1 : 0) + tr(" ► ") +
    QLocale().toString(distanceToGo, 'f', distanceToGo < 100.f ? 1 : 0) + tr(" nm, ") +
    tr(" Ground Altitude ") + QLocale().toString(alt, 'f', 0) + tr(" ft, ") +
    tr(" Above Ground Altitude ") + QLocale().toString(flightplanAltFt - alt, 'f', 0) + tr(" ft, ") +
    tr(" Leg Safe Altitude ") + QLocale().toString(maxElev, 'f', 0) + tr(" ft"));

  mouseEvent->accept();

  // Tell map widget to create a rubberband rectangle on the map
  emit highlightProfilePoint(pos);
}

/* Cursor leaves widget. Stop displaying the rubberband */
void ProfileWidget::leaveEvent(QEvent *)
{
  if(!widgetVisible || legList.elevationLegs.isEmpty() || legList.routeMapObjects.isEmpty())
    return;

  qDebug() << "leave";

  delete rubberBand;
  rubberBand = nullptr;

  mainWindow->getUi()->labelElevationInfo->setText(tr("No information."));

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
