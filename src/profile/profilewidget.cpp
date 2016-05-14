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

#include "profilewidget.h"
#include <algorithm>

#include <gui/mainwindow.h>
#include "geo/calculations.h"
#include "common/mapcolors.h"
#include "ui_mainwindow.h"
#include <QPainter>
#include <QLocale>
#include <QTimer>
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QMouseEvent>
#include <QRubberBand>
#include <QtConcurrent/QtConcurrentRun>
#include <mapgui/symbolpainter.h>

#include <route/routecontroller.h>

#include <marble/ElevationModel.h>
#include <marble/GeoDataCoordinates.h>

const int UPDATE_TIMEOUT = 1000;
const int X0 = 65, Y0 = 10;

using Marble::GeoDataCoordinates;
using atools::geo::Pos;

ProfileWidget::ProfileWidget(MainWindow *parent)
  : QWidget(parent), parentWindow(parent)
{
  setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
  elevationModel = parentWindow->getElevationModel();
  routeController = parentWindow->getRouteController();

  updateTimer = new QTimer(this);
  updateTimer->setSingleShot(true);
  connect(updateTimer, &QTimer::timeout, this, &ProfileWidget::updateTimeout);

  connect(elevationModel, &Marble::ElevationModel::updateAvailable,
          this, &ProfileWidget::updateElevation);
  connect(&watcher, &QFutureWatcher<ElevationLegList>::finished, this, &ProfileWidget::updateFinished);

  setMouseTracking(true);
}

ProfileWidget::~ProfileWidget()
{
  if(future.isRunning() || future.isStarted())
  {
    terminate = true;
    future.waitForFinished();
  }

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

void ProfileWidget::updateScreenCoords()
{
  int w = rect().width() - X0 * 2, h = rect().height() - Y0 * 2;

  // Add 1000 ft buffer and round up to the next 500 feet
  maxRouteElevationFt = std::ceil((legList.maxRouteElevation + 1000.f) / 500.f) * 500.f;
  flightplanAltFt = static_cast<float>(routeController->getFlightplan()->getCruisingAlt());
  maxHeight = std::max(maxRouteElevationFt, flightplanAltFt);
  vertScale = h / maxHeight;
  horizScale = w / legList.totalDistance;

  poly.append(QPoint(X0, h + Y0));

  waypointX.clear();
  poly.clear();

  float fromDistance = 0.f;
  for(const ElevationLeg& leg : legList.elevationLegs)
  {
    waypointX.append(X0 + static_cast<int>(fromDistance * horizScale));

    QPoint lastPt;
    for(int i = 0; i < leg.elevation.size(); i++)
    {
      QPoint pt(X0 + static_cast<int>(fromDistance * horizScale),
                Y0 + static_cast<int>(h - leg.elevation.at(i).getAltitude() * vertScale));

      if(lastPt.isNull() || i == leg.elevation.size() - 1 || (lastPt - pt).manhattanLength() > 5)
      {
        poly.append(pt);
        lastPt = pt;
      }
      fromDistance += leg.distances.at(i);
    }
  }
  waypointX.append(X0 + w);
  poly.append(QPoint(X0 + w, h + Y0));

}

void ProfileWidget::paintEvent(QPaintEvent *)
{
  if(!visible || legList.elevationLegs.isEmpty() || routeMapObjects.isEmpty())
    return;

  QElapsedTimer etimer;
  etimer.start();

  int w = rect().width() - X0 * 2, h = rect().height() - Y0 * 2;

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), QBrush(QColor::fromRgb(204, 204, 255)));

  // Draw grey vertical lines for waypoints
  painter.setPen(QPen(Qt::lightGray, 2, Qt::SolidLine));
  for(int wpx : waypointX)
    painter.drawLine(wpx, Y0, wpx, Y0 + h);

  // Draw the mountains
  painter.setBrush(QColor(Qt::darkGreen));
  painter.setBackgroundMode(Qt::OpaqueMode);
  painter.setPen(Qt::black);
  painter.drawPolygon(poly);

  // Draw the red maximum elevation line
  painter.setBrush(QColor(Qt::black));
  painter.setPen(QPen(Qt::red, 4, Qt::SolidLine));
  int maxAltY = Y0 + static_cast<int>(h - maxRouteElevationFt * vertScale);
  painter.drawLine(X0, maxAltY, X0 + static_cast<int>(w), maxAltY);

  // Draw the flightplan line
  int flightplanY = Y0 + static_cast<int>(h - flightplanAltFt * vertScale);
  painter.setBackgroundMode(Qt::OpaqueMode);
  painter.setPen(QPen(Qt::black, 6, Qt::SolidLine));
  painter.setBrush(QColor(Qt::black));
  painter.drawLine(X0, flightplanY, X0 + static_cast<int>(w), flightplanY);

  painter.setPen(QPen(Qt::yellow, 2, Qt::SolidLine));
  painter.drawLine(X0, flightplanY, X0 + w, flightplanY);

  // Draw flightplan symbols
  // Set default font to bold and reduce size
  QFont font = painter.font();
  font.setBold(true);
  font.setPointSizeF(font.pointSizeF() * 8.f / 10.f);
  painter.setFont(font);

  painter.setBackgroundMode(Qt::TransparentMode);

  SymbolPainter symPainter;
  textflags::TextFlags flags = textflags::IDENT | textflags::ROUTE_TEXT | textflags::ABS_POS;

  for(int i = routeMapObjects.size() - 1; i >= 0; i--)
  {
    const RouteMapObject& rmo = routeMapObjects.at(i);
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

  for(int i = routeMapObjects.size() - 1; i >= 0; i--)
  {
    const RouteMapObject& rmo = routeMapObjects.at(i);
    int symx = waypointX.at(i);

    switch(rmo.getMapObjectType())
    {
      case maptypes::NDB:
        symPainter.drawNdbSymbol(&painter, rmo.getNdb(), symx, flightplanY, 10, true, false);
        symPainter.drawNdbText(&painter, rmo.getNdb(), symx - 5, flightplanY + 18,
                               flags, 10, true, false);
        break;
      case maptypes::VOR:
        symPainter.drawVorSymbol(&painter, rmo.getVor(), symx, flightplanY, 10, true, false, false);
        symPainter.drawVorText(&painter, rmo.getVor(), symx - 5, flightplanY + 18,
                               flags, 10, true, false);
        break;
    }
  }

  font.setBold(true);
  font.setPointSizeF(font.pointSizeF() * 1.2f);
  painter.setFont(font);
  for(int i = routeMapObjects.size() - 1; i >= 0; i--)
  {
    const RouteMapObject& rmo = routeMapObjects.at(i);
    int symx = waypointX.at(i);

    switch(rmo.getMapObjectType())
    {
      case maptypes::AIRPORT:
        symPainter.drawAirportSymbol(&painter, rmo.getAirport(), symx, flightplanY, 10, false, false);
        symPainter.drawAirportText(&painter, rmo.getAirport(), symx - 5, flightplanY + 22,
                                   flags, 10, false, true, false);
        break;
    }
  }

  float startAlt = routeMapObjects.first().getPosition().getAltitude();
  QString startAltStr = QLocale().toString(startAlt, 'f', 0) + " ft";
  symPainter.textBox(&painter, {startAltStr},
                     QPen(Qt::black), X0 - 8,
                     Y0 + static_cast<int>(h - startAlt * vertScale) + 5,
                     textatt::BOLD | textatt::RIGHT, 255);

  float destAlt = routeMapObjects.last().getPosition().getAltitude();
  QString destAltStr = QLocale().toString(destAlt, 'f', 0) + " ft";
  symPainter.textBox(&painter, {destAltStr},
                     QPen(Qt::black), X0 + w + 4,
                     Y0 + static_cast<int>(h - destAlt * vertScale) + 5,
                     textatt::BOLD | textatt::LEFT, 255);

  QString maxAlt =
    QLocale().toString(maxRouteElevationFt, 'f', 0) + " ft";
  symPainter.textBox(&painter, {maxAlt},
                     QPen(Qt::red), X0 - 8, maxAltY + 5, textatt::BOLD | textatt::RIGHT, 255);

  QString routeAlt = QLocale().toString(routeController->getFlightplan()->getCruisingAlt()) + " ft";
  symPainter.textBox(&painter, {routeAlt},
                     QPen(Qt::black), X0 - 8, flightplanY + 5, textatt::BOLD | textatt::RIGHT, 255);

  qDebug() << "profile paint" << etimer.elapsed() << "ms";
}

ProfileWidget::ElevationLegList ProfileWidget::fetchRouteElevations()
{
  using atools::geo::meterToFeet;

  ElevationLegList legs;
  legs.totalNumPoints = 0;
  legs.totalDistance = 0.f;
  legs.maxRouteElevation = 0.f;
  legs.elevationLegs.clear();

  for(int i = 1; i < routeMapObjects.size(); i++)
  {
    if(terminate)
      return legs;

    const RouteMapObject& lastRmo = routeMapObjects.at(i - 1);
    const RouteMapObject& rmo = routeMapObjects.at(i);

    QList<GeoDataCoordinates> elev = elevationModel->heightProfile(
      lastRmo.getPosition().getLonX(), lastRmo.getPosition().getLatY(),
      rmo.getPosition().getLonX(), rmo.getPosition().getLatY());

    ElevationLeg leg;
    Pos lastPos;
    for(int j = 1; j < elev.size(); j++)
    {
      const GeoDataCoordinates& coord = elev.at(j);
      if(terminate)
        return ElevationLegList();

      Pos pos(coord.longitude(GeoDataCoordinates::Degree),
              coord.latitude(GeoDataCoordinates::Degree), meterToFeet(coord.altitude()));

      // Drop points with similar altitude except the first and last one on a segment
      if(lastPos.isValid() && j != 0 && j != elev.size() - 1 && legs.elevationLegs.size() > 2 &&
         atools::geo::almostEqual(pos.getAltitude(), lastPos.getAltitude(), 30.f))
        continue;

      float alt = pos.getAltitude();
      if(alt > leg.maxElevation)
        leg.maxElevation = alt;
      if(alt > legs.maxRouteElevation)
        legs.maxRouteElevation = alt;

      leg.elevation.append(pos);
      if(lastPos.isValid())
      {
        float dist = meterToFeet(lastPos.distanceMeterTo(pos));
        legs.totalDistance += dist;
        leg.distances.append(dist);
      }

      legs.totalNumPoints++;
      lastPos = pos;
    }
    leg.distances.append(0.f); // Add dummy
    legs.elevationLegs.append(leg);
  }

  qDebug() << "elevation legs" << legs.elevationLegs.size() << "total points" << legs.totalNumPoints
           << "total distance" << legs.totalDistance << "max route elevation" << legs.maxRouteElevation;
  return legs;
}

void ProfileWidget::updateElevation()
{
  if(!visible)
    return;

  qDebug() << "Profile update elevation";
  updateTimer->start(UPDATE_TIMEOUT);
}

void ProfileWidget::updateTimeout()
{
  if(!visible)
    return;

  qDebug() << "Profile update elevation timeout";

  if(future.isRunning() || future.isStarted())
  {
    terminate = true;
    future.waitForFinished();
  }

  terminate = false;

  // Need a copy to avoid synchronization problems
  routeMapObjects = routeController->getRouteMapObjects();

  // Start the computation in background
  future = QtConcurrent::run(this, &ProfileWidget::fetchRouteElevations);
  watcher.setFuture(future);
}

void ProfileWidget::updateFinished()
{
  if(!visible)
    return;

  qDebug() << "Profile update finished";

  if(!terminate)
  {
    legList = future.result();
    updateScreenCoords();
    update();
  }
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
  if(rubberBand == nullptr)
    rubberBand = new QRubberBand(QRubberBand::Line, this);

  int x = mouseEvent->pos().x();
  x = std::max(x, X0);
  x = std::min(x, rect().width() - X0);

  rubberBand->setGeometry(x, Y0, 2, rect().height());
  rubberBand->show();

  int index = 0;

  QVector<int>::iterator it = std::lower_bound(waypointX.begin(), waypointX.end(), x);
  if(it != waypointX.end())
  {
    index = static_cast<int>(std::distance(waypointX.begin(), it)) - 1;
    if(index < 0)
      index = 0;
  }

  const ElevationLeg& leg = legList.elevationLegs.at(index);

  float maxElev = std::ceil((leg.maxElevation + 1000.f) / 500.f) * 500.f;

  QString from = routeMapObjects.at(index).getIdent();
  QString to = routeMapObjects.at(index + 1).getIdent();
  float distance = atools::geo::feetToNm((x - X0) / horizScale);

  int index2 = 0;
  QVector<float>::const_iterator it2 = std::lower_bound(leg.distances.begin(), leg.distances.end(), distance);
  if(it2 != leg.distances.end())
  {
    index2 = static_cast<int>(std::distance(leg.distances.begin(), it2)) - 1;
    if(index2 < 0)
      index2 = 0;
  }
  float alt = leg.elevation.at(index2).getAltitude();

  parentWindow->getUi()->labelElevationInfo->setText(QString::number(x) + " / " +
                                                     QString::number(index) + " / " +
                                                     from + " −> " + to + ", " +
                                                     QString::number(distance, 'f',
                                                                     distance < 100.f ? 1 : 0) + " nm, " +
                                                     "Alt " + QString::number(alt, 'f', 0) + " ft" +
                                                     "MEA " + QString::number(maxElev, 'f', 0) + " ft");

  mouseEvent->accept();
}

void ProfileWidget::leaveEvent(QEvent *)
{
  delete rubberBand;
  rubberBand = nullptr;
  qDebug() << "leave";
}

void ProfileWidget::resizeEvent(QResizeEvent *)
{
  updateScreenCoords();
}
