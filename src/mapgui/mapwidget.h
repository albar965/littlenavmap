/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#ifndef LITTLENAVMAP_NAVMAPWIDGET_H
#define LITTLENAVMAP_NAVMAPWIDGET_H

#include "mapgui/mappaintwidget.h"
#include "mapgui/mapwidgetflags.h"
#include "gui/mapposhistory.h"
#include "geo/linestring.h"

#include <QDateTime>
#include <QTimer>

class JumpBack;
class MainWindow;
class MapTooltip;
class MapVisible;
class QContextMenuEvent;
class QPushButton;
class QInputEvent;

namespace atools {
namespace sql {
class SqlRecord;
}
}

namespace map {
struct MapUserpoint;
struct MapParking;
struct MapHelipad;
struct MapAirport;
struct MapWaypoint;
struct MapVor;
struct MapNdb;
struct MapBase;
}

/*
 * LNM main map drawing widget which also covers all keyboard and mouse interaction.
 *
 * Keeps map position history and takes care of aircraft/waypoint centering when flying.
 */
class MapWidget :
  public MapPaintWidget
{
  Q_OBJECT

public:
  explicit MapWidget(MainWindow *parent);
  virtual ~MapWidget() override;

  MapWidget(const MapWidget& other) = delete;
  MapWidget& operator=(const MapWidget& other) = delete;

  /* Map zoom and scroll position history goto next or last */
  void historyNext();
  void historyBack();

  /* Save and restore markers, Marble plug-in settings, loaded KML files and more */
  void saveState() const;
  void restoreState();

  /* Get zoom and scroll position history */
  const atools::gui::MapPosHistory *getHistory() const
  {
    return &history;
  }

  /* If currently dragging flight plan: start, mouse and end position of the moving line. Start of end might be omitted
   * if dragging departure or destination */
  void getRouteDragPoints(atools::geo::LineString& fixedPos, QPoint& cur);

  /* Get source and current position while dragging a userpoint */
  void getUserpointDragPoints(QPoint& cur, QPixmap& pixmap);

  map::MapWeatherSource getMapWeatherSource() const;

  /* Called from weather reporter */
  void updateTooltip();

  /* The main window show event was triggered after program startup. */
  void mainWindowShown();

  /* Show home position or last postion on map after program startup. Also set status of widget to active */
  void showSavedPosOnStartup();

  /* Show or hide all map overlays (optionally excluding scalebar) */
  void showOverlays(bool show, bool showScalebar);

  /* End all distance line and route dragging modes */
  virtual void cancelDragAll() override;

  /* Start function move userpoint */
  void startUserpointDrag(const map::MapUserpoint& userpoint, const QPoint& point);

  /* New data from simconnect has arrived. Update aircraft position and track. */
  void simDataChanged(const atools::fs::sc::SimConnectData& simulatorData);

  /* Update sun shading from UI elements */
  void updateSunShadingOption();

  /* Reset drawing actions in UI only */
  void resetSettingActionsToDefault();

  /* Update the shown map object types depending on action status (toolbar or menu) */
  virtual void updateMapObjectsShown() override;

  /* Opens a dialog for configuration of a traffic pattern display object */
  void addPatternMark(const map::MapAirport& airport);

  /* Remove pattern at index and update the map */
  void removePatternMark(int id);

  /* Opens a dialog for configuration and adds a hold */
  void addHold(const map::MapResult& result, const atools::geo::Pos& position);

  /* Remove hold at index and update the map */
  void removeHoldMark(int id);

  /* Adds MSA diagram at position */
  void addMsaMark(map::MapAirportMsa airportMsa);

  /* Remove airport MSA diagram at index and update the map */
  void removeMsaMark(int id);

  /* Jump to the search center mark using default zoom */
  void showSearchMark();

  /* Zoom to home position using default zoom */
  void showHome();

  /* Change the search center mark to given position */
  void changeSearchMark(const atools::geo::Pos& pos);

  /* Show jump to coordinates dialog called from main menu or context menu */
  void jumpToCoordinatesCenter();

  /* Copy coordinates to clipboard from context menu or shortcut Ctrl+C on the map. */
  void copyCoordinatesCursor();

  /* Stop jump back timer on shutdown */
  void cancelJumpBack();

  /* Save current position and zoom distance as home */
  void changeHome();

  /* Add general (red) range ring */
  void addRangeMark(const atools::geo::Pos& pos, bool showDialog);
  void addRangeMark(const atools::geo::Pos& pos, const map::MapResult& result, bool showDialog);
  void addRangeMark(const atools::geo::Pos& pos, const map::MapAirport *airport, const map::MapVor *vor,
                    const map::MapNdb *ndb, const map::MapWaypoint *waypoint, const map::MapUserpoint *userpoint, bool showDialog);
  void fillRangeMarker(map::RangeMarker& rangeMarker, const atools::geo::Pos& pos, const map::MapResult& result);
  void fillRangeMarker(map::RangeMarker& rangeMarker, const atools::geo::Pos& pos, const map::MapAirport *airport,
                       const map::MapVor *vor, const map::MapNdb *ndb, const map::MapWaypoint *waypoint,
                       const map::MapUserpoint *userpoint);

  /* Add radio navaid range ring. Falls back to normal range rings if range is 0. */
  void addNavRangeMark(const atools::geo::Pos& pos, map::MapTypes type, const QString& displayIdent, const QString& frequency, float range);

  /* Remove range rings on index, print message and update map */
  void removeRangeMark(int id);

  /* Remove measurement line on index, print message and update map */
  void removeDistanceMark(int id);

  /* Set map details in widget and update statusbar */
  void setMapDetail(int level, int levelText);

  /* Reset details and feature visibility on the map back to default */
  void resetSettingsToDefault();

  /* Removes all range rings and distance measurement lines */
  void clearAllMarkers(map::MapTypes types);

  /* Return number of truncated points */
  void loadAircraftTrail(const QString& filename, int& numLoaded, int& numTruncated);
  void appendAircraftTrail(const QString& filename, int& numLoaded, int& numTruncated);

  /* Delete the current aircraft track. Will not stop collecting new track points */
  void deleteAircraftTrail();

  /* Clear separate track collected for the logbook */
  void deleteAircraftTrailLogbook();

  /* Clear all entries and reset current index */
  void clearHistory();

  /* Add and remove button for exiting full screen on map */
  void addFullScreenExitButton();
  void removeFullScreenExitButton();

  /* Flown distance from takeoff event. Independent of automatically created logbook entries. */
  float getTakeoffFlownDistanceNm() const
  {
    // Distance is already counted up before takeoff - wait until time is valid
    return takeoffTimeSim.isValid() ? static_cast<float>(takeoffLandingDistanceNm) : 0.f;
  }

  /* Time of takeoff detected. Independent of automatically created logbook entries. */
  const QDateTime& getTakeoffDateTime() const
  {
    return takeoffTimeSim;
  }

  void resetTakeoffLandingDetection();

  /* Currently dragging measurement line */
  int getCurrentDistanceMarkerId() const
  {
    return currentDistanceMarkerId;
  }

  /* Shows the configuration dialog for the degree grid */
  void showGridConfiguration();

  /* Logs map display settings */
  void printMapTypesToLog();

signals:
  /* Emitted when connection is established and user aircraft turned from invalid to valid */
  void userAircraftValidChanged();

  /* Fuel flow started or stopped */
  void aircraftEngineStarted(const atools::fs::sc::SimConnectUserAircraft& aircraft);
  void aircraftEngineStopped(const atools::fs::sc::SimConnectUserAircraft& aircraft);

  /* Aircraft was close to departure point on runway */
  void aircraftHasPassedTakeoffPoint(const atools::fs::sc::SimConnectUserAircraft& aircraft);

  /* State isFlying between last and current aircraft has changed */
  void aircraftTakeoff(const atools::fs::sc::SimConnectUserAircraft& aircraft);
  void aircraftLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft, float flownDistanceNm);

  /* Set parking position, departure, destination for flight plan from context menu */
  void routeSetParkingStart(const map::MapParking& parking);
  void routeSetHelipadStart(const map::MapHelipad& helipad);

  /* Set route departure or destination from context menu */
  void routeSetStart(const map::MapAirport& ap);
  void routeSetDest(const map::MapAirport& ap);
  void routeAddAlternate(const map::MapAirport& ap);

  /* Add, replace or delete object from flight plan from context menu or drag and drop.
   *  index = 0: prepend to route
   *  index = size()-1: append to route */
  void routeAdd(int id, const atools::geo::Pos& userPos, map::MapTypes type, int legIndex);
  void routeReplace(int id, const atools::geo::Pos& userPos, map::MapTypes type, int oldIndex);

  /* Direct to flight plan waypoint, navaid or position */
  void directTo(int id, const atools::geo::Pos& userPos, map::MapTypes type, int legIndex);

  /* Show a map object in the search panel (context menu) */
  void showInSearch(map::MapTypes type, const atools::sql::SqlRecord& record, bool select);

  /* Show information about objects from single click or context menu */
  void showInformation(const map::MapResult& result);

  /* Select result in flight plan table */
  void showInRoute(int index);

  /* Add user point and pass result to it so it can prefill the dialog */
  void addUserpointFromMap(const map::MapResult& result, const atools::geo::Pos& pos, bool airportAddon);
  void editUserpointFromMap(const map::MapResult& result);
  void deleteUserpointFromMap(int id);

  void editLogEntryFromMap(int id);

  /* Passed point with updated coordinates */
  void moveUserpointFromMap(const map::MapUserpoint& point);

  /* Show approaches from context menu */
  void showProcedures(const map::MapAirport& airport, bool departureFilter, bool arrivalFilter);
  void showCustomApproach(const map::MapAirport& airport, const QString& suffix);
  void showCustomDeparture(const map::MapAirport& airport, const QString& suffix);

  /* Emitted when the user presses the on-screen button */
  void exitFullScreenPressed();

  /* Add the complete procedure to the route */
  void routeInsertProcedure(const proc::MapProcedureLegs& legs);

private:
  /* For touchscreen mode. Grid of 3x3 rectangles numbered from lef to right and top to bottom */
  enum TouchArea
  {
    ZOOMIN,
    MOVEUP,
    ZOOMOUT,

    MOVELEFT,
    CENTER,
    MOVERIGHT,

    BACKWARD,
    MOVEDOWN,
    FORWARD,

    NONE = 999
  };

  /* Change map detail level in paint layer and update map visible tooltip in statusbar */
  void setDetailLevel(int level, int levelText);

  /* Update tooltip in case of weather changes */
  void showTooltip(bool update);

  /* Convert paint layer value to menu actions checked / not checked */
  virtual map::MapWeatherSource weatherSourceFromUi() override;
  void weatherSourceToUi(map::MapWeatherSource weatherSource);

  /* Convert paint layer value to menu actions checked / not checked */
  virtual map::MapSunShading sunShadingFromUi() override;
  void sunShadingToUi(map::MapSunShading sunShading);

  virtual bool checkPos(const atools::geo::Pos& pos) override;

  virtual void resizeEvent(QResizeEvent *event) override;

  /* Connect menu actions to overlays */
  void connectOverlayMenus();

  /* Show information from context menu or single click */
  void handleInfoClick(const QPoint& point);

  /* Highlight flight plan leg if enabled. */
  void handleRouteClick(const QPoint& point);

  /* Scroll and zoom for touchscreen area mode */
  bool handleTouchAreaClick(QMouseEvent *event);
  TouchArea touchAreaClick(QMouseEvent *event);

  /* Touch/navigation areas are enabled and cursor is within a touch area. false for areas CENTER and NONE. */
  bool touchAreaClicked(QMouseEvent *event);

  /* Cancel mouse actions */
  void cancelDragDistance();
  void cancelDragRoute();
  void cancelDragUserpoint();

  /* Full redraw after timout when drag and drop to avoid missing objects while moving rubber band */
  void resetPaintForDrag();

  /* Display elevation at mouse cursor after a short timeout */
  void elevationDisplayTimerTimeout();

  /* Start a line measurement after context menu selection or click+modifier */
  void addDistanceMarker(const atools::geo::Pos& pos, const map::MapResult& result);
  void addDistanceMarker(const atools::geo::Pos& pos, const map::MapAirport *airport, const map::MapVor *vor,
                         const map::MapNdb *ndb, const map::MapWaypoint *waypoint, const map::MapUserpoint *userpoint);
  void fillDistanceMarker(map::DistanceMarker& distanceMarker, const atools::geo::Pos& pos, const map::MapResult& result);
  void fillDistanceMarker(map::DistanceMarker& distanceMarker, const atools::geo::Pos& pos, const map::MapAirport *airport,
                          const map::MapVor *vor, const map::MapNdb *ndb, const map::MapWaypoint *waypoint,
                          const map::MapUserpoint *userpoint);

  /* Show the given object in the search search window with filters and selection set */
  void showResultInSearch(const map::MapBase *base);

  /* Timer for takeoff and landing recognition fired */
  void takeoffLandingTimeout();
  void fuelOnOffTimeout();

  void simDataCalcTakeoffLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft,
                                 const atools::fs::sc::SimConnectUserAircraft& last);
  void simDataCalcFuelOnOff(const atools::fs::sc::SimConnectUserAircraft& aircraft,
                            const atools::fs::sc::SimConnectUserAircraft& last);

  /* Center aircraft again after scrolling or zooming */
  void jumpBackToAircraftTimeout(const atools::geo::Pos& pos);

  /* Needed filter to avoid and/or disable some Marble pecularities */
  bool eventFilter(QObject *obj, QEvent *eventParam) override;

  /* Check for modifiers on mouse click and start actions like range rings on Ctrl+Click */
  bool mousePressCheckModifierActions(QMouseEvent *event);

  /* Update the flight plan from a drag and drop result. Show a menu if multiple objects are
   * found at the button release position. */
  void updateRoute(const QPoint& point, int leg, int pointIndex, bool fromClickAdd, bool fromClickAppend);

  /* Show menu to allow selection of a map feature below the cursor */
  bool showFeatureSelectionMenu(int& id, map::MapTypes& type, const map::MapResult& result, const QString& menuText);

  void jumpToCoordinatesPos(const atools::geo::Pos& pos);
  void copyCoordinatesPos(const atools::geo::Pos& pos);

  /* MapPaintWidget overrides for UI updates mostly ============================================================ */
  virtual void optionsChanged() override;
  virtual void overlayStateFromMenu() override;
  virtual void overlayStateToMenu() const override;

  virtual void updateThemeUi(const QString& themeId) override;
  virtual void updateMapVisibleUi() const override;
  virtual void updateMapVisibleUiPostDatabaseLoad() const override;

  /* Called at start of user interaction like moving or scrolling */
  virtual void jumpBackToAircraftStart() override;

  /* From center button. Stops timer. */
  virtual void jumpBackToAircraftCancel() override;

  /* Hide and prevent re-show */
  virtual void hideTooltip() override;

  /* Update result set for tooltip */
  void updateTooltipResult();

  virtual void handleHistory() override;
  virtual void updateShowAircraftUi(bool centerAircraftChecked) override;

  /* Overloaded methods from QWidget ============================================================ */
  virtual void mousePressEvent(QMouseEvent *event) override;
  virtual void mouseReleaseEvent(QMouseEvent *event) override;
  virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
  virtual void mouseMoveEvent(QMouseEvent *event) override;
  virtual void wheelEvent(QWheelEvent *event) override;

  virtual void contextMenuEvent(QContextMenuEvent *event) override;
  virtual void focusOutEvent(QFocusEvent *) override;
  virtual void keyPressEvent(QKeyEvent *keyEvent) override;
  virtual void leaveEvent(QEvent *) override;

  /* Catch tooltip event */
  virtual bool event(QEvent *event) override;

  /* Aircraft and next leg centering set in options and all other conditions are met (flight plan present, etc.) */
  bool isCenterLegAndAircraftActive();

  void zoomInOut(bool directionIn, bool smooth);

  /* true if point is not hidden by globe */
  bool isPointVisible(const QPoint& point)
  {
    return getGeoPos(point).isValid();
  }

  void debugMovingAircraft(QInputEvent *event, int upDown);

  /* Some global actions which require handling in the context menu have to be disconnected to avoid double call */
  void disconnectGlobalActions();
  void connectGlobalActions();

  int screenSearchDistance /* Radius for click sensitivity */,
      screenSearchDistanceTooltip /* Radius for tooltip sensitivity */;

  /* Used to check if mouse moved between button down and up */
  QPoint mouseMoved;

  mapwin::MouseStates mouseState = mapwin::NONE;
  bool contextMenuActive = false;

  /* Current moving position when dragging a flight plan point or leg */
  QPoint routeDragCur;

  /* Do a full redraw after timout when using drag and drop */
  QTimer resetPaintForDragTimer;

  /* Fixed points of route drag which will not move with the mouse */
  atools::geo::LineString routeDragFixed;

  int routeDragPoint = -1 /* Index of changed point */,
      routeDragLeg = -1 /* index of changed leg */;

  /* Current position and pixmap when drawing userpoint on map */
  QPoint userpointDragCur;
  map::MapUserpoint *userpointDrag;
  QPixmap userpointDragPixmap;

  /* Save last tooltip position. If invalid/null no tooltip will be shown */
  QPoint tooltipGlobalPos;
  map::MapResult *mapResultTooltip, *mapResultTooltipLast, *mapResultInfoClick;

  MapTooltip *mapTooltip;

  /* Backup of distance marker for drag and drop in case the action is cancelled */
  map::DistanceMarker *distanceMarkerBackup;

  atools::gui::MapPosHistory history;

  /* Need to check if the zoom and position was changed by the map history to avoid recursion */
  bool noStoreInHistory = false;

  /* Delay display of elevation display to avoid lagging mouse movements */
  QTimer elevationDisplayTimer;

  /* Delay takeoff and landing messages to avoid false recognition of bumpy landings.
   * Calls MapWidget::takeoffLandingTimeout()  */
  QTimer takeoffLandingTimer, fuelOnOffTimer;

  /* Flown distance from takeoff event */
  double takeoffLandingDistanceNm = 0.;

  /* Used in simDataChanged() to zoom close to the airport after touchdown */
  bool touchdownDetectedZoom = false, takeoffDetectedZoom = false;

  /* Time of takeoff or invalid if not detected yet */
  QDateTime takeoffTimeSim;

  /* Saves jump back position and altitude in km */
  JumpBack *jumpBack;

  /* Sum up mouse wheel or trackpad movement before zooming */
  int lastWheelAngleX = 0, lastWheelAngleY = 0;

  MainWindow *mainWindow;

  MapVisible *mapVisible;

  /* Used for distance calculation */
  atools::fs::sc::SimConnectUserAircraft *takeoffLandingLastAircraft;

  /* Used to check for simulator aircraft updates */
  qint64 lastSimUpdateMs = 0L;
  qint64 lastCenterAcAndWp = 0L;
  qint64 lastSimUpdateTooltipMs = 0L;

  /* Maps overlay name to menu action for hide/show */
  QHash<QString, QAction *> mapOverlays;

  /* Distance marker that is changed using drag and drop */
  int currentDistanceMarkerId = -1;

  QPushButton *pushButtonExitFullscreen = nullptr;

};

#endif // LITTLENAVMAP_NAVMAPWIDGET_H
