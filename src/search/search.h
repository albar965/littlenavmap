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

#ifndef LITTLENAVMAP_SEARCHPANE_H
#define LITTLENAVMAP_SEARCHPANE_H

#include "common/maptypes.h"

#include <QObject>

namespace atools {
namespace gui {
class TableZoomHandler;
}
}

class QTableView;
class Controller;
class ColumnList;
class MainWindow;
class QItemSelection;
class MapQuery;
class QTimer;
class CsvExporter;

class Search :
  public QObject
{
  Q_OBJECT

public:
  Search(MainWindow *parent, QTableView *tableView, ColumnList *columnList,
         MapQuery *mapQuery, int tabWidgetIndex);
  virtual ~Search();

  virtual void preDatabaseLoad();
  virtual void postDatabaseLoad();

  virtual void connectSlots();

  virtual void saveState() = 0;
  virtual void restoreState() = 0;

  virtual void getSelectedMapObjects(maptypes::MapSearchResult& result) const = 0;

  void resetView();
  void resetSearch();
  void tableCopyClipboard();
  void loadAllRowsIntoView();
  void markChanged(const atools::geo::Pos& mark);

  void filterByIdent(const QString& ident, const QString& region = QString(),
                     const QString& airportIdent = QString());

  Controller *getController() const
  {
    return controller;
  }

  void optionsChanged();
  void tableSelectionChanged();

signals:
  void showRect(const atools::geo::Rect& rect);
  void showPos(const atools::geo::Pos& pos, int zoom);
  void changeMark(const atools::geo::Pos& pos);
  void selectionChanged(const Search *source, int selected, int visible, int total);
  void showInformation(maptypes::MapSearchResult result);

  void routeSetStart(maptypes::MapAirport airport);
  void routeSetDest(maptypes::MapAirport airport);
  void routeAdd(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type, int legIndex);

protected:
  virtual void updateMenu() = 0;

  void editStarted();
  void doubleClick(const QModelIndex& index);
  void initViewAndController();
  void tableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
  void reconnectSelectionModel();
  void connectModelSlots();
  void getNavTypeAndId(int row, maptypes::MapObjectTypes& navType, int& id);
  void showInformationMenu();
  void showOnMapMenu();
  void editTimeout();
  void connectSearchWidgets();
  void contextMenu(const QPoint& pos);

  atools::gui::TableZoomHandler *zoomHandler = nullptr;

  QIcon *boolIcon = nullptr;
  CsvExporter *csvExporter = nullptr;
  MapQuery *query;
  atools::geo::Pos mapMark;

  Controller *controller;
  ColumnList *columns;
  QTableView *view;
  MainWindow *mainWindow;
  QTimer *updateTimer;
  int tabIndex;

};

#endif // LITTLENAVMAP_SEARCHPANE_H
