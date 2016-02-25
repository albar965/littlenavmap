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

#ifndef SEARCHPANELIST_H
#define SEARCHPANELIST_H

#include <QObject>

class MainWindow;
class SearchPane;
class ColumnList;
class QLayout;
class QAction;

namespace atools {
namespace sql {
class SqlDatabase;
}
}

class SearchPaneList :
  public QObject
{
  Q_OBJECT

public:
  SearchPaneList(MainWindow *parent, atools::sql::SqlDatabase *sqlDb);
  virtual ~SearchPaneList();
  void createAirportSearch();

  void preDatabaseLoad();
  void postDatabaseLoad();

  void saveState();
  void restoreState();

  SearchPane *getAirportSearchPane() const;

private:
  atools::sql::SqlDatabase *db;
  ColumnList *airportColumns = nullptr;
  SearchPane *airportSearchPane = nullptr;

  MainWindow *parentWidget;

};

#endif // SEARCHPANELIST_H
