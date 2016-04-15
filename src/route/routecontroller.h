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
#include "routecommand.h"

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
class QItemSelection;
class RouteIconDelegate;
class QUndoStack;

class RouteController :
  public QObject
{
  Q_OBJECT

public:
  RouteController(MainWindow *parent, MapQuery *mapQuery, QTableView *tableView);
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

  void getSelectedRouteMapObjects(QList<RouteMapObject>& selRouteMapObjects) const;

  const atools::geo::Rect& getBoundingRect() const
  {
    return boundingRect;
  }

  void routeSetStart(maptypes::MapAirport airport);
  void routeSetDest(maptypes::MapAirport airport);
  void routeAdd(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type, int legIndex);
  void routeDelete(int id, maptypes::MapObjectTypes type);
  void routeSetParking(maptypes::MapParking parking);
  void selectDepartureParking();
  void routeReplace(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type, int legIndex);

  /* Used by undo/redo */
  void changeRouteUndoRedo(const atools::fs::pln::Flightplan& newFlightplan);

  bool hasChanged() const
  {
    return changed;
  }

  const QString& getRouteFilename() const
  {
    return routeFilename;
  }

  QString getDefaultFilename() const;
  bool isFlightplanEmpty() const;
  bool hasValidStart() const;
  bool hasValidDestination() const;

  void preDatabaseLoad();
  void postDatabaseLoad();

private:
  atools::fs::pln::Flightplan *flightplan = nullptr;
  atools::geo::Rect boundingRect;
  QList<RouteMapObject> routeMapObjects;
  QString routeFilename, dockWindowTitle;
  MainWindow *parentWindow;
  QTableView *view;
  MapQuery *query;
  QStandardItemModel *model;
  RouteIconDelegate *iconDelegate;
  QUndoStack *undoStack;

  void updateWindowTitle();

  float totalDistance = 0.f;
  int curUserpointNumber = 1;
  bool changed = false;

  void updateLabel();

  void doubleClick(const QModelIndex& index);
  void tableContextMenu(const QPoint& pos);

  void tableSelectionChanged();
  void tableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

  void moveLegsDown();
  void moveLegsUp();
  void moveLegsInternal(int dir);
  void deleteLegs();
  void selectedRows(QList<int>& rows, bool reverse);

  void select(QList<int>& rows, int offset);

  void updateMoveAndDeleteActions();

  void buildFlightplanEntry(const maptypes::MapAirport& airport, atools::fs::pln::FlightplanEntry& entry);
  void buildFlightplanEntry(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type,
                            atools::fs::pln::FlightplanEntry& entry);
  int nearestLeg(const atools::geo::Pos& pos);

  void updateFlightplanData();

  void routeSetStartInternal(const maptypes::MapAirport& airport);
  void createRouteMapObjects();

  void updateModel();

  void updateRouteMapObjects();

  void routeAltChanged();
  void routeTypeChanged();

  void clearRoute();

  RouteCommand *preChange(const QString& text = QString(), rctype::RouteCmdType rcType = rctype::EDIT);
  void postChange(RouteCommand *undoCommand);

signals:
  void showRect(const atools::geo::Rect& rect);
  void showPos(const atools::geo::Pos& pos, int zoom);
  void changeMark(const atools::geo::Pos& pos);
  void routeSelectionChanged(int selected, int total);
  void routeChanged();

};

#endif // ROUTECONTROLLER_H
