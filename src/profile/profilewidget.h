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

#ifndef PROFILEWIDGET_H
#define PROFILEWIDGET_H

#include <QFuture>
#include <QFutureWatcher>
#include <QWidget>

#include <marble/GeoDataCoordinates.h>

#include <geo/pos.h>

#include <route/routemapobject.h>

#include <fs/sc/simconnectdata.h>

namespace Marble {
class ElevationModel;
}

class MainWindow;
class RouteController;
class QTimer;
class QRubberBand;

class ProfileWidget :
  public QWidget
{
  Q_OBJECT

public:
  ProfileWidget(MainWindow *parent = nullptr);
  virtual ~ProfileWidget();

  void routeChanged(bool geometryChanged);

  void simDataChanged(const atools::fs::sc::SimConnectData& simulatorData);

  void disconnectedFromSimulator();

signals:
  void highlightProfilePoint(atools::geo::Pos pos);

private:
  virtual void paintEvent(QPaintEvent *) override;

  atools::fs::sc::SimConnectData simData, lastSimData;
  float aircraftDistanceFromStart, aircraftDistanceToDest;

  struct ElevationLeg
  {
    QVector<atools::geo::Pos> elevation;
    QVector<float> distances;
    float maxElevation = 0.f;
  };

  struct ElevationLegList
  {
    QList<RouteMapObject> routeMapObjects;
    QList<ElevationLeg> elevationLegs;
    float maxRouteElevation = 0.f, totalDistance = 0.f;
    int totalNumPoints = 0;
  };

  ElevationLegList legList;

  QTimer *updateTimer = nullptr;
  const Marble::ElevationModel *elevationModel = nullptr;
  RouteController *routeController = nullptr;

  MainWindow *parentWindow;
  ElevationLegList fetchRouteElevationsThread();

  QFuture<ProfileWidget::ElevationLegList> future;
  QFutureWatcher<ElevationLegList> watcher;
  bool terminate = false;
  QRubberBand *rubberBand = nullptr;

  void updateElevation();
  void updateTimeout();

  void updateFinished();

  virtual void showEvent(QShowEvent *) override;
  virtual void hideEvent(QHideEvent *) override;
  virtual void mouseMoveEvent(QMouseEvent *mouseEvent) override;
  virtual void resizeEvent(QResizeEvent *) override;
  virtual void leaveEvent(QEvent *) override;

  bool visible = false;

  void updateScreenCoords();

  QVector<int> waypointX;
  QPolygon poly;
  float maxRouteElevationFt, flightplanAltFt, maxHeight, vertScale, horizScale;

};

#endif // PROFILEWIDGET_H
