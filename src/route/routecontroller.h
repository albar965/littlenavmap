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

#ifndef ROUTECONTROLLER_H
#define ROUTECONTROLLER_H

#include "routemapobject.h"

#include <QObject>

namespace atools {
namespace fs {
namespace pln {
class Flightplan;
class FlightplanEntry;
}
}
}

class MainWindow;
class MapQuery;
class QTableView;
class QStandardItemModel;
class QStandardItem;

class RouteController :
  public QObject
{
  Q_OBJECT

public:
  RouteController(MainWindow *parent, MapQuery *mapQuery, QTableView *view);
  virtual ~RouteController();

  void newFlightplan();
  void loadFlightplan(const QString& filename);
  void saveFlighplanAs(const QString& filename);
  void saveFlightplan();

  void saveState();
  void restoreState();

  const QList<RouteMapObject>& getRouteMapObjects() const
  {
    return routeMapObjects;
  }

private:
  bool changed = false;
  atools::fs::pln::Flightplan *flightplan = nullptr;
  QList<RouteMapObject> routeMapObjects;
  QString routeFilename, dockWindowTitle;
  MainWindow *parentWindow;
  QTableView *tableView;
  MapQuery *query;
  QStandardItemModel *model;
  void flightplanToView();
  void updateWindowTitle();

};

#endif // ROUTECONTROLLER_H
