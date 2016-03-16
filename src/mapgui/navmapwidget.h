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

namespace atools {
namespace geo {
class Pos;
class Rect;
}
namespace sql {
class SqlDatabase;
}
}

class QContextMenuEvent;
class MainWindow;
class MapPaintLayer;
class MapQuery;

class NavMapWidget :
  public Marble::MarbleWidget
{
  Q_OBJECT

public:
  NavMapWidget(MainWindow *parent, atools::sql::SqlDatabase *sqlDb);
  virtual ~NavMapWidget();

  void contextMenu(const QPoint& pos);

  void saveState();
  void restoreState();

  void showPos(const atools::geo::Pos& pos, int zoomValue = 3000);
  void showRect(const atools::geo::Rect& rect);

  void showMark();
  void showHome();
  void changeMark(const atools::geo::Pos& pos);
  void changeHome();
  void changeHighlight(const maptypes::MapSearchResult& positions);

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
  void setShowMapFeatures(maptypes::ObjectTypes type, bool show);
  void setDetailFactor(int factor);

signals:
  void markChanged(const atools::geo::Pos& mark);
  void homeChanged(const atools::geo::Pos& mark);
  void objectSelected(maptypes::MapObjectType type, const QString& ident, const QString& region);

private:
  MainWindow *parentWindow;
  MapPaintLayer *paintLayer;
  MapQuery *mapQuery;
  atools::sql::SqlDatabase *db;
  int homeZoom = -1;
  bool showMapPois = true;
  atools::geo::Pos markPos, homePos;
  maptypes::MapSearchResult highlightMapObjects;

  maptypes::MapSearchResult rangeRingMapObjects;
  QList<int> rangeRingMapObjectRanges;

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

};

#endif // NAVMAPWIDGET_H
