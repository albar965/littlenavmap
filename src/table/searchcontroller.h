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

#include "maptypes.h"

#include <QObject>
#include <QString>

#ifndef SEARCHPANELIST_H
#define SEARCHPANELIST_H

class MainWindow;
class Search;
class AirportSearch;
class NavSearch;
class ColumnList;
class QLayout;
class QAction;
class QTableView;

namespace atools {
namespace sql {
class SqlDatabase;
}
}

class SearchController :
  public QObject
{
  Q_OBJECT

public:
  SearchController(MainWindow *parent, atools::sql::SqlDatabase *sqlDb);
  virtual ~SearchController();

  void createAirportSearch(QTableView *tableView);
  void createNavSearch(QTableView *tableView);

  void preDatabaseLoad();
  void postDatabaseLoad();

  void saveState();
  void restoreState();

  AirportSearch *getAirportSearch() const;
  NavSearch *getNavSearch() const;

  void objectSelected(maptypes::ObjectType type, const QString& ident, const QString& region);

private:
  atools::sql::SqlDatabase *db;
  ColumnList *airportColumns = nullptr;
  AirportSearch *airportSearch = nullptr;

  ColumnList *navColumns = nullptr;
  NavSearch *navSearch = nullptr;

  MainWindow *parentWidget;

};

#endif // SEARCHPANELIST_H
