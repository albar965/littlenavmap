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

#include "search/airportsearch.h"

#include "common/constants.h"
#include "common/unit.h"
#include "common/maptypes.h"
#include "common/unitstringtool.h"
#include "search/sqlcontroller.h"
#include "navapp.h"
#include "search/column.h"
#include "fs/util/fsutil.h"
#include "ui_mainwindow.h"
#include "search/columnlist.h"
#include "gui/widgetutil.h"
#include "gui/widgetstate.h"
#include "airporticondelegate.h"
#include "common/maptypesfactory.h"
#include "common/mapcolors.h"
#include "atools.h"
#include "sql/sqlrecord.h"
#include "settings/settings.h"
#include "query/airportquery.h"
#include "gui/mainwindow.h"

#include <QRandomGenerator>
#include <QtMath>
#include <QMessageBox>

/* Default values for minimum and maximum random flight plan distance */
const static int FLIGHTPLAN_MIN_DISTANCE_DEFAULT = 100;
const static int FLIGHTPLAN_MAX_DISTANCE_DEFAULT = 1000;

// Align right and omit if value is 0
const QSet<QString> AirportSearch::NUMBER_COLUMNS(
  {"num_approach", "num_runway_hard", "num_runway_soft",
   "num_runway_water", "num_runway_light", "num_runway_end_ils",
   "num_parking_gate", "num_parking_ga_ramp", "num_parking_cargo",
   "num_parking_mil_cargo", "num_parking_mil_combat",
   "num_helipad"});

AirportSearch::AirportSearch(QMainWindow *parent, QTableView *tableView, si::TabSearchId tabWidgetIndex)
  : SearchBaseTable(parent, tableView, new ColumnList("airport", "airport_id"), tabWidgetIndex)
{
  // Have to convert units for these two spin boxes here since they are not registered in the base
  Ui::MainWindow *ui = NavApp::getMainUi();
  unitStringTool = new UnitStringTool;
  unitStringTool->init({ui->spinBoxAirportFlightplanMinSearch, ui->spinBoxAirportFlightplanMaxSearch});

  // All widgets that will have their state and visibility saved and restored
  airportSearchWidgets =
  {
    ui->horizontalLayoutAirportNameSearch,
    ui->horizontalLayoutAirportNameSearch2,
    ui->gridLayoutAirportExtSearch,
    ui->horizontalLayoutAirportRatingSearch,
    ui->gridLayoutAirportSearchParking,
    ui->gridLayoutAirportSearchRunway,
    ui->horizontalLayoutAirportAltitudeSearch,
    ui->horizontalLayoutAirportScenerySearch,
    ui->lineAirportExtSearch,
    ui->lineAirportRunwaySearch,
    ui->lineAirportAltSearch,
    ui->lineAirportDistSearch,
    ui->lineAirportFlightplanSearch,
    ui->lineAirportScenerySearch,
    ui->actionAirportSearchShowAllOptions,
    ui->actionAirportSearchShowExtOptions,
    ui->actionAirportSearchShowFuelParkOptions,
    ui->actionAirportSearchShowRunwayOptions,
    ui->actionAirportSearchShowAltOptions,
    ui->actionAirportSearchShowDistOptions,
    ui->actionAirportSearchShowFlightplanOptions,
    ui->actionAirportSearchShowSceneryOptions
  };

  // All drop down menu actions
  airportSearchMenuActions =
  {
    ui->actionAirportSearchShowAllOptions,
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
  connect(ui->actionAirportSearchShowAllOptions, &QAction::toggled, this, [ = ](bool state)
  {
    for(QAction *a: airportSearchMenuActions)
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
  append(Column("ident", tr("Ident")).defaultSort()).
  append(Column("icao", tr("ICAO"))).
  append(Column("faa", tr("FAA"))).
  append(Column("iata", tr("IATA"))).
  append(Column("local", tr("Local\nCode"))).

  append(Column("name", ui->lineEditAirportNameSearch, tr("Name")).filter()).

  append(Column("city", ui->lineEditAirportCitySearch, tr("City")).filter()).
  append(Column("state", ui->lineEditAirportStateSearch, tr("State or\nProvince")).filter()).
  append(Column("country", ui->lineEditAirportCountrySearch, tr("Country or\nArea Code")).filter()).

  append(Column("rating", ui->comboBoxAirportRatingSearch, tr("Rating")).includesName().indexCondMap(ratingCondMap)).

  append(Column("altitude", tr("Elevation\n%alt%")).
         convertFunc(Unit::altFeetF)).
  append(Column("mag_var", tr("Mag.\nDecl.°"))).
  append(Column("has_avgas", ui->checkBoxAirportAvgasSearch, tr("Avgas")).hidden()).
  append(Column("has_jetfuel", ui->checkBoxAirportJetASearch, tr("Jetfuel")).hidden()).

  append(Column("tower_frequency", ui->checkBoxAirportTowerSearch, tr("Tower\nMHz")).
         conditions("is not null", "is null")).

  append(Column("atis_frequency", tr("ATIS\nMHz")).hidden()).
  append(Column("awos_frequency", tr("AWOS\nMHz")).hidden()).
  append(Column("asos_frequency", tr("ASOS\nMHz")).hidden()).
  append(Column("unicom_frequency", tr("UNICOM\nMHz")).hidden()).

  append(Column("is_closed", ui->checkBoxAirportClosedSearch, tr("Closed")).hidden()).
  append(Column("is_military", ui->checkBoxAirportMilSearch, tr("Military")).hidden()).
  append(Column("is_addon", ui->checkBoxAirportAddonSearch, tr("Add-on")).hidden()).
  append(Column("is_3d", tr("3D")).hidden()).

  append(Column("num_runway_soft", ui->comboBoxAirportSurfaceSearch, tr("Soft\nRunways")).
         includesName().indexCondMap(rwSurface).hidden()).

  append(Column("num_runway_hard", tr("Hard\nRunways")).hidden()).
  append(Column("num_runway_water", tr("Water\nRunways")).hidden()).
  append(Column("num_runway_light", ui->checkBoxAirportLightSearch,
                tr("Lighted\nRunways")).conditions("> 0", "== 0").hidden())
  .
  append(Column("num_runway_end_ils", ui->checkBoxAirportIlsSearch, tr("ILS")).
         conditions("> 0", "== 0").hidden()).
  append(Column("num_approach", ui->checkBoxAirportApprSearch, tr("Procedures")).
         conditions("> 0", "== 0").hidden()).

  append(Column("largest_parking_ramp", ui->comboBoxAirportRampSearch, tr("Largest\nRamp")).
         includesName().indexCondMap(rampCondMap)).
  append(Column("largest_parking_gate", ui->comboBoxAirportGateSearch, tr("Largest\nGate")).
         indexCondMap(gateCondMap)).
  append(Column("num_helipad", ui->comboBoxAirportHelipadSearch, tr("Helipads")).
         includesName().indexCondMap(helipadCondMap).hidden()).

  append(Column("num_parking_gate", tr("Gates")).hidden()).
  append(Column("num_parking_ga_ramp", tr("Ramps\nGA")).hidden()).
  append(Column("num_parking_cargo", tr("Ramps\nCargo")).hidden()).
  append(Column("num_parking_mil_cargo", tr("Ramps\nMil Cargo")).hidden()).
  append(Column("num_parking_mil_combat", tr("Ramps\nMil Combat")).hidden()).

  append(Column("longest_runway_length", tr("Longest\nRunway Length %distshort%")).
         convertFunc(Unit::distShortFeetF)).
  append(Column("longest_runway_width", tr("Longest\nRunway Width ft")).hidden()).
  append(Column("longest_runway_surface", tr("Longest\nRunway Surface")).hidden()).
  append(Column("longest_runway_heading").hidden()).
  append(Column("num_runway_end_closed").hidden()).

  append(Column("scenery_local_path", ui->lineEditAirportScenerySearch, tr("Scenery Paths")).filter()).
  append(Column("bgl_filename", ui->lineEditAirportFileSearch, tr("Files")).filter()).

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
  using namespace std::placeholders;
  columns->setQueryBuilder(QueryBuilder(std::bind(&AirportSearch::airportQueryBuilderFunc, this, _1),
                                        ui->lineEditAirportIcaoSearch, {"ident", "icao", "iata", "faa", "local"}));

  SearchBaseTable::initViewAndController(NavApp::getDatabaseSim());

  // Add model data handler and model format handler as callbacks
  setCallbacks();
}

AirportSearch::~AirportSearch()
{
  delete iconDelegate;
  delete unitStringTool;
}

QueryBuilderResult AirportSearch::airportQueryBuilderFunc(QWidget *widget)
{
  if(widget != nullptr)
  {
    // Widget list is always one line edit as registered in airport search
    QLineEdit *lineEdit = dynamic_cast<QLineEdit *>(widget);
    if(lineEdit != nullptr)
    {
      QString text = lineEdit->text().simplified();

      // Check text length without placeholders for override
      QString overrideText(text);
      overrideText.remove(QChar('*'));
      if(text.startsWith('-'))
        overrideText = overrideText.mid(1);
      bool overrideQuery = overrideText.size() >= 3;

      // Adjust the query string to SQL
      // Replace "*" with "%" for SQL
      if(text.contains(QChar('*')))
        text = text.replace(QChar('*'), QChar('%'));
      else if(!text.isEmpty())
        // Default is string starts with text
        text = text + "%";

      // Exclude if prefixed with "-"
      bool exclude = false;
      if(text.startsWith('-'))
      {
        text = text.mid(1);
        exclude = true;
      }

      if(!text.isEmpty())
      {
        QString query;

        if(exclude)
          // Use exclude on ident column only
          query = "(ident not like '" + text + "')";
        else
        {
          // Cannot use "arg" to build string since percent confuses QString
          query = "(ident like '" + text + "'";

          if(controller->hasDatabaseColumn("icao"))
            query += " or icao like '" + text + "'";
          if(controller->hasDatabaseColumn("iata"))
            query += " or iata like '" + text + "'";
          if(controller->hasDatabaseColumn("faa"))
            query += " or faa like '" + text + "'";
          if(controller->hasDatabaseColumn("local"))
            query += " or local like '" + text + "'";
          query += ")";
        }

        return QueryBuilderResult(query, overrideQuery);
      }
    }
  }
  return QueryBuilderResult();
}

void AirportSearch::overrideMode(const QStringList& overrideColumnTitles)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

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
  Ui::MainWindow *ui = NavApp::getMainUi();

  // Small push buttons on top
  connect(ui->pushButtonAirportSearchClearSelection, &QPushButton::clicked,
          this, &SearchBaseTable::nothingSelectedTriggered);
  connect(ui->pushButtonAirportSearchReset, &QPushButton::clicked, this, &AirportSearch::resetSearch);
  connect(ui->pushButtonAirportFlightplanSearch, &QPushButton::clicked, this, &AirportSearch::randomFlightplanClicked);

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
  ui->toolButtonAirportSearch->addActions({ui->actionAirportSearchShowAllOptions,
                                           ui->actionAirportSearchShowExtOptions,
                                           ui->actionAirportSearchShowFuelParkOptions,
                                           ui->actionAirportSearchShowRunwayOptions,
                                           ui->actionAirportSearchShowAltOptions,
                                           ui->actionAirportSearchShowDistOptions,
                                           ui->actionAirportSearchShowFlightplanOptions,
                                           ui->actionAirportSearchShowSceneryOptions});

  // Drop down menu actions
  connect(ui->actionAirportSearchShowExtOptions, &QAction::toggled, this, [ = ](bool state)
  {
    atools::gui::util::showHideLayoutElements({ui->gridLayoutAirportExtSearch}, state,
                                              {ui->lineAirportExtSearch});
    updateButtonMenu();
  });

  connect(ui->actionAirportSearchShowFuelParkOptions, &QAction::toggled, this, [ = ](bool state)
  {
    atools::gui::util::showHideLayoutElements({ui->gridLayoutAirportSearchParking}, state,
                                              {ui->lineAirportFuelParkSearch});
    updateButtonMenu();
  });

  connect(ui->actionAirportSearchShowRunwayOptions, &QAction::toggled, this, [ = ](bool state)
  {
    atools::gui::util::showHideLayoutElements({ui->gridLayoutAirportSearchRunway}, state,
                                              {ui->lineAirportRunwaySearch});
    updateButtonMenu();
  });

  connect(ui->actionAirportSearchShowAltOptions, &QAction::toggled, this, [ = ](bool state)
  {
    atools::gui::util::showHideLayoutElements({ui->horizontalLayoutAirportAltitudeSearch}, state,
                                              {ui->lineAirportAltSearch});
    updateButtonMenu();
  });

  connect(ui->actionAirportSearchShowDistOptions, &QAction::toggled, this, [ = ](bool state)
  {
    atools::gui::util::showHideLayoutElements({ui->horizontalLayoutAirportDistanceSearch}, state,
                                              {ui->lineAirportDistSearch});
    updateButtonMenu();
  });

  connect(ui->actionAirportSearchShowFlightplanOptions, &QAction::toggled, this, [ = ](bool state)
  {
    atools::gui::util::showHideLayoutElements({ui->horizontalLayoutAirportFlightplanSearch}, state,
                                              {ui->lineAirportFlightplanSearch});
    updateButtonMenu();
  });

  connect(ui->actionAirportSearchShowSceneryOptions, &QAction::toggled, this, [ = ](bool state)
  {
    atools::gui::util::showHideLayoutElements({ui->horizontalLayoutAirportScenerySearch}, state,
                                              {ui->lineAirportScenerySearch});
    updateButtonMenu();
  });

  connect(controller->getSqlModel(), &SqlModel::overrideMode, this, &AirportSearch::overrideMode);
}

void AirportSearch::saveState()
{
  atools::gui::WidgetState widgetState(lnm::SEARCHTAB_AIRPORT_WIDGET);
  widgetState.save(airportSearchWidgets);

  Ui::MainWindow *ui = NavApp::getMainUi();
  widgetState.save({ui->horizontalLayoutAirportDistanceSearch, ui->actionSearchAirportFollowSelection,
                    ui->spinBoxAirportFlightplanMinSearch, ui->spinBoxAirportFlightplanMaxSearch});
  saveViewState(ui->checkBoxAirportDistSearch->isChecked());
}

void AirportSearch::restoreState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState widgetState(lnm::SEARCHTAB_AIRPORT_WIDGET);
  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH)
  {
    widgetState.restore(airportSearchWidgets);

    // Need to block signals here to avoid unwanted behavior (will enable
    // distance search and avoid saving of wrong view widget state)
    widgetState.setBlockSignals(true);
    widgetState.restore({ui->horizontalLayoutAirportDistanceSearch, ui->actionSearchAirportFollowSelection,
                         ui->spinBoxAirportFlightplanMinSearch, ui->spinBoxAirportFlightplanMaxSearch});
    restoreViewState(ui->checkBoxAirportDistSearch->isChecked());

    if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_SEARCH)
    {
      bool distSearchChecked = ui->checkBoxAirportDistSearch->isChecked();
      if(distSearchChecked)
        // Activate distance search if it was active - otherwise leave default behavior
        distanceSearchChanged(distSearchChecked, false /* Change view state */);

      QSpinBox *minDistanceWidget = columns->getMinDistanceWidget();
      QSpinBox *maxDistanceWidget = columns->getMaxDistanceWidget();
      minDistanceWidget->setMaximum(maxDistanceWidget->value());
      maxDistanceWidget->setMinimum(minDistanceWidget->value());
    }
  }
  else
  {
    QList<QObject *> objList;
    atools::convertList(objList, airportSearchMenuActions);
    widgetState.restore(objList);

    atools::gui::WidgetState(lnm::SEARCHTAB_AIRPORT_VIEW_WIDGET).restore(NavApp::getMainUi()->tableViewAirportSearch);
  }

  if(!atools::settings::Settings::instance().childGroups().contains("SearchPaneAirport"))
  {
    // Disable the less used search options on a clean installation
    ui->actionAirportSearchShowSceneryOptions->setChecked(false);
    ui->actionAirportSearchShowAltOptions->setChecked(false);
    ui->actionAirportSearchShowFuelParkOptions->setChecked(false);
  }
}

void AirportSearch::saveViewState(bool distSearchActive)
{
  // Save layout for normal and distance search separately
  atools::gui::WidgetState(
    distSearchActive ? lnm::SEARCHTAB_AIRPORT_VIEW_DIST_WIDGET : lnm::SEARCHTAB_AIRPORT_VIEW_WIDGET
    ).save(NavApp::getMainUi()->tableViewAirportSearch);
}

void AirportSearch::restoreViewState(bool distSearchActive)
{
  atools::gui::WidgetState(
    distSearchActive ? lnm::SEARCHTAB_AIRPORT_VIEW_DIST_WIDGET : lnm::SEARCHTAB_AIRPORT_VIEW_WIDGET
    ).restore(NavApp::getMainUi()->tableViewAirportSearch);
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
    return map::magvarText(displayRoleValue.toFloat());
  else if(NUMBER_COLUMNS.contains(col->getColumnName()))
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
  if(!NavApp::getMainUi()->dockWidgetSearch->isVisible())
    return;

  const QString idColumnName = columns->getIdColumnName();

  // Build a SQL record with three fields
  atools::sql::SqlRecord rec;
  rec.appendField(idColumnName, QVariant::Int);
  rec.appendField("lonx", QVariant::Double);
  rec.appendField("laty", QVariant::Double);

  MapTypesFactory factory;

  // Fill the result with incomplete airport objects (only id and lat/lon)
  const QItemSelection& selection = controller->getSelection();
  int range = 0;
  for(const QItemSelectionRange& rng :  selection)
  {
    for(int row = rng.top(); row <= rng.bottom(); ++row)
    {
      map::MapAirport ap;
      QVariant idVar = controller->getRawData(row, idColumnName);
      if(idVar.isValid())
      {
        rec.setValue(0, idVar);
        rec.setValue(1, controller->getRawData(row, "lonx"));
        rec.setValue(2, controller->getRawData(row, "laty"));

#ifdef DEBUG_INFORMATION_SELECTION
        qDebug() << Q_FUNC_INFO << "range" << range << "row" << row << rec;
#endif
        // Not fully populated
        factory.fillAirport(rec, ap, false /* complete */, false /* nav */,
                            NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::XPLANE11);
        result.airports.append(ap);
      }
      else
        qWarning() << Q_FUNC_INFO << "Invalid selection: range" << range
                   << "row" << row << "col" << idColumnName << idVar;
    }
    range++;
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
  Ui::MainWindow *ui = NavApp::getMainUi();
  QList<const QAction *> menus =
  {
    ui->actionAirportSearchShowExtOptions,
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
  atools::gui::util::changeStarIndication(ui->actionAirportSearchShowExtOptions,
                                          atools::gui::util::anyWidgetChanged(
                                            {ui->gridLayoutAirportExtSearch, ui->horizontalLayoutAirportRatingSearch}));
  atools::gui::util::changeStarIndication(ui->actionAirportSearchShowFuelParkOptions,
                                          atools::gui::util::anyWidgetChanged(
                                            {ui->gridLayoutAirportSearchParking}));

  atools::gui::util::changeStarIndication(ui->actionAirportSearchShowRunwayOptions,
                                          atools::gui::util::anyWidgetChanged(
                                            {ui->gridLayoutAirportSearchRunway}));

  atools::gui::util::changeStarIndication(ui->actionAirportSearchShowAltOptions,
                                          atools::gui::util::anyWidgetChanged(
                                            {ui->horizontalLayoutAirportAltitudeSearch}));

  bool distanceSearchChanged = false;
  if(ui->checkBoxAirportDistSearch->isChecked())
    distanceSearchChanged = atools::gui::util::anyWidgetChanged({ui->horizontalLayoutAirportDistanceSearch});

  atools::gui::util::changeStarIndication(ui->actionAirportSearchShowDistOptions, distanceSearchChanged);

  atools::gui::util::changeStarIndication(ui->actionAirportSearchShowSceneryOptions,
                                          atools::gui::util::anyWidgetChanged(
                                            {ui->horizontalLayoutAirportScenerySearch}));
}

void AirportSearch::updatePushButtons()
{
  QItemSelectionModel *sm = view->selectionModel();
  NavApp::getMainUi()->pushButtonAirportSearchClearSelection->setEnabled(sm != nullptr && sm->hasSelection());

  // Need sufficient result set and no distance query
  NavApp::getMainUi()->pushButtonAirportFlightplanSearch->setEnabled(view->model()->rowCount() > 1 &&
                                                                     !controller->isDistanceSearch());
}

QAction *AirportSearch::followModeAction()
{
  return NavApp::getMainUi()->actionSearchAirportFollowSelection;
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
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->spinBoxAirportFlightplanMinSearch->setValue(FLIGHTPLAN_MIN_DISTANCE_DEFAULT);
  ui->spinBoxAirportFlightplanMaxSearch->setValue(FLIGHTPLAN_MAX_DISTANCE_DEFAULT);

  SearchBaseTable::resetSearch();
}

void AirportSearch::randomFlightplanClicked()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  int distanceMin = ui->spinBoxAirportFlightplanMinSearch->value();
  int distanceMax = ui->spinBoxAirportFlightplanMaxSearch->value();

  // Log minimum and maximum distance from UI
  qDebug() << Q_FUNC_INFO << "random flight, distance min: " << distanceMin
           << ", random flight, distance max: " << distanceMax;

  if(distanceMin < distanceMax) {
    // TODO:
    // non-blocking notification: the closer the distance (max - min) is to 0, the longer the random selection might take
    // give estimated search value: ((20500 - (max - min)) / 20500) * 100% of all airports; 20500 is current max of distanceMax
    // TODO:
    // if estimated search value is > 99,5% : ask user if he we wants to let continue ( > 99,5% == < 100 km distance)
    // NOTE:
    // currently no interrupt after time or x attempts
    // NOTE:
    // even searching for 10 km max distance (the lowest possible max distance, potentially taking the most time) from 37000 airports was "instantaneous" (i7-8700K)

    // Fetch data from SQL model
    QVector<std::pair<int, atools::geo::Pos> > result;
    controller->getSqlModel()->getFullResultSet(result);

    const int countResult = result.size();

    qDebug() << Q_FUNC_INFO << "random flight, count source airports: " << countResult;

    const int randomLimit = countResult / 10 * 7;                                 // above this limit do not try to find a random value beacuse this will only have few "space" to "pick" from many already picked

    const std::pair<int, atools::geo::Pos>* data = result.data();

    const double R_earth = 6371;                                                  // km radius Earth
    const double degToRad = M_PI / 180;

    QMap<int, bool> triedIndexDeparture;                                          // acts as a lookup which indices have been tried already; QMap keys are sorted, lookup is very fast

    bool departureSuccess;

    bool noSuccess = true;
    int indexDeparture, indexDestination;

    do
    {
      // split index finding into 2 approaches (if(while) rather than while(if) for performance reason)
      if(triedIndexDeparture.count() < randomLimit)
      {
        // random picking
        // on small result sets, if all are invalid value, we wouldn't switch to the incremented random approach (because the "if" is outside), but being a small result set, this approach might still try every index after some time
        do
        {
          departureSuccess = false;
          if(triedIndexDeparture.count() == countResult)
            goto allDeparturesTried;
          do
          {
            indexDeparture = QRandomGenerator::global()->bounded(countResult);
          }
          while(triedIndexDeparture.contains(indexDeparture));
          triedIndexDeparture.insert(indexDeparture, true);
          departureSuccess = true;
        }
        while(data[indexDeparture].second.getLonX() == atools::geo::Pos::INVALID_VALUE || data[indexDeparture].second.getLatY() == atools::geo::Pos::INVALID_VALUE);
      }
      else {
        // random pick, then increment
        indexDeparture = QRandomGenerator::global()->bounded(countResult);
        do
        {
          departureSuccess = false;
          if(triedIndexDeparture.count() == countResult)
            goto allDeparturesTried;
          while(triedIndexDeparture.contains(indexDeparture))
          {
            indexDeparture = (indexDeparture + 1) % countResult;
          }
          triedIndexDeparture.insert(indexDeparture, true);
          departureSuccess = true;
        }
        while(data[indexDeparture].second.getLonX() == atools::geo::Pos::INVALID_VALUE || data[indexDeparture].second.getLatY() == atools::geo::Pos::INVALID_VALUE);
      }

      QMap<int, bool> triedIndexDestination;                                      // acts as a lookup which indices have been tried already; QMap keys are sorted, lookup is very fast

      triedIndexDestination.insert(indexDeparture, true);                         // destination shall != departure

      bool destinationSuccess;

      const double lon1 = data[indexDeparture].second.getLonX() * degToRad;
      const double lat1 = data[indexDeparture].second.getLatY() * degToRad;
      double dist;

      do
      {
        destinationSuccess = false;
        float lonX, latY;
        do
        {
          if(triedIndexDestination.count() == countResult)
            goto destinationsEnd;                                                 // all destinations have been depleted, try a different departure
          if(triedIndexDestination.count() < randomLimit)                         // the "if" is inside the "while" for the destination because the indices get used up during "picking"
          {
            do
            {
              indexDestination = QRandomGenerator::global()->bounded(countResult);
            }
            while(triedIndexDestination.contains(indexDestination));
          }
          else
          {
            indexDestination = QRandomGenerator::global()->bounded(countResult);
            while(triedIndexDestination.contains(indexDestination))
            {
              indexDestination = (indexDestination + 1) % countResult;
            }
          }
          triedIndexDestination.insert(indexDestination, true);
          lonX = data[indexDestination].second.getLonX();
          latY = data[indexDestination].second.getLatY();
        }
        while(lonX == atools::geo::Pos::INVALID_VALUE || latY == atools::geo::Pos::INVALID_VALUE);
        const double lon2 = lonX * degToRad;
        const double lat2 = latY * degToRad;
        dist = R_earth * qAcos(qSin(lat1) * qSin(lat2) + qCos(lat1) * qCos(lat2) * qCos(lon2 - lon1));     // http://www.movable-type.co.uk/scripts/latlong.html Spherical Law of Cosines
        destinationSuccess = true;
      }
      while(dist < distanceMin || dist > distanceMax);
destinationsEnd:
      if(destinationSuccess)                                                      // the last triedIndexDestination might be taken but it might have passed the last condition
      {
        noSuccess = false;
        continue;
      }
    }
    while(noSuccess);
allDeparturesTried:
    if(departureSuccess)                                                          // the last triedIndexDeparture might be taken but it might have passed the last condition, destinationSuccess must be true if this "if" is reached
    {
      qDebug() << Q_FUNC_INFO << "random flight, index departure: " << indexDeparture
               << ", random flight, index destination: " << indexDestination;

      map::MapAirport airportDeparture;
      map::MapAirport airportDestination;

      AirportQuery *airportQuery = NavApp::getAirportQuerySim();

      airportQuery->getAirportById(airportDeparture, data[indexDeparture].first);
      airportQuery->getAirportById(airportDestination, data[indexDestination].first);

      NavApp::getMainWindow()->routeNewFromAirports(airportDeparture, airportDestination);
    }
    else
    {
      QMessageBox msgBox;
      msgBox.setText(tr("No airports found in the search result satisfying the criteria."));
      msgBox.exec();
    }
  }
  else
  {
    QMessageBox msgBox;
    msgBox.setText(tr("Minimum distance is not smaller than maximum distance!"));
    msgBox.exec();
  }
}
