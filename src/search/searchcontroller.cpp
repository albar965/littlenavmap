/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#include "search/searchcontroller.h"

#include "navapp.h"
#include "search/column.h"
#include "search/columnlist.h"
#include "search/airportsearch.h"
#include "search/navsearch.h"
#include "mapgui/mapwidget.h"
#include "gui/helphandler.h"
#include "sql/sqlrecord.h"
#include "ui_mainwindow.h"
#include "search/userdatasearch.h"
#include "common/constants.h"
#include "search/proceduresearch.h"
#include "options/optiondata.h"
#include "userdata/userdatacontroller.h"
#include "search/onlineclientsearch.h"
#include "search/onlinecentersearch.h"
#include "search/onlineserversearch.h"

#include <QTabWidget>
#include <QUrl>

using atools::gui::HelpHandler;

SearchController::SearchController(QMainWindow *parent, QTabWidget *tabWidgetSearch)
  : mapQuery(NavApp::getMapQuery()), mainWindow(parent), tabWidget(tabWidgetSearch)
{
  connect(tabWidget, &QTabWidget::currentChanged, this, &SearchController::tabChanged);
  connect(NavApp::getMainUi()->pushButtonAirportHelpSearch, &QPushButton::clicked,
          this, &SearchController::helpPressed);
  connect(NavApp::getMainUi()->pushButtonNavHelpSearch, &QPushButton::clicked,
          this, &SearchController::helpPressed);
  connect(NavApp::getMainUi()->pushButtonProcedureHelpSearch, &QPushButton::clicked,
          this, &SearchController::helpPressedProcedure);

  connect(NavApp::getMainUi()->pushButtonUserdataHelp, &QPushButton::clicked,
          this, &SearchController::helpPressedUserdata);

  connect(NavApp::getMainUi()->pushButtonOnlineClientHelpSearch, &QPushButton::clicked,
          this, &SearchController::helpPressedOnlineClient);
  connect(NavApp::getMainUi()->pushButtonOnlineCenterHelpSearch, &QPushButton::clicked,
          this, &SearchController::helpPressedOnlineCenter);
}

SearchController::~SearchController()
{
  delete airportSearch;
  delete navSearch;
  delete procedureSearch;
  delete userdataSearch;
  delete onlineClientSearch;
  delete onlineCenterSearch;
  delete onlineServerSearch;
}

void SearchController::getSelectedMapObjects(map::MapSearchResult& result) const
{
  allSearchTabs.at(tabWidget->currentIndex())->getSelectedMapObjects(result);
}

void SearchController::optionsChanged()
{
  for(AbstractSearch *search : allSearchTabs)
    search->optionsChanged();
}

void SearchController::helpPressed()
{
  HelpHandler::openHelpUrl(mainWindow, lnm::HELP_ONLINE_URL + "SEARCH.html", lnm::helpLanguagesOnline());
}

void SearchController::helpPressedProcedure()
{
  HelpHandler::openHelpUrl(mainWindow, lnm::HELP_ONLINE_URL + "SEARCHPROCS.html", lnm::helpLanguagesOnline());
}

void SearchController::helpPressedUserdata()
{
  HelpHandler::openHelpUrl(mainWindow, lnm::HELP_ONLINE_URL + "SEARCHUSERDATA.html", lnm::helpLanguagesOnline());
}

void SearchController::helpPressedOnlineClient()
{
  HelpHandler::openHelpUrl(mainWindow, lnm::HELP_ONLINE_URL + "SEARCHONLINECLIENT.html", lnm::helpLanguagesOnline());
}

void SearchController::helpPressedOnlineCenter()
{
  HelpHandler::openHelpUrl(mainWindow, lnm::HELP_ONLINE_URL + "SEARCHONLINECENTER.html", lnm::helpLanguagesOnline());
}

/* Forces an emit of selection changed signal if the active tab changes */
void SearchController::tabChanged(int index)
{
  for(int i = 0; i < allSearchTabs.size(); i++)
  {
    if(i != index)
      allSearchTabs.at(i)->tabDeactivated();
  }

  allSearchTabs.at(index)->updateTableSelection();
}

void SearchController::saveState()
{
  for(AbstractSearch *s : allSearchTabs)
    s->saveState();
}

void SearchController::restoreState()
{
  for(AbstractSearch *s : allSearchTabs)
    s->restoreState();
}

void SearchController::createAirportSearch(QTableView *tableView)
{
  airportSearch = new AirportSearch(mainWindow, tableView, si::SEARCH_AIRPORT);
  postCreateSearch(airportSearch);
}

void SearchController::createNavSearch(QTableView *tableView)
{
  navSearch = new NavSearch(mainWindow, tableView, si::SEARCH_NAV);
  postCreateSearch(navSearch);
}

void SearchController::createUserdataSearch(QTableView *tableView)
{
  userdataSearch = new UserdataSearch(mainWindow, tableView, si::SEARCH_USER);
  postCreateSearch(userdataSearch);

  // Get edit and delete signals from user search action and pushbuttons
  connect(userdataSearch, &UserdataSearch::editUserpoints,
          NavApp::getUserdataController(), &UserdataController::editUserpoints);
  connect(userdataSearch, &UserdataSearch::deleteUserpoints,
          NavApp::getUserdataController(), &UserdataController::deleteUserpoints);
  connect(userdataSearch, &UserdataSearch::addUserpoint,
          NavApp::getUserdataController(), &UserdataController::addUserpoint);

}

void SearchController::createOnlineClientSearch(QTableView *tableView)
{
  onlineClientSearch = new OnlineClientSearch(mainWindow, tableView, si::SEARCH_ONLINE_CLIENT);
  postCreateSearch(onlineClientSearch);
}

void SearchController::createOnlineCenterSearch(QTableView *tableView)
{
  onlineCenterSearch = new OnlineCenterSearch(mainWindow, tableView, si::SEARCH_ONLINE_CENTER);
  postCreateSearch(onlineCenterSearch);
}

void SearchController::createOnlineServerSearch(QTableView *tableView)
{
  onlineServerSearch = new OnlineServerSearch(mainWindow, tableView, si::SEARCH_ONLINE_SERVER);
  postCreateSearch(onlineServerSearch);
}

void SearchController::createProcedureSearch(QTreeWidget *treeWidget)
{
  procedureSearch = new ProcedureSearch(mainWindow, treeWidget, si::SEARCH_PROC);
  postCreateSearch(procedureSearch);
}

/* Connect signals and append search object to all search tabs list */
void SearchController::postCreateSearch(AbstractSearch *search)
{
  search->connectSearchSlots();
  search->updateUnits();

  SearchBaseTable *base = dynamic_cast<SearchBaseTable *>(search);
  if(base != nullptr)
    NavApp::getMapWidget()->connect(NavApp::getMapWidget(), &MapWidget::searchMarkChanged,
                                    base, &SearchBaseTable::searchMarkChanged);
  allSearchTabs.append(search);
}

void SearchController::preDatabaseLoad()
{
  for(AbstractSearch *search : allSearchTabs)
    search->preDatabaseLoad();
}

void SearchController::postDatabaseLoad()
{
  for(AbstractSearch *search : allSearchTabs)
    search->postDatabaseLoad();
}

void SearchController::refreshUserdata()
{
  userdataSearch->refreshData(false /* load all */, true /* keep selection */);
}

void SearchController::showInSearch(map::MapObjectTypes type, const atools::sql::SqlRecord& record)
{
  qDebug() << Q_FUNC_INFO << record;

  switch(type)
  {
    case map::AIRPORT:
      // Shown in airport tab
      airportSearch->resetSearch();
      airportSearch->filterByRecord(record);
      break;

    case map::NDB:
    case map::VOR:
    case map::WAYPOINT:
      // Shown in navaid tab
      navSearch->resetSearch();
      navSearch->filterByRecord(record);
      break;

    case map::USERPOINT:
      // Shown in user search tab
      userdataSearch->resetSearch();
      userdataSearch->filterByRecord(record);
      break;

    case map::AIRCRAFT_ONLINE:
      // Shown in user search tab
      onlineClientSearch->resetSearch();
      onlineClientSearch->filterByRecord(record);
      break;

    case map::AIRSPACE_ONLINE:
      // Shown in user search tab
      onlineCenterSearch->resetSearch();
      onlineCenterSearch->filterByRecord(record);
      break;

    case map::ILS:
    case map::MARKER:
    case map::NONE:
    case map::NAV_ALL:
    case map::ALL:
      qWarning() << "showInSearch invalid type" << type;
      break;
  }
}
