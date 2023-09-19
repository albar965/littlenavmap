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

#include "routing/routenetworktypes.h"
#include "route/route.h"
#include "route/routecommandflags.h"

#include <QTimer>

class QUndoStack;

class QAction;
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

class AirportQuery;
class AirwayTrackQuery;
class FlightplanEntryBuilder;
class QItemSelection;
class QMainWindow;
class QStandardItemModel;
class QTableView;
class QTextCursor;
class RouteCalcDialog;
class RouteCommand;
class RouteLabel;
class SymbolPainter;
class UnitStringTool;

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
  explicit RouteController(QMainWindow *parent, QTableView *tableView);
  virtual ~RouteController() override;

  RouteController(const RouteController& other) = delete;
  RouteController& operator=(const RouteController& other) = delete;

  /* Creates a new plan and emits routeChanged */
  void newFlightplan();

  /* Loads flight plan from FSX PLN file, checks for proper start position (shows notification dialog)
   * and emits routeChanged. Uses file name as new current name  */
  bool loadFlightplan(const QString& filename);
  void loadFlightplan(atools::fs::pln::Flightplan flightplan, atools::fs::pln::FileFormat format,
                      const QString& filename, bool changed, bool adjustAltitude, bool undo, bool warnAltitude);

  /* Load the plan from a string in LNMPLN format */
  bool loadFlightplanLnmStr(const QString& string);

  /* Load from route description */
  void loadFlightplanRouteStr(const QString& routeString);

  /* Loads flight plan from FSX PLN file and appends it to the current flight plan.
   * Use -1 for insertBefore to append.
   * Emits routeChanged. */
  bool insertFlightplan(const QString& filename, int insertBefore);

  /* Saves flight plan using the given name and file format and uses file name as new current name */
  bool saveFlightplanLnm();
  bool saveFlightplanLnmAs(const QString& filename);
  bool saveFlightplanLnmAsSelection(const QString& filename);

  /* Save temporary to settings folder or delete temp if plan is empty */
  void saveFlightplanLnmDefault();

  /* Called if export dialog saved an LNMPLN file */
  void saveFlightplanLnmExported(const QString& filename);

  /* Save and reload widgets state and current flight plan name */
  void saveState();
  void restoreState();

  /* Get the route only */
  const Route& getRouteConst() const
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

  float getRouteDistanceNm() const
  {
    return route.getTotalDistance();
  }

  /* get altitude in feet as set in the widget */
  float getCruiseAltitudeWidget() const;

  bool  doesLnmFilenameMatchRoute() const;

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
   * @param legIndex Insert after the leg with this index. Will use nearest leg if index is -1 and append if INVALID_INDEX.
   */
  void routeAdd(int id, atools::geo::Pos userPos, map::MapTypes type, int legIndex);

  /* Add an approach and/or a transition */
  void routeAddProcedure(proc::MapProcedureLegs legs);

  /* Same as above but replaces waypoint at legIndex */
  void routeReplace(int id, atools::geo::Pos userPos, map::MapTypes type, int legIndex);

  /* Delete waypoint at the given index. Will also delete departure or destination */
  void routeDelete(int index);

  /* Called by action */
  void deleteSelectedLegsTriggered();

  void routeDirectTo(int id, atools::geo::Pos userPos, map::MapTypes type, int legIndexDirectTo);
  void directToTriggered();

  /* Set departure parking position. If the airport of the parking spot is different to
   * the current departure it will be replaced too. */
  void routeSetParking(const map::MapParking& parking); /* From map context menu */
  void routeSetHelipad(const map::MapHelipad& helipad); /* From map context menu */
  void routeClearParkingAndStart(); /* Main menu and parking dialog - set airport as start */

  /* Shows the dialog to select departure parking or start position.
   *  @return true if position was set. false is dialog was canceled. */
  bool selectDepartureParking();

  /* "Calculate" a direct flight plan that has no waypoints. */
  void calculateDirect();

  /* Open flight plan calculation window. */
  void calculateRouteWindowFull();
  void calculateRouteWindowSelection();
  void calculateRouteWindowShow();

  /* Reverse order of all waypoints, swap departure and destination and automatically
   * select a new start position (best runway) */
  void reverseRoute();

  /* Change in options dialog */
  void optionsChanged();

  /* Tracks downloaded or deleted */
  void tracksChanged();

  /* UI style changed */
  void styleChanged();

  /* Get the route table as a HTML snippet only containing the table and header.
   * Uses own colors for table background. Used by web server. */
  QString getFlightplanTableAsHtml(float iconSizePixel, bool print) const;

  /* Same as above but full HTML document for export */
  QString getFlightplanTableAsHtmlDoc(float iconSizePixel) const;

  /* Get flight plan extracted from table selection */
  Route getRouteForSelection() const;

  /* Insert a flight plan table as QTextTable object at the cursor position.
   * @param selectedCols Physical/logical and not view order. */
  void flightplanTableAsTextTable(QTextCursor& cursor, const QBitArray& selectedCols, float fontPointSize) const;

  /* Get header for print report */
  void flightplanHeaderPrint(atools::util::HtmlBuilder& html, bool titleOnly) const;

  /* Copy the route as a string to the clipboard */
  void routeStringToClipboard() const;

  /* Adjust altitude according to simple east/west VFR/IFR rules */
  void adjustFlightplanAltitude();

  /* Select result in flight plan table */
  void showInRoute(int index);

  FlightplanEntryBuilder *getFlightplanEntryBuilder() const
  {
    return entryBuilder;
  }

  void disconnectedFromSimulator();

  void simDataChanged(const atools::fs::sc::SimConnectData& simulatorData);

  void editUserWaypointName(int index);

  void shownMapFeaturesChanged(map::MapTypes types);

  void activateLegManually(int index);
  void resetActiveLeg();
  void updateActiveLeg();

  QString procedureTypeText(const RouteLeg& leg);

  void clearTableSelection();

  bool hasTableSelection();

  void aircraftPerformanceChanged();
  void windUpdated();

  /* Get all available table columns with linefeeds and units replaced. Fixed order independent of table view. */
  QStringList getAllRouteColumns() const;

  /* Add custom procedure and probably set new destination airport */
  void showCustomApproach(map::MapAirport airport, QString dialogHeader);
  void showCustomDeparture(map::MapAirport airport, QString dialogHeader);

  /* Add custom proc for departure or destination airport. Called from main menu. */
  void showCustomApproachMainMenu();
  void showCustomDepartureMainMenu();

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
  bool isLnmFormatFlightplan() const;

  /* Get error messages from route parsing */
  bool hasErrors() const;
  QStringList getErrorStrings() const;

  /* Update header label with sim information */
  void updateRemarkHeader();

  /* Reset tab bar */
  void resetTabLayout();

  /* Update error label footer in flight plan window */
  void updateFooterErrorLabel();

  /* Update travel times, fuel, wind and calculated altitude values in table view model after update */
  void updateModelTimeFuelWindAlt();

signals:
  /* Show airport on map */
  void showRect(const atools::geo::Rect& rect, bool doubleClick);

  /* Show flight plan waypoint or user position on map */
  void showPos(const atools::geo::Pos& pos, float zoom, bool doubleClick);

  /* Change distance search center */
  void changeMark(const atools::geo::Pos& pos);

  /* Selection in table view has changed. Update highlights on map */
  void routeSelectionChanged(int selected, int total);

  /* Route has changed */
  void routeChanged(bool geometryChanged, bool newFlightplan = false);

  void routeAltitudeChanged(float altitudeFeet);

  /* Show information about the airports or navaids in the search result */
  void showInformation(map::MapResult result);

  /* Show approach information about the airport */
  void showProcedures(map::MapAirport airport, bool departureFilter, bool arrivalFilter);

  /* Emitted before route calculation to stop any background tasks */
  void preRouteCalc();

  /* Emitted by context menu */
  void routeInsert(int beforeRow);

  void addAirportMsa(map::MapAirportMsa airportMsa);

  void addUserpointFromMap(const map::MapResult& result, const atools::geo::Pos& pos, bool airportAddon);

private:
  friend class RouteCommand;

  /* Move selected rows */
  enum MoveDirection
  {
    MOVE_NONE = 0,
    MOVE_DOWN = 1,
    MOVE_UP = -1
  };

  /* Saves flight plan using LNM format. Returns true on success. */
  bool saveFlightplanLnmInternal(const QString& filename, bool silent);

  /* Saves flight plan sippet using LNM format to given name. Given range must not contains procedures or alternates. */
  bool saveFlightplanLnmSelectionAs(const QString& filename, int from, int to) const;

  /* Called by undo command */
  void changeRouteUndo(const atools::fs::pln::Flightplan& newFlightplan);

  /* Called by undo command */
  void changeRouteRedo(const atools::fs::pln::Flightplan& newFlightplan);

  /* Save undo state before and after change */
  RouteCommand *preChange(const QString& text = QString(), rctype::RouteCmdType rcType = rctype::EDIT);
  void postChange(RouteCommand *undoCommand);

  void routeSetStartPosition(map::MapStart start);

  void doubleClick(const QModelIndex& index);
  void showAtIndex(int index, bool info, bool map, bool doubleClick);

  void tableContextMenu(const QPoint& pos);

  void tableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

  void moveSelectedLegsDownTriggered();
  void moveSelectedLegsUpTriggered();
  void moveSelectedLegsInternal(MoveDirection direction);
  void deleteSelectedLegs(const QList<int>& rows);
  void deleteSelectedLegsInternal(const QList<int>& rows);

  QList<int> getSelectedRows(bool reverseRoute) const;

  void selectList(const QList<int>& selectedRows, int offset);
  void selectRange(int from, int to);

  /* Enable/disable move, delete and direct to */
  void updateActions();
  void updateDirectToAction();

  int routeAddInternal(int id, atools::geo::Pos userPos, map::MapTypes type, int legIndex, bool presentPos = false);

  void routeSetDepartureInternal(const map::MapAirport& airport);
  void routeSetDestinationInternal(const map::MapAirport& airport);

  void updateTableModelAndErrors();

  void routeAltChanged();
  void routeAltChangedDelayed();

  void routeTypeChanged();

  /* Reset route and clear undo stack (new route) */
  void clearRouteAndUndo();
  void clearRoute();

  /* Calculate flight plan pressed in dock window */
  void calculateRoute();
  bool calculateRouteInternal(atools::routing::RouteFinder *routeFinder,
                              const QString& commandName,
                              bool fetchAirways, float altitudeFt, int fromIndex, int toIndex,
                              atools::routing::Modes mode);

  /* Assign type and altitude from GUI */
  void updateFlightplanFromWidgets(atools::fs::pln::Flightplan& flightplan);
  void updateFlightplanFromWidgets();

  /* Used by undo/redo */
  void changeRouteUndoRedo(const atools::fs::pln::Flightplan& newFlightplan);

  void tableCopyClipboardTriggered();

  /* From context menu */
  void showInformationTriggered();

  /* From context menu */
  void showProceduresTriggered();

  /* From context menu */
  void showCustomApproachRouteTriggered();

  /* From context menu */
  void showCustomDepartureRouteTriggered();

  void showOnMapTriggered();

  void showInformationInternal(const RouteLeg& routeLeg);

  /* Enable or disable remarks widget */
  void updateRemarkWidget();

  /* Remarks changed */
  void remarksTextChanged();

  void remarksFlightPlanToWidget();

  void undoTriggered();
  void redoTriggered();
  void helpClicked();

  void dockVisibilityChanged(bool visible);

  void updateTableHeaders();
  void highlightNextWaypoint(int activeLegIdx);

  /* Set colors for procedures and missing objects like waypoints and airways.
   * Also fills flight plan errors */
  void updateModelHighlightsAndErrors();

  /* Fill the route procedure legs structures with data based on the procedure properties in the flight plan */
  void loadProceduresFromFlightplan(bool clearOldProcedureProperties, bool cleanupRoute, bool autoresolveTransition);

  void beforeRouteCalc();
  void updateFlightplanEntryAirway(int airwayId, atools::fs::pln::FlightplanEntry& entry);
  QIcon iconForLeg(const RouteLeg& leg, int size) const;

  int calculateInsertIndex(const atools::geo::Pos& pos, int legIndex);
  proc::MapProcedureTypes affectedProcedures(const QList<int>& indexes);

  void selectAllTriggered();

  void activateLegTriggered();
  void fontChanged();

  void routeTableOptions();
  void contextMenu(const QPoint& pos);

  void updateUnits();

  void editUserWaypointTriggered();

  bool canCalcSelection();
  bool canSaveSelection();

  /* Move active leg to second top position */
  void scrollToActive();

  /* Remove all errors from lists */
  void clearAllErrors();

  /* Departure or destination link in the header clicked */
  void flightplanLabelLinkActivated(const QString& link);

  void updatePlaceholderWidget();

  /* Time for clear selection triggered or scroll active to top */
  void cleanupTableTimeout();

  /* Restart timer when user interacts with the table */
  void updateCleanupTimer();

  /* true if neither context menu is open nor scroll sliders are pressed down.*/
  bool canCleanupTable();

  /* Clear all names and other properties */
  void clearFlightplan();

  void updateComboBoxFromFlightplanType();

  /* Selected rows in table. Updated on selection change. */
  QList<int> selectedRows;

  /* If route distance / direct distance if bigger than this value fail routing */
  static Q_DECL_CONSTEXPR float MAX_DISTANCE_DIRECT_RATIO = 2.0f;

  static Q_DECL_CONSTEXPR int ROUTE_UNDO_LIMIT = 50;

  atools::gui::ItemViewZoomHandler *zoomHandler = nullptr;

  /* Need a workaround since QUndoStack does not report current indices and clean state correctly */
  int undoIndex = 0;
  /* Clean index of the undo stack or -1 if no clean state exists */
  int undoIndexClean = 0;

  /* Network cache for flight plan calculation */
  atools::routing::RouteNetwork *routeNetworkRadio = nullptr, *routeNetworkAirway = nullptr;

  /* Flightplan and route objects */
  Route route; /* real route containing all segments */

  /* Current filename of empty if no route - also remember start and dest to avoid accidental overwriting */
  QString routeFilename, routeFilenameDefault, fileDepartureIdent, fileDestinationIdent;

  /* Same as above for cruise altitude */
  float fileCruiseAltFt;

  /* Current loaded or saved format since the plans in the undo stack have different values */
  atools::fs::pln::FlightplanType fileIfrVfr;

  bool contextMenuOpen = false;

  QMainWindow *mainWindow;
  QTableView *tableViewRoute;
  AirportQuery *airportQuery;
  QStandardItemModel *model;
  QUndoStack *undoStack = nullptr;
  FlightplanEntryBuilder *entryBuilder = nullptr;
  atools::fs::pln::FlightplanIO *flightplanIO = nullptr;

  // Ignore follow selection in tableSelectionChanged() if its origin is from showInRoute()
  bool ignoreSelectionEvent = false;

  QAction *undoAction = nullptr, *redoAction = nullptr;

  /* Takes care of the top label */
  RouteLabel *routeLabel = nullptr;

  /* Route calculation dock window controller */
  RouteCalcDialog *routeCalcDialog = nullptr;

  bool loadingDatabaseState = false;
  qint64 lastSimUpdate = 0;

  /* Currently active leg or -1 if none. Used for table highlighting. */
  int activeLegIndex = -1;

  /* Copy of current active aircraft updated every MIN_SIM_UPDATE_TIME_MS */
  atools::fs::sc::SimConnectUserAircraft aircraft;

  SymbolPainter *symbolPainter = nullptr;

  atools::gui::TabWidgetHandler *tabHandlerRoute = nullptr;

  /* Timers for updating altitude delayer, clear selection while flying and moving active to top */
  QTimer routeAltDelayTimer, tableCleanupTimer;

  /* Route table colum headings */
  QStringList routeColumns, routeColumnDescription;
  UnitStringTool *units = nullptr;

  /* Errors collected when parsing route for model, reading route legs or reading procedures */
  QStringList flightplanErrors, parkingErrors, procedureErrors, alternateErrors;

  /* String to save flight plan temporarily in LNMPLN format when switching databases */
  QString tempFlightplanStr;
  bool trackErrors = false;
};

#endif // LITTLENAVMAP_ROUTECONTROLLER_H
