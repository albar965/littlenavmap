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

#ifndef LITTLENAVMAP_PROFILEWIDGET_H
#define LITTLENAVMAP_PROFILEWIDGET_H

#include "route/routemapobjectlist.h"
#include "fs/sc/simconnectdata.h"

#include <QFuture>
#include <QFutureWatcher>
#include <QWidget>

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
  ProfileWidget(MainWindow *parent);
  virtual ~ProfileWidget();

  void routeChanged(bool geometryChanged);

  void simDataChanged(const atools::fs::sc::SimConnectData& simulatorData);

  void disconnectedFromSimulator();

  void updateProfileShowFeatures();

  void deleteAircraftTrack();

  void preDatabaseLoad();
  void postDatabaseLoad();

  struct SimUpdateDelta
  {
    int manhattanLengthDelta;
    float altitudeDelta;
  };

  void optionsChanged();

signals:
  void highlightProfilePoint(atools::geo::Pos pos);

private:
  struct ElevationLeg
  {
    QVector<atools::geo::Pos> elevation;
    QVector<float> distances;
    float maxElevation = 0.f;
  };

  struct ElevationLegList
  {
    RouteMapObjectList routeMapObjects;
    QList<ElevationLeg> elevationLegs;
    float maxElevationFt = 0.f, totalDistance = 0.f;
    int totalNumPoints = 0;
  };

  virtual void paintEvent(QPaintEvent *) override;
  virtual void showEvent(QShowEvent *) override;
  virtual void hideEvent(QHideEvent *) override;
  virtual void mouseMoveEvent(QMouseEvent *mouseEvent) override;
  virtual void resizeEvent(QResizeEvent *) override;
  virtual void leaveEvent(QEvent *) override;

  ElevationLegList fetchRouteElevationsThread(ElevationLegList legs) const;
  void updateElevation();
  void updateTimeout();
  void updateThreadFinished();
  void updateScreenCoords();
  void terminateThread();

  static Q_DECL_CONSTEXPR int NUM_SCALE_STEPS = 5;
  const int SCALE_STEPS[NUM_SCALE_STEPS] = {500, 1000, 2000, 5000, 10000};
  static Q_DECL_CONSTEXPR int MIN_SCALE_SCREEN_DISTANCE = 25;
  static Q_DECL_CONSTEXPR int UPDATE_TIMEOUT = 1000;
  const int X0 = 65, Y0 = 14;

  atools::fs::sc::SimConnectData simData, lastSimData;
  QPolygon aircraftTrackPoints;
  float aircraftDistanceFromStart, aircraftDistanceToDest;
  ElevationLegList legList;
  QTimer *updateTimer = nullptr;
  const Marble::ElevationModel *elevationModel = nullptr;
  RouteController *routeController = nullptr;
  MainWindow *mainWindow;
  QFuture<ElevationLegList> future;
  QFutureWatcher<ElevationLegList> watcher;
  bool databaseLoadStatus = false;
  bool terminate = false;
  QRubberBand *rubberBand = nullptr;
  bool visible = false;
  bool showAircraft = false, showAircraftTrack = false;
  QVector<int> waypointX;
  QPolygon landPolygon;
  float minSafeAltitudeFt, flightplanAltFt, maxAlt, vertScale, horizScale;

};

Q_DECLARE_TYPEINFO(ProfileWidget::SimUpdateDelta, Q_PRIMITIVE_TYPE);

#endif // LITTLENAVMAP_PROFILEWIDGET_H
