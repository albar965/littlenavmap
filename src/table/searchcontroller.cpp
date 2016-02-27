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

#include "table/searchcontroller.h"
#include "gui/mainwindow.h"
#include "table/column.h"
#include "table/columnlist.h"
#include "table/airportsearch.h"
#include "navsearch.h"
#include "ui_mainwindow.h"
#include "map/navmapwidget.h"
#include "gui/widgetstate.h"

#include <gui/widgetstate.h>

SearchController::SearchController(MainWindow *parent, atools::sql::SqlDatabase *sqlDb)
  : QObject(parent), db(sqlDb), parentWidget(parent)
{
}

SearchController::~SearchController()
{
  delete airportSearch;
  delete airportColumns;

  delete navSearch;
  delete navColumns;
}

void SearchController::saveState()
{
  airportSearch->saveState();
  navSearch->saveState();

  atools::gui::WidgetState("Search/").save(parentWidget->getUi()->tabWidgetSearch);
}

void SearchController::restoreState()
{
  atools::gui::WidgetState("Search/").restore(parentWidget->getUi()->tabWidgetSearch);

  airportSearch->restoreState();
  navSearch->restoreState();
}

Search *SearchController::getAirportSearch() const
{
  return airportSearch;
}

Search *SearchController::getNavSearch() const
{
  return navSearch;
}

void SearchController::createAirportSearch()
{
  airportColumns = new ColumnList("airport", "airport_id");

  airportSearch = new AirportSearch(parentWidget, parentWidget->getUi()->tableViewAirportSearch,
                                    airportColumns, db);
  airportSearch->connectSlots();

  connect(parentWidget->getMapWidget(), &NavMapWidget::markChanged,
          airportSearch, &Search::markChanged);
}

void SearchController::createNavSearch()
{
  navColumns = new ColumnList("nav_search", "nav_search_id");

  navSearch = new NavSearch(parentWidget, parentWidget->getUi()->tableViewNavSearch,
                                navColumns, db);
  navSearch->connectSlots();

  connect(parentWidget->getMapWidget(), &NavMapWidget::markChanged,
          navSearch, &Search::markChanged);
}

void SearchController::preDatabaseLoad()
{
  if(airportSearch != nullptr)
    airportSearch->preDatabaseLoad();

  if(navSearch != nullptr)
    navSearch->preDatabaseLoad();
}

void SearchController::postDatabaseLoad()
{
  if(airportSearch != nullptr)
    airportSearch->postDatabaseLoad();

  if(navSearch != nullptr)
    navSearch->postDatabaseLoad();
}
