/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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
#include "routing/routenetworktypes.h"
#include "route/route.h"
#include "common/tabindexes.h"

#include <QTimer>

namespace atools {
namespace routing {
class RouteFinder;
class RouteNetwork;
}
namespace gui {
class ItemViewZoomHandler;
class TabWidgetHandler;
}
namespace util {
class HtmlBuilder;
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
class FlightplanEntryBuilder;
class SymbolPainter;
class AirportQuery;
class AirwayTrackQuery;
class UnitStringTool;
class QTextCursor;
class RouteCalcWindow;

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
  virtual ~RouteController() override;

  /* Creates a new plan and emits routeChanged */
  void newFlightplan();

  /* Loads flight plan from FSX PLN file, checks for proper start position (shows notification dialog)
   * and emits routeChanged. Uses file name as new current name  */
  bool loadFlightplan(const QString& filename);
  void loadFlightplan(atools::fs::pln::Flightplan flightplan, atools::fs::pln::FileFormat format,
                      const QString& filename, bool quiet, bool changed, bool adjustAltitude);

  /* Load the plan from a string in LNMPLN format */
  bool loadFlightplanLnmStr(const QString& string);

  /* Loads flight plan from FSX PLN file and appends it to the current flight plan.
   * Use -1 for insertBefore to append.
   * Emits routeChanged. */
  bool insertFlightplan(const QString& filename, int insertBefore);

  /* Saves flight plan using the given name and file format and uses file name as new current name */
  bool saveFlightplanLnm();
  bool saveFlightplanLnmAs(const QString& filename);

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
  const QString& getCurrentRouteFilepath() const
  {
    return routeFilename;
  }

  float getRouteDistanceNm() const
  {
    return route.getTotalDistance();
  }

  /* get altitude in feet as set in the widget */
  float getCruiseAltitudeWidget() const;

  bool  doesLnmFilenameMatchRoute();

  /* Clear routing network cache and disconnect all queries */
  void preDatabaseLoad();
  void postDatabaseLoad();

  /* Replaces departure airport or adds departure if not valid. Adds best start position (runway). */
  void routeSetDeparture(map::MapAirport airport);

  /* Replaces destination airport or adds destination if not valid */
  void routeSetDestination(map::MapAirport airport);

  /* Add an alternate airport */
  void routeAddAlternate(map::MapAirport airport);

  /*
   * Adds a navaid, airport or user defined position to flight plan.
   * @param id Id of object to insert
   * @param userPos Coordinates of user defined position if no navaid is to be inserted.
   * @param type Type of object to insert. maptypes::USER if userPos is set.
   * @param legIndex Insert after the leg with this index. Will use nearest leg if index is -1.
   */
  void routeAdd(int id, atools::geo::Pos userPos, map::MapObjectTypes type, int legIndex);

  /* Add an approach and/or a transition */
  void routeAddProcedure(proc::MapProcedureLegs legs, const QString& sidStarRunway);

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

  /* Open flight plan calculation window. */
  void calculateRouteWindowFull();
  void calculateRouteWindowSelection();

  /* Reverse order of all waypoints, swap departure and destination and automatically
   * select a new start position (best runway) */
  void reverseRoute();

  /* Change in options dialog */
  void optionsChanged();

  /* Tracks downloaded or deleted */
  void tracksChanged();

  /* UI style changed */
  void styleChanged();

  /* Get the route table as a HTML snipped only containing the table and header.
   * Uses own colors for table background. */
  QString getFlightplanTableAsHtml(float iconSizePixel, bool print) const;

  /* Same as above but full HTML document */
  QString getFlightplanTableAsHtmlDoc(float iconSizePixel) const;

  /* Insert a flight plan table as QTextTable object at the cursor position */
  void flightplanTableAsTextTable(QTextCursor& cursor, const QBitArray& selectedCols, float fontPointSize) const;

  /* Get header for print report */
  void flightplanHeaderPrint(atools::util::HtmlBuilder& html, bool titleOnly) const;

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
  void resetActiveLeg();
  void updateActiveLeg();

  QString procedureTypeText(const RouteLeg& leg);

  void clearTableSelection();

  bool hasTableSelection();

  void aircraftPerformanceChanged();
  void windUpdated();

  /* Get table columns from the view */
  QStringList getRouteColumns() const;

  /* Add custom procedure and probably set new destination airport */
  void showProceduresCustom(map::MapAirport airport);

  /* Name of currently loaded flight plan file */
  const QString& getRouteFilepath() const
  {
    return routeFilename;
  }

  /* Update the changed file indication in the flight plan tab header */
  void updateRouteTabChangedStatus();

  atools::gui::TabWidgetHandler *getTabHandler() const
  {
    return tabHandlerRoute;
  }

  /* Clear network, so it will be reloaded before next flight plan calculation. */
  void clearAirwayNetworkCache();

#ifdef DEBUG_NETWORK_INFORMATION
  void debugNetworkClick(const atools::geo::Pos& pos);

#endif

  /* true if flight plan was loaded in LNMPLN format. Otherwise imported from PLN, FMS, etc. */
  bool isLnmFormatFlightplan();

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
  void routeChanged(bool geometryChanged, bool newFlightplan = false);

  void routeAltitudeChanged(float altitudeFeet);

  /* Show information about the airports or navaids in the search result */
  void showInformation(map::MapSearchResult result, map::MapObjectTypes preferredType = map::NONE);

  /* Show approach information about the airport */
  void showProcedures(map::MapAirport airport);

  /* Emitted before route calculation to stop any background tasks */
  void preRouteCalc();

  /* Emitted by context menu */
  void routeInsert(int beforeRow);

private:
  friend class RouteCommand;

  /* Move selected rows */
  enum MoveDirection
  {
    MOVE_NONE = 0,
    MOVE_DOWN = 1,
    MOVE_UP = -1
  };

  /* Saves flight plan using LNM format */
  bool saveFlightplanLnmInternal();

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
  void getSelectedRows(QList<int>& selectedRows, bool reverseRoute);

  void selectList(const QList<int>& selectedRows, int offset);
  void selectRange(int from, int to);

  void updateMoveAndDeleteActions();

  void routeToFlightPlan();

  void routeSetDepartureInternal(const map::MapAirport& airport);
  void routeSetDestinationInternal(const map::MapAirport& airport);

  void updateTableModel();

  void routeAltChanged();
  void routeAltChangedDelayed();

  void routeTypeChanged();

  void clearRoute();

  /* Calculate flight plan pressed in dock window */
  void calculateRoute();
  bool calculateRouteInternal(atools::routing::RouteFinder *routeFinder,
                              const QString& commandName,
                              bool fetchAirways, float altitudeFt, int fromIndex, int toIndex,
                              atools::routing::Modes mode);

  void updateModelRouteTimeFuel();

  /* Assign type and altitude from GUI */
  void updateFlightplanFromWidgets(atools::fs::pln::Flightplan& flightplan);
  void updateFlightplanFromWidgets();

  /* Insert properties for aircraft performance */
  void assignFlightplanPerfProperties(atools::fs::pln::Flightplan& flightplan);

  /* Used by undo/redo */
  void changeRouteUndoRedo(const atools::fs::pln::Flightplan& newFlightplan);

  void tableCopyClipboard();

  void showInformationMenu();
  void showProceduresMenu();
  void showProceduresMenuCustom();
  void showOnMapMenu();

  /* Enable or disable remarks widget */
  void updateRemarkWidget();

  /* Remarks changed */
  void remarksTextChanged();

  void remarksFlightPlanToWidget();

  void undoTriggered();
  void redoTriggered();
  bool updateStartPositionBestRunway(bool force, bool undo);
  void helpClicked();

  void dockVisibilityChanged(bool visible);
  void eraseAirway(int row);

  /* Time for clear selection triggered or scroll active to top */
  void cleanupTableTimeout();

  /* Departure, destination and procedures. */
  QString buildFlightplanLabel(bool print = false, bool titleOnly = false, QString *tooltip = nullptr) const;

  /* Distance and time. */
  QString buildFlightplanLabel2(bool print = false) const;

  void updateTableHeaders();
  void highlightNextWaypoint(int nearestLegIndex);
  void updateModelHighlights();
  void loadProceduresFromFlightplan(bool clearOldProcedureProperties, bool quiet, QStringList *procedureLoadingErrors);
  void loadAlternateFromFlightplan(bool quiet);

  void beforeRouteCalc();
  void updateFlightplanEntryAirway(int airwayId, atools::fs::pln::FlightplanEntry& entry);
  QIcon iconForLeg(const RouteLeg& leg, int size) const;

  void routeAddInternal(const atools::fs::pln::FlightplanEntry& entry, int insertIndex);
  int calculateInsertIndex(const atools::geo::Pos& pos, int legIndex);
  proc::MapProcedureTypes affectedProcedures(const QList<int>& indexes);
  void reportProcedureErrors(const QStringList& procedureLoadingErrors);

  void selectAllTriggered();

  void activateLegTriggered();
  void fontChanged();

  void visibleColumnsTriggered();
  void contextMenu(const QPoint& pos);

  void updateUnits();
  void updateErrorLabel();

  void editUserWaypointTriggered();

  bool canCalcSelection();

  /* Update navdata properties in flightplan properties for export and save */
  void updateRouteCycleMetadata();

  /* Move active leg to second top position */
  void scrollToActive();

  void viewScrolled(int);

  /* Selected rows in table. Updated on selection change. */
  QList<int> selectedRows;

  /* If route distance / direct distance if bigger than this value fail routing */
  static Q_DECL_CONSTEXPR float MAX_DISTANCE_DIRECT_RATIO = 2.0f;

  static Q_DECL_CONSTEXPR int ROUTE_UNDO_LIMIT = 50;

  atools::gui::ItemViewZoomHandler *zoomHandler = nullptr;

  /* Need a workaround since QUndoStack does not report current indices and clean state correctly */
  int undoIndex = 0;
  /* Clean index of the undo stack or -1 if not clean state exists */
  int undoIndexClean = 0;

  /* Network cache for flight plan calculation */
  atools::routing::RouteNetwork *routeNetworkRadio = nullptr, *routeNetworkAirway = nullptr;

  /* Flightplan and route objects */
  Route route; /* real route containing all segments */

  /* Current filename of empty if no route - also remember start and dest to avoid accidental overwriting */
  QString routeFilename, fileDepartureIdent, fileDestinationIdent;

  /* Same as above for cruise altitude */
  float fileCruiseAlt;

  /* Current loaded or saved format since the plans in the undo stack have different values */
  atools::fs::pln::FlightplanType fileIfrVfr;

  bool contextMenuOpen = false;

  QMainWindow *mainWindow;
  QTableView *view;
  MapQuery *mapQuery;
  AirwayTrackQuery *airwayQuery;
  AirportQuery *airportQuery;
  QStandardItemModel *model;
  QUndoStack *undoStack = nullptr;
  FlightplanEntryBuilder *entryBuilder = nullptr;
  atools::fs::pln::FlightplanIO *flightplanIO = nullptr;

  /* Route calculation dock window controller */
  RouteCalcWindow *routeWindow = nullptr;

  /* Do not update aircraft information more than every 0.1 seconds */
  static Q_DECL_CONSTEXPR int MIN_SIM_UPDATE_TIME_MS = 100;
  static Q_DECL_CONSTEXPR int ROUTE_ALT_CHANGE_DELAY_MS = 500;

  bool loadingDatabaseState = false;
  qint64 lastSimUpdate = 0;

  /* Copy of current active aircraft updated every MIN_SIM_UPDATE_TIME_MS */
  atools::fs::sc::SimConnectUserAircraft aircraft;

  SymbolPainter *symbolPainter = nullptr;

  atools::gui::TabWidgetHandler *tabHandlerRoute = nullptr;

  /* Timers for updating altitude delayer, clear selection while flying and moving active to top */
  QTimer routeAltDelayTimer, cleanupTableTimer;

  // Route table colum headings
  QStringList routeColumns, routeColumnTooltips;
  UnitStringTool *units = nullptr;
};

#endif // LITTLENAVMAP_ROUTECONTROLLER_H
