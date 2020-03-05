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

#include "search/logdatasearch.h"

#include "navapp.h"
#include "common/constants.h"
#include "search/sqlcontroller.h"
#include "search/column.h"
#include "query/airportquery.h"
#include "ui_mainwindow.h"
#include "search/columnlist.h"
#include "gui/widgetutil.h"
#include "gui/widgetstate.h"
#include "common/mapcolors.h"
#include "common/unit.h"
#include "atools.h"
#include "common/maptypesfactory.h"
#include "sql/sqlrecord.h"

LogdataSearch::LogdataSearch(QMainWindow *parent, QTableView *tableView, si::TabSearchId tabWidgetIndex)
  : SearchBaseTable(parent, tableView, new ColumnList("logbook", "logbook_id"), tabWidgetIndex)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  // All widgets that will have their state and visibility saved and restored
  logdataSearchWidgets =
  {
    ui->horizontalLayoutLogdata,
    ui->horizontalLayoutLogdataMore,
    ui->horizontalLayoutLogdataDist,
    ui->lineLogdataMore,
    ui->lineLogdataMoreDist,
    ui->actionLogdataSearchShowMoreOptions,
    ui->actionLogdataSearchShowDistOptions
  };

  // All drop down menu actions
  logdataSearchMenuActions =
  {
    ui->actionLogdataSearchShowMoreOptions, ui->actionLogdataSearchShowDistOptions
  };

  // Default view column descriptors
  // Hidden columns are part of the query and can be used as search criteria but are not shown in the table
  // Columns that are hidden are also needed to fill MapAirport object and for the icon delegate
  columns->
  append(Column("logbook_id").hidden()).
  append(Column("departure_time", tr("Departure\nReal Time")).defaultSort(true).defaultSortOrder(Qt::DescendingOrder)).
  append(Column("departure_ident", ui->lineEditLogdataDeparture, tr("Departure\nICAO")).filter()).
  append(Column("departure_name", tr("Departure"))).
  append(Column("departure_runway").hidden()).
  append(Column("destination_ident", ui->lineEditLogdataDestination, tr("Destination\nICAO")).filter()).
  append(Column("destination_name", tr("Destination"))).
  append(Column("destination_runway").hidden()).
  append(Column("aircraft_name", ui->lineEditLogdataAircraftModel, tr("Aircraft\nModel")).filter()).
  append(Column("aircraft_registration", ui->lineEditLogdataAircraftRegistration,
                tr("Aircraft\nRegistration")).filter()).
  append(Column("aircraft_type", ui->lineEditLogdataAircraftType, tr("Aircraft\nType")).filter()).
  append(Column("simulator", ui->lineEditLogdataSimulator, tr("Simulator")).filter()).
  append(Column("performance_file").hidden()).
  append(Column("flightplan_file").hidden()).
  append(Column("distance", tr("Distance\n%dist%"))).
  append(Column("departure_time_sim", tr("Departure\nSim. Time UTC"))).
  append(Column("route_string").hidden()).
  append(Column("description", ui->lineEditLogdataDescription, tr("Description")).filter()).
  append(Column("departure_lonx").hidden()).
  append(Column("departure_laty").hidden()).
  append(Column("departure_alt").hidden()).
  append(Column("destination_lonx").hidden()).
  append(Column("destination_laty").hidden()).
  append(Column("destination_alt").hidden());

  SearchBaseTable::initViewAndController(NavApp::getDatabaseLogbook());

  // Add model data handler and model format handler as callbacks
  setCallbacks();
}

LogdataSearch::~LogdataSearch()
{
}

void LogdataSearch::connectSearchSlots()
{
  SearchBaseTable::connectSearchSlots();

  Ui::MainWindow *ui = NavApp::getMainUi();

  // Small push buttons on top
  connect(ui->pushButtonLogdataClearSelection, &QPushButton::clicked,
          this, &SearchBaseTable::nothingSelectedTriggered);
  connect(ui->pushButtonLogdataReset, &QPushButton::clicked, this, &SearchBaseTable::resetSearch);

  // Install filter for cursor down action
  installEventFilterForWidget(ui->lineEditLogdataDeparture);
  installEventFilterForWidget(ui->lineEditLogdataDestination);
  installEventFilterForWidget(ui->lineEditLogdataAircraftType);
  installEventFilterForWidget(ui->lineEditLogdataAircraftRegistration);
  installEventFilterForWidget(ui->lineEditLogdataAircraftModel);
  installEventFilterForWidget(ui->lineEditLogdataDescription);
  installEventFilterForWidget(ui->lineEditLogdataSimulator);

  columns->assignMinMaxWidget("distance", ui->spinBoxLogdataMinDist, ui->spinBoxLogdataMaxDist);

  // Connect widgets to the controller
  SearchBaseTable::connectSearchWidgets();
  ui->toolButtonLogdata->addActions({ui->actionLogdataSearchShowMoreOptions, ui->actionLogdataSearchShowDistOptions});

  // Drop down menu actions
  connect(ui->actionLogdataSearchShowMoreOptions, &QAction::toggled, [ = ](bool state)
  {
    atools::gui::util::showHideLayoutElements({ui->horizontalLayoutLogdataMore}, state, {ui->lineLogdataMore});
    updateButtonMenu();
  });

  connect(ui->actionLogdataSearchShowDistOptions, &QAction::toggled, [ = ](bool state)
  {
    atools::gui::util::showHideLayoutElements({ui->horizontalLayoutLogdataDist}, state, {ui->lineLogdataMoreDist});
    updateButtonMenu();
  });

  ui->actionLogdataEdit->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionLogdataAdd->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionLogdataDelete->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  ui->tableViewLogdata->addActions({ui->actionLogdataEdit, ui->actionLogdataAdd, ui->actionLogdataDelete});

  connect(ui->pushButtonLogdataEdit, &QPushButton::clicked, this, &LogdataSearch::editLogEntriesTriggered);
  connect(ui->actionLogdataEdit, &QAction::triggered, this, &LogdataSearch::editLogEntriesTriggered);

  connect(ui->pushButtonLogdataDel, &QPushButton::clicked, this, &LogdataSearch::deleteLogEntriesTriggered);
  connect(ui->actionLogdataDelete, &QAction::triggered, this, &LogdataSearch::deleteLogEntriesTriggered);

  connect(ui->pushButtonLogdataAdd, &QPushButton::clicked, this, &LogdataSearch::addLogEntryTriggered);
  connect(ui->actionLogdataAdd, &QAction::triggered, this, &LogdataSearch::addLogEntryTriggered);
}

void LogdataSearch::addLogEntryTriggered()
{
  emit addLogEntry();
}

void LogdataSearch::editLogEntriesTriggered()
{
  emit editLogEntries(getSelectedIds());
}

void LogdataSearch::deleteLogEntriesTriggered()
{
  emit deleteLogEntries(getSelectedIds());
}

void LogdataSearch::saveState()
{
  atools::gui::WidgetState widgetState(lnm::SEARCHTAB_LOGDATA_VIEW_WIDGET);
  widgetState.save(logdataSearchWidgets);

  Ui::MainWindow *ui = NavApp::getMainUi();
  widgetState.save({ui->horizontalLayoutLogdata, ui->horizontalLayoutLogdataMore, ui->horizontalLayoutLogdataDist,
                    ui->actionSearchLogdataFollowSelection});
}

void LogdataSearch::restoreState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState widgetState(lnm::SEARCHTAB_LOGDATA_VIEW_WIDGET);
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH)
  {
    widgetState.restore(logdataSearchWidgets);

    restoreViewState(false);

    // Need to block signals here to avoid unwanted behavior
    widgetState.setBlockSignals(true);
    widgetState.restore({ui->horizontalLayoutLogdata, ui->horizontalLayoutLogdataMore, ui->horizontalLayoutLogdataDist,
                         ui->actionSearchLogdataFollowSelection});
  }
  else
  {
    QList<QObject *> objList;
    atools::convertList(objList, logdataSearchMenuActions);
    widgetState.restore(objList);

    atools::gui::WidgetState(lnm::SEARCHTAB_LOGDATA_VIEW_WIDGET).restore(ui->tableViewLogdata);
  }
}

void LogdataSearch::saveViewState(bool distSearchActive)
{
  Q_UNUSED(distSearchActive);
  atools::gui::WidgetState(lnm::SEARCHTAB_LOGDATA_VIEW_WIDGET).save(NavApp::getMainUi()->tableViewLogdata);
}

void LogdataSearch::restoreViewState(bool distSearchActive)
{
  Q_UNUSED(distSearchActive);
  atools::gui::WidgetState(lnm::SEARCHTAB_LOGDATA_VIEW_WIDGET).restore(NavApp::getMainUi()->tableViewLogdata);
}

/* Callback for the controller. Will be called for each table cell and should return a formatted value */
QVariant LogdataSearch::modelDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant& roleValue,
                                         const QVariant& displayRoleValue, Qt::ItemDataRole role) const
{
  Q_UNUSED(roleValue);

  switch(role)
  {
    case Qt::DisplayRole:
      return formatModelData(col, displayRoleValue);

    case Qt::TextAlignmentRole:
      if(col->getColumnName().endsWith("_ident") || col->getColumnName() == "distance" ||
         col->getColumnName().startsWith("departure_time") || col->getColumnName().startsWith("destination_time") ||
         displayRoleValue.type() == QVariant::Int || displayRoleValue.type() == QVariant::UInt ||
         displayRoleValue.type() == QVariant::LongLong || displayRoleValue.type() == QVariant::ULongLong ||
         displayRoleValue.type() == QVariant::Double)
        // Align all numeric columns right
        return Qt::AlignRight;

      break;
    case Qt::BackgroundRole:
      if(colIndex == controller->getSortColumnIndex())
        // Use another alternating color if this is a field in the sort column
        return mapcolors::alternatingRowColor(rowIndex, true);

      break;
    case Qt::ToolTipRole:
      if(col->getColumnName() == "description")
        return atools::elideTextLinesShort(displayRoleValue.toString(), 40);

      break;

    case Qt::FontRole:
      if(col->getColumnName().endsWith("_ident"))
      {
        QFont font = view->font();
        font.setBold(true);
        return font;
      }

      break;

    default:
      break;
  }

  return QVariant();
}

/* Formats the QVariant to a QString depending on column name */
QString LogdataSearch::formatModelData(const Column *col, const QVariant& displayRoleValue) const
{
  // Called directly by the model for export functions
  if(col->getColumnName().startsWith("departure_time") || col->getColumnName().startsWith("destination_time"))
    return QLocale().toString(displayRoleValue.toDateTime(), QLocale::NarrowFormat);
  else if(col->getColumnName() == "distance")
    return Unit::distNm(displayRoleValue.toFloat(), false, 0);
  else if(col->getColumnName() == "description")
    return atools::elideTextShort(displayRoleValue.toString().simplified(), 80);
  else if(displayRoleValue.type() == QVariant::Int || displayRoleValue.type() == QVariant::UInt)
    return QLocale().toString(displayRoleValue.toInt());
  else if(displayRoleValue.type() == QVariant::LongLong || displayRoleValue.type() == QVariant::ULongLong)
    return QLocale().toString(displayRoleValue.toLongLong());
  else if(displayRoleValue.type() == QVariant::Double)
    return QLocale().toString(displayRoleValue.toDouble());

  return displayRoleValue.toString();
}

void LogdataSearch::getSelectedMapObjects(map::MapSearchResult& result) const
{
  if(!NavApp::getMainUi()->dockWidgetSearch->isVisible())
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

        map::MapLogbookEntry entry;
        MapTypesFactory().fillLogbookEntry(rec, entry);
        airportQuery->getAirportByIdent(entry.destination, entry.destinationIdent);
        airportQuery->getAirportByIdent(entry.departure, entry.departureIdent);
        result.airports.append(entry.destination);
        result.airports.append(entry.departure);
        result.logbookEntries.append(entry);
      }
    }
  }
}

void LogdataSearch::postDatabaseLoad()
{
  SearchBaseTable::postDatabaseLoad();
  setCallbacks();
}

/* Sets controller data formatting callback and desired data roles */
void LogdataSearch::setCallbacks()
{
  using namespace std::placeholders;
  controller->setDataCallback(std::bind(&LogdataSearch::modelDataHandler, this, _1, _2, _3, _4, _5, _6),
                              {Qt::DisplayRole, Qt::BackgroundRole, Qt::TextAlignmentRole, Qt::ToolTipRole,
                               Qt::FontRole});
}

/* Update the button menu actions. Add * for changed search criteria and toggle show/hide all
 * action depending on other action states */
void LogdataSearch::updateButtonMenu()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  atools::gui::util::changeStarIndication(ui->actionLogdataSearchShowMoreOptions,
                                          atools::gui::util::anyWidgetChanged({ui->horizontalLayoutLogdataMore}));
  atools::gui::util::changeStarIndication(ui->actionLogdataSearchShowDistOptions,
                                          atools::gui::util::anyWidgetChanged({ui->horizontalLayoutLogdataDist}));
}

void LogdataSearch::updatePushButtons()
{
  QItemSelectionModel *sm = view->selectionModel();
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->pushButtonLogdataClearSelection->setEnabled(sm != nullptr && sm->hasSelection());
  ui->pushButtonLogdataDel->setEnabled(sm != nullptr && sm->hasSelection());
  ui->pushButtonLogdataEdit->setEnabled(sm != nullptr && sm->hasSelection());

  // Update actions and keys too
  ui->actionLogdataEdit->setEnabled(sm != nullptr && sm->hasSelection());
  ui->actionLogdataDelete->setEnabled(sm != nullptr && sm->hasSelection());
}

QAction *LogdataSearch::followModeAction()
{
  return NavApp::getMainUi()->actionSearchLogdataFollowSelection;
}
