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

#include "search/airportsearch.h"

#include "airporticondelegate.h"
#include "app/navapp.h"
#include "atools.h"
#include "common/constants.h"
#include "common/mapcolors.h"
#include "common/mapresult.h"
#include "common/maptypes.h"
#include "common/maptypesfactory.h"
#include "common/unit.h"
#include "common/unitstringtool.h"
#include "fs/util/fsutil.h"
#include "gui/choicedialog.h"
#include "gui/dialog.h"
#include "gui/mainwindow.h"
#include "gui/widgetstate.h"
#include "gui/widgetutil.h"
#include "query/airportquery.h"
#include "query/mapquery.h"
#include "query/querymanager.h"
#include "route/route.h"
#include "search/column.h"
#include "search/columnlist.h"
#include "search/randomdepartureairportpickingbycriteria.h"
#include "search/sqlcontroller.h"
#include "search/sqlmodel.h"
#include "settings/settings.h"
#include "ui_mainwindow.h"

#include <QProgressDialog>
#include <QStringBuilder>

/* Default values for minimum and maximum random flight plan distance */
const static float FLIGHTPLAN_MIN_DISTANCE_DEFAULT_NM = 0.f;
const static float FLIGHTPLAN_MAX_DISTANCE_DEFAULT_NM = 10800.f;

// Align right and omit if value is 0
const static QSet<QString> AIRPORT_NUMBER_COLUMNS({"num_approach", "num_runway_hard", "num_runway_soft", "num_runway_water",
                                                   "num_runway_light", "num_runway_end_ils", "num_parking_gate",
                                                   "num_parking_ga_ramp", "num_parking_cargo", "num_parking_mil_cargo",
                                                   "num_parking_mil_combat", "num_helipad"});

AirportSearch::AirportSearch(QMainWindow *parent, QTableView *tableView, si::TabSearchId tabWidgetIndex)
  : SearchBaseTable(parent, tableView, new ColumnList("airport", "airport_id"), tabWidgetIndex)
{
  // Have to convert units for these two spin boxes here since they are not registered in the base
  unitStringTool = new UnitStringTool;
  unitStringTool->init({ui->spinBoxAirportFlightplanMinSearch, ui->spinBoxAirportFlightplanMaxSearch});

  /* *INDENT-OFF* */
  ui->pushButtonAirportHelpSearch->setToolTip(
    "<p>All set search conditions have to match.</p>"
    "<p>Search tips for text fields:</p>"
    "<ul>"
      "<li>Default is to search for airports that contain entered text or words in all data fields like ident or city name, for example.</li>"
      "<li>Use &quot;*&quot; as a placeholder for any text.</li>"
      "<li>Use double quotes like &quot;FRA&quot; or &quot;EDDF&quot; to force exact search.</li>"
      "<li>Only fields that search for idents: Enter a space separated list of words or idents to look for more than one airport.</li>"
    "</ul>"
    "<p>Check boxes: </p>"
    "<ul>"
      "<li>Gray means: Condition is ignored.</li>"
      "<li>Checked means: Condition must match.</li>"
      "<li>Unchecked means: Condition must not match.</li>"
    "</ul>" );
  /* *INDENT-ON* */

  // All widgets that will have their state and visibility saved and restored
  airportSearchWidgets =
  {
    ui->horizontalLayoutAirportTextSearch,
    ui->verticalLayoutAirportAdminSearch,
    ui->gridLayoutAirportExtSearch,
    ui->gridLayoutAirportSearchParking,
    ui->gridLayoutAirportSearchRunway,
    ui->horizontalLayoutAirportAltitudeSearch,
    ui->horizontalLayoutAirportScenerySearch,
    ui->horizontalLayoutAirportDistanceSearch,
    ui->actionSearchAirportFollowSelection,

    ui->lineAirportExtSearch,
    ui->lineAirportRunwaySearch,
    ui->lineAirportAltSearch,
    ui->lineAirportDistSearch,
    ui->lineAirportFlightplanSearch,
    ui->lineAirportScenerySearch,

    ui->actionAirportSearchShowAllOptions,
    ui->actionAirportSearchShowAdminOptions,
    ui->actionAirportSearchShowExtOptions,
    ui->actionAirportSearchShowFuelParkOptions,
    ui->actionAirportSearchShowRunwayOptions,
    ui->actionAirportSearchShowAltOptions,
    ui->actionAirportSearchShowDistOptions,
    ui->actionAirportSearchShowFlightplanOptions,
    ui->actionAirportSearchShowSceneryOptions,

    ui->spinBoxAirportFlightplanMaxSearch,
    ui->spinBoxAirportFlightplanMinSearch
  };

  // All drop down menu actions
  airportSearchMenuActions =
  {
    ui->actionAirportSearchShowAllOptions,
    ui->actionAirportSearchShowAdminOptions,
    ui->actionAirportSearchShowExtOptions,
    ui->actionAirportSearchShowFuelParkOptions,
    ui->actionAirportSearchShowRunwayOptions,
    ui->actionAirportSearchShowAltOptions,
    ui->actionAirportSearchShowDistOptions,
    ui->actionAirportSearchShowFlightplanOptions,
    ui->actionAirportSearchShowSceneryOptions
  };

  // set tri state checkboxes to partially checked
  ui->checkBoxAirportMilSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportLightSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportTowerSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportIlsSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportApprSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportClosedSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportAddonSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportJetASearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportAvgasSearch->setCheckState(Qt::PartiallyChecked);

  // Show/hide all search options menu action
  connect(ui->actionAirportSearchShowAllOptions, &QAction::toggled, this, [this](bool state) {
    for(QAction *a: qAsConst(airportSearchMenuActions))
      a->setChecked(state);
  });

  // Build SQL query conditions
  QStringList gateCondMap;
  gateCondMap << QString()
              << "like 'G%'"
              << "in ('GM', 'GH')"
              << "= 'GH'";

  QStringList ratingCondMap;
  ratingCondMap << QString()
                << "rating >= 1"
                << "rating >= 2"
                << "rating >= 3"
                << "rating >= 4"
                << "rating = 5"
                << "is_3d > 0 /*is_3d*/"; // Add required column for this query as comment - will be checked if available

  QStringList rampCondMap;
  rampCondMap << QString()
              << "largest_parking_ramp like 'RGA%'"
              << "largest_parking_ramp in ('RGAM', 'RGAL')"
              << "largest_parking_ramp = 'RGAL'"
              << "num_parking_cargo > 0"
              << "num_parking_mil_cargo > 0"
              << "num_parking_mil_combat > 0";

  QStringList rwSurface;
  rwSurface << QString()
            << "num_runway_hard > 0"
            << "num_runway_soft > 0"
            << "num_runway_water > 0"
            << "num_runway_hard > 0 and num_runway_soft = 0 and num_runway_water = 0"
            << "num_runway_soft > 0 and num_runway_hard = 0 and num_runway_water = 0"
            << "num_runway_water > 0 and num_runway_hard = 0 and num_runway_soft = 0"
            << "num_runway_water = 0 and num_runway_hard = 0 and num_runway_soft = 0";

  QStringList helipadCondMap;
  helipadCondMap << QString()
                 << "num_helipad > 0"
                 << "num_helipad > 0 and num_runway_hard = 0  and "
     "num_runway_soft = 0 and num_runway_water = 0";

  // Default view column descriptors
  // Hidden columns are part of the query and can be used as search criteria but are not shown in the table.
  // Columns that are hidden are also needed to fill MapAirport object and for the icon delegate
  columns->
  append(Column("airport_id").hidden()).
  append(Column("distance", tr("Distance\n%dist%")).distanceCol()).
  append(Column("heading", tr("Heading\n°T")).distanceCol()).

  append(Column("ident", tr("Ident")).defaultSort().filterByBuilder()).
  append(Column("icao", tr("ICAO")).filterByBuilder()).
  append(Column("faa", tr("FAA")).filterByBuilder()).
  append(Column("iata", tr("IATA")).filterByBuilder()).
  append(Column("local", tr("Local\nCode")).filterByBuilder()).

  append(Column("name", ui->lineEditAirportNameSearch, tr("Name")).filterByBuilder()).
  append(Column("city", ui->lineEditAirportCitySearch, tr("City")).filterByBuilder()).
  append(Column("state", ui->lineEditAirportStateSearch, tr("State or\nProvince")).filterByBuilder()).
  append(Column("country", ui->lineEditAirportCountrySearch, tr("Country or\nArea Code")).filterByBuilder()).

  append(Column("rating", ui->comboBoxAirportRatingSearch, tr("Rating")).includesName().indexCondMap(ratingCondMap)).

  append(Column("altitude", tr("Elev.\n%alt%")).convertFunc(Unit::altFeetF)).
  append(Column("mag_var", tr("Mag.\nDecl.°"))).
  append(Column("has_avgas", ui->checkBoxAirportAvgasSearch, tr("Avgas")).hidden()).
  append(Column("has_jetfuel", ui->checkBoxAirportJetASearch, tr("Jetfuel")).hidden()).

  append(Column("tower_frequency", ui->checkBoxAirportTowerSearch, tr("Tower\nMHz")).conditions("is not null", "is null")).

  append(Column("atis_frequency", tr("ATIS\nMHz")).hidden()).
  append(Column("awos_frequency", tr("AWOS\nMHz")).hidden()).
  append(Column("asos_frequency", tr("ASOS\nMHz")).hidden()).
  append(Column("unicom_frequency", tr("UNICOM\nMHz")).hidden()).

  append(Column("is_closed", ui->checkBoxAirportClosedSearch, tr("Closed")).hidden()).
  append(Column("is_military", ui->checkBoxAirportMilSearch, tr("Military")).hidden()).
  append(Column("is_addon", ui->checkBoxAirportAddonSearch, tr("Add-on")).hidden()).
  append(Column("is_3d", tr("3D")).hidden()).

  append(Column("num_runway_soft", ui->comboBoxAirportSurfaceSearch, tr("Soft\nRunways")).includesName().indexCondMap(rwSurface).hidden()).

  append(Column("num_runway_hard", tr("Hard\nRunways")).hidden()).
  append(Column("num_runway_water", tr("Water\nRunways")).hidden()).
  append(Column("num_runway_light", ui->checkBoxAirportLightSearch, tr("Lighted\nRunways")).conditions("> 0", "== 0").hidden())
  .
  append(Column("num_runway_end_ils", ui->checkBoxAirportIlsSearch, tr("ILS")).conditions("> 0", "== 0").hidden()).
  append(Column("num_approach", ui->checkBoxAirportApprSearch, tr("Procedures")).conditions("> 0", "== 0").hidden()).

  append(Column("largest_parking_ramp", ui->comboBoxAirportRampSearch, tr("Largest\nRamp")).includesName().indexCondMap(rampCondMap)).
  append(Column("largest_parking_gate", ui->comboBoxAirportGateSearch, tr("Largest\nGate")).indexCondMap(gateCondMap)).
  append(Column("num_helipad", ui->comboBoxAirportHelipadSearch, tr("Helipads")).includesName().indexCondMap(helipadCondMap).hidden()).

  append(Column("num_parking_gate", tr("Gates")).hidden()).
  append(Column("num_parking_ga_ramp", tr("Ramps\nGA")).hidden()).
  append(Column("num_parking_cargo", tr("Ramps\nCargo")).hidden()).
  append(Column("num_parking_mil_cargo", tr("Ramps\nMil Cargo")).hidden()).
  append(Column("num_parking_mil_combat", tr("Ramps\nMil Combat")).hidden()).

  append(Column("longest_runway_length", tr("Longest\nRunway Length %distshort%")).convertFunc(Unit::distShortFeetF)).
  append(Column("longest_runway_width", tr("Longest\nRunway Width %distshort%")).hidden()).
  append(Column("longest_runway_surface", tr("Longest\nRunway Surface")).hidden()).
  append(Column("longest_runway_heading").hidden()).
  append(Column("num_runway_end_closed").hidden()).

  append(Column("scenery_local_path", ui->lineEditAirportScenerySearch,
                tr("Scenery Paths")).filter(true, ui->actionAirportSearchShowSceneryOptions)).
  append(Column("bgl_filename", ui->lineEditAirportFileSearch,
                tr("Files")).filter(true, ui->actionAirportSearchShowSceneryOptions)).

  append(Column("num_apron").hidden()).
  append(Column("num_taxi_path").hidden()).
  append(Column("has_tower_object").hidden()).
  append(Column("num_runway_end_vasi").hidden()).
  append(Column("num_runway_end_als").hidden()).

  append(Column("tower_lonx").hidden()).
  append(Column("tower_laty").hidden()).

  append(Column("left_lonx").hidden()).
  append(Column("top_laty").hidden()).
  append(Column("right_lonx").hidden()).
  append(Column("bottom_laty").hidden()).

  append(Column("lonx", tr("Longitude")).hidden()).
  append(Column("laty", tr("Latitude")).hidden());

  ui->labelAirportSearchOverride->hide();

  // Add icon delegate for the ident column
  iconDelegate = new AirportIconDelegate(columns);
  view->setItemDelegateForColumn(columns->getColumn("ident")->getIndex(), iconDelegate);

  // Assign the callback which builds a part of the where clause for the airport search ======================
  // First query builder having matching columns in the list is used
  columns->setQueryBuilder(QueryBuilder(std::bind(&SearchBaseTable::queryBuilderFunc, this, std::placeholders::_1), {
    QueryWidget(ui->lineEditAirportTextSearch,
                {"ident", "icao", "iata", "faa", "local", "name", "city", "state", "country"},
                false /* allowOverride */, false /* allowExclude */),
    QueryWidget(ui->lineEditAirportIcaoSearch,
                {"ident", "icao", "iata", "faa", "local"},
                true /* allowOverride */, false /* allowExclude */)}));

  SearchBaseTable::initViewAndController(NavApp::getDatabaseSim());

  // Add model data handler and model format handler as callbacks
  setCallbacks();
}

AirportSearch::~AirportSearch()
{
  delete iconDelegate;
  delete unitStringTool;
}

void AirportSearch::overrideMode(const QStringList& overrideColumnTitles)
{
  if(overrideColumnTitles.isEmpty())
  {
    ui->labelAirportSearchOverride->hide();
    ui->labelAirportSearchOverride->clear();
  }
  else
  {
    ui->labelAirportSearchOverride->show();
    ui->labelAirportSearchOverride->setText(tr("%1 overriding other search options.").
                                            arg(atools::strJoin(overrideColumnTitles, tr(", "), tr(" and "))));
  }
}

void AirportSearch::connectSearchSlots()
{
  SearchBaseTable::connectSearchSlots();

  // Small push buttons on top
  connect(ui->pushButtonAirportSearchClearSelection, &QPushButton::clicked, this, &SearchBaseTable::nothingSelectedTriggered);
  connect(ui->pushButtonAirportSearchReset, &QPushButton::clicked, this, &AirportSearch::resetSearch);

  connect(ui->pushButtonAirportFlightplanSearch, &QPushButton::clicked,
          std::bind(&AirportSearch::randomFlightClicked, this, true /* showDialog */));
  connect(ui->spinBoxAirportFlightplanMinSearch, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &AirportSearch::keepRandomFlightRangeSane);
  connect(ui->spinBoxAirportFlightplanMaxSearch, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &AirportSearch::keepRandomFlightRangeSane);

  installEventFilterForWidget(ui->lineEditAirportTextSearch);
  installEventFilterForWidget(ui->lineEditAirportIcaoSearch);
  installEventFilterForWidget(ui->lineEditAirportCitySearch);
  installEventFilterForWidget(ui->lineEditAirportCountrySearch);
  installEventFilterForWidget(ui->lineEditAirportNameSearch);
  installEventFilterForWidget(ui->lineEditAirportStateSearch);

  // Runways
  columns->assignMinMaxWidget("longest_runway_length",
                              ui->spinBoxAirportRunwaysMinSearch,
                              ui->spinBoxAirportRunwaysMaxSearch);
  // Altitude
  columns->assignMinMaxWidget("altitude",
                              ui->spinBoxAirportAltitudeMinSearch,
                              ui->spinBoxAirportAltitudeMaxSearch);

  // Distance
  columns->assignDistanceSearchWidgets(ui->checkBoxAirportDistSearch,
                                       ui->comboBoxAirportDistDirectionSearch,
                                       ui->spinBoxAirportDistMinSearch,
                                       ui->spinBoxAirportDistMaxSearch);

  // Connect widgets to the controller
  SearchBaseTable::connectSearchWidgets();

  QMenu *menu = new QMenu(ui->toolButtonAirportSearch);
  ui->toolButtonAirportSearch->setMenu(menu);
  menu->addAction(airportSearchMenuActions.first());
  menu->addSeparator();
  menu->addActions(airportSearchMenuActions.mid(1));

  // Drop down menu actions
  connect(ui->actionAirportSearchShowAdminOptions, &QAction::toggled, this, [this](bool state) {
    buttonMenuTriggered(ui->verticalLayoutAirportAdminSearch, ui->lineAirportAdminSearch, state, false /* distanceSearch */);
  });

  connect(ui->actionAirportSearchShowExtOptions, &QAction::toggled, this, [this](bool state) {
    buttonMenuTriggered(ui->gridLayoutAirportExtSearch, ui->lineAirportExtSearch, state, false /* distanceSearch */);
  });

  connect(ui->actionAirportSearchShowFuelParkOptions, &QAction::toggled, this, [this](bool state) {
    buttonMenuTriggered(ui->gridLayoutAirportSearchParking, ui->lineAirportFuelParkSearch, state, false /* distanceSearch */);
  });

  connect(ui->actionAirportSearchShowRunwayOptions, &QAction::toggled, this, [this](bool state) {
    buttonMenuTriggered(ui->gridLayoutAirportSearchRunway, ui->lineAirportRunwaySearch, state, false /* distanceSearch */);
  });

  connect(ui->actionAirportSearchShowAltOptions, &QAction::toggled, this, [this](bool state) {
    buttonMenuTriggered(ui->horizontalLayoutAirportAltitudeSearch, ui->lineAirportAltSearch, state, false /* distanceSearch */);
  });

  connect(ui->actionAirportSearchShowDistOptions, &QAction::toggled, this, [this](bool state) {
    buttonMenuTriggered(ui->horizontalLayoutAirportDistanceSearch, ui->lineAirportDistSearch, state, true /* distanceSearch */);
  });

  connect(ui->actionAirportSearchShowFlightplanOptions, &QAction::toggled, this, [this](bool state) {
    buttonMenuTriggered(ui->horizontalLayoutAirportFlightplanSearch, ui->lineAirportFlightplanSearch, state, false /* distanceSearch */);
  });

  connect(ui->actionAirportSearchShowSceneryOptions, &QAction::toggled, this, [this](bool state) {
    buttonMenuTriggered(ui->horizontalLayoutAirportScenerySearch, ui->lineAirportScenerySearch, state, false /* distanceSearch */);
  });

  connect(controller->getSqlModel(), &SqlModel::overrideMode, this, &AirportSearch::overrideMode);
}

void AirportSearch::saveState()
{
  atools::gui::WidgetState widgetState(lnm::SEARCHTAB_AIRPORT_WIDGET);
  widgetState.save(airportSearchWidgets);
  saveViewState(viewStateDistSearch);
}

void AirportSearch::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::SEARCHTAB_AIRPORT_WIDGET);
  if(OptionData::instance().getFlags().testFlag(opts::STARTUP_LOAD_SEARCH) && !atools::gui::Application::isSafeMode())
  {
    widgetState.restore(airportSearchWidgets);

    QSpinBox *minDistanceWidget = columns->getMinDistanceWidget();
    QSpinBox *maxDistanceWidget = columns->getMaxDistanceWidget();
    minDistanceWidget->setMaximum(maxDistanceWidget->value());
    maxDistanceWidget->setMinimum(minDistanceWidget->value());
  }
  else
  {
    QList<QObject *> objList;
    atools::convertList(objList, airportSearchMenuActions);
    widgetState.restore(objList);
    atools::gui::WidgetState(lnm::SEARCHTAB_AIRPORT_VIEW_WIDGET).restore(ui->tableViewAirportSearch);
  }

  if(!atools::settings::Settings::instance().childGroups().contains("SearchPaneAirport"))
  {
    // ui->actionAirportSearchShowAdminOptions,
    // ui->actionAirportSearchShowExtOptions,
    // ui->actionAirportSearchShowFuelParkOptions,
    // ui->actionAirportSearchShowRunwayOptions,
    // ui->actionAirportSearchShowAltOptions,
    // ui->actionAirportSearchShowDistOptions,
    // ui->actionAirportSearchShowFlightplanOptions,
    // ui->actionAirportSearchShowSceneryOptions

    // Disable the less used search options on a clean installation - default values
    // All actions in the .ui file have to be checked - otherwise no signals are sent
    ui->actionAirportSearchShowAdminOptions->setChecked(false);
    ui->actionAirportSearchShowSceneryOptions->setChecked(false);
    ui->actionAirportSearchShowAltOptions->setChecked(false);
    ui->actionAirportSearchShowFuelParkOptions->setChecked(false);
  }

  // Adapt min/max in spin boxes for random plan search
  keepRandomFlightRangeSane();

  finishRestore();
}

void AirportSearch::saveViewState(bool distanceSearchState)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "distSearchActive" << distanceSearchState;
#endif

  // Save layout for normal and distance search separately
  atools::gui::WidgetState(distanceSearchState ?
                           lnm::SEARCHTAB_AIRPORT_VIEW_DIST_WIDGET :
                           lnm::SEARCHTAB_AIRPORT_VIEW_WIDGET).save(ui->tableViewAirportSearch);
}

void AirportSearch::restoreViewState(bool distanceSearchState)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "distSearchActive" << distanceSearchState;
#endif

  atools::gui::WidgetState(
    distanceSearchState ?
    lnm::SEARCHTAB_AIRPORT_VIEW_DIST_WIDGET :
    lnm::SEARCHTAB_AIRPORT_VIEW_WIDGET).restore(ui->tableViewAirportSearch);
}

/* Callback for the controller. Is called for each table cell and should return a formatted value. */
QVariant AirportSearch::modelDataHandler(int colIndex, int rowIndex, const Column *col,
                                         const QVariant&, const QVariant& displayRoleValue,
                                         Qt::ItemDataRole role) const
{
  switch(role)
  {
    case Qt::DisplayRole:
      return formatModelData(col, displayRoleValue);

    case Qt::TextAlignmentRole:
      if(col->getColumnName() == "rating")
        return Qt::AlignLeft;
      else if(col->getColumnName() == "ident" ||
              displayRoleValue.type() == QVariant::Int || displayRoleValue.type() == QVariant::UInt ||
              displayRoleValue.type() == QVariant::LongLong || displayRoleValue.type() ==
              QVariant::ULongLong ||
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
QString AirportSearch::formatModelData(const Column *col, const QVariant& displayRoleValue) const
{
  // Called directly by the model for export functions
  if(col->getColumnName() == "tower_frequency" || col->getColumnName() == "atis_frequency" ||
     col->getColumnName() == "awos_frequency" || col->getColumnName() == "asos_frequency" ||
     col->getColumnName() == "unicom_frequency")
  {
    if(displayRoleValue.isNull())
      return QString();
    else
      return QLocale().toString(atools::fs::util::roundComFrequency(displayRoleValue.toInt()), 'f', 3);
  }
  else if(col->getColumnName() == "altitude")
    return Unit::altFeet(displayRoleValue.toFloat(), false);
  else if(col->getColumnName() == "longest_runway_length")
    return Unit::distShortFeet(displayRoleValue.toFloat(), false);
  else if(col->getColumnName() == "mag_var")
    return map::magvarText(displayRoleValue.toFloat(), true /* shortText */, false /* degSign */);
  else if(AIRPORT_NUMBER_COLUMNS.contains(col->getColumnName()))
    return displayRoleValue.toInt() > 0 ? displayRoleValue.toString() : QString();
  else if(col->getColumnName() == "longest_runway_surface")
    return map::surfaceName(displayRoleValue.toString());
  else if(col->getColumnName() == "largest_parking_ramp")
    return map::parkingRampName(displayRoleValue.toString());
  else if(col->getColumnName() == "largest_parking_gate")
    return map::parkingGateName(displayRoleValue.toString());
  else if(col->getColumnName() == "rating")
    return atools::ratingString(displayRoleValue.toInt(), 5);
  else if(displayRoleValue.type() == QVariant::Int || displayRoleValue.type() == QVariant::UInt)
    return QLocale().toString(displayRoleValue.toInt());
  else if(displayRoleValue.type() == QVariant::LongLong || displayRoleValue.type() == QVariant::ULongLong)
    return QLocale().toString(displayRoleValue.toLongLong());
  else if(displayRoleValue.type() == QVariant::Double)
    return QLocale().toString(displayRoleValue.toDouble());

  return displayRoleValue.toString();
}

void AirportSearch::getSelectedMapObjects(map::MapResult& result) const
{
  if(!ui->dockWidgetSearch->isVisible())
    return;

  AirportQuery *airportQueryNav = QueryManager::instance()->getQueriesGui()->getAirportQueryNav();

  // Fill the result with incomplete airport objects (only id and lat/lon)
  const QItemSelection& selection = controller->getSelection();
  for(const QItemSelectionRange& rng :  selection)
  {
    for(int row = rng.top(); row <= rng.bottom(); ++row)
    {
      QVariant idVar = controller->getRawData(row, columns->getIdColumnName());
      if(idVar.isValid())
      {
        mapQuery->getMapObjectById(result, map::AIRPORT, map::AIRSPACE_SRC_NONE, idVar.toInt(), false /* airportFromNavDatabase */);

        if(result.hasAirports())
          airportQueryNav->correctAirportProcedureFlag(result.airports.first());
      }
      else
        qWarning() << Q_FUNC_INFO << "Invalid selection: range" << row << "col" << columns->getIdColumnName() << idVar;
    }
  }
}

void AirportSearch::postDatabaseLoad()
{
  SearchBaseTable::postDatabaseLoad();
  setCallbacks();
}

/* Sets controller data formatting callback and desired data roles */
void AirportSearch::setCallbacks()
{
  using namespace std::placeholders;
  controller->setDataCallback(std::bind(&AirportSearch::modelDataHandler, this, _1, _2, _3, _4, _5, _6),
                              {Qt::DisplayRole, Qt::BackgroundRole, Qt::TextAlignmentRole});
}

/* Update the button menu actions. Add * for changed search criteria and toggle show/hide all
 * action depending on other action states */
void AirportSearch::updateButtonMenu()
{
  // List without all button
  const QList<const QAction *> menus =
  {
    ui->actionAirportSearchShowExtOptions,
    ui->actionAirportSearchShowAdminOptions,
    ui->actionAirportSearchShowFuelParkOptions,
    ui->actionAirportSearchShowRunwayOptions,
    ui->actionAirportSearchShowAltOptions,
    ui->actionAirportSearchShowDistOptions,
    ui->actionAirportSearchShowFlightplanOptions,
    ui->actionAirportSearchShowSceneryOptions
  };

  // Change state of show all action
  ui->actionAirportSearchShowAllOptions->blockSignals(true);
  if(atools::gui::util::allChecked(menus))
    ui->actionAirportSearchShowAllOptions->setChecked(true);
  else if(atools::gui::util::noneChecked(menus))
    ui->actionAirportSearchShowAllOptions->setChecked(false);
  else
    ui->actionAirportSearchShowAllOptions->setChecked(false);
  ui->actionAirportSearchShowAllOptions->blockSignals(false);

  // Show star in action for all widgets that are not in default state
  atools::gui::util::changeIndication(ui->actionAirportSearchShowAdminOptions,
                                      atools::gui::util::anyWidgetChanged({ui->verticalLayoutAirportAdminSearch}));

  atools::gui::util::changeIndication(ui->actionAirportSearchShowExtOptions,
                                      atools::gui::util::anyWidgetChanged({ui->gridLayoutAirportExtSearch}));

  atools::gui::util::changeIndication(ui->actionAirportSearchShowFuelParkOptions,
                                      atools::gui::util::anyWidgetChanged({ui->gridLayoutAirportSearchParking}));

  atools::gui::util::changeIndication(ui->actionAirportSearchShowRunwayOptions,
                                      atools::gui::util::anyWidgetChanged({ui->gridLayoutAirportSearchRunway}));

  atools::gui::util::changeIndication(ui->actionAirportSearchShowAltOptions,
                                      atools::gui::util::anyWidgetChanged({ui->horizontalLayoutAirportAltitudeSearch}));

  bool distSearchChanged = false;
  if(columns->isDistanceCheckBoxChecked())
    distSearchChanged = atools::gui::util::anyWidgetChanged({ui->horizontalLayoutAirportDistanceSearch});

  atools::gui::util::changeIndication(ui->actionAirportSearchShowDistOptions, distSearchChanged);

  atools::gui::util::changeIndication(ui->actionAirportSearchShowSceneryOptions,
                                      atools::gui::util::anyWidgetChanged({ui->horizontalLayoutAirportScenerySearch}));
}

void AirportSearch::updatePushButtons()
{
  QItemSelectionModel *sm = view->selectionModel();
  ui->pushButtonAirportSearchClearSelection->setEnabled(sm != nullptr && sm->hasSelection());

  // Need sufficient result set and no distance query
  ui->pushButtonAirportFlightplanSearch->setEnabled(view->model()->rowCount() > 1 && !controller->isDistanceSearch());
}

QAction *AirportSearch::followModeAction()
{
  return ui->actionSearchAirportFollowSelection;
}

void AirportSearch::optionsChanged()
{
  // Update units in this object
  unitStringTool->update();
  SearchBaseTable::optionsChanged();
}

void AirportSearch::resetSearch()
{
  qDebug() << Q_FUNC_INFO;

  // Flight plan search widgets are not registered and need to be changed here
  // Convert NM to user selected display units
  ui->spinBoxAirportFlightplanMinSearch->setValue(static_cast<int>(Unit::distNmF(FLIGHTPLAN_MIN_DISTANCE_DEFAULT_NM)));
  ui->spinBoxAirportFlightplanMaxSearch->setValue(static_cast<int>(Unit::distNmF(FLIGHTPLAN_MAX_DISTANCE_DEFAULT_NM)));

  SearchBaseTable::resetSearch();
}

void AirportSearch::showRandomRouteCalc()
{
  qDebug() << Q_FUNC_INFO;
  ui->actionAirportSearchShowFlightplanOptions->setChecked(true);
}

void AirportSearch::keepRandomFlightRangeSane()
{
  ui->spinBoxAirportFlightplanMaxSearch->setMinimum(ui->spinBoxAirportFlightplanMinSearch->value() +
                                                    ui->spinBoxAirportFlightplanMinSearch->singleStep());
  ui->spinBoxAirportFlightplanMinSearch->setMaximum(ui->spinBoxAirportFlightplanMaxSearch->value() -
                                                    ui->spinBoxAirportFlightplanMaxSearch->singleStep());
}

void AirportSearch::randomFlightClicked(bool showDialog)
{
  if(randomFlightSearchProgress != nullptr) // previous search did not complete yet
    return;

  // Get airports from flight plan =======================================
  const Route& route = NavApp::getRouteConst();
  const map::MapAirport& departureAirport = route.getDepartureAirportLeg().getAirport();
  const map::MapAirport& destinationAirport = route.getDestinationAirportLeg().getAirport();

  int dialogResult = QDialog::Accepted;

  if(showDialog)
  {
    // Keep ids stable since they are used to save state
    // Method called by user click and not "Search again" - show selection dialog
    enum {RANDOM_ALL, RANDOM_FIXED_DEPARTURE, RANDOM_FIXED_DESTINATION, RANDOM_BUTTON_GROUP};

    QString label =
      tr(
        "<p>Create a new flight plan from a destination and departure airport chosen at random from the airport search result table.</p>"
          "<p>Modify the selection criteria to your liking by setting the controls in the airport search result panel to values you like, for example runway length minimum to 1,000 ft.</p>"
            "<p>Add an airport to the flight plan table to use it as a fixed departure or destination.</p>");

    // Adjust label if plan is empty or not valid
    if(!departureAirport.isValid() && !destinationAirport.isValid())
      label +=
        tr("\n\nAdd a departure or destination airport to your current flight plan if you wish to have that fixed instead of random.");

    // Build selection dialog ===========================================================
    atools::gui::ChoiceDialog choiceDialog(mainWindow, QCoreApplication::applicationName() % tr(" - Random Flight"), label,
                                           lnm::SEARCHTAB_AIRPORT_RANDOM, "/RANDOM.html");
    choiceDialog.setHelpOnlineUrl(lnm::helpOnlineUrl);
    choiceDialog.setHelpLanguageOnline(lnm::helpLanguageOnline());

    // Departure and destination random ================================
    choiceDialog.addRadioButton(RANDOM_ALL, RANDOM_BUTTON_GROUP, tr("Let select departure and destination airport\n"
                                                                    "from airport search result table at random."), QString(),
                                true /* checked */);

    if(route.getSizeWithoutAlternates() == 1 && departureAirport.isValid())
    {
      // Current flight plan has only one airport - use as fixed departure or destination ================================
      choiceDialog.addRadioButton(RANDOM_FIXED_DEPARTURE, RANDOM_BUTTON_GROUP,
                                  tr("Use current flight plan's departure airport"
                                     "\n%1\n"
                                     "as fixed departure airport and let select a\n"
                                     "destination airport from airport search result\n"
                                     "table at random.").
                                  arg(map::airportTextShort(departureAirport)));

      choiceDialog.addRadioButton(RANDOM_FIXED_DESTINATION, RANDOM_BUTTON_GROUP,
                                  tr("Use current flight plan's departure airport"
                                     "\n%1\n"
                                     "as fixed destination airport and let select a\n"
                                     "departure airport from airport search result\n"
                                     "table at random.").
                                  arg(map::airportTextShort(destinationAirport)));
    }
    else if(departureAirport.isValid() && destinationAirport.isValid())
    {
      // Current flight plan has valid departure and destination - use either one as fixed ================================
      choiceDialog.addRadioButton(RANDOM_FIXED_DEPARTURE, RANDOM_BUTTON_GROUP,
                                  tr("Keep current flight plan's departure airport"
                                     "\n%1\n"
                                     "as fixed departure airport and let select a\n"
                                     "destination airport from airport search result\n"
                                     "table at random.").
                                  arg(map::airportTextShort(departureAirport)));

      choiceDialog.addRadioButton(RANDOM_FIXED_DESTINATION, RANDOM_BUTTON_GROUP,
                                  tr("Keep current flight plan's destination airport"
                                     "\n%1\n"
                                     "as fixed destination airport and let select a\n"
                                     "departure airport from airport search result\n"
                                     "table at random.").
                                  arg(map::airportTextShort(destinationAirport)));
    }
    else
    {
      // Current flight plan has neither valid departure nor destination use all random and disable radio buttons =======
      choiceDialog.addRadioButton(RANDOM_FIXED_DEPARTURE, RANDOM_BUTTON_GROUP,
                                  tr("Let select only a destination airport from\n"
                                     "airport search result table at random."));

      choiceDialog.addRadioButton(RANDOM_FIXED_DESTINATION, RANDOM_BUTTON_GROUP,
                                  tr("Let select only a departure airport from\n"
                                     "airport search result table at random."));

      choiceDialog.disableWidget(RANDOM_ALL);
      choiceDialog.disableWidget(RANDOM_FIXED_DEPARTURE);
      choiceDialog.disableWidget(RANDOM_FIXED_DESTINATION);
    }

    choiceDialog.addSpacer();
    choiceDialog.restoreState();

    dialogResult = choiceDialog.exec();

    // Remember selection if user clicks "Search again" =====================
    randomFixedDeparture = randomFixedDestination = false;
    if(choiceDialog.isButtonChecked(RANDOM_FIXED_DEPARTURE)) // Button has to be checked and enabled
      randomFixedDeparture = true;
    else if(choiceDialog.isButtonChecked(RANDOM_FIXED_DESTINATION))
      randomFixedDestination = true;
  }

  // Also true if "Search again" was clicked ===============================
  if(dialogResult == QDialog::Accepted)
  {
    // Disable button to avoid multiple clicks
    ui->pushButtonAirportFlightplanSearch->setDisabled(true);

    // Init progress bar, argument maximum 200 equals 3 seconds passed to progress bar 100%,
    // progress called every 15ms, progress bar loops
    randomFlightSearchProgress = new QProgressDialog(tr("Random picking and criteria comparison running..."),
                                                     tr("Abort running"), 0, 200, NavApp::getQMainWidget());
    randomFlightSearchProgress->setValue(randomFlightSearchProgress->minimum());
    randomFlightSearchProgress->setAutoClose(false);
    randomFlightSearchProgress->setWindowModality(Qt::ApplicationModal);
    // Let progress dialog pop up early to block application, allows to avoid waiting cursor
    randomFlightSearchProgress->setMinimumDuration(200); // see https://doc.qt.io/qt-5/qprogressdialog.html#value-prop
                                                         // Issues with Modal include non-showing of the progress bar.

    // Convert user selected display units to meter
    float distanceMinMeter = Unit::rev(ui->spinBoxAirportFlightplanMinSearch->value(), Unit::distMeterF);
    float distanceMaxMeter = Unit::rev(ui->spinBoxAirportFlightplanMaxSearch->value(), Unit::distMeterF);
    // Log minimum and maximum distance from UI
    qDebug() << Q_FUNC_INFO << "random flight, distance min" << distanceMinMeter << "random flight, distance max" << distanceMaxMeter;

    // Fetch data from SQL model
    if(randomSearchAirports.isEmpty())
      controller->getSqlModel()->getFullResultSet(randomSearchAirports);

    // (re)set both to "no predefinition"
    predefinedDeparture = predefinedDestination = -1;
    // now set either one accordingly
    if(randomFixedDeparture)
    {
      // predefine departure
      std::pair<int, atools::geo::Pos> pair = std::make_pair(departureAirport.id, departureAirport.position);
      predefinedDeparture = randomSearchAirports.indexOf(pair);
      if(predefinedDeparture == -1)
      {
        // Append route departure airport id and set index to vector position
        predefinedDeparture = randomSearchAirports.size();
        randomSearchAirports.append(pair);
      }
    }
    else if(randomFixedDestination)
    {
      // predefine destination
      std::pair<int, atools::geo::Pos> pair = std::make_pair(destinationAirport.id, destinationAirport.position);
      predefinedDestination = randomSearchAirports.indexOf(pair);
      if(predefinedDestination == -1)
      {
        // Append route destination airport id and set index to vector position
        predefinedDestination = randomSearchAirports.size();
        randomSearchAirports.append(pair);
      }
    }

    qDebug() << Q_FUNC_INFO << "randomSearchAirports.size()" << randomSearchAirports.size()
             << "predefinedDeparture" << predefinedDeparture << "predefinedDestination" << predefinedDestination;

    RandomDepartureAirportPickingByCriteria::initStatics(&randomSearchAirports,
                                                         &randomUnwantedAirports,
                                                         atools::roundToInt(distanceMinMeter),
                                                         atools::roundToInt(distanceMaxMeter),
                                                         predefinedDeparture > -1 ? predefinedDeparture : predefinedDestination);
    departurePicker = new RandomDepartureAirportPickingByCriteria();
    connect(randomFlightSearchProgress, &QProgressDialog::canceled,
            departurePicker, &RandomDepartureAirportPickingByCriteria::cancellationReceived);
    connect(departurePicker, &RandomDepartureAirportPickingByCriteria::progressing,
            this, &AirportSearch::randomFlightSearchProgressing);
    connect(departurePicker, &RandomDepartureAirportPickingByCriteria::resultReady,
            this, &AirportSearch::dataRandomAirportsReceived);
    departurePicker->start();
  }
}

void AirportSearch::randomFlightSearchProgressing()
{
  if(randomFlightSearchProgress != nullptr)
    randomFlightSearchProgress->setValue((randomFlightSearchProgress->value() + 1) % randomFlightSearchProgress->maximum() + 1);
}

void AirportSearch::dataRandomAirportsReceived(bool isSuccess, int indexDeparture, int indexDestination,
                                               QVector<std::pair<int, atools::geo::Pos> > *data)
{
  randomFlightSearchProgress->hide();

  bool tryAgain = false;

  if(!randomFlightSearchProgress->wasCanceled())
  {
    // early delete, prevent show progressbar timeout is after search result found
    delete randomFlightSearchProgress;

    if(isSuccess)
    {
      // only 1 predefined index should be > -1 if at all
      // if predefinedDeparture > -1 nothing needs to be done
      // if predefinedDestination == -1 nothing needs to be done
      if(predefinedDestination > -1)
      {
        // this is a swap
        indexDeparture -= indexDestination;
        indexDestination += indexDeparture;
        indexDeparture -= indexDestination;
        indexDeparture = -indexDeparture;
      }

      qDebug() << Q_FUNC_INFO << "random flight, index departure: " << indexDeparture
               << ", random flight, index destination: " << indexDestination;

      map::MapAirport airportDeparture, airportDestination;
      QString text;
      if(!data->isEmpty())
      {
        AirportQuery *airportQuery = QueryManager::instance()->getQueriesGui()->getAirportQuerySim();
        airportDeparture = airportQuery->getAirportById(data->at(indexDeparture).first);
        airportDestination = airportQuery->getAirportById(data->at(indexDestination).first);

        text = tr("<p><b>%1</b> to <b>%2</b></p><p>Direct distance: %3</p>").
               arg(map::airportTextShort(airportDeparture, 100 /* elide */)).
               arg(map::airportTextShort(airportDestination, 100 /* elide */)).
               arg(Unit::distMeter(airportDeparture.position.distanceMeterTo(airportDestination.position)));
      }
      else
      {
        qWarning() << Q_FUNC_INFO << "data is empty";
        text = tr("Nothing found");
      }

      QMessageBox box(QMessageBox::Question, tr("Little Navmap - Random flight found:"), text,
                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, NavApp::getQMainWidget());

      // Rename yes and no buttons
      box.setButtonText(QMessageBox::Yes, tr("&Use as Flight Plan"));
      box.setButtonText(QMessageBox::No, tr("&Search again"));
      box.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
      box.setWindowModality(Qt::ApplicationModal);
      box.setTextInteractionFlags(Qt::TextSelectableByMouse);

      int result = box.exec();

      if(airportDeparture.isValid() && airportDestination.isValid())
      {
        if(result == QMessageBox::Yes)
          // Use data
          // Does not show a question dialog if flight plan has changes since this is save to the undo/redo stack
          NavApp::getMainWindow()->routeNewFromAirports(airportDeparture, airportDestination);
        else if(result == QMessageBox::No)
          // Start again in main event loop after leaving this method
          tryAgain = true;
        // else if(result == QMessageBox::Cancel)
        // Nothing to do
      }
    }
    else
      atools::gui::Dialog::information(NavApp::getMainWindow(),
                                       tr("No (further) airports satisfying your criteria\nfound in the airport search result table."));

    // we can delete, due to dialog giving signalling thread
    // breathing room to advance that "1 instruction further"
    // to be truly finished
    delete departurePicker;
  }
  else
  {
    // Do not show any dialogs at all if user canceled.

    delete randomFlightSearchProgress;

    // This will defer to application exit = main event
    // loop end as far as I understood Qt documentation
    departurePicker->deleteLater();
  }

  randomFlightSearchProgress = nullptr;

  if(tryAgain)
  {
    // we don't want this pair of airport id anymore
    randomUnwantedAirports.append(std::make_pair(data->at(indexDeparture).first, data->at(indexDestination).first));

    // Do again but do not show dialog
    QTimer::singleShot(1, this, std::bind(&AirportSearch::randomFlightClicked, this, false /* showDialog */));
    // the documentation doesn't state asynchronity is guaranteed when passing 0
  }
  else
  {
    // Clear data from controller->getSqlModel()->getFullResultSet()
    randomSearchAirports.clear();
    randomUnwantedAirports.clear();

    // Enable button again, don't risk randomFlightClicked called due to user
    // clicking an enabled button just before the "if true"-case does it too
    // when this enabling would occur prior
    ui->pushButtonAirportFlightplanSearch->setDisabled(false);
  }
}
