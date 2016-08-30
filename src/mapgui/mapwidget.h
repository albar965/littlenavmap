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

#ifndef LITTLENAVMAP_NAVMAPWIDGET_H
#define LITTLENAVMAP_NAVMAPWIDGET_H

#include "common/maptypes.h"
#include "gui/mapposhistory.h"
#include "fs/sc/simconnectdata.h"
#include "common/aircrafttrack.h"

#include <QWidget>

#include <marble/GeoDataLatLonAltBox.h>
#include <marble/MarbleWidget.h>

namespace atools {
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
class RouteController;
class MapTooltip;
class QRubberBand;
class MapScreenIndex;
class RouteMapObjectList;

namespace mw {
/* State of click, drag and drop actions on the map */
enum MouseState
{
  NONE = 0x00, /* Nothing */
  DRAG_DISTANCE = 0x01, /* A new distance measurement line is dragged */
  DRAG_CHANGE_DISTANCE = 0x02, /* A present distance measurement line is changed dragging */
  DRAG_ROUTE_LEG = 0x04, /* Changing a flight plan leg by adding a new point */
  DRAG_ROUTE_POINT = 0x08, /* Changing the flight plan by replacing a present waypoint */
  DRAG_POST = 0x10, /* Mouse released - all done */
  DRAG_POST_MENU = 0x20, /* A menu is opened after selecting multiple objects.
                          * Avoid cancelling all drag when loosing focus */
  DRAG_POST_CANCEL = 0x40 /* Right mousebutton clicked - cancel all actions */
};

Q_DECLARE_FLAGS(MouseStates, MouseState);
Q_DECLARE_OPERATORS_FOR_FLAGS(mw::MouseStates);
}

class MapWidget :
  public Marble::MarbleWidget
{
  Q_OBJECT

public:
  MapWidget(MainWindow *parent, MapQuery *query);
  virtual ~MapWidget();

  /* Save and restore markers, Marble plug-in settings, loaded KML files and more */
  void saveState();
  void restoreState();

  /* Jump to position on the map using the given zoom distance (if not equal -1) */
  void showPos(const atools::geo::Pos& pos, int distanceNm = -1);

  /* Show the bounding rectangle on the map */
  void showRect(const atools::geo::Rect& rect);

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
  void changeSearchHighlights(const maptypes::MapSearchResult& positions);
  void changeRouteHighlights(const RouteMapObjectList& routeHighlight);

  /* Update route screen coordinate index */
  void routeChanged(bool geometryChanged);

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
  const maptypes::MapSearchResult& getSearchHighlights() const;

  const RouteMapObjectList& getRouteHighlights() const;

  const QList<maptypes::RangeMarker>& getRangeRings() const;

  const QList<maptypes::DistanceMarker>& getDistanceMarkers() const;

  const AircraftTrack& getAircraftTrack() const
  {
    return aircraftTrack;
  }

  /* If currently dragging flight plan: start, mouse and end position of the moving line. Start of end might be omitted
   * if dragging departure or destination */
  void getRouteDragPoints(atools::geo::Pos& from, atools::geo::Pos& to, QPoint& cur);

  /* Delete the current aircraft track. Will not stop collecting new track points */
  void deleteAircraftTrack();

  /* Add general (red) range ring */
  void addRangeRing(const atools::geo::Pos& pos);

  /* Add radio navaid range ring */
  void addNavRangeRing(const atools::geo::Pos& pos, maptypes::MapObjectTypes type, const QString& ident,
                       int frequency, int range);

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
  void setShowMapFeatures(maptypes::MapObjectTypes type, bool show);
  maptypes::MapObjectTypes getShownMapFeatures();

  /* Change map detail level */
  void increaseMapDetail();
  void decreaseMapDetail();
  void defaultMapDetail();
  void setMapDetail(int factor);

  RouteController *getRouteController() const;

  /* Update the shown map object types depending on action status (toolbar or menu) */
  void updateMapObjectsShown();

  /* Update tooltip in case of weather changes */
  void updateTooltip();

  const atools::fs::sc::SimConnectData& getSimData() const
  {
    return simData;
  }

  MainWindow *getParentWindow() const
  {
    return mainWindow;
  }

  bool isConnected() const;

  const QStringList& getKmlFiles() const
  {
    return kmlFilePaths;
  }

  /* The main window show event was triggered after program startup. */
  void mainWindowShown();

  /* End all distance line and route dragging modes */
  void cancelDragAll();

  /* Stores delta values depending on fast or slow update. User aircraft is only updated if
   * delta values are exceeded. */
  struct SimUpdateDelta
  {
    int manhattanLengthDelta;
    float headingDelta;
    float speedDelta;
    float altitudeDelta;
  };

  /* Index values of the map theme combo box */
  enum MapThemeComboIndex
  {
    OPENSTREETMAP,
    OPENTOPOMAP,
    SIMPLE,
    PLAIN,
    INVALID = -1
  };

signals:
  /* Search center has changed by context menu */
  void searchMarkChanged(const atools::geo::Pos& mark);

  /* Show a map object in the search panel (context menu) */
  void showInSearch(maptypes::MapObjectTypes type, const QString& ident, const QString& region,
                    const QString& airportIdent);

  /* Set parking position, departure, destination for flight plan from context menu */
  void routeSetParkingStart(maptypes::MapParking parking);

  /* Set route departure or destination from context menu */
  void routeSetStart(maptypes::MapAirport ap);
  void routeSetDest(maptypes::MapAirport ap);

  /* Add, replace or delete object from flight plan from context menu or drag and drop */
  void routeAdd(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type, int legIndex);
  void routeReplace(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type, int oldIndex);
  void routeDelete(int routeIndex);

  /* Update action state (disabled/enabled) */
  void updateActionStates();

  /* Show information about objects from single click or context menu */
  void showInformation(maptypes::MapSearchResult result);

  /* Aircraft track was pruned and needs to be updated */
  void aircraftTrackPruned();

private:
  bool eventFilter(QObject *obj, QEvent *e) override;
  void setDetailLevel(int factor);

  /* Hide and prevent re-show */
  void hideTooltip();

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

  void updateRouteFromDrag(QPoint newPoint, mw::MouseStates state, int leg, int point);
  void updateVisibleObjectsStatusBar();

  void handleInfoClick(QPoint pos);
  bool loadKml(const QString& filename, bool center);
  void updateCacheSizes();

  void cancelDragDistance();
  void cancelDragRoute();

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
  /* Current position when dragging a flight plan point or leg */
  QPoint routeDragCur;
  atools::geo::Pos routeDragFrom /* Fist fixed point of route drag */,
                   routeDragTo /* Second fixed point of route drag */;
  int routeDragPoint = -1 /* Index of changed point */,
      routeDragLeg = -1 /* index of changed leg */;

  /* Save last tooltip position. If invalid/null no tooltip will be shown */
  QPoint tooltipPos;
  maptypes::MapSearchResult mapSearchResultTooltip;
  MapTooltip *mapTooltip;

  MainWindow *mainWindow;
  MapPaintLayer *paintLayer;
  MapQuery *mapQuery;
  MapScreenIndex *screenIndex = nullptr;

  atools::geo::Pos searchMarkPos, homePos;
  double homeDistance = 0.;

  /* Highlight when mousing over the elevation profile */
  QRubberBand *profileRubberRect = nullptr;

  /* Distance marker that is changed using drag and drop */
  int currentDistanceMarkerIndex = -1;
  /* Backup of distance marker for drag and drop in case the action is cancelled */
  maptypes::DistanceMarker distanceMarkerBackup;

  atools::gui::MapPosHistory history;
  Marble::Projection currentProjection;

  atools::fs::sc::SimConnectData simData, lastSimData;
  AircraftTrack aircraftTrack;

  /* Need to check if the zoom and position was changed by the map history to avoid recursion */
  bool changedByHistory = false;

  /* Values used to check if view has changed */
  Marble::GeoDataLatLonAltBox currentViewBoundingBox;

  /* Current zoom value (NOT distance) */
  int currentZoom = -1;

};

Q_DECLARE_TYPEINFO(MapWidget::SimUpdateDelta, Q_PRIMITIVE_TYPE);

#endif // LITTLENAVMAP_NAVMAPWIDGET_H
