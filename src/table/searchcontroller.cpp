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
#include "ui_mainwindow.h"
#include "map/navmapwidget.h"
#include "gui/widgetstate.h"

SearchController::SearchController(MainWindow *parent, atools::sql::SqlDatabase *sqlDb)
  : QObject(parent), db(sqlDb), parentWidget(parent)
{
}

SearchController::~SearchController()
{
  delete airportSearchPane;
  delete airportColumns;
}

void SearchController::saveState()
{
  airportSearchPane->saveState();
}

void SearchController::restoreState()
{
  airportSearchPane->restoreState();
}

Search *SearchController::getAirportSearchPane() const
{
  return airportSearchPane;
}

void SearchController::createAirportSearch()
{
  airportColumns = new ColumnList("airport", "airport_id");

  airportSearchPane = new AirportSearch(parentWidget, parentWidget->getUi()->tableViewAirportSearch,
                                        airportColumns, db);
  airportSearchPane->connectSlots();

  connect(parentWidget->getMapWidget(), &NavMapWidget::markChanged,
          airportSearchPane, &Search::markChanged);
}

void SearchController::preDatabaseLoad()
{
  if(airportSearchPane != nullptr)
    airportSearchPane->preDatabaseLoad();
}

void SearchController::postDatabaseLoad()
{
  if(airportSearchPane != nullptr)
    airportSearchPane->postDatabaseLoad();
}
