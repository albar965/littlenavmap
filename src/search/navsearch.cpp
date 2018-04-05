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

#include "search/navsearch.h"

#include "navapp.h"
#include "common/maptypes.h"
#include "common/mapflags.h"
#include "common/constants.h"
#include "search/sqlcontroller.h"
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

NavSearch::NavSearch(QMainWindow *parent, QTableView *tableView, si::SearchTabIndex tabWidgetIndex)
  : SearchBaseTable(parent, tableView, new ColumnList("nav_search", "nav_search_id"), tabWidgetIndex)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

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

  // Possible VOR types
  // H
  // L
  // T
  // TC
  // VTH
  // VTL
  // VTT

  // Possible combinations
  // type   nav_type
  // N	     W
  // NCP	   N
  // NH	     N
  // NHH	   N
  // NMH	   N
  // TC	     TC
  // V	     W
  // VH	     D
  // VH	     V
  // VH	     VD
  // VH	     VT
  // VL	     V
  // VL	     VD
  // VL	     VT
  // VT	     V
  // VT	     VD
  // VT	     VT
  // WN	     W
  // WU	     W
  // Build SQL query conditions
  QStringList typeCondMap;
  typeCondMap << QString()
              << "type = 'VH'"
              << "type = 'VL'"
              << "type = 'VT'"
              << "type = 'NHH'"
              << "type = 'NH'"
              << "type = 'NMH'"
              << "type = 'NCP'"
              << "type = 'WN'"
              << "type = 'WU'"
              << "type = 'V'"
              << "type = 'N'";

  QStringList navTypeCondMap;
  navTypeCondMap << QString()
                 << "(nav_type like ('V%') or nav_type in ('D', 'TC'))"
                 << "(nav_type like ('V%') or nav_type in ('D', 'TC', 'N'))"
                 << "nav_type in ('VD')"
                 << "nav_type in ('V')"
                 << "nav_type in ('D')"
                 << "nav_type in ('VT')"
                 << "nav_type in ('TC', 'TCD')"
                 << "nav_type = 'N'"
                 << "nav_type = 'W'"
                 << "nav_type = 'W' and "
     "(waypoint_num_victor_airway > 0 or waypoint_num_jet_airway > 0)";

  // Default view column descriptors
  // Hidden columns are part of the query and can be used as search criteria but are not shown in the table
  // Columns that are hidden are also needed to fill MapAirport object and for the icon delegate
  columns->
  append(Column("nav_search_id").hidden()).
  append(Column("distance", tr("Distance\n%dist%")).distanceCol()).
  append(Column("heading", tr("Heading\n°T")).distanceCol()).
  append(Column("ident", ui->lineEditNavIcaoSearch, tr("ICAO")).filter().defaultSort()).

  append(Column("nav_type", ui->comboBoxNavNavAidSearch, tr("Navaid\nType")).
         indexCondMap(navTypeCondMap).includesName()).

  append(Column("type", ui->comboBoxNavTypeSearch, tr("Type")).indexCondMap(typeCondMap).includesName()).
  append(Column("name", ui->lineEditNavNameSearch, tr("Name")).filter()).
  append(Column("region", ui->lineEditNavRegionSearch, tr("Region")).filter()).
  append(Column("airport_ident", ui->lineEditNavAirportIcaoSearch, tr("Airport\nICAO")).filter()).
  append(Column("frequency", tr("Frequency\nkHz/MHz"))).
  append(Column("channel", tr("Channel"))).
  append(Column("range", ui->spinBoxNavMaxRangeSearch, tr("Range\n%dist%")).
         filter().condition(">").convertFunc(Unit::distNmF)).
  append(Column("mag_var", tr("Mag.\nDecl.°"))).
  append(Column("altitude", tr("Elevation\n%alt%")).convertFunc(Unit::altFeetF)).
  append(Column("scenery_local_path", ui->lineEditNavScenerySearch, tr("Scenery Path")).filter()).
  append(Column("bgl_filename", ui->lineEditNavFileSearch, tr("File")).filter()).
  append(Column("waypoint_num_victor_airway").hidden()).
  append(Column("waypoint_num_jet_airway").hidden()).
  append(Column("vor_id").hidden()).
  append(Column("ndb_id").hidden()).
  append(Column("waypoint_id").hidden()).
  append(Column("lonx").hidden()).
  append(Column("laty").hidden())
  ;

  ui->labelNavSearchOverride->hide();

  // Add icon delegate for the ident column
  iconDelegate = new NavIconDelegate(columns);
  view->setItemDelegateForColumn(columns->getColumn("ident")->getIndex(), iconDelegate);

  SearchBaseTable::initViewAndController(NavApp::getDatabaseNav());

  // Add model data handler and model format handler as callbacks
  setCallbacks();
}

NavSearch::~NavSearch()
{
  delete iconDelegate;
}

void NavSearch::overrideMode(const QStringList& overrideColumnTitles)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  if(overrideColumnTitles.isEmpty())
  {
    ui->labelNavSearchOverride->hide();
    ui->labelNavSearchOverride->clear();
  }
  else
  {
    ui->labelNavSearchOverride->show();
    ui->labelNavSearchOverride->setText(tr("%1 overriding all other search options.").
                                        arg(overrideColumnTitles.join(" and ")));
  }
}

void NavSearch::connectSearchSlots()
{
  SearchBaseTable::connectSearchSlots();

  Ui::MainWindow *ui = NavApp::getMainUi();

  // Small push buttons on top
  connect(ui->pushButtonNavSearchClearSelection, &QPushButton::clicked,
          this, &SearchBaseTable::nothingSelectedTriggered);
  connect(ui->pushButtonNavSearchReset, &QPushButton::clicked, this, &SearchBaseTable::resetSearch);

  installEventFilterForWidget(ui->lineEditNavIcaoSearch);
  installEventFilterForWidget(ui->lineEditNavNameSearch);
  installEventFilterForWidget(ui->lineEditNavRegionSearch);
  installEventFilterForWidget(ui->lineEditNavAirportIcaoSearch);

  // Distance
  columns->assignDistanceSearchWidgets(ui->checkBoxNavDistSearch,
                                       ui->comboBoxNavDistDirectionSearch,
                                       ui->spinBoxNavDistMinSearch,
                                       ui->spinBoxNavDistMaxSearch);

  // Connect widgets to the controller
  SearchBaseTable::connectSearchWidgets();
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

  connect(controller->getSqlModel(), &SqlModel::overrideMode, this, &NavSearch::overrideMode);
}

void NavSearch::saveState()
{
  atools::gui::WidgetState widgetState(lnm::SEARCHTAB_NAV_WIDGET);
  widgetState.save(navSearchWidgets);

  Ui::MainWindow *ui = NavApp::getMainUi();
  widgetState.save({ui->horizontalLayoutNavDistanceSearch, ui->actionSearchNavaidFollowSelection});
  saveViewState(ui->checkBoxNavDistSearch->isChecked());
}

void NavSearch::restoreState()
{
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH)
  {
    atools::gui::WidgetState widgetState(lnm::SEARCHTAB_NAV_WIDGET);
    widgetState.restore(navSearchWidgets);

    // Need to block signals here to avoid unwanted behavior (will enable
    // distance search and avoid saving of wrong view widget state)
    widgetState.setBlockSignals(true);
    Ui::MainWindow *ui = NavApp::getMainUi();
    widgetState.restore({ui->horizontalLayoutNavDistanceSearch, ui->actionSearchNavaidFollowSelection});
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
  else
    atools::gui::WidgetState(lnm::SEARCHTAB_NAV_VIEW_WIDGET).restore(NavApp::getMainUi()->tableViewNavSearch);
}

void NavSearch::saveViewState(bool distSearchActive)
{
  // Save layout for normal and distance search separately
  atools::gui::WidgetState(
    distSearchActive ? lnm::SEARCHTAB_NAV_VIEW_DIST_WIDGET : lnm::SEARCHTAB_NAV_VIEW_WIDGET
    ).save(NavApp::getMainUi()->tableViewNavSearch);
}

void NavSearch::restoreViewState(bool distSearchActive)
{
  atools::gui::WidgetState(
    distSearchActive ? lnm::SEARCHTAB_NAV_VIEW_DIST_WIDGET : lnm::SEARCHTAB_NAV_VIEW_WIDGET
    ).restore(NavApp::getMainUi()->tableViewNavSearch);
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
  if(col->getColumnName() == "nav_type")
    return map::navName(displayRoleValue.toString());
  else if(col->getColumnName() == "type")
    return map::navTypeName(displayRoleValue.toString());
  else if(col->getColumnName() == "name")
    return atools::capString(displayRoleValue.toString());
  else if(col->getColumnName() == "range" && displayRoleValue.toFloat() > 0.f)
    return Unit::distNm(displayRoleValue.toFloat(), false);
  else if(col->getColumnName() == "altitude")
    return !displayRoleValue.isNull() && displayRoleValue.toFloat() < map::INVALID_ALTITUDE_VALUE ?
           Unit::altFeet(displayRoleValue.toFloat(), false) : QString();
  else if(col->getColumnName() == "frequency" && !displayRoleValue.isNull())
  {
    // VOR and/or DME
    int freq = displayRoleValue.toInt();
    // VOR and DME are scaled up in nav_search to easily differentiate from NDB
    if(freq >= 1000000 && freq <= 1200000)
      return QLocale().toString(static_cast<float>(freq) / 10000.f, 'f', 2);
    else if(freq >= 10000 && freq <= 120000)
      return QLocale().toString(static_cast<float>(freq) / 100.f, 'f', 1);
    else
      return "Invalid";
  }
  else if(col->getColumnName() == "mag_var")
    return map::magvarText(displayRoleValue.toFloat());
  else if(displayRoleValue.type() == QVariant::Int || displayRoleValue.type() == QVariant::UInt)
    return QLocale().toString(displayRoleValue.toInt());
  else if(displayRoleValue.type() == QVariant::LongLong || displayRoleValue.type() == QVariant::ULongLong)
    return QLocale().toString(displayRoleValue.toLongLong());
  else if(displayRoleValue.type() == QVariant::Double)
    return QLocale().toString(displayRoleValue.toDouble());

  return displayRoleValue.toString();
}

void NavSearch::getSelectedMapObjects(map::MapSearchResult& result) const
{
  if(!NavApp::getMainUi()->dockWidgetSearch->isVisible())
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
      map::MapObjectTypes type = map::navTypeToMapObjectType(navType);

      if(type == map::WAYPOINT)
      {
        map::MapWaypoint obj;
        factory.fillWaypointFromNav(rec, obj);
        result.waypoints.append(obj);
      }
      else if(type == map::NDB)
      {
        map::MapNdb obj;
        factory.fillNdb(rec, obj);
        result.ndbs.append(obj);
      }
      else if(type == map::VOR)
      {
        map::MapVor obj;
        factory.fillVorFromNav(rec, obj);
        result.vors.append(obj);
      }
    }
  }
}

void NavSearch::postDatabaseLoad()
{
  SearchBaseTable::postDatabaseLoad();
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
  Ui::MainWindow *ui = NavApp::getMainUi();

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

void NavSearch::updatePushButtons()
{
  QItemSelectionModel *sm = view->selectionModel();
  NavApp::getMainUi()->pushButtonNavSearchClearSelection->setEnabled(sm != nullptr && sm->hasSelection());
}

QAction *NavSearch::followModeAction()
{
  return NavApp::getMainUi()->actionSearchNavaidFollowSelection;
}
