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

#include "search/onlineclientsearch.h"

#include "navapp.h"
#include "common/constants.h"
#include "online/onlinedatacontroller.h"
#include "fs/online/onlinetypes.h"
#include "search/sqlcontroller.h"
#include "search/column.h"
#include "ui_mainwindow.h"
#include "search/columnlist.h"
#include "gui/widgetstate.h"
#include "common/unit.h"
#include "atools.h"
#include "sql/sqlrecord.h"
#include "common/formatter.h"

OnlineClientSearch::OnlineClientSearch(QMainWindow *parent, QTableView *tableView, si::SearchTabIndex tabWidgetIndex)
  : SearchBaseTable(parent, tableView, new ColumnList("client", "client_id"), tabWidgetIndex)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

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

  append(Column("prefile", tr("Prefile"))).
  append(Column("on_ground", tr("On Ground"))).
  append(Column("groundspeed", tr("Groundspeed\n%speed%"))).
  append(Column("altitude", tr("Altitude\n%alt%"))).
  append(Column("flightplan_aircraft", tr("Aircraft"))).
  append(Column("flightplan_departure_time", tr("Planned\nDeparture"))).
  append(Column("flightplan_actual_departure_time", tr("Actual\nDeparture"))).
  append(Column("flightplan_cruising_speed", tr("Planned\nCruising Speed")).hidden()).
  append(Column("flightplan_cruising_level", tr("Planned\nCruising Level")).hidden()).
  append(Column("transponder_code", tr("Transponder\nCode")).hidden()).
  append(Column("flightplan_flight_rules", tr("ICAO Flight\nRules")).hidden()).
  append(Column("flightplan_type_of_flight", tr("ICAO Flight\nType")).hidden()).
  append(Column("flightplan_enroute_minutes", tr("Enroute\nhh:mm")).hidden()).
  append(Column("flightplan_endurance_minutes", tr("Endurance\nhh:mm")).hidden()).
  append(Column("combined_rating", tr("Combined\nRating"))).
  append(Column("administrative_rating", tr("Admin\nRating"))).
  append(Column("atc_pilot_rating", tr("Pilot\nRating"))).
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

void OnlineClientSearch::overrideMode(const QStringList& overrideColumnTitles)
{
  Q_UNUSED(overrideColumnTitles);
}

void OnlineClientSearch::connectSearchSlots()
{
  SearchBaseTable::connectSearchSlots();

  Ui::MainWindow *ui = NavApp::getMainUi();

  // All widgets that will have their state and visibility saved and restored
  onlineClientSearchWidgets =
  {
    ui->horizontalLayoutOnlineClient
  };

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

  Ui::MainWindow *ui = NavApp::getMainUi();
  widgetState.save(ui->horizontalLayoutOnlineClient);
}

void OnlineClientSearch::restoreState()
{
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH)
  {
    atools::gui::WidgetState widgetState(lnm::SEARCHTAB_ONLINE_CLIENT_VIEW_WIDGET);
    widgetState.restore(onlineClientSearchWidgets);

    restoreViewState(false);

    // Need to block signals here to avoid unwanted behavior (will enable
    // distance search and avoid saving of wrong view widget state)
    widgetState.setBlockSignals(true);
    Ui::MainWindow *ui = NavApp::getMainUi();
    widgetState.restore(ui->horizontalLayoutOnlineClient);
  }
  else
    atools::gui::WidgetState(lnm::SEARCHTAB_ONLINE_CLIENT_VIEW_WIDGET).restore(
      NavApp::getMainUi()->tableViewOnlineClientSearch);
}

void OnlineClientSearch::saveViewState(bool distSearchActive)
{
  Q_UNUSED(distSearchActive);
  atools::gui::WidgetState(lnm::SEARCHTAB_ONLINE_CLIENT_VIEW_WIDGET).save(
    NavApp::getMainUi()->tableViewOnlineClientSearch);
}

void OnlineClientSearch::restoreViewState(bool distSearchActive)
{
  Q_UNUSED(distSearchActive);
  atools::gui::WidgetState(lnm::SEARCHTAB_ONLINE_CLIENT_VIEW_WIDGET).restore(
    NavApp::getMainUi()->tableViewOnlineClientSearch);
}

/* Callback for the controller. Will be called for each table cell and should return a formatted value */
QVariant OnlineClientSearch::modelDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant& roleValue,
                                              const QVariant& displayRoleValue, Qt::ItemDataRole role) const
{
  switch(role)
  {
    case Qt::DisplayRole:
      return formatModelData(col, displayRoleValue);

      break;
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
    if(col->getColumnName() == "on_ground" || col->getColumnName() == "prefile")
      return displayRoleValue.toInt() > 0 ? tr("Yes") : QString();
    else if(col->getColumnName() == "administrative_rating")
      return atools::fs::online::admRatingText(
        static_cast<atools::fs::online::adm::AdministrativeRating>(displayRoleValue.toInt()));
    else if(col->getColumnName() == "atc_pilot_rating")
      return atools::fs::online::pilotRatingText(
        static_cast<atools::fs::online::pilot::PilotRating>(displayRoleValue.toInt()));
    else if(col->getColumnName() == "facility_type")
      return atools::fs::online::facilityTypeText(
        static_cast<atools::fs::online::fac::FacilityType>(displayRoleValue.toInt()));
    else if(col->getColumnName() == "atis")
      return displayRoleValue.toString().simplified();
    else if(col->getColumnName() == "atis_time" || col->getColumnName() == "connection_time")
      return QLocale().toString(displayRoleValue.toDateTime(), QLocale::NarrowFormat);
    else if(col->getColumnName() == "flightplan_endurance_minutes" ||
            col->getColumnName() == "flightplan_enroute_minutes")
      return formatter::formatMinutesHours(displayRoleValue.toDouble() / 60.);

    return SearchBaseTable::formatModelData(col, displayRoleValue);
  }

  return SearchBaseTable::formatModelData(col, displayRoleValue);
}

void OnlineClientSearch::getSelectedMapObjects(map::MapSearchResult& result) const
{
  if(!NavApp::getMainUi()->dockWidgetSearch->isVisible())
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
        result.onlineAircraft.append(ac);
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
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->pushButtonOnlineClientSearchClearSelection->setEnabled(sm != nullptr && sm->hasSelection());
}

QAction *OnlineClientSearch::followModeAction()
{
  return NavApp::getMainUi()->actionSearchOnlineClientFollowSelection;
}
