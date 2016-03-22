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

signals:
  void markChanged(const atools::geo::Pos& mark);
  void homeChanged(const atools::geo::Pos& mark);
  void objectSelected(maptypes::MapObjectTypes type, const QString& ident, const QString& region,
                      const QString& airportIdent);

private:
  enum MouseState
  {
    NONE,
    DISTANCE_DRAG,
    DISTANCE_DRAG_CHANGE
  };

  MouseState mouseState = NONE;
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
  int currentDistanceMarkerIndex = -1;
  maptypes::DistanceMarker distanceMarkerBackup;
  MapPosHistory history;

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

  int getNearestDistanceMarkerIndex(int xs, int ys, int screenDistance);
  int getNearestRangeMarkerIndex(int xs, int ys, int screenDistance);

  void getNearestRouteMapObjects(int xs, int ys, int screenDistance,
                                 const QList<RouteMapObject>& routeMapObjects,
                                 maptypes::MapSearchResult& mapobjects);

};

#endif // NAVMAPWIDGET_H
