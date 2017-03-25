/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#include "common/maptypes.h"

#include <QObject>

#ifndef LITTLENAVMAP_SEARCHCONTROLLER_H
#define LITTLENAVMAP_SEARCHCONTROLLER_H

class MainWindow;
class AbstractSearch;
class AirportSearch;
class NavSearch;
class ProcedureSearch;
class ColumnList;
class QTableView;
class QTabWidget;
class QTreeWidget;
class MapQuery;

namespace atools {
namespace geo {
class Pos;
}
}

/*
 * Manages all search tabs.
 */
class SearchController :
  public QObject
{
  Q_OBJECT

public:
  SearchController(MainWindow *parent, MapQuery *mQuery, QTabWidget *tabWidgetSearch);
  virtual ~SearchController();

  /* Create the airport search tab */
  void createAirportSearch(QTableView *tableView);

  AirportSearch *getAirportSearch() const
  {
    return airportSearch;
  }

  /* Create the navaid search tab */
  void createNavSearch(QTableView *tableView);

  NavSearch *getNavSearch() const
  {
    return navSearch;
  }

  void createProcedureSearch(QTreeWidget *treeWidget);

  ProcedureSearch *getProcedureSearch() const
  {
    return procedureSearch;
  }

  /* Disconnect and reconnect all queries if a new database is loaded or changed */
  void preDatabaseLoad();
  void postDatabaseLoad();

  /* Save table view and search parameters to settings file */
  void saveState();
  void restoreState();

  /* Reset search and show the given type in the search result. Search widgets are populated with the
   * given parameters. Types can be airport, VOR, NDB or waypoint */
  void showInSearch(map::MapObjectTypes type, const QString& ident, const QString& region,
                    const QString& airportIdent);

  /* Get all selected airports or navaids from the active search tab */
  void getSelectedMapObjects(map::MapSearchResult& result) const;

  /* Options have changed. Update table font, empty airport handling etc. */
  void optionsChanged();

private:
  void tabChanged(int index);
  void postCreateSearch(AbstractSearch *search);
  void helpPressed();
  void helpPressedProcedure();

  MapQuery *mapQuery;

  AirportSearch *airportSearch = nullptr;
  NavSearch *navSearch = nullptr;
  ProcedureSearch *procedureSearch = nullptr;

  MainWindow *mainWindow;
  QTabWidget *tabWidget = nullptr;
  QList<AbstractSearch *> allSearchTabs;

};

#endif // LITTLENAVMAP_SEARCHCONTROLLER_H
