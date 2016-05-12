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

#include <QPainter>
#include <QTimer>
#include <QGuiApplication>
#include <QElapsedTimer>

#include <route/routecontroller.h>

#include <marble/ElevationModel.h>
#include <marble/GeoDataCoordinates.h>

const int UPDATE_TIMEOUT = 1000;

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
}

ProfileWidget::~ProfileWidget()
{

}

void ProfileWidget::routeChanged(bool geometryChanged)
{
  if(geometryChanged)
  {
    qDebug() << "Profile route geometry changed";
    updateTimer->start(UPDATE_TIMEOUT);
  }
}

void ProfileWidget::paintEvent(QPaintEvent *)
{
  QElapsedTimer etimer;
  etimer.start();

  int w = rect().width(), h = rect().height();

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), QBrush(QColor(Qt::white)));

  float vertScale = h / maxRouteElevation;
  float horizScale = w / totalDistance;

  QVector<int> waypointX;
  waypointX.append(0);

  QPolygon poly;
  poly.append(QPoint(0, h));

  float fromDistance = 0.f;
  for(const ElevationLeg& leg : elevationLegs)
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

  painter.setBrush(QColor(Qt::darkGreen));
  painter.setBackgroundMode(Qt::OpaqueMode);
  painter.setPen(Qt::black);
  painter.drawPolygon(poly);

  painter.setPen(QPen(Qt::black, 2, Qt::SolidLine));
  for(int wpx : waypointX)
    painter.drawLine(wpx, 0, wpx, h);

  qDebug() << "profile paint" << etimer.elapsed() << "ms";
}

void ProfileWidget::fetchRouteElevations()
{
  totalNumPoints = 0;
  totalDistance = 0.f;
  maxRouteElevation = 0.f;
  elevationLegs.clear();

  const QList<RouteMapObject>& routeMapObjects = routeController->getRouteMapObjects();

  for(int i = 1; i < routeMapObjects.size(); i++)
  {
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
      Pos pos(coord.longitude(GeoDataCoordinates::Degree),
              coord.latitude(GeoDataCoordinates::Degree), coord.altitude());

      float alt = pos.getAltitude();
      if(alt > leg.maxElevation)
        leg.maxElevation = alt;
      if(alt > maxRouteElevation)
        maxRouteElevation = alt;

      leg.elevation.append(pos);
      if(lastPos.isValid())
      {
        float dist = lastPos.distanceMeterTo(pos);
        leg.distances.append(dist);
        totalDistance += dist;
      }

      totalNumPoints++;
      lastPos = pos;
    }
    leg.distances.append(0.f); // Add dummy
    elevationLegs.append(leg);
  }
  qDebug() << "elevation legs" << elevationLegs.size() << "total points" << totalNumPoints
           << "total distance" << totalDistance << "max route elevation" << maxRouteElevation;
}

void ProfileWidget::updateElevation()
{
  qDebug() << "Profile update elevation";
  updateTimer->start(UPDATE_TIMEOUT);
}

void ProfileWidget::updateTimeout()
{
  qDebug() << "Profile update elevation timeout";

  QGuiApplication::setOverrideCursor(Qt::WaitCursor);
  fetchRouteElevations();
  QGuiApplication::restoreOverrideCursor();

  update();
}
