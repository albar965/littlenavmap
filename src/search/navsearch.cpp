/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#include "search/navsearch.h"

#include "common/constants.h"
#include "search/sqlcontroller.h"
#include "gui/mainwindow.h"
#include "search/column.h"
#include "search/navicondelegate.h"
#include "ui_mainwindow.h"
#include "search/columnlist.h"
#include "gui/widgetutil.h"
#include "gui/widgetstate.h"
#include "common/mapcolors.h"
#include "common/unit.h"
#include "atools.h"
#include "common/maptypesfactory.h"
#include "sql/sqlrecord.h"

NavSearch::NavSearch(MainWindow *parent, QTableView *tableView,
                     MapQuery *mapQuery, int tabWidgetIndex)
  : SearchBase(parent, tableView, new ColumnList("nav_search", "nav_search_id"), mapQuery, tabWidgetIndex)
{
  Ui::MainWindow *ui = mainWindow->getUi();

  // All widgets that will have their state and visibility saved and restored
  navSearchWidgets =
  {
    ui->horizontalLayoutNavNameSearch,
    ui->horizontalLayoutNavExtSearch,
    ui->horizontalLayoutNavScenerySearch,
    ui->lineNavDistSearch,
    ui->lineNavScenerySearch,
    ui->actionNavSearchShowAllOptions,
    ui->actionNavSearchShowDistOptions,
    ui->actionNavSearchShowSceneryOptions
  };

  // All drop down menu actions
  navSearchMenuActions =
  {
    ui->actionNavSearchShowAllOptions,
    ui->actionNavSearchShowDistOptions,
    ui->actionNavSearchShowSceneryOptions
  };

  // Show/hide all search options menu action
  connect(ui->actionNavSearchShowAllOptions, &QAction::toggled, [ = ](bool state)
          {
            for(QAction *a: navSearchMenuActions)
              a->setChecked(state);
          });

  // Build SQL query conditions
  QStringList typeCondMap;
  typeCondMap << QString()
              << "= 'HIGH'"
              << "= 'LOW'"
              << "= 'TERMINAL'"
              << "= 'HH'"
              << "= 'H'"
              << "= 'MH'"
              << "= 'COMPASS_POINT'"
              << "= 'NAMED'"
              << "= 'UNNAMED'"
              << "= 'VOR'"
              << "= 'NDB'";

  QStringList navTypeCondMap;
  navTypeCondMap << QString()
                 << "nav_type in ('VOR', 'VORDME', 'DME')"
                 << "nav_type in ('VOR', 'VORDME', 'DME', 'NDB')"
                 << "nav_type = 'VORDME'"
                 << "nav_type = 'VOR'"
                 << "nav_type = 'DME'"
                 << "nav_type = 'NDB'"
                 << "nav_type = 'WAYPOINT'"
                 << "nav_type = 'WAYPOINT' and "
     "(waypoint_num_victor_airway > 0 or waypoint_num_jet_airway > 0)";

  // Default view column descriptors
  // Hidden columns are part of the query and can be used as search criteria but are not shown in the table
  // Columns that are hidden are also needed to fill MapAirport object and for the icon delegate
  columns->
  append(Column("nav_search_id").hidden()).
  append(Column("distance", tr("Distance\n%dist%")).distanceCol()).
  append(Column("heading", tr("Heading\n°T")).distanceCol()).
  append(Column("ident", ui->lineEditNavIcaoSearch, tr("ICAO")).filter().defaultSort()).

  append(Column("nav_type", ui->comboBoxNavNavAidSearch, tr("Nav Aid\nType")).
         indexCondMap(navTypeCondMap).includesName()).

  append(Column("type", ui->comboBoxNavTypeSearch, tr("Type")).indexCondMap(typeCondMap)).
  append(Column("name", ui->lineEditNavNameSearch, tr("Name")).filter()).
  append(Column("region", ui->lineEditNavRegionSearch, tr("Region")).filter()).
  append(Column("airport_ident", ui->lineEditNavAirportIcaoSearch, tr("Airport\nICAO")).filter()).
  append(Column("frequency", tr("Frequency\nkHz/MHz"))).
  append(Column("range", ui->spinBoxNavMaxRangeSearch, tr("Range\n%dist%")).
         filter().condition(">").convertFunc(Unit::distNmF)).
  append(Column("mag_var", tr("Mag\nVar°"))).
  append(Column("altitude", tr("Elevation\n%alt%")).convertFunc(Unit::altFeetF)).
  append(Column("scenery_local_path", ui->lineEditNavScenerySearch, tr("Scenery Path")).filter()).
  append(Column("bgl_filename", ui->lineEditNavFileSearch, tr("BGL File")).filter()).
  append(Column("waypoint_num_victor_airway").hidden()).
  append(Column("waypoint_num_jet_airway").hidden()).
  append(Column("vor_id").hidden()).
  append(Column("ndb_id").hidden()).
  append(Column("waypoint_id").hidden()).
  append(Column("lonx").hidden()).
  append(Column("laty").hidden())
  ;

  // Add icon delegate for the ident column
  iconDelegate = new NavIconDelegate(columns);
  view->setItemDelegateForColumn(columns->getColumn("ident")->getIndex(), iconDelegate);

  SearchBase::initViewAndController();

  // Add model data handler and model format handler as callbacks
  setCallbacks();
}

NavSearch::~NavSearch()
{
  delete iconDelegate;
}

void NavSearch::connectSearchSlots()
{
  SearchBase::connectSearchSlots();

  Ui::MainWindow *ui = mainWindow->getUi();

  connect(ui->lineEditNavIcaoSearch, &QLineEdit::returnPressed, this, &SearchBase::showFirstEntry);

  // Distance
  columns->assignDistanceSearchWidgets(ui->checkBoxNavDistSearch,
                                       ui->comboBoxNavDistDirectionSearch,
                                       ui->spinBoxNavDistMinSearch,
                                       ui->spinBoxNavDistMaxSearch);

  // Connect widgets to the controller
  SearchBase::connectSearchWidgets();
  ui->toolButtonNavSearch->addActions({ui->actionNavSearchShowAllOptions,
                                       ui->actionNavSearchShowDistOptions,
                                       ui->actionNavSearchShowSceneryOptions});

  // Drop down menu actions
  connect(ui->actionNavSearchShowDistOptions, &QAction::toggled, [ = ](bool state)
          {
            atools::gui::util::showHideLayoutElements({ui->horizontalLayoutNavDistanceSearch}, state,
                                                      {ui->lineNavDistSearch});
            updateButtonMenu();
          });
  connect(ui->actionNavSearchShowSceneryOptions, &QAction::toggled, [ = ](bool state)
          {
            atools::gui::util::showHideLayoutElements({ui->horizontalLayoutNavScenerySearch}, state,
                                                      {ui->lineNavScenerySearch});
            updateButtonMenu();
          });
}

void NavSearch::saveState()
{
  atools::gui::WidgetState widgetState(lnm::SEARCHTAB_NAV_WIDGET);
  widgetState.save(navSearchWidgets);

  Ui::MainWindow *ui = mainWindow->getUi();
  widgetState.save(ui->horizontalLayoutNavDistanceSearch);
  saveViewState(ui->checkBoxNavDistSearch->isChecked());
}

void NavSearch::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::SEARCHTAB_NAV_WIDGET);
  widgetState.restore(navSearchWidgets);

  // Need to block signals here to avoid unwanted behavior (will enable
  // distance search and avoid saving of wrong view widget state)
  widgetState.setBlockSignals(true);
  Ui::MainWindow *ui = mainWindow->getUi();
  widgetState.restore(ui->horizontalLayoutNavDistanceSearch);
  restoreViewState(ui->checkBoxNavDistSearch->isChecked());

  bool distSearchChecked = ui->checkBoxNavDistSearch->isChecked();
  if(distSearchChecked)
    // Activate distance search if it was active - otherwise leave default behavior
    distanceSearchChanged(distSearchChecked, false /* Change view state */);

  QSpinBox *minDistanceWidget = columns->getMinDistanceWidget();
  QSpinBox *maxDistanceWidget = columns->getMaxDistanceWidget();
  minDistanceWidget->setMaximum(maxDistanceWidget->value());
  maxDistanceWidget->setMinimum(minDistanceWidget->value());
}

void NavSearch::saveViewState(bool distSearchActive)
{
  // Save layout for normal and distance search separately
  atools::gui::WidgetState(
    distSearchActive ? lnm::SEARCHTAB_NAV_VIEW_DIST_WIDGET : lnm::SEARCHTAB_NAV_VIEW_WIDGET
    ).save(mainWindow->getUi()->tableViewNavSearch);
}

void NavSearch::restoreViewState(bool distSearchActive)
{
  atools::gui::WidgetState(
    distSearchActive ? lnm::SEARCHTAB_NAV_VIEW_DIST_WIDGET : lnm::SEARCHTAB_NAV_VIEW_WIDGET
    ).restore(mainWindow->getUi()->tableViewNavSearch);
}

/* Callback for the controller. Will be called for each table cell and should return a formatted value */
QVariant NavSearch::modelDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant& roleValue,
                                     const QVariant& displayRoleValue, Qt::ItemDataRole role) const
{
  Q_UNUSED(roleValue);

  switch(role)
  {
    case Qt::DisplayRole:
      return formatModelData(col, displayRoleValue);

    case Qt::TextAlignmentRole:
      if(col->getColumnName() == "ident" || col->getColumnName() == "airport_ident" ||
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
    default:
      break;
  }

  return QVariant();
}

/* Formats the QVariant to a QString depending on column name */
QString NavSearch::formatModelData(const Column *col, const QVariant& displayRoleValue) const
{
  // Called directly by the model for export functions
  if(col->getColumnName() == "type")
    return maptypes::navTypeName(displayRoleValue.toString());
  else if(col->getColumnName() == "nav_type")
    return maptypes::navName(displayRoleValue.toString());
  else if(col->getColumnName() == "name")
    return atools::capString(displayRoleValue.toString());
  else if(col->getColumnName() == "range")
    return Unit::distNm(displayRoleValue.toFloat(), false);
  else if(col->getColumnName() == "altitude")
    return Unit::altFeet(displayRoleValue.toFloat(), false);
  else if(col->getColumnName() == "frequency" && !displayRoleValue.isNull())
  {
    double freq = displayRoleValue.toDouble();

    // VOR and DME are scaled up in nav_search to easily differentiate from NDB
    if(freq >= 1000000 && freq <= 1200000)
      return QLocale().toString(displayRoleValue.toDouble() / 10000., 'f', 2);
    else if(freq >= 10000 && freq <= 120000)
      return QLocale().toString(displayRoleValue.toDouble() / 100., 'f', 1);
    else
      return "Invalid";
  }
  else if(col->getColumnName() == "mag_var")
    return maptypes::magvarText(displayRoleValue.toFloat());
  else if(displayRoleValue.type() == QVariant::Int || displayRoleValue.type() == QVariant::UInt)
    return QLocale().toString(displayRoleValue.toInt());
  else if(displayRoleValue.type() == QVariant::LongLong || displayRoleValue.type() == QVariant::ULongLong)
    return QLocale().toString(displayRoleValue.toLongLong());
  else if(displayRoleValue.type() == QVariant::Double)
    return QLocale().toString(displayRoleValue.toDouble());

  return displayRoleValue.toString();
}

void NavSearch::getSelectedMapObjects(maptypes::MapSearchResult& result) const
{
  if(!mainWindow->getUi()->dockWidgetSearch->isVisible())
    return;

  // Build a SQL record with all available fields
  atools::sql::SqlRecord rec;
  controller->initRecord(rec);

  MapTypesFactory factory;

  // Fill the result with all (mixed) navaids
  const QItemSelection& selection = controller->getSelection();
  for(const QItemSelectionRange& rng :  selection)
  {
    for(int row = rng.top(); row <= rng.bottom(); ++row)
    {
      controller->fillRecord(row, rec);

      // All objects are fully populated
      QString navType = rec.valueStr("nav_type");
      maptypes::MapObjectTypes type = maptypes::navTypeToMapObjectType(navType);

      if(type == maptypes::WAYPOINT)
      {
        maptypes::MapWaypoint obj;
        factory.fillWaypointFromNav(rec, obj);
        result.waypoints.append(obj);
      }
      else if(type == maptypes::NDB)
      {
        maptypes::MapNdb obj;
        factory.fillNdb(rec, obj);
        result.ndbs.append(obj);
      }
      else if(type == maptypes::VOR)
      {
        maptypes::MapVor obj;
        factory.fillVorFromNav(rec, obj);
        result.vors.append(obj);
      }
    }
  }
}

void NavSearch::postDatabaseLoad()
{
  SearchBase::postDatabaseLoad();
  setCallbacks();
}

/* Sets controller data formatting callback and desired data roles */
void NavSearch::setCallbacks()
{
  using namespace std::placeholders;
  controller->setDataCallback(std::bind(&NavSearch::modelDataHandler, this, _1, _2, _3, _4, _5, _6),
                              {Qt::DisplayRole, Qt::BackgroundRole, Qt::TextAlignmentRole});
}

/* Update the button menu actions. Add * for changed search criteria and toggle show/hide all
 * action depending on other action states */
void NavSearch::updateButtonMenu()
{
  Ui::MainWindow *ui = mainWindow->getUi();

  // Change state of show all action
  ui->actionNavSearchShowAllOptions->blockSignals(true);
  if(atools::gui::util::allChecked({ui->actionNavSearchShowDistOptions, ui->actionNavSearchShowSceneryOptions}))
    ui->actionNavSearchShowAllOptions->setChecked(true);
  else if(atools::gui::util::noneChecked({ui->actionNavSearchShowDistOptions,
                                          ui->actionNavSearchShowSceneryOptions}))
    ui->actionNavSearchShowAllOptions->setChecked(false);
  else
    ui->actionNavSearchShowAllOptions->setChecked(false);
  ui->actionNavSearchShowAllOptions->blockSignals(false);

  // Show star in action for all widgets that are not in default state
  bool distanceSearchChanged = false;
  if(ui->checkBoxNavDistSearch->isChecked())
    distanceSearchChanged = atools::gui::util::anyWidgetChanged({ui->horizontalLayoutNavDistanceSearch});

  atools::gui::util::changeStarIndication(ui->actionNavSearchShowDistOptions, distanceSearchChanged);

  atools::gui::util::changeStarIndication(ui->actionNavSearchShowSceneryOptions,
                                          atools::gui::util::anyWidgetChanged(
                                            {ui->horizontalLayoutNavScenerySearch}));
}
