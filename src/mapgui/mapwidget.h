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

enum MouseState
{
  NONE = 0x00,
  DRAG_DISTANCE = 0x01,
  DRAG_CHANGE_DISTANCE = 0x02,
  DRAG_ROUTE_LEG = 0x04,
  DRAG_ROUTE_POINT = 0x08,
  DRAG_POST = 0x10,
  DRAG_POST_CANCEL = 0x20
};

Q_DECLARE_FLAGS(MouseStates, MouseState);
Q_DECLARE_OPERATORS_FOR_FLAGS(MouseStates);

class MapWidget :
  public Marble::MarbleWidget
{
  Q_OBJECT

public:
  MapWidget(MainWindow *parent, MapQuery *query);
  virtual ~MapWidget();

  void saveState();
  void restoreState();

  void showPos(const atools::geo::Pos& pos, int zoomValue = 3000);
  void showRect(const atools::geo::Rect& rect);

  void showMark();
  void showHome();
  void changeMark(const atools::geo::Pos& pos);
  void changeHome();
  void showAircraft(bool state);
  void changeHighlight(const maptypes::MapSearchResult& positions);
  void changeRouteHighlight(const RouteMapObjectList& routeHighlight);
  void routeChanged(bool geometryChanged);
  void simDataChanged(const atools::fs::sc::SimConnectData& simulatorData);
  void highlightProfilePoint(atools::geo::Pos pos);

  void disconnectedFromSimulator();

  bool addKmlFile(const QString& kmlFile);
  void clearKmlFiles();

  bool eventFilter(QObject *obj, QEvent *e) override;

  const atools::geo::Pos& getMarkPos() const
  {
    return markPos;
  }

  const atools::geo::Pos& getHomePos() const
  {
    return homePos;
  }

  const maptypes::MapSearchResult& getHighlightMapObjects() const;
  const RouteMapObjectList& getRouteHighlightMapObjects() const;

  const QList<maptypes::RangeMarker>& getRangeRings() const;

  const QList<maptypes::DistanceMarker>& getDistanceMarkers() const;

  const AircraftTrack& getAircraftTrack() const
  {
    return aircraftTrack;
  }

  void deleteAircraftTrack();

  void getRouteDragPoints(atools::geo::Pos& from, atools::geo::Pos& to, QPoint& cur);

  void addRangeRing(const atools::geo::Pos& pos);
  void addNavRangeRing(const atools::geo::Pos& pos, maptypes::MapObjectTypes type, const QString& ident,
                       int frequency, int range);
  void clearRangeRingsAndDistanceMarkers();

  void workOffline(bool offline);

  void preDatabaseLoad();
  void postDatabaseLoad();

  void setTheme(const QString& theme, int index);

  void historyNext();
  void historyBack();

  atools::gui::MapPosHistory *getHistory()
  {
    return &history;
  }

  void showSavedPos();

  void setShowMapPois(bool show);
  void setShowMapFeatures(maptypes::MapObjectTypes type, bool show);
  void setDetailFactor(int factor);

  maptypes::MapObjectTypes getShownMapFeatures();

  RouteController *getRouteController() const;

  Marble::Projection getCurrentProjection() const
  {
    return currentProjection;
  }

  void updateMapShowFeatures();
  void updateTooltip();

  const atools::fs::sc::SimConnectData& getSimData() const
  {
    return simData;
  }

  MainWindow *getParentWindow() const
  {
    return mainWindow;
  }

  const QStringList& getKmlFiles() const
  {
    return kmlFiles;
  }

  void mainWindowShown();

  struct SimUpdateDelta
  {
    int manhattanLengthDelta;
    float headingDelta;
    float speedDelta;
    float altitudeDelta;
  };

  void optionsChanged();

signals:
  void markChanged(const atools::geo::Pos& mark);
  void homeChanged(const atools::geo::Pos& mark);
  void objectSelected(maptypes::MapObjectTypes type, const QString& ident, const QString& region,
                      const QString& airportIdent);

  void routeSetParkingStart(maptypes::MapParking parking);
  void routeSetStart(maptypes::MapAirport ap);
  void routeSetDest(maptypes::MapAirport ap);
  void routeAdd(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type, int legIndex);
  void routeReplace(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type, int oldIndex);
  void routeDelete(int id, maptypes::MapObjectTypes type);
  void updateActionStates();
  void showInformation(maptypes::MapSearchResult result);

private:
  virtual void mousePressEvent(QMouseEvent *event) override;
  virtual void mouseReleaseEvent(QMouseEvent *event) override;
  virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
  virtual void mouseMoveEvent(QMouseEvent *event) override;
  virtual bool event(QEvent *event) override;
  virtual void contextMenuEvent(QContextMenuEvent *event) override;

  virtual void paintEvent(QPaintEvent *paintEvent) override;

  void updateRouteFromDrag(QPoint newPoint, MouseStates state, int leg, int point);
  void updateVisibleObjectsStatusBar();

  void handleInfoClick(QPoint pos);
  bool loadKml(const QString& filename, bool center);

  QStringList kmlFiles;
  bool databaseLoadStatus = false;
  enum MapThemeComboIndex
  {
    OSM,
    OPENTOPOMAP,
    POLITICAL,
    PLAIN,
    INVALID = -1
  };

  int screenSearchDistance = 16, screenSearchDistanceTooltip = 10;
  MapThemeComboIndex currentComboIndex = INVALID;
  MouseStates mouseState = NONE;
  QPoint mouseMoved;
  MainWindow *mainWindow;
  QPoint tooltipPos;
  maptypes::MapSearchResult mapSearchResultTooltip;

  MapPaintLayer *paintLayer;
  MapQuery *mapQuery;
  double homeDistance = 0.;
  bool showMapPois = true;
  atools::geo::Pos markPos, homePos;

  MapScreenIndex *screenIndex = nullptr;

  QPoint routeDragCur;
  atools::geo::Pos routeDragFrom, routeDragTo;
  int routeDragPoint = -1, routeDragLeg = -1;

  QRubberBand *profilePoint = nullptr;

  int currentDistanceMarkerIndex = -1;
  maptypes::DistanceMarker distanceMarkerBackup;
  atools::gui::MapPosHistory history;
  MapTooltip *mapTooltip;
  Marble::Projection currentProjection;

  atools::fs::sc::SimConnectData simData, lastSimData;
  AircraftTrack aircraftTrack;

  bool changedByHistory = false;
  int curZoom = -1;
  Marble::GeoDataLatLonAltBox curBox;

  void updateCacheSizes();
};

Q_DECLARE_TYPEINFO(MapWidget::SimUpdateDelta, Q_PRIMITIVE_TYPE);

#endif // LITTLENAVMAP_NAVMAPWIDGET_H
