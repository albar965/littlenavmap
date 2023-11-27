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

#include "search/navsearch.h"

#include "atools.h"
#include "common/constants.h"
#include "common/mapcolors.h"
#include "common/mapflags.h"
#include "common/mapresult.h"
#include "common/maptypes.h"
#include "common/maptypesfactory.h"
#include "common/unit.h"
#include "gui/widgetstate.h"
#include "gui/widgetutil.h"
#include "app/navapp.h"
#include "search/column.h"
#include "search/columnlist.h"
#include "search/navicondelegate.h"
#include "search/sqlcontroller.h"
#include "settings/settings.h"
#include "sql/sqlrecord.h"
#include "ui_mainwindow.h"

#include <QStringBuilder>

NavSearch::NavSearch(QMainWindow *parent, QTableView *tableView, si::TabSearchId tabWidgetIndex)
  : SearchBaseTable(parent, tableView, new ColumnList("nav_search", "nav_search_id"), tabWidgetIndex)
{
  /* *INDENT-OFF* */
  ui->pushButtonNavHelpSearch->setToolTip(
    "<p>All set search conditions have to match.</p>"
    "<p>Search tips for text fields: </p>"
    "<ul>"
      "<li>Default is search for navaids that contain the entered text.</li>"
      "<li>Use &quot;*&quot; as a placeholder for any text. </li>"
      "<li>Prefix with &quot;-&quot; as first character to negate search.</li>"
      "<li>Only ident field: Use double quotes like &quot;TAU&quot; or &quot;BOMBI&quot; to force exact search.</li>"
      "<li>Only ident field: Enter a space separated list of idents to look for more than one navaid.</li>"
    "</ul>");
  /* *INDENT-ON* */


  // All widgets that will have their state and visibility saved and restored
  navSearchWidgets =
  {
    ui->horizontalLayoutNavNameSearch,
    ui->gridLayoutNavSearchType,
    ui->horizontalLayoutNavScenerySearch,
    ui->horizontalLayoutNavDistanceSearch,

    ui->actionSearchNavaidFollowSelection,

    ui->lineNavDistanceSearch,
    ui->lineNavScenerySearch,
    ui->actionNavSearchShowAllOptions,
    ui->actionNavSearchShowTypeOptions,
    ui->actionNavSearchShowDistOptions,
    ui->actionNavSearchShowSceneryOptions
  };

  // All drop down menu actions
  navSearchMenuActions =
  {
    ui->actionNavSearchShowAllOptions,
    ui->actionNavSearchShowTypeOptions,
    ui->actionNavSearchShowDistOptions,
    ui->actionNavSearchShowSceneryOptions
  };

  // Show/hide all search options menu action
  connect(ui->actionNavSearchShowAllOptions, &QAction::toggled, this, [this](bool state)
  {
    for(QAction *a: qAsConst(navSearchMenuActions))
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
              << "type = 'VH'"  // VOR/VORTAC - High
              << "type = 'VL'"  // VOR/VORTAC - Low
              << "type = 'VT'"  // VOR/VORTAC - Terminal
              << "type = 'NHH'" // NDB - HH
              << "type = 'NH'"  // NDB - H
              << "type = 'NMH'" // NDB - MH
              << "type = 'NCP'" // NDB - Compass Locator
              << "type = 'V'"   // Waypoint - VOR
              << "type = 'N'"  // Waypoint - NDB
              << "type in ('WN', 'WU', 'FAF', 'IAF', 'VFR', 'RNAV', 'OA')"; // Waypoint - Other

  QStringList navTypeCondMap;
  navTypeCondMap << QString()
                 << "(nav_type like ('V%') or nav_type in ('D', 'TC'))"      // All VOR/VORTAC/TACAN
                 << "(nav_type like ('V%') or nav_type in ('D', 'TC', 'N'))" // All VOR/VORTAC/TACAN/NDB
                 << "nav_type = 'VD'"                                        // Only VOR-DME
                 << "nav_type = 'V'"                                         // Only VOR
                 << "nav_type = 'D'"                                         // Only DME
                 << "nav_type = 'VT'"                                        // Only VORTAC
                 << "nav_type in ('TC', 'TCD')"                              // Only TACAN
                 << "nav_type = 'N'"                                         // All NDB
                 << "nav_type = 'W'"                                         // All Waypoints
                 << "nav_type = 'W' and "                                    // All Waypoints on Airways
     "(waypoint_num_victor_airway > 0 or waypoint_num_jet_airway > 0)";

  // Default view column descriptors
  // Hidden columns are part of the query and can be used as search criteria but are not shown in the table
  // Columns that are hidden are also needed to fill MapAirport object and for the icon delegate
  columns->
  append(Column("nav_search_id").hidden()).
  append(Column("distance", tr("Distance\n%dist%")).distanceCol()).
  append(Column("heading", tr("Heading\n°T")).distanceCol()).
  append(Column("ident", tr("Ident")).filter().defaultSort().filterByBuilder()).

  append(Column("nav_type", ui->comboBoxNavNavAidSearch, tr("Navaid\nType")).
         indexCondMap(navTypeCondMap).includesName()).

  append(Column("type", ui->comboBoxNavTypeSearch, tr("Type")).indexCondMap(typeCondMap).includesName()).
  append(Column("name", ui->lineEditNavNameSearch, tr("Name")).filter()).
  append(Column("region", ui->lineEditNavRegionSearch, tr("Region")).filter()).
  append(Column("airport_ident", ui->lineEditNavAirportIcaoSearch, tr("Airport\nIdent")).filter()).
  append(Column("frequency", tr("Frequency\nkHz/MHz"))).
  append(Column("channel", tr("Channel"))).
  append(Column("range", ui->spinBoxNavMaxRangeSearch, tr("Range\n%dist%")).
         filter().condition(">").convertFunc(Unit::distNmF)).
  append(Column("mag_var", tr("Mag.\nDecl.°"))).
  append(Column("altitude", tr("Elevation\n%alt%")).convertFunc(Unit::altFeetF)).
  append(Column("scenery_local_path", ui->lineEditNavScenerySearch,
                tr("Scenery Path")).filter(true, ui->actionNavSearchShowSceneryOptions)).
  append(Column("bgl_filename", ui->lineEditNavFileSearch,
                tr("File")).filter(true, ui->actionNavSearchShowSceneryOptions)).
  append(Column("waypoint_num_victor_airway").hidden()).
  append(Column("waypoint_num_jet_airway").hidden()).
  append(Column("vor_id").hidden()).
  append(Column("ndb_id").hidden()).
  append(Column("waypoint_id").hidden()).
  append(Column("lonx").hidden()).
  append(Column("laty").hidden())
  ;

  // No override mode used here
  ui->labelNavSearchOverride->hide();

  // Add icon delegate for the ident column
  iconDelegate = new NavIconDelegate(columns);
  view->setItemDelegateForColumn(columns->getColumn("ident")->getIndex(), iconDelegate);

  // Assign the callback which builds a part of the where clause for the airport search ======================
  columns->setQueryBuilder(QueryBuilder(std::bind(&SearchBaseTable::queryBuilderFunc, this, std::placeholders::_1),
                                        {QueryWidget(ui->lineEditNavIcaoSearch, {"ident"},
                                                     false /* allowOverride */, false /* allowExclude */)}));

  SearchBaseTable::initViewAndController(NavApp::getDatabaseNav());

  // Add model data handler and model format handler as callbacks
  setCallbacks();
}

NavSearch::~NavSearch()
{
  delete iconDelegate;
}

void NavSearch::connectSearchSlots()
{
  SearchBaseTable::connectSearchSlots();

  // Small push buttons on top
  connect(ui->pushButtonNavSearchClearSelection, &QPushButton::clicked, this, &SearchBaseTable::nothingSelectedTriggered);
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
  ui->toolButtonNavSearch->addActions(navSearchMenuActions);

  QMenu *menu = new QMenu(ui->toolButtonNavSearch);
  ui->toolButtonNavSearch->setMenu(menu);
  menu->addAction(navSearchMenuActions.first());
  menu->addSeparator();
  menu->addActions(navSearchMenuActions.mid(1));

  // Drop down menu actions
  connect(ui->actionNavSearchShowTypeOptions, &QAction::toggled, this, [this](bool state) {
    buttonMenuTriggered(ui->gridLayoutNavSearchType, ui->lineNavTypeSearch, state, false /* distanceSearch */);
  });

  connect(ui->actionNavSearchShowDistOptions, &QAction::toggled, this, [this](bool state) {
    buttonMenuTriggered(ui->horizontalLayoutNavDistanceSearch, ui->lineNavDistanceSearch, state, true /* distanceSearch */);
  });

  connect(ui->actionNavSearchShowSceneryOptions, &QAction::toggled, this, [this](bool state) {
    buttonMenuTriggered(ui->horizontalLayoutNavScenerySearch, ui->lineNavScenerySearch, state, false /* distanceSearch */);
  });
}

void NavSearch::saveState()
{
  atools::gui::WidgetState widgetState(lnm::SEARCHTAB_NAV_WIDGET);
  widgetState.save(navSearchWidgets);
  saveViewState(viewStateDistSearch);
}

void NavSearch::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::SEARCHTAB_NAV_WIDGET);
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH && !NavApp::isSafeMode())
  {
    widgetState.restore(navSearchWidgets);

    QSpinBox *minDistanceWidget = columns->getMinDistanceWidget();
    QSpinBox *maxDistanceWidget = columns->getMaxDistanceWidget();
    minDistanceWidget->setMaximum(maxDistanceWidget->value());
    maxDistanceWidget->setMinimum(minDistanceWidget->value());
  }
  else
  {
    QList<QObject *> objList;
    atools::convertList(objList, navSearchMenuActions);
    widgetState.restore(objList);

    atools::gui::WidgetState(lnm::SEARCHTAB_NAV_VIEW_WIDGET).restore(ui->tableViewNavSearch);
  }

  if(!atools::settings::Settings::instance().childGroups().contains("SearchPaneNav"))
  {
    // Disable the less used search options on a clean installation
    ui->actionNavSearchShowTypeOptions->setChecked(false);
    ui->actionNavSearchShowDistOptions->setChecked(false);
    ui->actionNavSearchShowSceneryOptions->setChecked(false);
  }

  finishRestore();
}

void NavSearch::saveViewState(bool distanceSearchState)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "distSearchActive" << distanceSearchState;
#endif

  // Save layout for normal and distance search separately
  atools::gui::WidgetState(distanceSearchState ? lnm::SEARCHTAB_NAV_VIEW_DIST_WIDGET : lnm::SEARCHTAB_NAV_VIEW_WIDGET).save(
    ui->tableViewNavSearch);
}

void NavSearch::restoreViewState(bool distanceSearchState)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "distSearchActive" << distanceSearchState;
#endif

  atools::gui::WidgetState(distanceSearchState ? lnm::SEARCHTAB_NAV_VIEW_DIST_WIDGET : lnm::SEARCHTAB_NAV_VIEW_WIDGET).restore(
    ui->tableViewNavSearch);
}

/* Callback for the controller. Will be called for each table cell and should return a formatted value */
QVariant NavSearch::modelDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant&,
                                     const QVariant& displayRoleValue, Qt::ItemDataRole role) const
{
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

void NavSearch::getSelectedMapObjects(map::MapResult& result) const
{
  if(!ui->dockWidgetSearch->isVisible())
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
      map::MapTypes type = map::navTypeToMapType(navType);

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
  // List without all button
  const QList<const QAction *> menus =
  {
    ui->actionNavSearchShowTypeOptions,
    ui->actionNavSearchShowDistOptions,
    ui->actionNavSearchShowSceneryOptions
  };

  // Change state of show all action
  ui->actionNavSearchShowAllOptions->blockSignals(true);
  if(atools::gui::util::allChecked(menus))
    ui->actionNavSearchShowAllOptions->setChecked(true);
  else if(atools::gui::util::noneChecked(menus))
    ui->actionNavSearchShowAllOptions->setChecked(false);
  else
    ui->actionNavSearchShowAllOptions->setChecked(false);
  ui->actionNavSearchShowAllOptions->blockSignals(false);

  // Show star in action for all widgets that are not in default state
  bool distanceSearchChanged = false;
  if(columns->isDistanceCheckBoxChecked())
    distanceSearchChanged = atools::gui::util::anyWidgetChanged({ui->horizontalLayoutNavDistanceSearch});

  atools::gui::util::changeIndication(ui->actionNavSearchShowDistOptions, distanceSearchChanged);

  atools::gui::util::changeIndication(ui->actionNavSearchShowTypeOptions,
                                      atools::gui::util::anyWidgetChanged({ui->gridLayoutNavSearchType}));

  atools::gui::util::changeIndication(ui->actionNavSearchShowSceneryOptions,
                                      atools::gui::util::anyWidgetChanged({ui->horizontalLayoutNavScenerySearch}));

  if(controller->isRestoreFinished())
    controller->rebuildQuery();
}

void NavSearch::updatePushButtons()
{
  QItemSelectionModel *sm = view->selectionModel();
  ui->pushButtonNavSearchClearSelection->setEnabled(sm != nullptr && sm->hasSelection());
}

QAction *NavSearch::followModeAction()
{
  return ui->actionSearchNavaidFollowSelection;
}
