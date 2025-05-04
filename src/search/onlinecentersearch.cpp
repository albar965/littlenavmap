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

#include "search/onlinecentersearch.h"

#include "atools.h"
#include "common/constants.h"
#include "common/mapresult.h"
#include "common/maptypes.h"
#include "common/maptypesfactory.h"
#include "common/unit.h"
#include "fs/online/onlinetypes.h"
#include "gui/widgetstate.h"
#include "app/navapp.h"
#include "search/column.h"
#include "search/columnlist.h"
#include "search/sqlcontroller.h"
#include "sql/sqlrecord.h"
#include "ui_mainwindow.h"

#include <QDateTime>

OnlineCenterSearch::OnlineCenterSearch(QMainWindow *parent, QTableView *tableView, si::TabSearchId tabWidgetIndex)
  : SearchBaseTable(parent, tableView, new ColumnList("atc", "atc_id"), tabWidgetIndex)
{
  /* *INDENT-OFF* */
  ui->pushButtonOnlineCenterHelpSearch->setToolTip(
    tr("<p>All set search conditions have to match.</p>"
    "<p>Search tips for text fields: </p>"
    "<ul>"
      "<li>Default is search for online centers that contain the entered text.</li>"
      "<li>Use &quot;*&quot; as a placeholder for any text. </li>"
      "<li>Prefix with &quot;-&quot; as first character to negate search.</li>"
      "<li>Only callsign field: Use double quotes like &quot;EGGB_TWR&quot; to force exact search.</li>"
    "</ul>"));
  /* *INDENT-ON* */

  QStringList facilityType;
  facilityType << QString()
               << " > 0"    // No OBSERVER,
               << " = 0"    // OBSERVER = 0,
               << " = 1"    // FLIGHT_INFORMATION = 1,
               << " = 2"    // DELIVERY = 2,
               << " = 3"    // GROUND = 3,
               << " = 4"    // TOWER = 4,
               << " = 5"    // APPROACH = 5,
               << " = 6"    // ACC = 6,
               << " = 7";   // DEPARTURE = 7

  // Default view column descriptors
  // Hidden columns are part of the query and can be used as search criteria but are not shown in the table
  // Columns that are hidden are also needed to fill MapAirport object and for the icon delegate
  columns->
  append(Column("atc_id").hidden()).

  append(Column("callsign", ui->lineEditOnlineCenterCallsign, tr("Callsign")).defaultSort().filter()).
  append(Column("name", ui->lineEditOnlineCenterName, tr("Name")).filter()).
  append(Column("facility_type", ui->comboBoxOnlineCenterFacilityType,
                tr("Facility\nType")).indexCondMap(facilityType)).
  append(Column("server", ui->lineEditOnlineCenterServer, tr("Server")).filter()).
  append(Column("frequency", tr("Frequency\nMHz"))).
  append(Column("visual_range", tr("Range\n%dist%"))).
  append(Column("atis", tr("ATIS"))).
  append(Column("atis_time", tr("ATIS\nTime"))).
  append(Column("connection_time", tr("Connection\nTime"))).
  append(Column("com_type").hidden()).
  append(Column("type").hidden()).
  append(Column("max_lonx").hidden()).
  append(Column("max_laty").hidden()).
  append(Column("min_lonx").hidden()).
  append(Column("min_laty").hidden()).
  append(Column("lonx").hidden()).
  append(Column("laty").hidden())
  ;

  SearchBaseTable::initViewAndController(NavApp::getDatabaseOnline());

  // Add model data handler and model format handler as callbacks
  setCallbacks();
}

OnlineCenterSearch::~OnlineCenterSearch()
{
}

void OnlineCenterSearch::overrideMode(const QStringList&)
{
}

void OnlineCenterSearch::connectSearchSlots()
{
  SearchBaseTable::connectSearchSlots();

  // All widgets that will have their state and visibility saved and restored
  onlineCenterSearchWidgets =
  {
    ui->horizontalLayoutOnlineCenter,
    ui->actionSearchOnlineCenterFollowSelection
  };

  // Small push buttons on top
  connect(ui->pushButtonOnlineCenterSearchClearSelection, &QPushButton::clicked, this, &SearchBaseTable::nothingSelectedTriggered);
  connect(ui->pushButtonOnlineCenterSearchReset, &QPushButton::clicked, this, &SearchBaseTable::resetSearch);

  installEventFilterForWidget(ui->lineEditOnlineCenterCallsign);
  installEventFilterForWidget(ui->lineEditOnlineCenterName);
  installEventFilterForWidget(ui->lineEditOnlineCenterServer);
  installEventFilterForWidget(ui->comboBoxOnlineCenterFacilityType);

  // Connect widgets to the controller
  SearchBaseTable::connectSearchWidgets();
}

void OnlineCenterSearch::saveState()
{
  atools::gui::WidgetState widgetState(lnm::SEARCHTAB_ONLINE_CENTER_VIEW_WIDGET);
  widgetState.save(onlineCenterSearchWidgets);
}

void OnlineCenterSearch::restoreState()
{
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH)
  {
    atools::gui::WidgetState widgetState(lnm::SEARCHTAB_ONLINE_CENTER_VIEW_WIDGET);
    widgetState.restore(onlineCenterSearchWidgets);
  }
  else
    atools::gui::WidgetState(lnm::SEARCHTAB_ONLINE_CENTER_VIEW_WIDGET).restore(ui->tableViewOnlineCenterSearch);

  finishRestore();
}

void OnlineCenterSearch::saveViewState(bool)
{
  atools::gui::WidgetState(lnm::SEARCHTAB_ONLINE_CENTER_VIEW_WIDGET).save(ui->tableViewOnlineCenterSearch);
}

void OnlineCenterSearch::restoreViewState(bool)
{
  atools::gui::WidgetState(lnm::SEARCHTAB_ONLINE_CENTER_VIEW_WIDGET).restore(ui->tableViewOnlineCenterSearch);
}

/* Callback for the controller. Will be called for each table cell and should return a formatted value */
QVariant OnlineCenterSearch::modelDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant& roleValue,
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
      if(col->getColumnName() == "frequency")
        return Qt::AlignRight;
      else if(col->getColumnName() == "facility_type")
        return Qt::AlignLeft;

      break;
    default:
      break;
  }

  return SearchBaseTable::modelDataHandler(colIndex, rowIndex, col, roleValue, displayRoleValue, role);
}

/* Formats the QVariant to a QString depending on column name */
QString OnlineCenterSearch::formatModelData(const Column *col, const QVariant& displayRoleValue) const
{
  if(!displayRoleValue.isNull())
  {
    // Called directly by the model for export functions
    if(col->getColumnName() == "frequency")
    {
      QStringList freqs;
      const QStringList dispRoles = displayRoleValue.toString().split('&');
      for(const QString& str : dispRoles)
        freqs.append(QLocale().toString(str.toDouble() / 1000., 'f', 3));
      return freqs.join(tr(", "));
    }
    else if(col->getColumnName() == "visual_range")
      return !displayRoleValue.isNull() &&
             displayRoleValue.toFloat() < map::INVALID_ALTITUDE_VALUE &&
             displayRoleValue.toFloat() > 0.f ?
             Unit::distNm(displayRoleValue.toFloat(), false) : QString();
    else if(col->getColumnName() == "facility_type")
      return atools::fs::online::facilityTypeText(
        static_cast<atools::fs::online::fac::FacilityType>(displayRoleValue.toInt()));
    else if(col->getColumnName() == "atis")
      return displayRoleValue.toString().simplified();
    else if(col->getColumnName() == "atis_time" || col->getColumnName() == "connection_time")
      return QLocale().toString(displayRoleValue.toDateTime(), QLocale::NarrowFormat);

    return SearchBaseTable::formatModelData(col, displayRoleValue);
  }

  return SearchBaseTable::formatModelData(col, displayRoleValue);
}

void OnlineCenterSearch::getSelectedMapObjects(map::MapResult& result) const
{
  if(!ui->dockWidgetSearch->isVisible())
    return;

  // Build a SQL record with all available fields
  atools::sql::SqlRecord rec;
  controller->initRecord(rec);
  // qDebug() << Q_FUNC_INFO << rec;

  const QItemSelection& selection = controller->getSelection();
  for(const QItemSelectionRange& rng :  selection)
  {
    for(int row = rng.top(); row <= rng.bottom(); ++row)
    {
      if(controller->hasRow(row))
      {
        controller->fillRecord(row, rec);
        // qDebug() << Q_FUNC_INFO << rec;

        map::MapAirspace obj;
        MapTypesFactory().fillAirspace(rec, obj, map::AIRSPACE_SRC_ONLINE);
        result.airspaces.append(obj);
      }
    }
  }
}

void OnlineCenterSearch::postDatabaseLoad()
{
  SearchBaseTable::postDatabaseLoad();
  setCallbacks();
}

/* Sets controller data formatting callback and desired data roles */
void OnlineCenterSearch::setCallbacks()
{
  using namespace std::placeholders;
  controller->setDataCallback(std::bind(&OnlineCenterSearch::modelDataHandler, this, _1, _2, _3, _4, _5, _6),
                              {Qt::DisplayRole, Qt::BackgroundRole, Qt::TextAlignmentRole, Qt::ToolTipRole});
}

/* Update the button menu actions. Add * for changed search criteria and toggle show/hide all
 * action depending on other action states */
void OnlineCenterSearch::updateButtonMenu()
{
}

void OnlineCenterSearch::updatePushButtons()
{
  QItemSelectionModel *sm = view->selectionModel();
  ui->pushButtonOnlineCenterSearchClearSelection->setEnabled(sm != nullptr && sm->hasSelection());
}

QAction *OnlineCenterSearch::followModeAction()
{
  return ui->actionSearchOnlineCenterFollowSelection;
}
