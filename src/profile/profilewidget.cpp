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

#include <gui/mainwindow.h>
#include "geo/calculations.h"

#include <QPainter>
#include <QTimer>
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <route/routecontroller.h>

#include <marble/ElevationModel.h>
#include <marble/GeoDataCoordinates.h>

const int UPDATE_TIMEOUT = 2000;

using Marble::GeoDataCoordinates;
using atools::geo::Pos;

ProfileWidget::ProfileWidget(MainWindow *parent)
  : QWidget(parent), parentWindow(parent)
{
  elevationModel = parentWindow->getElevationModel();
  routeController = parentWindow->getRouteController();

  updateTimer = new QTimer(this);
  updateTimer->setSingleShot(true);
  connect(updateTimer, &QTimer::timeout, this, &ProfileWidget::updateTimeout);

  connect(elevationModel, &Marble::ElevationModel::updateAvailable,
          this, &ProfileWidget::updateElevation);
  connect(&watcher, &QFutureWatcher<ElevationLegList>::finished, this, &ProfileWidget::updateFinished);

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
}

void ProfileWidget::paintEvent(QPaintEvent *)
{
  if(!visible)
    return;

  QElapsedTimer etimer;
  etimer.start();

  int w = rect().width(), h = rect().height();

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), QBrush(QColor(Qt::white)));

  float flightplanAlt = atools::geo::feetToMeter(
    static_cast<float>(routeController->getFlightplan()->getCruisingAlt()));
  float altBuffer = atools::geo::feetToMeter(1000.f);

  float maxHeight = std::max(legList.maxRouteElevation, flightplanAlt) + altBuffer;
  float vertScale = h / maxHeight;
  float horizScale = w / legList.totalDistance;

  QVector<int> waypointX;
  waypointX.append(0);

  QPolygon poly;
  poly.append(QPoint(0, h));

  float fromDistance = 0.f;
  for(const ElevationLeg& leg : legList.elevationLegs)
  {
    waypointX.append(static_cast<int>(fromDistance * horizScale));

    QPoint lastPt;
    for(int i = 0; i < leg.elevation.size(); i++)
    {
      QPoint pt(static_cast<int>(fromDistance * horizScale),
                static_cast<int>(h - leg.elevation.at(i).getAltitude() * vertScale));

      if(lastPt.isNull() || (lastPt - pt).manhattanLength() > 5)
      {
        poly.append(pt);
        lastPt = pt;
      }
      fromDistance += leg.distances.at(i);
    }
  }

  poly.append(QPoint(w, h));

  painter.setPen(QPen(Qt::lightGray, 2, Qt::SolidLine));
  for(int wpx : waypointX)
    painter.drawLine(wpx, 0, wpx, h);

  painter.setBrush(QColor(Qt::darkGreen));
  painter.setBackgroundMode(Qt::OpaqueMode);
  painter.setPen(Qt::black);
  painter.drawPolygon(poly);

  painter.setBrush(QColor(Qt::black));
  painter.setPen(QPen(Qt::red, 3, Qt::SolidLine));
  painter.drawLine(0, static_cast<int>(h - (legList.maxRouteElevation + altBuffer) * vertScale),
                   w, static_cast<int>(h - (legList.maxRouteElevation + altBuffer) * vertScale));

  painter.setPen(QPen(Qt::black, 7, Qt::SolidLine));
  painter.drawLine(0, static_cast<int>(h - flightplanAlt * vertScale),
                   w, static_cast<int>(h - flightplanAlt * vertScale));

  painter.setPen(QPen(Qt::yellow, 3, Qt::SolidLine));
  painter.drawLine(0, static_cast<int>(h - flightplanAlt * vertScale),
                   w, static_cast<int>(h - flightplanAlt * vertScale));

  qDebug() << "profile paint" << etimer.elapsed() << "ms";
}

ProfileWidget::ElevationLegList ProfileWidget::fetchRouteElevations()
{
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

    QList<GeoDataCoordinates> elev = elevationModel->heightProfile(lastRmo.getPosition().getLonX(),
                                                                   lastRmo.getPosition().getLatY(),
                                                                   rmo.getPosition().getLonX(),
                                                                   rmo.getPosition().getLatY());

    ElevationLeg leg;
    Pos lastPos;
    for(const GeoDataCoordinates& coord : elev)
    {
      if(terminate)
        return ElevationLegList();

      Pos pos(coord.longitude(GeoDataCoordinates::Degree),
              coord.latitude(GeoDataCoordinates::Degree), coord.altitude());

      if(lastPos.isValid() && pos.getAltitude() > 0.f && // lastPos.getAltitude() > 0.f &&
         atools::geo::almostEqual(pos.getAltitude(), lastPos.getAltitude(), 10.f))
        continue;

      float alt = pos.getAltitude();
      if(alt > leg.maxElevation)
        leg.maxElevation = alt;
      if(alt > legs.maxRouteElevation)
        legs.maxRouteElevation = alt;

      leg.elevation.append(pos);
      if(lastPos.isValid())
      {
        float dist = lastPos.distanceMeterTo(pos);
        leg.distances.append(dist);
        legs.totalDistance += dist;
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
