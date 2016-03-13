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
#include "mapgui/navmapwidget.h"
#include "gui/widgetstate.h"

SearchController::SearchController(MainWindow *parent, atools::sql::SqlDatabase *sqlDb)
  : db(sqlDb), parentWidget(parent)
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
}

void SearchController::restoreState()
{
  airportSearch->restoreState();
  navSearch->restoreState();
}

AirportSearch *SearchController::getAirportSearch() const
{
  return airportSearch;
}

NavSearch *SearchController::getNavSearch() const
{
  return navSearch;
}

void SearchController::createAirportSearch(QTableView *tableView)
{
  airportColumns = new ColumnList("airport", "airport_id");

  airportSearch = new AirportSearch(parentWidget, tableView,
                                    airportColumns, db, 0);

  airportSearch->connectSlots();

  parentWidget->getMapWidget()->connect(parentWidget->getMapWidget(), &NavMapWidget::markChanged,
                                        airportSearch, &Search::markChanged);
}

void SearchController::createNavSearch(QTableView *tableView)
{
  navColumns = new ColumnList("nav_search", "nav_search_id");

  navSearch = new NavSearch(parentWidget, tableView,
                            navColumns, db, 1);
  navSearch->connectSlots();

  parentWidget->getMapWidget()->connect(parentWidget->getMapWidget(), &NavMapWidget::markChanged,
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

void SearchController::objectSelected(maptypes::ObjectType type, const QString& ident, const QString& region)
{
  qDebug() << "SearchController::objectSelected type" << type << "ident" << ident << "region" << region;

  switch(type)
  {
    case maptypes::MARKER:
      break;
    case maptypes::AIRPORT:
      airportSearch->resetSearch();
      airportSearch->filterByIdent(ident);
      break;
    case maptypes::NDB:
      break;
    case maptypes::VOR:
      break;
    case maptypes::ILS:
      break;
    case maptypes::WAYPOINT:
      break;
  }
}
