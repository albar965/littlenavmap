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

#include <QWidget>

#include <marble/GeoDataCoordinates.h>

#include <geo/pos.h>

namespace Marble {
class ElevationModel;
}

class MainWindow;
class RouteController;
class QTimer;

class ProfileWidget :
  public QWidget
{
  Q_OBJECT

public:
  ProfileWidget(MainWindow *parent = nullptr);
  virtual ~ProfileWidget();

  void routeChanged(bool geometryChanged);

private:
  virtual void paintEvent(QPaintEvent *) override;

  struct ElevationLeg
  {
    QVector<atools::geo::Pos> elevation;
    QVector<float> distances;
    float maxElevation = 0.f;
  };

  QList<ElevationLeg> elevationLegs;
  float maxRouteElevation, totalDistance;
  int totalNumPoints;

  QTimer *updateTimer = nullptr;
  const Marble::ElevationModel *elevationModel = nullptr;
  RouteController *routeController = nullptr;

  MainWindow *parentWindow;
  void fetchRouteElevations();
  void updateElevation();
  void updateTimeout();

};

#endif // PROFILEWIDGET_H
