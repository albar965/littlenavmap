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

#include <QObject>

class QWidget;
class QTableView;
class Controller;
class ColumnList;
class QAction;
class MainWindow;

namespace atools {
namespace sql {
class SqlDatabase;
}
}

class SearchPane :
  public QObject
{
  Q_OBJECT

public:
  explicit SearchPane(MainWindow *parent, QTableView *view, ColumnList *columns,
                      atools::sql::SqlDatabase *sqlDb);
  virtual ~SearchPane();

  void preDatabaseLoad();
  void postDatabaseLoad();

  void connectSearchWidgets();

private:
  void connectSlots();

  atools::sql::SqlDatabase *db;

  Controller *airportController;
  ColumnList *airportColumns;
  QTableView *tableViewAirportSearch;
  MainWindow *parentWidget;

  void tableContextMenu(const QPoint& pos);

  void resetView();
  void resetSearch();
  void tableCopyCipboard();
  void loadAllRowsIntoView();

  void doubleClick(const QModelIndex& index);

signals:
  void showPoint(double lonX, double latY, int zoom);

};

#endif // SEARCHPANE_H
