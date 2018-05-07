/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#include "route/routecommand.h"
#include "route/route.h"

#include <QIcon>
#include <QObject>
#include <QTimer>

namespace atools {
namespace gui {
class ItemViewZoomHandler;
}

namespace fs {

namespace sc {
class SimConnectData;
}
namespace pln {
class Flightplan;
class FlightplanIO;
class FlightplanEntry;
}
}
}

class QMainWindow;
class QTableView;
class QStandardItemModel;
class QItemSelection;
class RouteNetwork;
class RouteFinder;
class FlightplanEntryBuilder;
class SymbolPainter;
class RouteViewEventFilter;
class AirportQuery;

/*
 * All flight plan related tasks like saving, loading, modification, calculation and table
 * view display are managed in this class.
 *
 * Flight plan and route map objects are maintained in parallel to keep flight plan structure
 * similar to the loaded original (i.e. waypoints not in database, missing airways)
 */
class RouteController :
  public QObject
{
  Q_OBJECT

public:
  RouteController(QMainWindow *parent, QTableView *tableView);
  virtual ~RouteController();

  /* Creates a new plan and emits routeChanged */
  void newFlightplan();

  /* Loads flight plan from FSX PLN file, checks for proper start position (shows notification dialog)
   * and emits routeChanged. Uses file name as new current name  */
  bool loadFlightplan(const QString& filename);
  void loadFlightplan(atools::fs::pln::Flightplan flightplan,
                      const QString& filename, bool quiet, bool changed, bool adjustAltitude, float speedKts);

  /* Loads flight plan from FSX PLN file and appends it to the current flight plan.
   * Emits routeChanged. */
  bool appendFlightplan(const QString& filename);

  /* Saves flight plan using the given name and file format and uses file name as new current name */
  bool saveFlighplanAs(const QString& filename, atools::fs::pln::FileFormat targetFileFormat);

  /* Saves flight plan using current name and current format */
  bool saveFlightplan(bool cleanExport);

  bool exportFlighplanAsClean(const QString& filename);

  /* Save and reload widgets state and current flight plan name */
  void saveState();
  void restoreState();

  /* Get the route only */
  const Route& getRoute() const
  {
    return route;
  }

  Route& getRoute()
  {
    return route;
  }

  float getSpinBoxSpeedKts() const;

  /* Get a copy of all route map objects (legs) that are selected in the flight plan table view */
  void getSelectedRouteLegs(QList<int>& selLegIndexes) const;

  /* Get bounding rectangle for flight plan */
  const atools::geo::Rect& getBoundingRect() const
  {
    return route.getBoundingRect();
  }

  /* Has flight plan changed */
  bool hasChanged() const;

  /* Get the current flight plan name or empty if no plan is loaded */
  const QString& getCurrentRouteFilename() const
  {
    return routeFilename;
  }

  float getRouteDistanceNm() const
  {
    return route.getTotalDistance();
  }

  bool  doesFilenameMatchRoute(atools::fs::pln::FileFormat format);

  /* Clear routing network cache and disconnect all queries */
  void preDatabaseLoad();
  void postDatabaseLoad();

  /* Replaces departure airport or adds departure if not valid. Adds best start position (runway). */
  void routeSetDeparture(map::MapAirport airport);

  /* Replaces destination airport or adds destination if not valid */
  void routeSetDestination(map::MapAirport airport);

  /*
   * Adds a navaid, airport or user defined position to flight plan.
   * @param id Id of object to insert
   * @param userPos Coordinates of user defined position if no navaid is to be inserted.
   * @param type Type of object to insert. maptypes::USER if userPos is set.
   * @param legIndex Insert after the leg with this index. Will use nearest leg if index is -1.
   */
  void routeAdd(int id, atools::geo::Pos userPos, map::MapObjectTypes type, int legIndex);

  /* Add an approach and/or a transition */
  void routeAttachProcedure(proc::MapProcedureLegs legs, const QString& sidStarRunway);

  /* Same as above but replaces waypoint at legIndex */
  void routeReplace(int id, atools::geo::Pos userPos, map::MapObjectTypes type, int legIndex);

  /* Delete waypoint at the given index. Will also delete departure or destination */
  void routeDelete(int index);

  /* Set departure parking position. If the airport of the parking spot is different to
   * the current departure it will be replaced too. */
  void routeSetParking(const map::MapParking& parking);
  void routeSetHelipad(const map::MapHelipad& helipad);

  /* Shows the dialog to select departure parking or start position.
   *  @return true if position was set. false is dialog was canceled. */
  bool selectDepartureParking();

  /* "Calculate" a direct flight plan that has no waypoints. */
  void calculateDirect();

  /* Calculate a flight plan from radio navaid to radio navaid */
  void calculateRadionav(int fromIndex, int toIndex);
  void calculateRadionav();

  /* Calculate a flight plan along high altitude (Jet) airways */
  void calculateHighAlt(int fromIndex, int toIndex);
  void calculateHighAlt();

  /* Calculate a flight plan along low altitude (Victor) airways */
  void calculateLowAlt(int fromIndex, int toIndex);
  void calculateLowAlt();

  /* Calculate a flight plan along low and high altitude airways that have the given altitude from
   *  the spin box as minimum altitude */
  void calculateSetAlt(int fromIndex, int toIndex);
  void calculateSetAlt();

  /* Reverse order of all waypoints, swap departure and destination and automatically
   * select a new start position (best runway) */
  void reverseRoute();

  void optionsChanged();

  /* Get the route table as a HTML document only containing the table and header */
  QString flightplanTableAsHtml(int iconSizePixel) const;

  /* Copy the route as a string to the clipboard */
  void routeStringToClipboard() const;

  /* Adjust altitude according to simple east/west VFR/IFR rules */
  void adjustFlightplanAltitude();

  FlightplanEntryBuilder *getFlightplanEntryBuilder() const
  {
    return entryBuilder;
  }

  void disconnectedFromSimulator();

  void simDataChanged(const atools::fs::sc::SimConnectData& simulatorData);

  void editUserWaypointName(int index);

  void shownMapFeaturesChanged(map::MapObjectTypes types);

  void activateLegManually(int index);

  QString procedureTypeText(const RouteLeg& leg);

signals:
  /* Show airport on map */
  void showRect(const atools::geo::Rect& rect, bool doubleClick);

  /* Show flight plan waypoint or user position on map */
  void showPos(const atools::geo::Pos& pos, float zoom, bool doubleClick);

  /* Change distance search center */
  void changeMark(const atools::geo::Pos& pos);

  /* Selection in table view has changed. Update hightlights on map */
  void routeSelectionChanged(int selected, int total);

  /* Route has changed */
  void routeChanged(bool geometryChanged);

  void routeAltitudeChanged(float altitudeFeet);

  /* Show information about the airports or navaids in the search result */
  void showInformation(map::MapSearchResult result);

  /* Show approach information about the airport */
  void showProcedures(map::MapAirport airport);

  /* Emitted before route calculation to stop any background tasks */
  void preRouteCalc();

private:
  friend class RouteCommand;

  /* Move selected rows */
  enum MoveDirection
  {
    MOVE_NONE = 0,
    MOVE_DOWN = 1,
    MOVE_UP = -1
  };

  /* Called by route command */
  void changeRouteUndo(const atools::fs::pln::Flightplan& newFlightplan);

  /* Called by route command */
  void changeRouteRedo(const atools::fs::pln::Flightplan& newFlightplan);

  /* Called by route command */
  void undoMerge();

  /* Save undo state before and after change */
  RouteCommand *preChange(const QString& text = QString(), rctype::RouteCmdType rcType = rctype::EDIT);
  void postChange(RouteCommand *undoCommand);

  void routeSetStartPosition(map::MapStart start);

  void updateWindowLabel();

  void doubleClick(const QModelIndex& index);
  void tableContextMenu(const QPoint& pos);

  void tableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

  void moveSelectedLegsDown();
  void moveSelectedLegsUp();
  void moveSelectedLegsInternal(MoveDirection direction);
  void deleteSelectedLegs();
  void selectedRows(QList<int>& rows, bool reverseRoute);

  void select(QList<int>& rows, int offset);

  void updateMoveAndDeleteActions();

  void routeToFlightPlan();

  void routeSetDepartureInternal(const map::MapAirport& airport);
  void routeSetDestinationInternal(const map::MapAirport& airport);

  void updateTableModel();

  void routeAltChanged();
  void routeAltChangedDelayed();
  void routeSpeedChanged();

  void routeTypeChanged();

  void clearRoute();

  bool calculateRouteInternal(RouteFinder *routeFinder, atools::fs::pln::RouteType type,
                              const QString& commandName,
                              bool fetchAirways, bool useSetAltitude, int fromIndex, int toIndex);

  void updateModelRouteTime();

  void updateFlightplanFromWidgets(atools::fs::pln::Flightplan& flightplan);
  void updateFlightplanFromWidgets();

  /* Used by undo/redo */
  void changeRouteUndoRedo(const atools::fs::pln::Flightplan& newFlightplan);

  void tableCopyClipboard();

  void showInformationMenu();
  void showProceduresMenu();
  void showOnMapMenu();

  void undoTriggered();
  void redoTriggered();
  bool updateStartPositionBestRunway(bool force, bool undo);
  void helpClicked();

  void dockVisibilityChanged(bool visible);
  void eraseAirway(int row);

  QString buildFlightplanLabel(bool html) const;
  QString buildFlightplanLabel2() const;

  void updateTableHeaders();
  void updateSpinboxSuffices();
  float calcTravelTime(float distance) const;
  void highlightNextWaypoint(int nearestLegIndex);
  void highlightProcedureItems();
  void loadProceduresFromFlightplan(bool quiet);
  void updateIcons();
  void beforeRouteCalc();
  void updateFlightplanEntryAirway(int airwayId, atools::fs::pln::FlightplanEntry& entry);
  QIcon iconForLeg(const RouteLeg& leg, int size) const;

  void routeAddInternal(const atools::fs::pln::FlightplanEntry& entry, int insertIndex);
  int calculateInsertIndex(const atools::geo::Pos& pos, int legIndex);
  proc::MapProcedureTypes affectedProcedures(const QList<int>& indexes);
  void nothingSelectedTriggered();
  void activateLegTriggered();
  void fontChanged();

  /* If route distance / direct distance if bigger than this value fail routing */
  static Q_DECL_CONSTEXPR float MAX_DISTANCE_DIRECT_RATIO = 1.5f;

  static Q_DECL_CONSTEXPR int ROUTE_UNDO_LIMIT = 50;

  atools::gui::ItemViewZoomHandler *zoomHandler = nullptr;

  /* Need a workaround since QUndoStack does not report current indices and clean state correctly */
  int undoIndex = 0;
  /* Clean index of the undo stack or -1 if not clean state exists */
  int undoIndexClean = 0;

  /* Network cache for flight plan calculation */
  RouteNetwork *routeNetworkRadio = nullptr, *routeNetworkAirway = nullptr;

  /* Flightplan and route objects */
  Route route; /* real route containing all segments */

  /* Current filename of empty if no route - also remember start and dest to avoid accidental overwriting */
  QString routeFilename, fileDeparture, fileDestination;
  atools::fs::pln::FlightplanType fileIfrVfr;

  QMainWindow *mainWindow;
  QTableView *view;
  MapQuery *mapQuery;
  AirportQuery *airportQuery;
  QStandardItemModel *model;
  QUndoStack *undoStack = nullptr;
  FlightplanEntryBuilder *entryBuilder = nullptr;
  atools::fs::pln::FlightplanIO *flightplanIO = nullptr;

  /* Do not update aircraft information more than every 0.1 seconds */
  static Q_DECL_CONSTEXPR int MIN_SIM_UPDATE_TIME_MS = 100;
  static Q_DECL_CONSTEXPR int ROUTE_ALT_CHANGE_DELAY_MS = 1000;
  qint64 lastSimUpdate = 0;

  QIcon ndbIcon, waypointIcon, userpointIcon, invalidIcon, procedureIcon;
  SymbolPainter *symbolPainter = nullptr;
  int iconSize = 20;

  QTimer routeAltDelayTimer;

  // Route table colum headings
  QList<QString> routeColumns;
};

#endif // LITTLENAVMAP_ROUTECONTROLLER_H
