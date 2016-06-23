/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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
#include "search/controller.h"
#include "gui/mainwindow.h"
#include "search/column.h"
#include "ui_mainwindow.h"
#include "search/columnlist.h"
#include "gui/widgettools.h"
#include "gui/widgetstate.h"
#include "airporticondelegate.h"
#include "common/maptypesfactory.h"
#include "common/mapcolors.h"
#include "atools.h"
#include "sql/sqlrecord.h"

using atools::gui::WidgetTools;

const QSet<QString> AirportSearch::BOOL_COLUMNS({"has_avgas", "has_jetfuel", "has_tower", "is_closed",
                                                 "is_military",
                                                 "is_addon"});
const QSet<QString> AirportSearch::NUMBER_COLUMNS(
  {"num_approach", "num_runway_hard", "num_runway_soft",
   "num_runway_water", "num_runway_light", "num_runway_end_ils",
   "num_parking_gate", "num_parking_ga_ramp", "num_parking_cargo",
   "num_parking_mil_cargo", "num_parking_mil_combat",
   "num_helipad"});

AirportSearch::AirportSearch(MainWindow *parent, QTableView *tableView, ColumnList *columnList,
                             MapQuery *mapQuery, int tabWidgetIndex)
  : Search(parent, tableView, columnList, mapQuery, tabWidgetIndex)
{
  Ui::MainWindow *ui = mainWindow->getUi();

  airportSearchWidgets =
  {
    ui->tableViewAirportSearch,
    ui->horizontalLayoutAirportNameSearch,
    ui->horizontalLayoutAirportNameSearch2,
    ui->gridLayoutAirportExtSearch,
    ui->horizontalLayoutAirportFuelParkSearch,
    ui->horizontalLayoutAirportRunwaySearch,
    ui->horizontalLayoutAirportAltitudeSearch,
    ui->horizontalLayoutAirportDistanceSearch,
    ui->horizontalLayoutAirportScenerySearch,
    ui->lineAirportExtSearch,
    ui->lineAirportRunwaySearch,
    ui->lineAirportAltSearch,
    ui->lineAirportDistSearch,
    ui->lineAirportScenerySearch,
    ui->actionAirportSearchShowAllOptions,
    ui->actionAirportSearchShowExtOptions,
    ui->actionAirportSearchShowFuelParkOptions,
    ui->actionAirportSearchShowRunwayOptions,
    ui->actionAirportSearchShowAltOptions,
    ui->actionAirportSearchShowDistOptions,
    ui->actionAirportSearchShowSceneryOptions
  };

  airportSearchMenuActions =
  {
    ui->actionAirportSearchShowAllOptions,
    ui->actionAirportSearchShowExtOptions,
    ui->actionAirportSearchShowFuelParkOptions,
    ui->actionAirportSearchShowRunwayOptions,
    ui->actionAirportSearchShowAltOptions,
    ui->actionAirportSearchShowDistOptions,
    ui->actionAirportSearchShowSceneryOptions
  };

  ui->checkBoxAirportScenerySearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportMilSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportLightSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportTowerSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportIlsSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportApprSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportClosedSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportAddonSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportJetASearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportAvgasSearch->setCheckState(Qt::PartiallyChecked);

  /* *INDENT-OFF* */
  connect(ui->actionAirportSearchShowAllOptions, &QAction::toggled, [=](bool state)
  {
    for(QAction *a : airportSearchMenuActions)
      a->setChecked(state);
  });
  /* *INDENT-ON* */

  QStringList gateCondMap;
  gateCondMap << QString()
              << "like 'GATE_%'"
              << "in ('GATE_MEDIUM', 'GATE_HEAVY')"
              << "= 'GATE_HEAVY'";

  QStringList rampCondMap;
  rampCondMap << QString()
              << "largest_parking_ramp like 'RAMP_GA_%'"
              << "largest_parking_ramp in ('RAMP_GA_MEDIUM', 'RAMP_GA_LARGE')"
              << "largest_parking_ramp = 'RAMP_GA_LARGE'"
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
            << "num_runway_water > 0 and num_runway_hard = 0 and num_runway_soft = 0";

  QStringList helipadCondMap;
  helipadCondMap << QString()
                 << "num_helipad > 0"
                 << "num_helipad > 0 and num_runway_hard = 0  and "
     "num_runway_soft = 0 and num_runway_water = 0";

  // Default view column descriptors
  columns->
  append(Column("airport_id").hidden()).
  append(Column("distance", tr("Distance\nnm")).distanceCol()).
  append(Column("heading", tr("Heading\n°T")).distanceCol()).
  append(Column("ident", ui->lineEditAirportIcaoSearch, tr("ICAO")).filter().defaultSort()).
  append(Column("name", ui->lineEditAirportNameSearch, tr("Name")).filter()).
  append(Column("city", ui->lineEditAirportCitySearch, tr("City")).filter()).
  append(Column("state", ui->lineEditAirportStateSearch, tr("State")).filter()).
  append(Column("country", ui->lineEditAirportCountrySearch, tr("Country")).filter()).

  append(Column("rating", ui->checkBoxAirportScenerySearch,
                tr("Scenery\nRating")).conditions("> 0", "== 0")).

  append(Column("altitude", tr("Altitude\nft"))).
  append(Column("mag_var", tr("Mag\nVar°"))).
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
  append(Column("is_addon", ui->checkBoxAirportAddonSearch, tr("Addon")).hidden()).

  append(Column("num_runway_soft", ui->comboBoxAirportSurfaceSearch, tr("Soft\nRunways")).
         includesName().indexCondMap(rwSurface).hidden()).

  append(Column("num_runway_hard", tr("Hard\nRunways")).hidden()).
  append(Column("num_runway_water", tr("Water\nRunways")).hidden()).
  append(Column("num_runway_light", ui->checkBoxAirportLightSearch,
                tr("Lighted\nRunways")).conditions("> 0", "== 0").hidden())
  .
  append(Column("num_runway_end_ils", ui->checkBoxAirportIlsSearch, tr("ILS")).
         conditions("> 0", "== 0").hidden()).
  append(Column("num_approach", ui->checkBoxAirportApprSearch, tr("Approaches")).
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

  append(Column("longest_runway_length", tr("Longest\nRunway Length ft"))).
  append(Column("longest_runway_width", tr("Longest\nRunway Width ft")).hidden()).
  append(Column("longest_runway_surface", tr("Longest\nRunway Surface")).hidden()).
  append(Column("longest_runway_heading").hidden()).
  append(Column("num_runway_end_closed").hidden()).

  append(Column("scenery_local_path", ui->lineEditAirportScenerySearch, tr("Scenery")).filter()).
  append(Column("bgl_filename", ui->lineEditAirportFileSearch, tr("File")).filter()).

  append(Column("num_apron").hidden()).
  append(Column("num_taxi_path").hidden()).
  append(Column("has_tower_object").hidden()).
  append(Column("num_runway_end_vasi").hidden()).
  append(Column("num_runway_end_als").hidden()).
  append(Column("num_boundary_fence").hidden()).

  append(Column("tower_lonx").hidden()).
  append(Column("tower_laty").hidden()).

  append(Column("left_lonx").hidden()).
  append(Column("top_laty").hidden()).
  append(Column("right_lonx").hidden()).
  append(Column("bottom_laty").hidden()).

  append(Column("lonx", tr("Longitude")).hidden()).
  append(Column("laty", tr("Latitude")).hidden())
  ;

  iconDelegate = new AirportIconDelegate(columns);
  view->setItemDelegateForColumn(columns->getColumn("ident")->getIndex(), iconDelegate);

  Search::initViewAndController();

  setCallbacks();
}

AirportSearch::~AirportSearch()
{
  delete iconDelegate;
}

void AirportSearch::connectSlots()
{
  Search::connectSlots();

  Ui::MainWindow *ui = mainWindow->getUi();

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
  Search::connectSearchWidgets();
  ui->toolButtonAirportSearch->addActions({ui->actionAirportSearchShowAllOptions,
                                           ui->actionAirportSearchShowExtOptions,
                                           ui->actionAirportSearchShowFuelParkOptions,
                                           ui->actionAirportSearchShowRunwayOptions,
                                           ui->actionAirportSearchShowAltOptions,
                                           ui->actionAirportSearchShowDistOptions,
                                           ui->actionAirportSearchShowSceneryOptions});

  // Drop down menu actions
  using atools::gui::WidgetTools;
  /* *INDENT-OFF* */
  connect(ui->actionAirportSearchShowExtOptions, &QAction::toggled, [=](bool state)
  {
    WidgetTools::showHideLayoutElements({ui->gridLayoutAirportExtSearch}, state, {ui->lineAirportExtSearch});
    updateMenu();
  });

  connect(ui->actionAirportSearchShowFuelParkOptions, &QAction::toggled, [=](bool state)
  {
    WidgetTools::showHideLayoutElements({ui->horizontalLayoutAirportFuelParkSearch}, state, {ui->lineAirportFuelParkSearch});
    updateMenu();
  });

  connect(ui->actionAirportSearchShowRunwayOptions, &QAction::toggled, [=](bool state)
  {
    WidgetTools::showHideLayoutElements({ui->horizontalLayoutAirportRunwaySearch}, state, {ui->lineAirportRunwaySearch});
    updateMenu();
  });

  connect(ui->actionAirportSearchShowAltOptions, &QAction::toggled, [=](bool state)
  {
    WidgetTools::showHideLayoutElements({ui->horizontalLayoutAirportAltitudeSearch}, state, {ui->lineAirportAltSearch});
    updateMenu();
  });

  connect(ui->actionAirportSearchShowDistOptions, &QAction::toggled, [=](bool state)
  {
    WidgetTools::showHideLayoutElements({ui->horizontalLayoutAirportDistanceSearch}, state, {ui->lineAirportDistSearch});
    updateMenu();
  });

  connect(ui->actionAirportSearchShowSceneryOptions, &QAction::toggled, [=](bool state)
  {
    WidgetTools::showHideLayoutElements({ui->horizontalLayoutAirportScenerySearch}, state, {ui->lineAirportScenerySearch});
    updateMenu();
  });
  /* *INDENT-ON* */
}

void AirportSearch::saveState()
{
  atools::gui::WidgetState saver(lnm::SEARCHPANEAIRPORT_WIDGET);
  saver.save(airportSearchWidgets);
}

void AirportSearch::restoreState()
{
  atools::gui::WidgetState saver(lnm::SEARCHPANEAIRPORT_WIDGET);
  saver.restore(airportSearchWidgets);
}

QVariant AirportSearch::modelDataHandler(int colIndex, int rowIndex, const Column *col, const QVariant& value,
                                         const QVariant& dataValue, Qt::ItemDataRole role) const
{
  switch(role)
  {
    case Qt::DisplayRole:
      return modelFormatHandler(col, value, dataValue);

    case Qt::DecorationRole:
      if(BOOL_COLUMNS.contains(col->getColumnName()) && dataValue.toInt() > 0)
        return *boolIcon;

      break;
    case Qt::TextAlignmentRole:
      if(BOOL_COLUMNS.contains(col->getColumnName()) && dataValue.toInt() > 0)
        return Qt::AlignCenter;
      else if(col->getColumnName() == "rating")
        return Qt::AlignLeft;
      else if(col->getColumnName() == "ident" ||
              dataValue.type() == QVariant::Int || dataValue.type() == QVariant::UInt ||
              dataValue.type() == QVariant::LongLong || dataValue.type() == QVariant::ULongLong ||
              dataValue.type() == QVariant::Double)
        return Qt::AlignRight;

      break;
    case Qt::BackgroundRole:
      if(colIndex == controller->getSortColumnIndex())
        return mapcolors::alternatingRowColor(rowIndex, true);

      break;
    case Qt::CheckStateRole:
      // if(boolColumns.contains(col->getColumnName()))
      // return dataValue.toInt() > 0 ? Qt::Checked : Qt::Unchecked;

      break;
    case Qt::ForegroundRole:
    case Qt::EditRole:
    case Qt::ToolTipRole:
    case Qt::StatusTipRole:
    case Qt::WhatsThisRole:
    case Qt::FontRole:
    case Qt::AccessibleTextRole:
    case Qt::AccessibleDescriptionRole:
    case Qt::SizeHintRole:
    case Qt::InitialSortOrderRole:
    case Qt::DisplayPropertyRole:
    case Qt::DecorationPropertyRole:
    case Qt::ToolTipPropertyRole:
    case Qt::StatusTipPropertyRole:
    case Qt::WhatsThisPropertyRole:
    case Qt::UserRole:
      break;
  }

  return QVariant();
}

QString AirportSearch::modelFormatHandler(const Column *col, const QVariant& value,
                                          const QVariant& dataValue) const
{
  // Called directly by the model for export functions
  if(col->getColumnName() == "tower_frequency" || col->getColumnName() == "atis_frequency" ||
     col->getColumnName() == "awos_frequency" || col->getColumnName() == "asos_frequency" ||
     col->getColumnName() == "unicom_frequency")
  {
    if(value.isNull())
      return QString();
    else
      return QLocale().toString(value.toDouble() / 1000, 'f', 3);
  }
  else if(col->getColumnName() == "mag_var")
    return maptypes::magvarText(value.toFloat());
  else if(NUMBER_COLUMNS.contains(col->getColumnName()))
    return dataValue.toInt() > 0 ? dataValue.toString() : QString();
  else if(BOOL_COLUMNS.contains(col->getColumnName()))
    return QString();
  else if(col->getColumnName() == "longest_runway_surface")
    return maptypes::surfaceName(dataValue.toString());
  else if(col->getColumnName() == "largest_parking_ramp")
    return maptypes::parkingRampName(dataValue.toString());
  else if(col->getColumnName() == "largest_parking_gate")
    return maptypes::parkingGateName(dataValue.toString());
  else if(col->getColumnName() == "rating")
    return atools::ratingString(dataValue.toInt(), 5);
  else if(dataValue.type() == QVariant::Int || dataValue.type() == QVariant::UInt)
    return QLocale().toString(dataValue.toInt());
  else if(dataValue.type() == QVariant::LongLong || dataValue.type() == QVariant::ULongLong)
    return QLocale().toString(dataValue.toLongLong());
  else if(dataValue.type() == QVariant::Double)
    return QLocale().toString(dataValue.toDouble());

  return value.toString();
}

void AirportSearch::getSelectedMapObjects(maptypes::MapSearchResult& result) const
{
  const QString idColumnName = columns->getIdColumnName();

  atools::sql::SqlRecord rec;
  rec.appendField(idColumnName, QVariant::Int);
  rec.appendField("lonx", QVariant::Double);
  rec.appendField("laty", QVariant::Double);

  MapTypesFactory factory;

  const QItemSelection& selection = controller->getSelection();
  for(const QItemSelectionRange& rng :  selection)
  {
    for(int row = rng.top(); row <= rng.bottom(); ++row)
    {
      maptypes::MapAirport ap;
      rec.setValue(0, controller->getRawData(row, idColumnName));
      rec.setValue(1, controller->getRawData(row, "lonx"));
      rec.setValue(2, controller->getRawData(row, "laty"));

      // Not fully populated
      factory.fillAirport(rec, ap, false);
      result.airports.append(ap);
    }
  }
}

void AirportSearch::postDatabaseLoad()
{
  Search::postDatabaseLoad();
  setCallbacks();
}

void AirportSearch::setCallbacks()
{
  using namespace std::placeholders;
  controller->setDataCallback(std::bind(&AirportSearch::modelDataHandler, this, _1, _2, _3, _4, _5, _6));
  controller->setFormatCallback(std::bind(&AirportSearch::modelFormatHandler, this, _1, _2, _3));
  controller->setHandlerRoles({Qt::DisplayRole, Qt::BackgroundRole, Qt::TextAlignmentRole, Qt::DecorationRole});
}

void AirportSearch::updateMenu()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  QList<const QAction *> menus =
  {
    ui->actionAirportSearchShowExtOptions,
    ui->actionAirportSearchShowFuelParkOptions,
    ui->actionAirportSearchShowRunwayOptions,
    ui->actionAirportSearchShowAltOptions,
    ui->actionAirportSearchShowDistOptions,
    ui->actionAirportSearchShowSceneryOptions
  };

  ui->actionAirportSearchShowAllOptions->blockSignals(true);
  if(WidgetTools::allChecked(menus))
    ui->actionAirportSearchShowAllOptions->setChecked(true);
  else if(WidgetTools::noneChecked(menus))
    ui->actionAirportSearchShowAllOptions->setChecked(false);
  else
    ui->actionAirportSearchShowAllOptions->setChecked(false);
  ui->actionAirportSearchShowAllOptions->blockSignals(false);

  WidgetTools::changeStarIndication(ui->actionAirportSearchShowExtOptions,
                                    WidgetTools::anyWidgetChanged({ui->gridLayoutAirportExtSearch}));

  WidgetTools::changeStarIndication(ui->actionAirportSearchShowFuelParkOptions,
                                    WidgetTools::anyWidgetChanged({ui->horizontalLayoutAirportFuelParkSearch}));

  WidgetTools::changeStarIndication(ui->actionAirportSearchShowRunwayOptions,
                                    WidgetTools::anyWidgetChanged({ui->horizontalLayoutAirportRunwaySearch}));

  WidgetTools::changeStarIndication(ui->actionAirportSearchShowAltOptions,
                                    WidgetTools::anyWidgetChanged({ui->horizontalLayoutAirportAltitudeSearch}));

  WidgetTools::changeStarIndication(ui->actionAirportSearchShowDistOptions,
                                    WidgetTools::anyWidgetChanged({ui->horizontalLayoutAirportDistanceSearch}));

  WidgetTools::changeStarIndication(ui->actionAirportSearchShowSceneryOptions,
                                    WidgetTools::anyWidgetChanged({ui->horizontalLayoutAirportScenerySearch}));
}
