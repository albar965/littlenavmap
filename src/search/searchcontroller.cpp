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

#include "search/searchcontroller.h"

#include "atools.h"
#include "common/constants.h"
#include "common/formatter.h"
#include "common/mapresult.h"
#include "common/unit.h"
#include "gui/helphandler.h"
#include "online/onlinedatacontroller.h"
#include "gui/tabwidgethandler.h"
#include "logbook/logdatacontroller.h"
#include "mapgui/mapwidget.h"
#include "app/navapp.h"
#include "gui/mainwindow.h"
#include "search/airportsearch.h"
#include "search/logdatasearch.h"
#include "search/navsearch.h"
#include "search/onlinecentersearch.h"
#include "search/onlineclientsearch.h"
#include "search/onlineserversearch.h"
#include "search/proceduresearch.h"
#include "search/userdatasearch.h"
#include "ui_mainwindow.h"
#include "userdata/userdatacontroller.h"

#include <QTabWidget>
#include <QUrl>

using atools::gui::HelpHandler;

SearchController::SearchController(QMainWindow *parent, QTabWidget *tabWidgetSearchParam)
  : mapQuery(NavApp::getMapQueryGui()), mainWindow(parent), tabWidgetSearch(tabWidgetSearchParam)
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  connect(ui->pushButtonAirportHelpSearch, &QPushButton::clicked, this, &SearchController::helpPressed);
  connect(ui->pushButtonNavHelpSearch, &QPushButton::clicked, this, &SearchController::helpPressed);
  connect(ui->pushButtonProcedureHelpSearch, &QPushButton::clicked, this, &SearchController::helpPressedProcedure);

  connect(ui->pushButtonUserdataHelp, &QPushButton::clicked, this, &SearchController::helpPressedUserdata);

  connect(ui->pushButtonLogdataHelp, &QPushButton::clicked, this, &SearchController::helpPressedLogdata);

  connect(ui->pushButtonOnlineClientHelpSearch, &QPushButton::clicked, this, &SearchController::helpPressedOnlineClient);
  connect(ui->pushButtonOnlineCenterHelpSearch, &QPushButton::clicked, this, &SearchController::helpPressedOnlineCenter);

  connect(ui->dockWidgetSearch, &QDockWidget::visibilityChanged, this, &SearchController::dockVisibilityChanged);

  tabHandlerSearch = new atools::gui::TabWidgetHandler(ui->tabWidgetSearch, {}, QIcon(":/littlenavmap/resources/icons/tabbutton.svg"),
                                                       tr("Open or close tabs"));
  tabHandlerSearch->init(si::TabSearchIds, lnm::SEARCHTAB_WIDGET_TABS);

  connect(tabHandlerSearch, &atools::gui::TabWidgetHandler::tabChanged, this, &SearchController::tabChanged);
}

SearchController::~SearchController()
{
  ATOOLS_DELETE_LOG(tabHandlerSearch);
  ATOOLS_DELETE_LOG(airportSearch);
  ATOOLS_DELETE_LOG(navSearch);
  ATOOLS_DELETE_LOG(procedureSearch);
  ATOOLS_DELETE_LOG(userdataSearch);
  ATOOLS_DELETE_LOG(logdataSearch);
  ATOOLS_DELETE_LOG(onlineClientSearch);
  ATOOLS_DELETE_LOG(onlineCenterSearch);
  ATOOLS_DELETE_LOG(onlineServerSearch);
}

void SearchController::getSelectedMapObjects(map::MapResult& result) const
{
  int id = tabHandlerSearch->getCurrentTabId();
  if(id != -1)
    allSearchTabs.at(id)->getSelectedMapObjects(result);
}

void SearchController::optionsChanged()
{
  for(AbstractSearch *search : qAsConst(allSearchTabs))
    search->optionsChanged();
}

void SearchController::styleChanged()
{
  tabHandlerSearch->styleChanged();
  for(AbstractSearch *search : qAsConst(allSearchTabs))
    search->styleChanged();
}

void SearchController::dockVisibilityChanged(bool visible)
{
  // Have to remember dock widget visibility since it cannot be determined from QWidget::isVisisble()
  dockVisible = visible;

  // Show or remove marks
  tabChanged(getCurrentSearchTabId());
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
  HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl + "USERPOINT.html#userpoints-search", lnm::helpLanguageOnline());
}

void SearchController::helpPressedLogdata()
{
  HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl + "LOGBOOK.html#logbook-search", lnm::helpLanguageOnline());
}

void SearchController::helpPressedOnlineClient()
{
  HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl + "ONLINENETWORKS.html#search-client", lnm::helpLanguageOnline());
}

void SearchController::helpPressedOnlineCenter()
{
  HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl + "ONLINENETWORKS.html#search-center", lnm::helpLanguageOnline());
}

/* Forces an emit of selection changed signal if the active tab changes */
void SearchController::tabChanged(int index)
{
  if(index == -1)
    return;

  for(int i = 0; i < allSearchTabs.size(); i++)
  {
    if(i != index || !dockVisible)
      // Deactivate all if dock is not visible or all except current
      allSearchTabs.at(i)->tabDeactivated();
  }

  allSearchTabs.at(index)->updateTableSelection(true /* noFollow */);
}

void SearchController::saveState()
{
  for(AbstractSearch *searchTab : qAsConst(allSearchTabs))
    searchTab->saveState();

  tabHandlerSearch->saveState();
}

void SearchController::restoreState()
{
  tabHandlerSearch->restoreState();

  for(AbstractSearch *searchTab : qAsConst(allSearchTabs))
    searchTab->restoreState();
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
  connect(userdataSearch, &UserdataSearch::editUserpoints, NavApp::getUserdataController(), &UserdataController::editUserpoints);
  connect(userdataSearch, &UserdataSearch::deleteUserpoints, NavApp::getUserdataController(), &UserdataController::deleteUserpoints);
  connect(userdataSearch, &UserdataSearch::addUserpoint, NavApp::getUserdataController(), &UserdataController::addUserpoint);
  connect(userdataSearch, &UserdataSearch::cleanupUserdata, NavApp::getUserdataController(), &UserdataController::cleanupUserdata);
}

void SearchController::createLogdataSearch(QTableView *tableView)
{
  logdataSearch = new LogdataSearch(mainWindow, tableView, si::SEARCH_LOG);
  postCreateSearch(logdataSearch);

  // Get edit and delete signals from user search action and pushbuttons
  connect(logdataSearch, &LogdataSearch::editLogEntries, NavApp::getLogdataController(), &LogdataController::editLogEntries);
  connect(logdataSearch, &LogdataSearch::deleteLogEntries, NavApp::getLogdataController(), &LogdataController::deleteLogEntries);
  connect(logdataSearch, &LogdataSearch::addLogEntry, NavApp::getLogdataController(), &LogdataController::addLogEntry);
  connect(logdataSearch, &LogdataSearch::cleanupLogEntries, NavApp::getLogdataController(), &LogdataController::cleanupLogEntries);
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

void SearchController::postCreateSearch(AbstractSearch *search)
{
  search->connectSearchSlots();
  search->updateUnits();

  SearchBaseTable *base = dynamic_cast<SearchBaseTable *>(search);
  if(base != nullptr)
    MapWidget::connect(NavApp::getMapWidgetGui(), &MapWidget::searchMarkChanged, base, &SearchBaseTable::searchMarkChanged);
  allSearchTabs.append(search);
}

void SearchController::preDatabaseLoad()
{
  for(AbstractSearch *search : qAsConst(allSearchTabs))
    search->preDatabaseLoad();
}

void SearchController::postDatabaseLoad()
{
  for(AbstractSearch *search : qAsConst(allSearchTabs))
    search->postDatabaseLoad();
}

void SearchController::refreshUserdata()
{
  userdataSearch->refreshData(false /* load all */, true /* keep selection */, true /* force */);
}

void SearchController::refreshLogdata()
{
  logdataSearch->refreshData(false /* load all */, true /* keep selection */, true /* force */);
}

void SearchController::clearSelection()
{
  for(AbstractSearch *search : qAsConst(allSearchTabs))
    search->clearSelection();
}

bool SearchController::hasSelection()
{
  bool selection = false;
  for(AbstractSearch *search : qAsConst(allSearchTabs))
    selection |= search->hasSelection();
  return selection;
}

void SearchController::showInSearch(map::MapTypes type, const atools::sql::SqlRecord& record, bool select)
{
  qDebug() << Q_FUNC_INFO << record;

  Ui::MainWindow *ui = NavApp::getMainUi();

  ui->dockWidgetSearch->raise();
  ui->dockWidgetSearch->show();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"
  switch(type)
  {
    case map::AIRPORT:
      // Shown in airport tab
      airportSearch->resetSearch();

      airportSearch->showInSearch(record);
      if(select)
        airportSearch->selectAll();
      tabHandlerSearch->setCurrentTab(si::SEARCH_AIRPORT);
      break;

    case map::NDB:
    case map::VOR:
    case map::WAYPOINT:
      // Shown in navaid tab
      navSearch->resetSearch();

      navSearch->showInSearch(record);
      if(select)
        navSearch->selectAll();
      tabHandlerSearch->setCurrentTab(si::SEARCH_NAV);
      break;

    case map::USERPOINT:
      // Shown in user search tab
      userdataSearch->resetSearch();

      userdataSearch->showInSearch(record);
      if(select)
        userdataSearch->selectAll();
      tabHandlerSearch->setCurrentTab(si::SEARCH_USER);
      break;

    case map::LOGBOOK:
      // Shown in user search tab

      // Enable more search options to show user the search criteria
      ui->actionLogdataSearchShowMoreOptions->setChecked(true);
      logdataSearch->resetSearch();

      // Need to ignore the query builder since the columns of the combined search field overlap
      // with the single destination and departure columns
      logdataSearch->showInSearch(record, true /* ignoreQueryBuilder */);
      if(select)
        logdataSearch->selectAll();
      tabHandlerSearch->setCurrentTab(si::SEARCH_LOG);
      break;

    case map::AIRCRAFT_ONLINE:
      // Shown in user search tab
      onlineClientSearch->resetSearch();
      onlineClientSearch->showInSearch(record);
      if(select)
        onlineClientSearch->selectAll();
      tabHandlerSearch->setCurrentTab(si::SEARCH_ONLINE_CLIENT);
      break;

    case map::AIRSPACE:
      // Shown in user search tab
      onlineCenterSearch->resetSearch();

      onlineCenterSearch->showInSearch(record);
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
#pragma GCC diagnostic pop
}

void SearchController::setCurrentSearchTabId(si::TabSearchId tabId)
{
  tabHandlerSearch->setCurrentTab(tabId);
}

si::TabSearchId SearchController::getCurrentSearchTabId()
{
  return static_cast<si::TabSearchId>(tabHandlerSearch->getCurrentTabId());
}

void SearchController::resetTabLayout()
{
  tabHandlerSearch->reset();
}

void SearchController::searchSelectionChanged(const SearchBaseTable *source, int selected, int visible, int total)
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  bool updateAirspace = false, updateLogEntries = false;
  QString selectionLabelText = tr("%1 of %2 %3 selected, %4 visible.%5");
  QString type, lastUpdate;
  if(source->getTabIndex() == si::SEARCH_ONLINE_CLIENT || source->getTabIndex() == si::SEARCH_ONLINE_CENTER)
  {
    QDateTime lastUpdateTime = NavApp::getOnlinedataController()->getLastUpdateTime();
    lastUpdate = lastUpdateTime.isValid() ? tr(" Last Update: %1").arg(lastUpdateTime.toString(Qt::DefaultLocaleShortDate)) : QString();
  }

  if(source->getTabIndex() == si::SEARCH_AIRPORT)
  {
    type = tr("Airports");
    ui->labelAirportSearchStatus->setText(selectionLabelText.arg(selected).arg(total).arg(type).arg(visible).arg(QString()));
  }
  else if(source->getTabIndex() == si::SEARCH_NAV)
  {
    type = tr("Navaids");
    ui->labelNavSearchStatus->setText(selectionLabelText.arg(selected).arg(total).arg(type).arg(visible).arg(QString()));
  }
  else if(source->getTabIndex() == si::SEARCH_USER)
  {
    type = tr("Userpoints");
    ui->labelUserdata->setText(selectionLabelText.arg(selected).arg(total).arg(type).arg(visible).arg(QString()));
  }
  else if(source->getTabIndex() == si::SEARCH_LOG)
  {
    type = tr("Logbook Entries");
    updateLogEntries = true;

    map::MapResult result;
    source->getSelectedMapObjects(result);

    float travelTimeRealHours = 0.f, travelTimeSimHours = 0.f, distanceNm = 0.f;
    for(const map::MapLogbookEntry& entry : qAsConst(result.logbookEntries))
    {
      travelTimeRealHours += entry.travelTimeRealHours;
      travelTimeSimHours += entry.travelTimeSimHours;
      distanceNm += entry.distanceNm;
    }

    QStringList logInformation;
    if(travelTimeRealHours > 1.f / 60.f)
      logInformation.append(tr("Real time %1").arg(formatter::formatMinutesHoursLong(travelTimeRealHours)));
    if(travelTimeSimHours > 1.f / 60.f)
      logInformation.append(tr("Sim. time %1").arg(formatter::formatMinutesHoursLong(travelTimeSimHours)));
    if(distanceNm > 0.f)
      logInformation.append(tr("Dist. %1").arg(Unit::distNm(distanceNm)));

    QString logText;
    if(!logInformation.isEmpty())
      logText = tr("\nTravel Totals: %1.").arg(logInformation.join(tr(". ")));

    ui->labelLogdata->setText(selectionLabelText.arg(selected).arg(total).arg(type).arg(visible).arg(logText));
  }
  else if(source->getTabIndex() == si::SEARCH_ONLINE_CLIENT)
  {
    type = tr("Clients");
    ui->labelOnlineClientSearchStatus->setText(selectionLabelText.arg(selected).arg(total).arg(type).arg(visible).arg(lastUpdate));
  }
  else if(source->getTabIndex() == si::SEARCH_ONLINE_CENTER)
  {
    updateAirspace = true;
    type = tr("Centers");
    ui->labelOnlineCenterSearchStatus->setText(selectionLabelText.arg(selected).arg(total).arg(type).arg(visible).arg(lastUpdate));
  }

  map::MapResult result;

  // Leave result empty if dock window is not visible/close or hidden in stack
  if(dockVisible)
    getSelectedMapObjects(result);

  NavApp::getMapWidgetGui()->changeSearchHighlights(result, updateAirspace, updateLogEntries);
  NavApp::getMainWindow()->updateHighlightActionStates();
}
