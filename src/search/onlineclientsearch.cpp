/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#include "search/onlineclientsearch.h"

#include "atools.h"
#include "common/constants.h"
#include "common/formatter.h"
#include "common/mapresult.h"
#include "gui/widgetstate.h"
#include "app/navapp.h"
#include "online/onlinedatacontroller.h"
#include "options/optiondata.h"
#include "search/column.h"
#include "search/columnlist.h"
#include "search/sqlcontroller.h"
#include "sql/sqlrecord.h"
#include "ui_mainwindow.h"

OnlineClientSearch::OnlineClientSearch(QMainWindow *parent, QTableView *tableView, si::TabSearchId tabWidgetIndex)
  : SearchBaseTable(parent, tableView, new ColumnList("client", "client_id"), tabWidgetIndex)
{
  /* *INDENT-OFF* */
  ui->pushButtonOnlineClientHelpSearch->setToolTip(
    "<p>All set search conditions have to match.</p>"
    "<p>Search tips for text fields: </p>"
    "<ul>"
      "<li>Default is search for online clients that contain the entered text.</li>"
      "<li>Use &quot;*&quot; as a placeholder for any text. </li>"
      "<li>Prefix with &quot;-&quot; as first character to negate search.</li>"
      "<li>Only callsign field: Use double quotes like &quot;TAU&quot; to force exact search.</li>"
    "</ul>");
  /* *INDENT-ON* */

  // All widgets that will have their state and visibility saved and restored
  onlineClientSearchWidgets =
  {
    ui->horizontalLayoutOnlineClient,
    ui->actionSearchOnlineClientFollowSelection
  };

  // Default view column descriptors
  // Hidden columns are part of the query and can be used as search criteria but are not shown in the table
  // Columns that are hidden are also needed to fill MapAirport object and for the icon delegate
  columns->
  append(Column("client_id").hidden()).

  append(Column("callsign", ui->lineEditOnlineClientCallsign, tr("Callsign")).defaultSort().filter()).
  append(Column("name", ui->lineEditOnlineClientName, tr("Name")).filter()).
  append(Column("server", ui->lineEditOnlineClientServer, tr("Server")).filter()).
  append(Column("flightplan_departure_aerodrome", ui->lineEditOnlineClientDeparture, tr("Departure")).filter()).
  append(Column("flightplan_destination_aerodrome", ui->lineEditOnlineClientDestination, tr("Destination")).filter()).

  append(Column("state", tr("State"))).
  append(Column("on_ground", tr("On Ground"))).
  append(Column("groundspeed", tr("Groundspeed\n%speed%"))).
  append(Column("altitude", tr("Altitude\n%alt%"))).
  append(Column("flightplan_aircraft", tr("Aircraft"))).
  append(Column("flightplan_departure_time", tr("Planned\nDeparture"))).
  append(Column("flightplan_actual_departure_time", tr("Actual\nDeparture"))).
  append(Column("flightplan_estimated_arrival_time", tr("Estimated\nArrival"))).
  append(Column("flightplan_cruising_speed", tr("Planned\nCruising Speed")).hidden()).
  append(Column("flightplan_cruising_level", tr("Planned\nCruising Level")).hidden()).
  append(Column("transponder_code", tr("Transponder\nCode")).hidden()).
  append(Column("flightplan_flight_rules", tr("ICAO Flight\nRules")).hidden()).
  append(Column("flightplan_type_of_flight", tr("ICAO Flight\nType")).hidden()).
  append(Column("flightplan_enroute_minutes", tr("En-route\nhh:mm")).hidden()).
  append(Column("flightplan_endurance_minutes", tr("Endurance\nhh:mm")).hidden()).
  append(Column("connection_time", tr("Connection\nTime"))).
  append(Column("client_type").hidden()).
  append(Column("heading").hidden()).

  append(Column("lonx").hidden()).
  append(Column("laty").hidden())
  ;

  SearchBaseTable::initViewAndController(NavApp::getDatabaseOnline());

  // Add model data handler and model format handler as callbacks
  setCallbacks();
}

OnlineClientSearch::~OnlineClientSearch()
{
}

void OnlineClientSearch::overrideMode(const QStringList&)
{
}

void OnlineClientSearch::connectSearchSlots()
{
  SearchBaseTable::connectSearchSlots();

  // Small push buttons on top
  connect(ui->pushButtonOnlineClientSearchClearSelection, &QPushButton::clicked,
          this, &SearchBaseTable::nothingSelectedTriggered);
  connect(ui->pushButtonOnlineClientSearchReset, &QPushButton::clicked, this, &SearchBaseTable::resetSearch);

  installEventFilterForWidget(ui->lineEditOnlineClientCallsign);
  installEventFilterForWidget(ui->lineEditOnlineClientDeparture);
  installEventFilterForWidget(ui->lineEditOnlineClientDestination);
  installEventFilterForWidget(ui->lineEditOnlineClientName);
  installEventFilterForWidget(ui->lineEditOnlineClientServer);

  // Connect widgets to the controller
  SearchBaseTable::connectSearchWidgets();
}

void OnlineClientSearch::saveState()
{
  atools::gui::WidgetState widgetState(lnm::SEARCHTAB_ONLINE_CLIENT_VIEW_WIDGET);
  widgetState.save(onlineClientSearchWidgets);
}

void OnlineClientSearch::restoreState()
{
  if(OptionData::instance().getFlags().testFlag(opts::STARTUP_LOAD_SEARCH) && !atools::gui::Application::isSafeMode())
  {
    atools::gui::WidgetState widgetState(lnm::SEARCHTAB_ONLINE_CLIENT_VIEW_WIDGET);
    widgetState.restore(onlineClientSearchWidgets);
  }
  else
    atools::gui::WidgetState(lnm::SEARCHTAB_ONLINE_CLIENT_VIEW_WIDGET).restore(ui->tableViewOnlineClientSearch);

  finishRestore();
}

void OnlineClientSearch::saveViewState(bool)
{
  atools::gui::WidgetState(lnm::SEARCHTAB_ONLINE_CLIENT_VIEW_WIDGET).save(ui->tableViewOnlineClientSearch);
}

void OnlineClientSearch::restoreViewState(bool)
{
  atools::gui::WidgetState(lnm::SEARCHTAB_ONLINE_CLIENT_VIEW_WIDGET).restore(ui->tableViewOnlineClientSearch);
}

/* Callback for the controller. Will be called for each table cell and should return a formatted value */
QVariant OnlineClientSearch::modelDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant& roleValue,
                                              const QVariant& displayRoleValue, Qt::ItemDataRole role) const
{
  switch(role)
  {
    case Qt::DisplayRole:
      return formatModelData(col, displayRoleValue);

    case Qt::ToolTipRole:
      if(col->getColumnName() == "atis")
        return atools::elideTextLinesShort(displayRoleValue.toString(), 40);

      break;

    case Qt::TextAlignmentRole:
      if(col->getColumnName() == "ident" ||
         col->getColumnName() == "groundspeed" ||
         col->getColumnName() == "altitude" ||
         col->getColumnName() == "flightplan_departure_time" ||
         col->getColumnName() == "flightplan_actual_departure_time" ||
         col->getColumnName() == "flightplan_estimated_arrival_time" ||
         col->getColumnName() == "flightplan_cruising_speed" ||
         col->getColumnName() == "flightplan_cruising_level" ||
         col->getColumnName() == "transponder_code" ||
         col->getColumnName() == "flightplan_enroute_minutes" ||
         col->getColumnName() == "flightplan_endurance_minutes")
        return Qt::AlignRight;

      break;
    default:
      break;
  }

  return SearchBaseTable::modelDataHandler(colIndex, rowIndex, col, roleValue, displayRoleValue, role);
}

/* Formats the QVariant to a QString depending on column name */
QString OnlineClientSearch::formatModelData(const Column *col, const QVariant& displayRoleValue) const
{
  if(!displayRoleValue.isNull())
  {
    // Called directly by the model for export functions
    if(col->getColumnName() == "on_ground")
      return displayRoleValue.toInt() > 0 ? tr("Yes") : QString();
    else if(col->getColumnName() == "atis")
      return displayRoleValue.toString().simplified();
    else if(col->getColumnName() == "atis_time" || col->getColumnName() == "connection_time")
      return QLocale().toString(displayRoleValue.toDateTime(), QLocale::NarrowFormat);
    else if(col->getColumnName() == "flightplan_endurance_minutes" ||
            col->getColumnName() == "flightplan_enroute_minutes")
      return formatter::formatMinutesHours(displayRoleValue.toDouble() / 60.);
    else if(col->getColumnName() == "flightplan_departure_time" ||
            col->getColumnName() == "flightplan_actual_departure_time" ||
            col->getColumnName() == "flightplan_estimated_arrival_time")
      return atools::timeFromHourMinStr(displayRoleValue.toString()).toString("HH:mm");

    return SearchBaseTable::formatModelData(col, displayRoleValue);
  }

  return SearchBaseTable::formatModelData(col, displayRoleValue);
}

void OnlineClientSearch::getSelectedMapObjects(map::MapResult& result) const
{
  if(!ui->dockWidgetSearch->isVisible())
    return;

  // Build a SQL record with all available fields
  atools::sql::SqlRecord rec;
  controller->initRecord(rec);
  // qDebug() << Q_FUNC_INFO << rec;

  const QItemSelection selection = controller->getSelection();
  for(const QItemSelectionRange& rng :  selection)
  {
    for(int row = rng.top(); row <= rng.bottom(); ++row)
    {
      if(controller->hasRow(row))
      {
        controller->fillRecord(row, rec);
        // qDebug() << Q_FUNC_INFO << rec;

        atools::fs::sc::SimConnectAircraft ac;
        NavApp::getOnlinedataController()->fillAircraftFromClient(ac, rec);
        result.onlineAircraft.append(map::MapOnlineAircraft(ac));
      }
    }
  }
}

void OnlineClientSearch::postDatabaseLoad()
{
  SearchBaseTable::postDatabaseLoad();
  setCallbacks();
}

/* Sets controller data formatting callback and desired data roles */
void OnlineClientSearch::setCallbacks()
{
  using namespace std::placeholders;
  controller->setDataCallback(std::bind(&OnlineClientSearch::modelDataHandler, this, _1, _2, _3, _4, _5, _6),
                              {Qt::DisplayRole, Qt::BackgroundRole, Qt::TextAlignmentRole, Qt::ToolTipRole});
}

/* Update the button menu actions. Add * for changed search criteria and toggle show/hide all
 * action depending on other action states */
void OnlineClientSearch::updateButtonMenu()
{
}

void OnlineClientSearch::updatePushButtons()
{
  QItemSelectionModel *sm = view->selectionModel();
  ui->pushButtonOnlineClientSearchClearSelection->setEnabled(sm != nullptr && sm->hasSelection());
}

QAction *OnlineClientSearch::followModeAction()
{
  return ui->actionSearchOnlineClientFollowSelection;
}
