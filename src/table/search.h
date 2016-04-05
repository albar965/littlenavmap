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

#ifndef SEARCHPANE_H
#define SEARCHPANE_H

#include "geo/pos.h"
#include "common/maptypes.h"

#include <QColor>
#include <QObject>

class QWidget;
class QTableView;
class Controller;
class ColumnList;
class QAction;
class MainWindow;
class QItemSelection;
class MapQuery;

namespace atools {
namespace geo {
class Rect;
}
namespace sql {
class SqlDatabase;
}
}

class Search :
  public QObject
{
  Q_OBJECT

public:
  explicit Search(MainWindow *parent, QTableView *tableView, ColumnList *columnList,
                  MapQuery *query, int tabWidgetIndex);
  virtual ~Search();

  void preDatabaseLoad();
  void postDatabaseLoad();

  void markChanged(const atools::geo::Pos& mark);

  virtual void connectSlots();

  virtual void saveState() = 0;
  virtual void restoreState() = 0;

  virtual void getSelectedMapObjects(maptypes::MapSearchResult& result) const = 0;

  void resetView();
  void resetSearch();
  void tableCopyCipboard();
  void loadAllRowsIntoView();

  void filterByIdent(const QString& ident, const QString& region = QString(),
                     const QString& airportIdent = QString());

  Controller *getController() const
  {
    return controller;
  }

  void tableSelectionChanged();

protected:
  QIcon *boolIcon = nullptr;

  void connectSearchWidgets();
  void contextMenu(const QPoint& pos);

  MapQuery *mapQuery;
  atools::geo::Pos mapMark;

  Controller *controller;
  ColumnList *columns;
  QTableView *view;
  MainWindow *parentWidget;
  int tabIndex;

  void doubleClick(const QModelIndex& index);

  void initViewAndController();
  void tableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

  void reconnectSelectionModel();

signals:
  void showRect(const atools::geo::Rect& rect);
  void showPos(const atools::geo::Pos& pos, int zoom);
  void changeMark(const atools::geo::Pos& pos);
  void selectionChanged(const Search *source, int selected, int visible, int total);

  void routeSetStart(maptypes::MapAirport airport);
  void routeSetDest(maptypes::MapAirport airport);
  void routeAdd(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type);

};

#endif // SEARCHPANE_H
