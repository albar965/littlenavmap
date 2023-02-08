/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#include "common/mapflags.h"

#include "common/tabindexes.h"

#include <QObject>

#ifndef LITTLENAVMAP_SEARCHCONTROLLER_H
#define LITTLENAVMAP_SEARCHCONTROLLER_H

class QMainWindow;
class AbstractSearch;
class AirportSearch;
class NavSearch;
class ProcedureSearch;
class UserdataSearch;
class LogdataSearch;
class OnlineClientSearch;
class OnlineCenterSearch;
class OnlineServerSearch;
class ColumnList;
class QTableView;
class QTabWidget;
class QTreeWidget;
class MapQuery;
class SearchBaseTable;

namespace atools {
namespace gui {
class TabWidgetHandler;

}
namespace sql {
class SqlRecord;
}

namespace geo {
class Pos;
}
}

namespace map {
struct MapResult;

}

/*
 * Manages all search tabs.
 */
class SearchController :
  public QObject
{
  Q_OBJECT

public:
  explicit SearchController(QMainWindow *parent, QTabWidget *tabWidgetSearchParam);
  virtual ~SearchController() override;

  SearchController(const SearchController& other) = delete;
  SearchController& operator=(const SearchController& other) = delete;

  /* Create the airport search tab */
  void createAirportSearch(QTableView *tableView);

  /* Create the navaid search tab */
  void createNavSearch(QTableView *tableView);

  void createProcedureSearch(QTreeWidget *treeWidget);

  void createUserdataSearch(QTableView *tableView);
  void createLogdataSearch(QTableView *tableView);

  void createOnlineClientSearch(QTableView *tableView);
  void createOnlineCenterSearch(QTableView *tableView);
  void createOnlineServerSearch(QTableView *tableView);

  AirportSearch *getAirportSearch() const
  {
    return airportSearch;
  }

  NavSearch *getNavSearch() const
  {
    return navSearch;
  }

  ProcedureSearch *getProcedureSearch() const
  {
    return procedureSearch;
  }

  UserdataSearch *getUserdataSearch() const
  {
    return userdataSearch;
  }

  LogdataSearch *getLogdataSearch() const
  {
    return logdataSearch;
  }

  OnlineClientSearch *getOnlineClientSearch() const
  {
    return onlineClientSearch;
  }

  OnlineCenterSearch *getOnlineCenterSearch() const
  {
    return onlineCenterSearch;
  }

  OnlineServerSearch *getOnlineServerSearch() const
  {
    return onlineServerSearch;
  }

  /* Disconnect and reconnect all queries if a new database is loaded or changed */
  void preDatabaseLoad();
  void postDatabaseLoad();

  /* Save table view and search parameters to settings file */
  void saveState();
  void restoreState();

  /* Reset search and show the given type in the search result. Search widgets are populated with the
   * given parameters. Types can be airport, VOR, NDB or waypoint */
  void showInSearch(map::MapTypes type, const atools::sql::SqlRecord& record, bool select);

  /* Get all selected airports or navaids from the active search tab */
  void getSelectedMapObjects(map::MapResult& result) const;

  /* Options have changed. Update table font, empty airport handling etc. */
  void optionsChanged();

  /* GUI style has changed */
  void styleChanged();

  /* Refresh after import or changes */
  void refreshUserdata();

  void refreshLogdata();

  /* Clear selection in all search windows  */
  void clearSelection();

  /* True if any of the search windows has a selection */
  bool hasSelection();

  void setCurrentSearchTabId(si::TabSearchId tabId);

  si::TabSearchId getCurrentSearchTabId();

  /* Reset tab bar */
  void resetTabLayout();

  /* Selection in one of the search result tables has changed. Update status line text. */
  void searchSelectionChanged(const SearchBaseTable *source, int selected, int visible, int total);

private:
  void tabChanged(int index);

  /* Connect signals and append search object to all search tabs list */
  void postCreateSearch(AbstractSearch *search);

  void helpPressed();
  void helpPressedProcedure();
  void helpPressedUserdata();
  void helpPressedOnlineClient();
  void helpPressedOnlineCenter();
  void helpPressedLogdata();
  void dockVisibilityChanged(bool visible);

  MapQuery *mapQuery;

  AirportSearch *airportSearch = nullptr;
  NavSearch *navSearch = nullptr;
  ProcedureSearch *procedureSearch = nullptr;
  UserdataSearch *userdataSearch = nullptr;
  LogdataSearch *logdataSearch = nullptr;
  OnlineClientSearch *onlineClientSearch = nullptr;
  OnlineCenterSearch *onlineCenterSearch = nullptr;
  OnlineServerSearch *onlineServerSearch = nullptr;

  QMainWindow *mainWindow = nullptr;
  QTabWidget *tabWidgetSearch = nullptr;
  QList<AbstractSearch *> allSearchTabs;

  // Have to remember dock widget visibility since it cannot be determined from QWidget::isVisisble()
  bool dockVisible = false;

  atools::gui::TabWidgetHandler *tabHandlerSearch = nullptr;
};

#endif // LITTLENAVMAP_SEARCHCONTROLLER_H
