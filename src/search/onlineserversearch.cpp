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

#include "search/onlineserversearch.h"

#include "navapp.h"
#include "common/maptypes.h"
#include "common/mapflags.h"
#include "common/constants.h"
#include "search/sqlcontroller.h"
#include "search/column.h"
#include "ui_mainwindow.h"
#include "search/columnlist.h"
#include "gui/widgetutil.h"
#include "gui/widgetstate.h"
#include "common/mapcolors.h"
#include "common/unit.h"
#include "atools.h"
#include "common/maptypesfactory.h"
#include "sql/sqlrecord.h"

OnlineServerSearch::OnlineServerSearch(QMainWindow *parent, QTableView *tableView, si::SearchTabIndex tabWidgetIndex)
  : SearchBaseTable(parent, tableView, new ColumnList("server", "server_id"), tabWidgetIndex)
{
  // Default view column descriptors
  // Hidden columns are part of the query and can be used as search criteria but are not shown in the table
  // Columns that are hidden are also needed to fill MapAirport object and for the icon delegate
  columns->
  append(Column("server_id").hidden()).
  append(Column("name", tr("Name")).defaultSort()).
  append(Column("ident", tr("Ident"))).
  append(Column("hostname", tr("Hostname or\nIP-Address"))).
  append(Column("location", tr("Location"))).
  append(Column("voice_type", tr("Voice\nType")))
  ;

  SearchBaseTable::initViewAndController(NavApp::getDatabaseOnline());

  // Add model data handler and model format handler as callbacks
  setCallbacks();
}

OnlineServerSearch::~OnlineServerSearch()
{
}

void OnlineServerSearch::overrideMode(const QStringList& overrideColumnTitles)
{
  Q_UNUSED(overrideColumnTitles);
}

void OnlineServerSearch::connectSearchSlots()
{
  SearchBaseTable::connectSearchSlots();

  // Connect widgets to the controller
  SearchBaseTable::connectSearchWidgets();
}

void OnlineServerSearch::saveState()
{
}

void OnlineServerSearch::restoreState()
{
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH)
    restoreViewState(false);
  else
    atools::gui::WidgetState(lnm::SEARCHTAB_ONLINE_SERVER_VIEW_WIDGET).restore(
      NavApp::getMainUi()->tableViewOnlineServerSearch);
}

void OnlineServerSearch::saveViewState(bool distSearchActive)
{
  Q_UNUSED(distSearchActive);
  atools::gui::WidgetState(lnm::SEARCHTAB_ONLINE_SERVER_VIEW_WIDGET).save(
    NavApp::getMainUi()->tableViewOnlineServerSearch);
}

void OnlineServerSearch::restoreViewState(bool distSearchActive)
{
  Q_UNUSED(distSearchActive);
  atools::gui::WidgetState(lnm::SEARCHTAB_ONLINE_SERVER_VIEW_WIDGET).restore(
    NavApp::getMainUi()->tableViewOnlineServerSearch);
}

/* Callback for the controller. Will be called for each table cell and should return a formatted value */

void OnlineServerSearch::getSelectedMapObjects(map::MapSearchResult& result) const
{
  Q_UNUSED(result);
}

void OnlineServerSearch::postDatabaseLoad()
{
  SearchBaseTable::postDatabaseLoad();
  setCallbacks();
}

/* Sets controller data formatting callback and desired data roles */
void OnlineServerSearch::setCallbacks()
{
  using namespace std::placeholders;
  controller->setDataCallback(std::bind(&SearchBaseTable::modelDataHandler, this, _1, _2, _3, _4, _5, _6),
                              {Qt::DisplayRole, Qt::BackgroundRole, Qt::TextAlignmentRole});
}

void OnlineServerSearch::updateButtonMenu()
{
}

void OnlineServerSearch::updatePushButtons()
{
}

QAction *OnlineServerSearch::followModeAction()
{
  return nullptr;
}
