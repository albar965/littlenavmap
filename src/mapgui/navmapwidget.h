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

#ifndef NAVMAPWIDGET_H
#define NAVMAPWIDGET_H

#include "common/maptypes.h"

#include "mapposhistory.h"

#include <marble/GeoDataLatLonAltBox.h>
#include <marble/MarbleWidget.h>

#include <QWidget>

#include <geo/pos.h>

#include <route/routemapobject.h>

namespace atools {
namespace geo {
class Pos;
class Rect;
}
}

class QContextMenuEvent;
class MainWindow;
class MapPaintLayer;
class MapQuery;
class RouteController;
class MapTooltip;

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

class NavMapWidget :
  public Marble::MarbleWidget
{
  Q_OBJECT

public:
  NavMapWidget(MainWindow *parent, MapQuery *query);
  virtual ~NavMapWidget();

  void contextMenu(const QPoint& point);

  void saveState();
  void restoreState();

  void showPos(const atools::geo::Pos& pos, int zoomValue = 3000);
  void showRect(const atools::geo::Rect& rect);

  void showMark();
  void showHome();
  void changeMark(const atools::geo::Pos& pos);
  void changeHome();
  void changeHighlight(const maptypes::MapSearchResult& positions);
  void changeRouteHighlight(const QList<RouteMapObject>& routeHighlight);
  void routeChanged();

  bool eventFilter(QObject *obj, QEvent *e) override;

  const atools::geo::Pos& getMarkPos() const
  {
    return markPos;
  }

  const atools::geo::Pos& getHomePos() const
  {
    return homePos;
  }

  const maptypes::MapSearchResult& getHighlightMapObjects() const
  {
    return highlightMapObjects;
  }

  const QList<RouteMapObject>& getRouteHighlightMapObjects() const
  {
    return routeHighlightMapObjects;
  }

  const QList<maptypes::RangeMarker>& getRangeRings() const
  {
    return rangeMarkers;
  }

  void getRouteDragPoints(atools::geo::Pos& from, atools::geo::Pos& to, QPoint& cur);

  void addRangeRing(const atools::geo::Pos& pos);
  void addNavRangeRing(const atools::geo::Pos& pos, maptypes::MapObjectTypes type, const QString& ident,
                       int frequency, int range);
  void clearRangeRings();

  const QList<maptypes::DistanceMarker>& getDistanceMarkers() const
  {
    return distanceMarkers;
  }

  void preDatabaseLoad();
  void postDatabaseLoad();

  void setTheme(const QString& theme, int index);

  void historyNext();
  void historyBack();

  MapPosHistory *getHistory()
  {
    return &history;
  }

  void showSavedPos();

  void setShowMapPois(bool show);
  void setShowMapFeatures(maptypes::MapObjectTypes type, bool show);
  void setDetailFactor(int factor);

  RouteController *getRouteController() const;

  Marble::Projection getCurrentProjection() const
  {
    return currentProjection;
  }

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

private:
  enum MapThemeComboIndex
  {
    OSM,
    OSM_HILLSHADING,
    ATLAS,
    BLUE_MARBLE,
    SIMPLE,
    POLITICAL
  };

  MouseStates mouseState = NONE;
  MainWindow *parentWindow;
  MapPaintLayer *paintLayer;
  MapQuery *mapQuery;
  int homeZoom = -1;
  bool showMapPois = true;
  atools::geo::Pos markPos, homePos;
  maptypes::MapSearchResult highlightMapObjects;
  QList<RouteMapObject> routeHighlightMapObjects;
  QList<maptypes::RangeMarker> rangeMarkers;
  QList<maptypes::DistanceMarker> distanceMarkers;
  QList<std::pair<int, QLine> > routeScreenLines;
  QList<std::pair<int, QLine> > airwayScreenLines;
  QList<std::pair<int, QPoint> > routeScreenPoints;
  QPoint routeDragCur;
  atools::geo::Pos routeDragFrom, routeDragTo;
  int routeDragPoint = -1, routeDragLeg = -1;

  int currentDistanceMarkerIndex = -1;
  maptypes::DistanceMarker distanceMarkerBackup;
  MapPosHistory history;
  MapTooltip *mapTooltip;
  Marble::Projection currentProjection;

  virtual void mousePressEvent(QMouseEvent *event) override;
  virtual void mouseReleaseEvent(QMouseEvent *event) override;
  virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
  virtual void mouseMoveEvent(QMouseEvent *event) override;
  virtual bool event(QEvent *event) override;

  int curZoom = -1;
  Marble::GeoDataLatLonAltBox curBox;

  virtual void paintEvent(QPaintEvent *paintEvent) override;

  void getNearestHighlightMapObjects(int xs, int ys, int screenDistance,
                                     maptypes::MapSearchResult& mapobjects);

  void getNearestRouteMapObjects(int xs, int ys, int screenDistance,
                                 const QList<RouteMapObject>& routeMapObjects,
                                 maptypes::MapSearchResult& mapobjects);

  void getAllNearestMapObjects(int xs, int ys, int screenDistance,
                               maptypes::MapSearchResult& mapSearchResult);

  int getNearestDistanceMarkerIndex(int xs, int ys, int screenDistance);
  int getNearestRangeMarkerIndex(int xs, int ys, int screenDistance);

  int getNearestRouteLegIndex(int xs, int ys, int screenDistance);
  int getNearestRoutePointIndex(int xs, int ys, int screenDistance);

  void updateRouteScreenLines();
  void updateAirwayScreenLines();
  void updateRouteFromDrag(QPoint newPoint, MouseStates state, int leg, int point);

};

#endif // NAVMAPWIDGET_H
