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
using atools::geo::Pos;

ProfileWidget::ProfileWidget(MainWindow *parent)
  : QWidget(parent), mainWindow(parent)
{
  setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
  elevationModel = mainWindow->getElevationModel();
  routeController = mainWindow->getRouteController();

  updateTimer = new QTimer(this);
  updateTimer->setSingleShot(true);
  connect(updateTimer, &QTimer::timeout, this, &ProfileWidget::updateTimeout);

  connect(elevationModel, &Marble::ElevationModel::updateAvailable,
          this, &ProfileWidget::updateElevation);
  connect(&watcher, &QFutureWatcher<ElevationLegList>::finished, this, &ProfileWidget::updateThreadFinished);

  setMouseTracking(true);
}

ProfileWidget::~ProfileWidget()
{
  terminateThread();
}

void ProfileWidget::routeChanged(bool geometryChanged)
{
  if(!visible)
    return;

  if(geometryChanged)
  {
    qDebug() << "Profile route geometry changed";
    updateTimer->start(UPDATE_TIMEOUT);
  }
  else
  {
    updateScreenCoords();
    update();
  }
}

void ProfileWidget::simDataChanged(const atools::fs::sc::SimConnectData& simulatorData)
{
  if(databaseLoadStatus)
    return;

  bool updateProfile = false;

  if(!routeController->isFlightplanEmpty())
  {
    const RouteMapObjectList& rmoList = legList.routeMapObjects;

    if(showAircraft || showAircraftTrack)
    {
      simData = simulatorData;
      if(rmoList.getRouteDistances(simData.getPosition(), &aircraftDistanceFromStart, &aircraftDistanceToDest))
      {
        QPoint lastPt;
        if(lastSimData.getPosition().isValid())
          lastPt = QPoint(X0 + static_cast<int>(aircraftDistanceFromStart * horizScale),
                          Y0 + static_cast<int>(rect().height() - Y0 -
                                                lastSimData.getPosition().getAltitude() * vertScale));

        QPoint pt(X0 + static_cast<int>(aircraftDistanceFromStart * horizScale),
                  Y0 + static_cast<int>(rect().height() - Y0 -
                                        simData.getPosition().getAltitude() * vertScale));

        if(aircraftTrackPoints.isEmpty() || (aircraftTrackPoints.last() - pt).manhattanLength() > 3)
        {
          if(simData.getPosition().isValid())
          {
            aircraftTrackPoints.append(pt);
            updateProfile = true;
          }
        }

        const SimUpdateDelta& deltas = SIM_UPDATE_DELTA_MAP.value(OptionData::instance().getSimUpdateRate());

        using atools::almostNotEqual;
        if(!lastSimData.getPosition().isValid() ||
           (lastPt - pt).manhattanLength() > deltas.manhattanLengthDelta ||
           almostNotEqual(lastSimData.getPosition().getAltitude(),
                          simData.getPosition().getAltitude(), deltas.altitudeDelta))
        {
          lastSimData = simData;
          if(simData.getPosition().getAltitude() > maxAlt)
            // Scale up to keep the aircraft visible
            updateScreenCoords();
          updateProfile = true;
        }
      }
    }
    else
    {
      bool valid = simData.getPosition().isValid();
      simData = atools::fs::sc::SimConnectData();
      if(valid)
        updateProfile = true;
    }
  }
  if(updateProfile)
    update();
}

void ProfileWidget::disconnectedFromSimulator()
{
  simData = atools::fs::sc::SimConnectData();
  updateScreenCoords();
  update();
}

void ProfileWidget::updateScreenCoords()
{
  MapWidget *mapWidget = mainWindow->getMapWidget();

  int w = rect().width() - X0 * 2, h = rect().height() - Y0;

  // Update elevation polygon
  // Add 1000 ft buffer and round up to the next 500 feet
  minSafeAltitudeFt = std::ceil((legList.maxElevationFt +
                                 OptionData::instance().getRouteGroundBuffer()) / 500.f) * 500.f;
  flightplanAltFt = static_cast<float>(routeController->getFlightplan().getCruisingAlt());
  maxAlt = std::max(minSafeAltitudeFt, flightplanAltFt);

  if(simData.getPosition().isValid() &&
     (showAircraft || showAircraftTrack) && !routeController->isFlightplanEmpty())
    maxAlt = std::max(maxAlt, simData.getPosition().getAltitude());

  vertScale = h / maxAlt;
  horizScale = w / legList.totalDistance;

  waypointX.clear();
  landPolygon.clear();
  landPolygon.append(QPoint(X0, h + Y0));

  for(const ElevationLeg& leg : legList.elevationLegs)
  {
    waypointX.append(X0 + static_cast<int>(leg.distances.first() * horizScale));

    QPoint lastPt;
    for(int i = 0; i < leg.elevation.size(); i++)
    {
      float alt = leg.elevation.at(i).getAltitude();
      QPoint pt(X0 + static_cast<int>(leg.distances.at(i) * horizScale),
                Y0 + static_cast<int>(h - alt * vertScale));

      if(lastPt.isNull() || i == leg.elevation.size() - 1 || (lastPt - pt).manhattanLength() > 2)
      {
        landPolygon.append(pt);
        lastPt = pt;
      }
    }
  }
  waypointX.append(X0 + w);
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
        QPoint pt(X0 + static_cast<int>(distFromStart * horizScale),
                  Y0 + static_cast<int>(rect().height() - Y0 - p.getAltitude() * vertScale));

        if(aircraftTrackPoints.isEmpty() || (aircraftTrackPoints.last() - pt).manhattanLength() > 3)
          aircraftTrackPoints.append(pt);
      }
    }
  }
}

void ProfileWidget::paintEvent(QPaintEvent *)
{
  // QElapsedTimer etimer;
  // etimer.start();

  int w = rect().width() - X0 * 2, h = rect().height() - Y0;

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), mapcolors::profileBackgroundColor);
  painter.fillRect(X0, 0, w, h + Y0, mapcolors::profileSkyColor);

  SymbolPainter symPainter;

  if(!visible || legList.elevationLegs.isEmpty() || legList.routeMapObjects.isEmpty())
  {
    symPainter.textBox(&painter, {tr("No Flight Plan loaded.")}, QPen(Qt::black),
                       X0 + w / 4, Y0 + h / 2, textatt::BOLD, 255);
    return;
  }

  // Draw the mountains
  // QLinearGradient gradient(QPointF(0.5f,1),QPointF(0.5f,0));
  // gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
  // gradient.setColorAt(0, Qt::darkGreen);
  // gradient.setColorAt(1, Qt::lightGray);
  // QBrush mountainBrush(gradient);
  painter.setBrush( /*mountainBrush */ mapcolors::profileLandColor);
  painter.setPen(mapcolors::profileLandOutlineColor);
  painter.drawPolygon(landPolygon);

  // Draw grey vertical lines for waypoints
  int flightplanY = Y0 + static_cast<int>(h - flightplanAltFt * vertScale);
  painter.setPen(mapcolors::profileWaypointLinePen);
  for(int wpx : waypointX)
    painter.drawLine(wpx, flightplanY, wpx, Y0 + h);

  // Find best scale for elevation lines
  int step = 10000;
  for(int s : SCALE_STEPS)
  {
    if(s * vertScale > MIN_SCALE_SCREEN_DISTANCE)
    {
      step = s;
      break;
    }
  }

  // Draw elevation scale
  painter.setPen(mapcolors::profleElevationScalePen);
  for(int i = Y0 + h, a = 0; i > Y0; i -= step * vertScale, a += step)
  {
    painter.drawLine(X0, i, X0 + static_cast<int>(w), i);

    symPainter.textBox(&painter, {QLocale().toString(a)},
                       mapcolors::profleElevationScalePen, X0 - 8, i, textatt::BOLD | textatt::RIGHT, 0);

    symPainter.textBox(&painter, {QLocale().toString(a)},
                       mapcolors::profleElevationScalePen, X0 + w + 4, i, textatt::BOLD | textatt::LEFT, 0);
  }

  // Draw the red maximum elevation line
  painter.setPen(mapcolors::profileSafeAltLinePen);
  int maxAltY = Y0 + static_cast<int>(h - minSafeAltitudeFt * vertScale);
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

  // Draw the most unimportant symbols first
  for(int i = legList.routeMapObjects.size() - 1; i >= 0; i--)
  {
    const RouteMapObject& rmo = legList.routeMapObjects.at(i);
    int symx = waypointX.at(i);

    switch(rmo.getMapObjectType())
    {
      case maptypes::WAYPOINT:
        symPainter.drawWaypointSymbol(&painter, rmo.getWaypoint(),
                                      QColor(), symx, flightplanY, 8, true, false);
        symPainter.drawWaypointText(&painter, rmo.getWaypoint(), symx - 5, flightplanY + 18,
                                    flags, 10, true, false);
        break;
      case maptypes::USER:
        symPainter.drawUserpointSymbol(&painter, symx, flightplanY, 8, true, false);
        symPainter.textBox(&painter, {rmo.getIdent()}, mapcolors::routeUserPointColor,
                           symx - 5, flightplanY + 18, textatt::BOLD | textatt::ROUTE_BG_COLOR, 255);
        break;
      case maptypes::INVALID:
        symPainter.drawWaypointSymbol(&painter, rmo.getWaypoint(), mapcolors::routeInvalidPointColor,
                                      symx, flightplanY, 8, true, false);
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
        symPainter.drawNdbSymbol(&painter, rmo.getNdb(), symx, flightplanY, 12, true, false);
        symPainter.drawNdbText(&painter, rmo.getNdb(), symx - 5, flightplanY + 18,
                               flags, 10, true, false);
        break;
      case maptypes::VOR:
        symPainter.drawVorSymbol(&painter, rmo.getVor(), symx, flightplanY, 12, true, false, false);
        symPainter.drawVorText(&painter, rmo.getVor(), symx - 5, flightplanY + 18,
                               flags, 10, true, false);
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
      symPainter.drawAirportText(&painter, rmo.getAirport(), symx - 5, flightplanY + 22,
                                 flags, 10, false, true, false);
    }
  }

  // Draw text lables
  float startAlt = legList.routeMapObjects.first().getPosition().getAltitude();
  QString startAltStr = QLocale().toString(startAlt, 'f', 0) + tr(" ft");
  symPainter.textBox(&painter, {startAltStr},
                     QPen(Qt::black), X0 - 8,
                     Y0 + static_cast<int>(h - startAlt * vertScale),
                     textatt::BOLD | textatt::RIGHT, 255);

  float destAlt = legList.routeMapObjects.last().getPosition().getAltitude();
  QString destAltStr = QLocale().toString(destAlt, 'f', 0) + tr(" ft");
  symPainter.textBox(&painter, {destAltStr},
                     QPen(Qt::black), X0 + w + 4,
                     Y0 + static_cast<int>(h - destAlt * vertScale),
                     textatt::BOLD | textatt::LEFT, 255);

  symPainter.textBox(&painter, {QLocale().toString(minSafeAltitudeFt, 'f', 0) + tr(" ft")},
                     QPen(Qt::red), X0 - 8, maxAltY + 5, textatt::BOLD | textatt::RIGHT, 255);

  QString routeAlt = QLocale().toString(routeController->getFlightplan().getCruisingAlt()) + tr(" ft");
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
      int acx = X0 + static_cast<int>(aircraftDistanceFromStart * horizScale);
      int acy = Y0 + static_cast<int>(h - simData.getPosition().getAltitude() * vertScale);
      painter.translate(acx, acy);
      painter.rotate(90);
      symPainter.drawAircraftSymbol(&painter, 0, 0, 20, simData.getFlags() & atools::fs::sc::ON_GROUND);
      painter.resetTransform();

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
        att |= textatt::RIGHT;

      if(texty + rect.bottom() > Y0 + h)
        texty -= rect.bottom() + 20;

      symPainter.textBox(&painter, texts, QPen(Qt::black), textx, texty, att, 255);
    }
  }

  // qDebug() << "profile paint" << etimer.elapsed() << "ms";
}

void ProfileWidget::updateTimeout()
{
  if(!visible || databaseLoadStatus)
    return;

  qDebug() << "Profile update elevation timeout";

  terminateThread();
  terminate = false;

  // Need a copy of the leg list before starting thread to avoid synchronization problems
  // Start the computation in background
  ElevationLegList legs;
  legs.routeMapObjects = routeController->getRouteMapObjects();
  future = QtConcurrent::run(this, &ProfileWidget::fetchRouteElevationsThread, legs);
  watcher.setFuture(future);
}

void ProfileWidget::updateThreadFinished()
{
  if(!visible || databaseLoadStatus)
    return;

  qDebug() << "Profile update finished";

  if(!terminate)
  {
    legList = future.result();
    updateScreenCoords();
    update();
    // mainWindow->statusMessage(tr("Elevation data updated."));
  }
}

ProfileWidget::ElevationLegList ProfileWidget::fetchRouteElevationsThread(ElevationLegList legs) const
{
  using atools::geo::meterToNm;
  using atools::geo::meterToFeet;

  legs.totalNumPoints = 0;
  legs.totalDistance = 0.f;
  legs.maxElevationFt = 0.f;
  legs.elevationLegs.clear();

  for(int i = 1; i < legs.routeMapObjects.size(); i++)
  {
    if(terminate)
      return ElevationLegList();

    const RouteMapObject& lastRmo = legs.routeMapObjects.at(i - 1);
    const RouteMapObject& rmo = legs.routeMapObjects.at(i);

    QList<GeoDataCoordinates> elev = elevationModel->heightProfile(
      lastRmo.getPosition().getLonX(), lastRmo.getPosition().getLatY(),
      rmo.getPosition().getLonX(), rmo.getPosition().getLatY());

    if(elev.isEmpty())
    {
      // Workaround for invalid geometry data - add void
      elev.append(GeoDataCoordinates(lastRmo.getPosition().getLonX(),
                                     lastRmo.getPosition().getLatY(), 0., GeoDataCoordinates::Degree));
      elev.append(GeoDataCoordinates(rmo.getPosition().getLonX(),
                                     rmo.getPosition().getLatY(), 0., GeoDataCoordinates::Degree));
    }

    ElevationLeg leg;
    Pos lastPos;
    for(int j = 0; j < elev.size(); j++)
    {
      const GeoDataCoordinates& coord = elev.at(j);
      if(terminate)
        return ElevationLegList();

      Pos pos(coord.longitude(GeoDataCoordinates::Degree),
              coord.latitude(GeoDataCoordinates::Degree), meterToFeet(coord.altitude()));

      // Drop points with similar altitude except the first and last one on a segment
      if(lastPos.isValid() && j != 0 && j != elev.size() - 1 &&
         legs.elevationLegs.size() > 2 &&
         atools::almostEqual(pos.getAltitude(), lastPos.getAltitude(), 10.f))
        continue;

      float alt = pos.getAltitude();
      if(alt > leg.maxElevation)
        leg.maxElevation = alt;
      if(alt > legs.maxElevationFt)
        legs.maxElevationFt = alt;

      leg.elevation.append(pos);
      if(j > 0)
      {
        float dist = meterToNm(lastPos.distanceMeterTo(pos));
        legs.totalDistance += dist;
      }
      leg.distances.append(legs.totalDistance);

      legs.totalNumPoints++;
      lastPos = pos;
    }
    legs.elevationLegs.append(leg);
  }

  qDebug() << "elevation legs" << legs.elevationLegs.size()
           << "total points" << legs.totalNumPoints
           << "total distance" << legs.totalDistance
           << "max route elevation" << legs.maxElevationFt;
  return legs;
}

void ProfileWidget::updateElevation()
{
  if(!visible || databaseLoadStatus)
    return;

  qDebug() << "Profile update elevation";
  updateTimer->start(UPDATE_TIMEOUT);
}

void ProfileWidget::showEvent(QShowEvent *)
{
  visible = true;
  updateTimer->start(0);
}

void ProfileWidget::hideEvent(QHideEvent *)
{
  visible = false;
}

void ProfileWidget::mouseMoveEvent(QMouseEvent *mouseEvent)
{
  if(!visible || legList.elevationLegs.isEmpty() || legList.routeMapObjects.isEmpty())
    return;

  if(rubberBand == nullptr)
    rubberBand = new QRubberBand(QRubberBand::Line, this);

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

  float distance = (x - X0) / horizScale;
  if(distance < 0.f)
    distance = 0.f;

  float distanceToGo = legList.totalDistance - distance;
  if(distanceToGo < 0.f)
    distanceToGo = 0.f;

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
  float alt1 = leg.elevation.at(indexLowDist).getAltitude();
  float alt2 = leg.elevation.at(indexUpperDist).getAltitude();
  float alt = std::abs(alt1 + alt2) / 2.f;

  // Get Position for highlight
  float legdistpart = distance - leg.distances.first();
  float legdist = leg.distances.last() - leg.distances.first();

  // qDebug() << "legdistpart" << legdistpart << "legdist" << legdist << "fraction" << legdistpart / legdist;
  // qDebug() << "first" << leg.elevation.first() << "last" << leg.elevation.last();

  const atools::geo::Pos& pos = leg.elevation.first().interpolate(leg.elevation.last(), legdistpart / legdist);
  // qDebug() << "pos" << pos;

  float maxElev =
    std::ceil((leg.maxElevation + OptionData::instance().getRouteGroundBuffer()) / 500.f) * 500.f;

  // Get from/to text
  QString from = legList.routeMapObjects.at(index).getIdent();
  QString to = legList.routeMapObjects.at(index + 1).getIdent();

  mainWindow->getUi()->labelElevationInfo->setText(
    tr("<b>") + from + tr(" ► ") + to + tr("</b>, ") +
    QLocale().toString(distance, 'f', distance < 100.f ? 1 : 0) + tr(" ► ") +
    QLocale().toString(distanceToGo, 'f', distanceToGo < 100.f ? 1 : 0) + tr(" nm, ") +
    tr(" Ground Altitude ") + QLocale().toString(alt, 'f', 0) + tr(" ft, ") +
    tr(" Above Ground Altitude ") + QLocale().toString(flightplanAltFt - alt, 'f', 0) + tr(" ft, ") +
    tr(" Leg Safe Altitude ") + QLocale().toString(maxElev, 'f', 0) + tr(" ft"));

  mouseEvent->accept();

  emit highlightProfilePoint(pos);
}

void ProfileWidget::leaveEvent(QEvent *)
{
  if(!visible || legList.elevationLegs.isEmpty() || legList.routeMapObjects.isEmpty())
    return;

  qDebug() << "leave";

  delete rubberBand;
  rubberBand = nullptr;

  mainWindow->getUi()->labelElevationInfo->setText(tr("<b>No information.</b>"));

  emit highlightProfilePoint(atools::geo::EMPTY_POS);
}

void ProfileWidget::resizeEvent(QResizeEvent *)
{
  updateScreenCoords();
}

void ProfileWidget::deleteAircraftTrack()
{
  updateScreenCoords();
  update();
}

void ProfileWidget::preDatabaseLoad()
{
  terminateThread();
  databaseLoadStatus = true;
}

void ProfileWidget::postDatabaseLoad()
{
  databaseLoadStatus = false;
  routeChanged(true);
}

void ProfileWidget::updateProfileShowFeatures()
{
  Ui::MainWindow *ui = mainWindow->getUi();

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
    terminate = true;
    future.waitForFinished();
  }
}
