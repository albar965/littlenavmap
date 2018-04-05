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

#ifndef LITTLENAVMAP_NAVMAPWIDGET_H
#define LITTLENAVMAP_NAVMAPWIDGET_H

#include "common/maptypes.h"
#include "gui/mapposhistory.h"
#include "fs/sc/simconnectdata.h"
#include "common/aircrafttrack.h"

#include <QTimer>
#include <QWidget>

#include <marble/GeoDataLatLonAltBox.h>
#include <marble/MarbleWidget.h>

namespace atools {
namespace sql {
class SqlRecord;
}

namespace geo {
class Pos;
class Rect;
}
namespace fs {
class SimConnectData;
}
}

class QContextMenuEvent;
class MainWindow;
class MapPaintLayer;
class MapQuery;
class AirportQuery;
class RouteController;
class MapTooltip;
class QRubberBand;
class MapScreenIndex;
class Route;
class MapVisible;

namespace mw {
/* State of click, drag and drop actions on the map */
enum MouseState
{
  NONE = 0x0000, /* Nothing */

  DRAG_DISTANCE = 0x0001, /* A new distance measurement line is dragged */
  DRAG_CHANGE_DISTANCE = 0x0002, /* A present distance measurement line is changed dragging */

  DRAG_ROUTE_LEG = 0x0004, /* Changing a flight plan leg by adding a new point */
  DRAG_ROUTE_POINT = 0x0008, /* Changing the flight plan by replacing a present waypoint */

  DRAG_USER_POINT = 0x0010, /* Moving a userpoint around */

  DRAG_POST = 0x0020, /* Mouse released - all done */
  DRAG_POST_MENU = 0x0040, /* A menu is opened after selecting multiple objects.
                            * Avoid cancelling all drag when loosing focus */
  DRAG_POST_CANCEL = 0x0080, /* Right mousebutton clicked - cancel all actions */

  DRAG_ALL = mw::DRAG_DISTANCE | mw::DRAG_CHANGE_DISTANCE | mw::DRAG_ROUTE_LEG | mw::DRAG_ROUTE_POINT |
             mw::DRAG_USER_POINT
};

Q_DECLARE_FLAGS(MouseStates, MouseState);
Q_DECLARE_OPERATORS_FOR_FLAGS(mw::MouseStates);
}

namespace proc {
struct MapProcedureLeg;

struct MapProcedureLegs;

}
class MapWidget :
  public Marble::MarbleWidget
{
  Q_OBJECT

public:
  MapWidget(MainWindow *parent);
  virtual ~MapWidget();

  /* Save and restore markers, Marble plug-in settings, loaded KML files and more */
  void saveState();
  void restoreState();

  /* Jump to position on the map using the given zoom distance (if not equal -1) */
  void showPos(const atools::geo::Pos& pos, float distanceNm, bool doubleClick);

  /* Show the bounding rectangle on the map */
  void showRect(const atools::geo::Rect& rect, bool doubleClick);

  /* Jump to the search center mark using default zoom */
  void showSearchMark();

  /* Zoom to home position using default zoom */
  void showHome();

  /* Change the search center mark to given position */
  void changeSearchMark(const atools::geo::Pos& pos);

  /* Save current position and zoom distance as home */
  void changeHome();

  /* Show user simulator aircraft. state is tool button state */
  void showAircraft(bool centerAircraftChecked);

  /* Update hightlighted objects */
  void changeSearchHighlights(const map::MapSearchResult& positions);
  void changeRouteHighlights(const QList<int>& routeHighlight);
  void changeProcedureLegHighlights(const proc::MapProcedureLeg *leg);

  void changeApproachHighlight(const proc::MapProcedureLegs& approach);

  /* Update route screen coordinate index */
  void routeChanged(bool geometryChanged);
  void routeAltitudeChanged(float altitudeFeet);

  /* New data from simconnect has arrived. Update aircraft position and track. */
  void simDataChanged(const atools::fs::sc::SimConnectData& simulatorData);

  /* Hightlight a point along the route while mouse over in the profile window */
  void highlightProfilePoint(const atools::geo::Pos& pos);

  /* Stop showing aircraft position on map */
  void disconnectedFromSimulator();

  /* Clear previous aircraft track */
  void connectedToSimulator();

  /* Add a KML file to map display. The file will be restored on program startup */
  bool addKmlFile(const QString& kmlFile);

  /* Remove all KML files from map */
  void clearKmlFiles();

  const atools::geo::Pos& getSearchMarkPos() const
  {
    return searchMarkPos;
  }

  const atools::geo::Pos& getHomePos() const
  {
    return homePos;
  }

  /* Getters used by the painters */
  const map::MapSearchResult& getSearchHighlights() const;
  const proc::MapProcedureLeg& getProcedureLegHighlights() const;

  const proc::MapProcedureLegs& getProcedureHighlight() const;

  const QList<int>& getRouteHighlights() const;

  const QList<map::RangeMarker>& getRangeRings() const;

  const QList<map::DistanceMarker>& getDistanceMarkers() const;

  const AircraftTrack& getAircraftTrack() const
  {
    return aircraftTrack;
  }

  /* If currently dragging flight plan: start, mouse and end position of the moving line. Start of end might be omitted
   * if dragging departure or destination */
  void getRouteDragPoints(atools::geo::Pos& from, atools::geo::Pos& to, QPoint& cur);

  /* Get source and current position while dragging a userpoint */
  void getUserpointDragPoints(QPoint& cur, QPixmap& pixmap);

  /* Delete the current aircraft track. Will not stop collecting new track points */
  void deleteAircraftTrack();

  /* Add general (red) range ring */
  void addRangeRing(const atools::geo::Pos& pos);

  /* Add radio navaid range ring */
  void addNavRangeRing(const atools::geo::Pos& pos, map::MapObjectTypes type, const QString& ident,
                       const QString& frequency, int range);

  /* Removes all range rings and distance measurement lines */
  void clearRangeRingsAndDistanceMarkers();

  /* If true stop downloading map data */
  void workOffline(bool offline);

  /* Disconnect painter to avoid updates while no data is available */
  void preDatabaseLoad();

  void optionsChanged();

  /* Update map */
  void postDatabaseLoad();

  /* Set map theme.
   * @param theme filename of the map theme
   * @param index MapThemeComboIndex
   */
  void setTheme(const QString& theme, int index);

  /* Map zoom and scroll position history goto next or last */
  void historyNext();
  void historyBack();

  atools::gui::MapPosHistory *getHistory()
  {
    return &history;
  }

  /* Show home postion or last postion on map after program startup */
  void showSavedPosOnStartup();

  /* Show points of interest and other labels for certain map themes */
  void setShowMapPois(bool show);

  /* Define which airport or navaid types are shown on the map */
  void setShowMapFeatures(map::MapObjectTypes type, bool show);
  void setShowMapAirspaces(map::MapAirspaceFilter types);

  map::MapObjectTypes getShownMapFeatures() const;
  map::MapAirspaceFilter getShownAirspaces() const;
  map::MapAirspaceFilter getShownAirspaceTypesByLayer() const;

  /* Change map detail level */
  void increaseMapDetail();
  void decreaseMapDetail();
  void defaultMapDetail();
  void setMapDetail(int factor);

  /* Update the shown map object types depending on action status (toolbar or menu) */
  void updateMapObjectsShown();

  /* Update tooltip in case of weather changes */
  void showTooltip(bool update);
  void updateTooltip();

  const atools::fs::sc::SimConnectUserAircraft& getUserAircraft() const;

  const QVector<atools::fs::sc::SimConnectAircraft>& getAiAircraft() const;

  MainWindow *getParentWindow() const
  {
    return mainWindow;
  }

  const QStringList& getKmlFiles() const
  {
    return kmlFilePaths;
  }

  /* The main window show event was triggered after program startup. */
  void mainWindowShown();

  /* End all distance line and route dragging modes */
  void cancelDragAll();

  void showOverlays(bool show);

  /* Stores delta values depending on fast or slow update. User aircraft is only updated if
   * delta values are exceeded. */
  struct SimUpdateDelta
  {
    float manhattanLengthDelta;
    float headingDelta;
    float speedDelta;
    float altitudeDelta;
    qint64 timeDeltaMs;
  };

  /* Index values of the map theme combo box */
  enum MapThemeComboIndex
  {
    OPENSTREETMAP,
    OPENSTREETMAPROADS,
    OPENTOPOMAP,
    STAMENTERRAIN,
    CARTOLIGHT,
    CARTODARK,
    SIMPLE,
    PLAIN,
    ATLAS,
    CUSTOM, /* Custom maps count from this index up */
    INVALID = -1
  };

  void restoreHistoryState();

  void resetSettingsToDefault();

  void resetSettingActionsToDefault();

  /* Stop timer and cancel any jumping back */
  void jumpBackToAircraftCancel();

  void onlineClientAndAtcUpdated();
  void onlineNetworkChanged();

signals:
  /* Emitted whenever the result exceeds the limit clause in the queries */
  void resultTruncated(int truncatedTo);

  /* Search center has changed by context menu */
  void searchMarkChanged(const atools::geo::Pos& mark);

  /* Show a map object in the search panel (context menu) */
  void showInSearch(map::MapObjectTypes type, const atools::sql::SqlRecord& record);

  /* Set parking position, departure, destination for flight plan from context menu */
  void routeSetParkingStart(map::MapParking parking);
  void routeSetHelipadStart(map::MapHelipad helipad);

  /* Set route departure or destination from context menu */
  void routeSetStart(map::MapAirport ap);
  void routeSetDest(map::MapAirport ap);

  /* Add, replace or delete object from flight plan from context menu or drag and drop.
   *  index = 0: prepend to route
   *  index = size()-1: append to route */
  void routeAdd(int id, atools::geo::Pos userPos, map::MapObjectTypes type, int legIndex);
  void routeReplace(int id, atools::geo::Pos userPos, map::MapObjectTypes type, int oldIndex);

  /* Update action state (disabled/enabled) */
  void updateActionStates();

  /* Show information about objects from single click or context menu */
  void showInformation(map::MapSearchResult result);

  /* Add user point and pass result to it so it can prefill the dialog */
  void addUserpointFromMap(map::MapSearchResult result, const atools::geo::Pos& pos);
  void editUserpointFromMap(map::MapSearchResult result);
  void deleteUserpointFromMap(int id);

  /* Passed point with updated coordinates */
  void moveUserpointFromMap(const map::MapUserpoint& point);

  /* Show approaches from context menu */
  void showApproaches(map::MapAirport airport);

  /* Aircraft track was pruned and needs to be updated */
  void aircraftTrackPruned();

  void shownMapFeaturesChanged(map::MapObjectTypes types);

  /* State isFlying  between last and current aircraft has changed */
  void aircraftTakeoff(const atools::fs::sc::SimConnectUserAircraft& aircraft);
  void aircraftLanding(const atools::fs::sc::SimConnectUserAircraft& aircraft, float flownDistanceNm,
                       float averageTasKts);

private:
  bool eventFilter(QObject *obj, QEvent *e) override;
  void setDetailLevel(int factor);

  /* Hide and prevent re-show */
  void hideTooltip();

  void overlayStateToMenu();
  void overlayStateFromMenu();
  void connectOverlayMenus();

  /* Overloaded methods */
  virtual void mousePressEvent(QMouseEvent *event) override;
  virtual void mouseReleaseEvent(QMouseEvent *event) override;
  virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
  virtual void mouseMoveEvent(QMouseEvent *event) override;
  virtual bool event(QEvent *event) override;
  virtual void contextMenuEvent(QContextMenuEvent *event) override;
  virtual void paintEvent(QPaintEvent *paintEvent) override;
  virtual void focusOutEvent(QFocusEvent *event) override;
  virtual void keyPressEvent(QKeyEvent *event) override;
  virtual void leaveEvent(QEvent *) override;

  void updateRouteFromDrag(QPoint newPoint, mw::MouseStates state, int leg, int point);

  void handleInfoClick(QPoint pos);
  bool loadKml(const QString& filename, bool center);
  void updateCacheSizes();

  void cancelDragDistance();
  void cancelDragRoute();
  void elevationDisplayTimerTimeout();
  void cancelDragUserpoint();

  void jumpBackToAircraftTimeout();
  void jumpBackToAircraftStart();
  bool isCenterLegAndAircraftActive();

  /* Timer for takeoff and landing recognition fired */
  void takeoffLandingTimeout();

  /* Defines amount of objects and other attributes on the map. min 5, max 15, default 10. */
  int mapDetailLevel;

  QStringList kmlFilePaths;

  /* Do not draw while database is unavailable */
  bool databaseLoadStatus = false;

  int screenSearchDistance /* Radius for click sensitivity */,
      screenSearchDistanceTooltip /* Radius for tooltip sensitivity */;

  /* Used to check if mouse moved between button down and up */
  QPoint mouseMoved;
  MapThemeComboIndex currentComboIndex = INVALID;

  mw::MouseStates mouseState = mw::NONE;
  bool contextMenuActive = false;

  /* Current position when dragging a flight plan point or leg */
  QPoint routeDragCur;
  atools::geo::Pos routeDragFrom /* First fixed point of route drag */,
                   routeDragTo /* Second fixed point of route drag */;
  int routeDragPoint = -1 /* Index of changed point */,
      routeDragLeg = -1 /* index of changed leg */;

  /* Current position and pixmap when drawing userpoint on map */
  QPoint userpointDragCur;
  map::MapUserpoint userpointDrag;
  QPixmap userpointDragPixmap;

  /* Save last tooltip position. If invalid/null no tooltip will be shown */
  QPoint tooltipPos;
  map::MapSearchResult mapSearchResultTooltip;
  QList<proc::MapProcedurePoint> procPointsTooltip;
  MapTooltip *mapTooltip;

  MainWindow *mainWindow;
  MapPaintLayer *paintLayer;
  MapVisible *mapVisible;
  MapQuery *mapQuery;
  AirportQuery *airportQuery;
  MapScreenIndex *screenIndex = nullptr;

  atools::geo::Pos searchMarkPos, homePos;
  double homeDistance = 0.;

  /* Highlight when mousing over the elevation profile */
  QRubberBand *profileRubberRect = nullptr;

  /* Distance marker that is changed using drag and drop */
  int currentDistanceMarkerIndex = -1;
  /* Backup of distance marker for drag and drop in case the action is cancelled */
  map::DistanceMarker distanceMarkerBackup;

  atools::gui::MapPosHistory history;
  Marble::Projection currentProjection;

  AircraftTrack aircraftTrack;

  QHash<QString, QAction *> mapOverlays;

  /* Need to check if the zoom and position was changed by the map history to avoid recursion */
  bool noStoreInHistory = false;

  /* Values used to check if view has changed */
  Marble::GeoDataLatLonAltBox currentViewBoundingBox;

  /* Current zoom value (NOT distance) */
  int currentZoom = -1;
  qint64 lastSimUpdateMs = 0;
  bool active = false;

  /* Delay display of elevation display to avoid lagging mouse movements */
  QTimer elevationDisplayTimer;

  /* Delay takeoff and landing messages to avoid false recognition of bumpy landings */
  QTimer takeoffLandingTimer;

  /* Simulator zulu time timestamp of takeoff event */
  qint64 takeoffTimeMs = 0L;

  /* Flown distance from takeoff event */
  double takeoffLandingDistanceNm = 0.;

  /* Average true airspeed from takeoff event */
  double takeoffLandingAverageTasKts = 0.;

  /* Last sample from average value calculation */
  qint64 takeoffLastSampleTimeMs = 0L;

  /* Used for distance calculation */
  atools::fs::sc::SimConnectUserAircraft takeoffLandingLastAircraft;

  QTimer jumpBackToAircraftTimer;
  double jumpBackToAircraftDistance = 0.;
  atools::geo::Pos jumpBackToAircraftPos;
  bool jumpBackToAircraftActive = false;

  /* The the overlays from updating */
  bool ignoreOverlayUpdates = false;

#ifdef DEBUG_MOVING_AIRPLANE
  void debugMovingPlane(QMouseEvent *event);

#endif

};

Q_DECLARE_TYPEINFO(MapWidget::SimUpdateDelta, Q_PRIMITIVE_TYPE);

#endif // LITTLENAVMAP_NAVMAPWIDGET_H
