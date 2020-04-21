/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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
#include "search/logdatasearch.h"
#include "ui_mainwindow.h"
#include "search/userdatasearch.h"
#include "common/constants.h"
#include "search/proceduresearch.h"
#include "options/optiondata.h"
#include "userdata/userdatacontroller.h"
#include "logbook/logdatacontroller.h"
#include "search/onlineclientsearch.h"
#include "search/onlinecentersearch.h"
#include "search/onlineserversearch.h"
#include "gui/tabwidgethandler.h"

#include <QTabWidget>
#include <QUrl>

using atools::gui::HelpHandler;

SearchController::SearchController(QMainWindow *parent, QTabWidget *tabWidgetSearchParam)
  : mapQuery(NavApp::getMapQuery()), mainWindow(parent), tabWidgetSearch(tabWidgetSearchParam)
{
  connect(NavApp::getMainUi()->pushButtonAirportHelpSearch, &QPushButton::clicked,
          this, &SearchController::helpPressed);
  connect(NavApp::getMainUi()->pushButtonNavHelpSearch, &QPushButton::clicked,
          this, &SearchController::helpPressed);
  connect(NavApp::getMainUi()->pushButtonProcedureHelpSearch, &QPushButton::clicked,
          this, &SearchController::helpPressedProcedure);

  connect(NavApp::getMainUi()->pushButtonUserdataHelp, &QPushButton::clicked,
          this, &SearchController::helpPressedUserdata);

  connect(NavApp::getMainUi()->pushButtonLogdataHelp, &QPushButton::clicked,
          this, &SearchController::helpPressedLogdata);

  connect(NavApp::getMainUi()->pushButtonOnlineClientHelpSearch, &QPushButton::clicked,
          this, &SearchController::helpPressedOnlineClient);
  connect(NavApp::getMainUi()->pushButtonOnlineCenterHelpSearch, &QPushButton::clicked,
          this, &SearchController::helpPressedOnlineCenter);

  Ui::MainWindow *ui = NavApp::getMainUi();
  tabHandlerSearch = new atools::gui::TabWidgetHandler(ui->tabWidgetSearch,
                                                       QIcon(":/littlenavmap/resources/icons/tabbutton.svg"),
                                                       tr("Open or close tabs"));
  tabHandlerSearch->init(si::TabSearchIds, lnm::SEARCHTAB_WIDGET_TABS);

  connect(tabHandlerSearch, &atools::gui::TabWidgetHandler::tabChanged, this, &SearchController::tabChanged);
}

SearchController::~SearchController()
{
  delete tabHandlerSearch;
  delete airportSearch;
  delete navSearch;
  delete procedureSearch;
  delete userdataSearch;
  delete logdataSearch;
  delete onlineClientSearch;
  delete onlineCenterSearch;
  delete onlineServerSearch;
}

void SearchController::getSelectedMapObjects(map::MapSearchResult& result) const
{
  int id = tabHandlerSearch->getCurrentTabId();
  if(id != -1)
    allSearchTabs.at(id)->getSelectedMapObjects(result);
}

void SearchController::optionsChanged()
{
  for(AbstractSearch *search : allSearchTabs)
    search->optionsChanged();
}

void SearchController::styleChanged()
{
  tabHandlerSearch->styleChanged();
  for(AbstractSearch *search : allSearchTabs)
    search->styleChanged();
}

void SearchController::helpPressed()
{
  HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl + "SEARCH.html", lnm::helpLanguageOnline());
}

void SearchController::helpPressedProcedure()
{
  HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl + "SEARCHPROCS.html", lnm::helpLanguageOnline());
}

void SearchController::helpPressedUserdata()
{
  HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl + "USERPOINT.html#userpoints-search",
                              lnm::helpLanguageOnline());
}

void SearchController::helpPressedLogdata()
{
  HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl + "LOGBOOK.html#logbook-search",
                              lnm::helpLanguageOnline());
}

void SearchController::helpPressedOnlineClient()
{
  HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl + "ONLINENETWORKS.html#search-client",
                              lnm::helpLanguageOnline());
}

void SearchController::helpPressedOnlineCenter()
{
  HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl + "ONLINENETWORKS.html#search-center",
                              lnm::helpLanguageOnline());
}

/* Forces an emit of selection changed signal if the active tab changes */
void SearchController::tabChanged(int index)
{
  if(index == -1)
    return;

  for(int i = 0; i < allSearchTabs.size(); i++)
  {
    if(i != index)
      allSearchTabs.at(i)->tabDeactivated();
  }

  allSearchTabs.at(index)->updateTableSelection(true /* No follow */);
}

void SearchController::saveState()
{
  for(AbstractSearch *s : allSearchTabs)
    s->saveState();

  tabHandlerSearch->saveState();
}

void SearchController::restoreState()
{
  tabHandlerSearch->restoreState();

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

void SearchController::createLogdataSearch(QTableView *tableView)
{
  logdataSearch = new LogdataSearch(mainWindow, tableView, si::SEARCH_LOG);
  postCreateSearch(logdataSearch);

  // Get edit and delete signals from user search action and pushbuttons
  connect(logdataSearch, &LogdataSearch::editLogEntries,
          NavApp::getLogdataController(), &LogdataController::editLogEntries);
  connect(logdataSearch, &LogdataSearch::deleteLogEntries,
          NavApp::getLogdataController(), &LogdataController::deleteLogEntries);
  connect(logdataSearch, &LogdataSearch::addLogEntry,
          NavApp::getLogdataController(), &LogdataController::addLogEntry);
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
  connect(procedureSearch, &ProcedureSearch::showInSearch, this, &SearchController::showInSearch);
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

void SearchController::refreshLogdata()
{
  logdataSearch->refreshData(false /* load all */, true /* keep selection */);
}

void SearchController::clearSelection()
{
  for(AbstractSearch *search : allSearchTabs)
    search->clearSelection();
}

bool SearchController::hasSelection()
{
  bool selection = false;
  for(AbstractSearch *search : allSearchTabs)
    selection |= search->hasSelection();
  return selection;
}

void SearchController::showInSearch(map::MapObjectTypes type, const atools::sql::SqlRecord& record, bool select)
{
  qDebug() << Q_FUNC_INFO << record;

  Ui::MainWindow *ui = NavApp::getMainUi();

  ui->dockWidgetSearch->raise();
  ui->dockWidgetSearch->show();

  switch(type)
  {
    case map::AIRPORT:
      // Shown in airport tab
      airportSearch->resetSearch();
      airportSearch->filterByRecord(record);
      if(select)
        airportSearch->selectAll();
      tabHandlerSearch->setCurrentTab(si::SEARCH_AIRPORT);
      break;

    case map::NDB:
    case map::VOR:
    case map::WAYPOINT:
      // Shown in navaid tab
      navSearch->resetSearch();
      navSearch->filterByRecord(record);
      if(select)
        navSearch->selectAll();
      tabHandlerSearch->setCurrentTab(si::SEARCH_NAV);
      break;

    case map::USERPOINT:
      // Shown in user search tab
      userdataSearch->resetSearch();
      userdataSearch->filterByRecord(record);
      if(select)
        userdataSearch->selectAll();
      tabHandlerSearch->setCurrentTab(si::SEARCH_USER);
      break;

    case map::LOGBOOK:
      // Shown in user search tab
      logdataSearch->resetSearch();
      logdataSearch->filterByRecord(record);
      if(select)
        logdataSearch->selectAll();
      tabHandlerSearch->setCurrentTab(si::SEARCH_LOG);
      break;

    case map::AIRCRAFT_ONLINE:
      // Shown in user search tab
      onlineClientSearch->resetSearch();
      onlineClientSearch->filterByRecord(record);
      if(select)
        onlineClientSearch->selectAll();
      tabHandlerSearch->setCurrentTab(si::SEARCH_ONLINE_CLIENT);
      break;

    case map::AIRSPACE:
      // Shown in user search tab
      onlineCenterSearch->resetSearch();
      onlineCenterSearch->filterByRecord(record);
      if(select)
        onlineCenterSearch->selectAll();
      tabHandlerSearch->setCurrentTab(si::SEARCH_ONLINE_CENTER);
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

void SearchController::setCurrentSearchTabId(si::TabSearchId tabId)
{
  tabHandlerSearch->setCurrentTab(tabId);
}

si::TabSearchId SearchController::getCurrentSearchTabId()
{
  return static_cast<si::TabSearchId>(tabHandlerSearch->getCurrentTabId());
}

void SearchController::resetWindowLayout()
{
  tabHandlerSearch->reset();
}
