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

#include <QColor>
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

class Search :
  public QObject
{
  Q_OBJECT

public:
  explicit Search(MainWindow *parent, QTableView *tableView, ColumnList *columnList,
                  atools::sql::SqlDatabase *sqlDb);
  virtual ~Search();

  void preDatabaseLoad();
  void postDatabaseLoad();

  void markChanged(const atools::geo::Pos& mark);

  virtual void connectSlots();

  virtual void saveState() = 0;
  virtual void restoreState() = 0;

public slots:
  void resetView();
  void resetSearch();
  void tableCopyCipboard();
  void loadAllRowsIntoView();

protected:
  QIcon *boolIcon = nullptr;

  /* Alternating colors for normal display and display of sorted column */
  QColor rowBgColor, rowAltBgColor, rowSortBgColor, rowSortAltBgColor;

  void connectSearchWidgets();
  virtual void tableContextMenu(const QPoint& pos);

  atools::sql::SqlDatabase *db;
  atools::geo::Pos mapMark;

  Controller *controller;
  ColumnList *columns;
  QTableView *view;
  MainWindow *parentWidget;

  void doubleClick(const QModelIndex& index);

  void initViewAndController();

signals:
  void showPoint(double lonX, double latY, int zoom);

};

#endif // SEARCHPANE_H
