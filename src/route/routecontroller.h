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

#ifndef LITTLENAVMAP_ROUTECONTROLLER_H
#define LITTLENAVMAP_ROUTECONTROLLER_H

#include "routemapobject.h"
#include "routecommand.h"
#include "routemapobjectlist.h"

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
class RouteNetwork;
class RouteFinder;

class RouteController :
  public QObject
{
  Q_OBJECT

public:
  RouteController(MainWindow *parent, MapQuery *mapQuery, QTableView *tableView);
  virtual ~RouteController();

  void newFlightplan();
  bool loadFlightplan(const QString& filename);
  bool saveFlighplanAs(const QString& filename);
  bool saveFlightplan();

  void saveState();
  void restoreState();

  const RouteMapObjectList& getRouteMapObjects() const
  {
    return route;
  }

  const atools::fs::pln::Flightplan& getFlightplan() const
  {
    return route.getFlightplan();
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
  bool selectDepartureParking();
  void routeReplace(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type, int legIndex);

  bool hasChanged() const;

  const QString& getRouteFilename() const
  {
    return routeFilename;
  }

  QString getDefaultFilename() const;
  bool isFlightplanEmpty() const;
  bool hasValidStart() const;
  bool hasValidDestination() const;
  bool hasEntries() const;
  bool hasValidParking() const;

  void preDatabaseLoad();
  void postDatabaseLoad();

  void calculateDirect();
  void calculateRadionav();
  void calculateHighAlt();
  void calculateLowAlt();
  void calculateSetAlt();
  void reverse();

  void changeRouteUndo(const atools::fs::pln::Flightplan& newFlightplan);
  void changeRouteRedo(const atools::fs::pln::Flightplan& newFlightplan);

  void undoMerge();

signals:
  void showRect(const atools::geo::Rect& rect);
  void showPos(const atools::geo::Pos& pos, int zoom);
  void changeMark(const atools::geo::Pos& pos);
  void routeSelectionChanged(int selected, int total);
  void routeChanged(bool geometryChanged);
  void showInformation(maptypes::MapSearchResult result);

private:
  RouteNetwork *routeNetworkRadio, *routeNetworkAirway;
  atools::geo::Rect boundingRect;
  RouteMapObjectList route;
  QString routeFilename;
  MainWindow *mainWindow;
  QTableView *view;
  MapQuery *query;
  QStandardItemModel *model;
  RouteIconDelegate *iconDelegate;
  QUndoStack *undoStack;

  // Need a workaround since QUndoStack does not report current indices and clean state
  int undoIndex = 0, undoIndexClean = 0;
  void updateWindowTitle();

  int curUserpointNumber = 1;

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
                            atools::fs::pln::FlightplanEntry& entry, bool resolveWaypoints = -1);

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

  void calculateRouteInternal(RouteFinder *routeFinder, atools::fs::pln::RouteType type,
                              const QString& commandName,
                              bool fetchAirways, bool useSetAltitude);

  void updateFlightplanEntryAirway(int airwayId, atools::fs::pln::FlightplanEntry& entry, int& minAltitude);

  void updateModelRouteTime();

  void updateFlightplanFromWidgets();

  /* Used by undo/redo */
  void changeRouteUndoRedo(const atools::fs::pln::Flightplan& newFlightplan);

};

#endif // LITTLENAVMAP_ROUTECONTROLLER_H
