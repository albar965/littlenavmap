#include "searchpanelist.h"
#include "gui/mainwindow.h"
#include "table/searchpane.h"
#include "column.h"
#include "columnlist.h"
#include "ui_mainwindow.h"

#include <gui/widgetstate.h>

SearchPaneList::SearchPaneList(MainWindow *parent, atools::sql::SqlDatabase *sqlDb)
  : QObject(parent), db(sqlDb), parentWidget(parent)
{
  // Avoid stealing of Ctrl-C from other default menus
  parentWidget->getUi()->actionTableCopy->setShortcutContext(Qt::WidgetWithChildrenShortcut);
}

SearchPaneList::~SearchPaneList()
{
  delete airportSearchPane;
  delete airportColumns;
}

void SearchPaneList::saveState()
{
  Ui::MainWindow *ui = parentWidget->getUi();
  atools::gui::WidgetState saver("SearchPaneAirport/Widget");
  saver.save({ui->horizontalLayoutAirportNameSearch,
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
              ui->actionAirportSearchShowSceneryOptions});
}

void SearchPaneList::restoreState()
{
  Ui::MainWindow *ui = parentWidget->getUi();
  atools::gui::WidgetState saver("SearchPaneAirport/Widget");
  saver.restore({ui->horizontalLayoutAirportNameSearch,
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
                 ui->actionAirportSearchShowSceneryOptions});
}

SearchPane *SearchPaneList::getAirportSearchPane() const
{
  return airportSearchPane;
}

void SearchPaneList::createAirportSearch()
{
  Ui::MainWindow *ui = parentWidget->getUi();

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

  ui->toolButtonAirportSearch->addActions({ui->actionAirportSearchShowAllOptions,
                                           ui->actionAirportSearchShowExtOptions,
                                           ui->actionAirportSearchShowFuelParkOptions,
                                           ui->actionAirportSearchShowRunwayOptions,
                                           ui->actionAirportSearchShowAltOptions,
                                           ui->actionAirportSearchShowDistOptions,
                                           ui->actionAirportSearchShowSceneryOptions});

  /* *INDENT-OFF* */
  connect(ui->actionAirportSearchShowAllOptions, &QAction::toggled,
          [=](bool state) {
    ui->actionAirportSearchShowExtOptions->setChecked(state);
    ui->actionAirportSearchShowFuelParkOptions->setChecked(state);
    ui->actionAirportSearchShowRunwayOptions->setChecked(state);
    ui->actionAirportSearchShowAltOptions->setChecked(state);
    ui->actionAirportSearchShowDistOptions->setChecked(state);
    ui->actionAirportSearchShowSceneryOptions->setChecked(state);
  });
  /* *INDENT-ON* */

  airportColumns = new ColumnList("airport");

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

  // Default view column descriptors
  airportColumns->append(Column("airport_id", tr("ID")).hidden().defaultCol()).
  append(Column("ident", tr("ICAO")).canSort().canFilter().defaultCol().defaultSort()).
  append(Column("name", tr("Name")).canSort().canFilter().defaultCol()).
  append(Column("city", tr("City")).canSort().canFilter().defaultCol()).
  append(Column("state", tr("State")).canSort().canFilter().defaultCol()).
  append(Column("country", tr("Country")).canSort().canFilter().defaultCol()).

  append(Column("rating", tr("Scenery\nRating")).canSort().defaultCol().
         cond("> 0", "== 0")).

  append(Column("altitude", tr("Altitude")).canSort().defaultCol()).
  append(Column("has_avgas", tr("Avgas")).canSort().defaultCol()).
  append(Column("has_jetfuel", tr("Jetfuel")).canSort().defaultCol()).
  append(Column("has_tower", tr("Tower")).canSort().defaultCol()).
  append(Column("is_closed", tr("Closed")).canSort().defaultCol()).
  append(Column("is_military", tr("Military")).canSort().defaultCol()).
  append(Column("is_addon", tr("Addon")).canSort().defaultCol()).

  append(Column("num_runway_soft", tr("Soft\nRunways")).
         canSort().defaultCol().includesColName().indexCondMap(rwSurface)).

  append(Column("num_runway_hard", tr("Hard\nRunways")).canSort().defaultCol()).
  append(Column("num_runway_water", tr("Water\nRunways")).canSort().defaultCol()).

  append(Column("num_runway_light", tr("Lights")).canSort().defaultCol().
         cond("> 0", "== 0")).

  append(Column("num_runway_end_ils", tr("ILS")).canSort().defaultCol().
         cond("> 0", "== 0")).

  append(Column("num_approach", tr("Approach")).canSort().defaultCol().
         cond("> 0", "== 0")).

  append(Column("largest_parking_ramp", tr("Largest\nRamp")).
         canSort().defaultCol().includesColName().indexCondMap(rampCondMap)).

  append(Column("largest_parking_gate", tr("Largest\nGate")).
         canSort().defaultCol().indexCondMap(gateCondMap)).

  append(Column("num_parking_cargo", tr("Cargo Ramps")).canSort().defaultCol()).
  append(Column("num_parking_mil_cargo", tr("Mil Cargo")).canSort().defaultCol()).
  append(Column("num_parking_mil_combat", tr("Mil Combat")).canSort().defaultCol()).
  append(Column("longest_runway_length", tr("Longest\nRunway Length")).canSort().defaultCol()).
  append(Column("longest_runway_width", tr("Longest\nRunway Width")).canSort().defaultCol()).
  append(Column("longest_runway_surface", tr("Longest\nRunway Surface")).canSort().defaultCol()).
  append(Column("scenery_local_path", tr("Scenery")).canSort().canFilter().defaultCol()).
  append(Column("bgl_filename", tr("File")).canSort().canFilter().defaultCol());

  airportSearchPane = new SearchPane(parentWidget, parentWidget->getUi()->tableViewAirportSearch,
                                     airportColumns, db);

  airportSearchPane->addSearchWidget("ident", ui->lineEditAirportIcaoSearch);
  airportSearchPane->addSearchWidget("name", ui->lineEditAirportNameSearch);
  airportSearchPane->addSearchWidget("city", ui->lineEditAirportCitySearch);
  airportSearchPane->addSearchWidget("state", ui->lineEditAirportStateSearch);
  airportSearchPane->addSearchWidget("country", ui->lineEditAirportCountrySearch);

  // Extended
  airportSearchPane->addSearchWidget("rating", ui->checkBoxAirportScenerySearch);
  airportSearchPane->addSearchWidget("is_military", ui->checkBoxAirportMilSearch);
  airportSearchPane->addSearchWidget("num_runway_light", ui->checkBoxAirportLightSearch);
  airportSearchPane->addSearchWidget("has_tower", ui->checkBoxAirportTowerSearch);
  airportSearchPane->addSearchWidget("num_runway_end_ils", ui->checkBoxAirportIlsSearch);
  airportSearchPane->addSearchWidget("num_approach", ui->checkBoxAirportApprSearch);
  airportSearchPane->addSearchWidget("is_closed", ui->checkBoxAirportClosedSearch);
  airportSearchPane->addSearchWidget("is_addon", ui->checkBoxAirportAddonSearch);
  airportSearchPane->addSearchWidget("has_jetfuel", ui->checkBoxAirportJetASearch);
  airportSearchPane->addSearchWidget("has_avgas", ui->checkBoxAirportAvgasSearch);

  // Parking
  airportSearchPane->addSearchWidget("largest_parking_gate", ui->comboBoxAirportGateSearch);
  airportSearchPane->addSearchWidget("largest_parking_ramp", ui->comboBoxAirportRampSearch);

  // Runways
  airportSearchPane->addSearchWidget("num_runway_soft", ui->comboBoxAirportSurfaceSearch);
  airportSearchPane->addMinMaxSearchWidget("longest_runway_length",
                                           ui->spinBoxAirportRunwaysMinSearch,
                                           ui->spinBoxAirportRunwaysMaxSearch);
  // Altitude
  airportSearchPane->addMinMaxSearchWidget("altitude",
                                           ui->spinBoxAirportAltitudeMinSearch,
                                           ui->spinBoxAirportAltitudeMaxSearch);

  // Scenery
  airportSearchPane->addSearchWidget("scenery_local_path", ui->lineEditAirportScenerySearch);
  airportSearchPane->addSearchWidget("bgl_filename", ui->lineEditAirportFileSearch);

  // Connect widgets to the controller
  airportSearchPane->connectSearchWidgets();

  // Drop down menu actions
  /* *INDENT-OFF* */
  connect(ui->actionAirportSearchShowExtOptions, &QAction::toggled, [=](bool state)
  {showHideLayoutElements({ui->gridLayoutAirportExtSearch}, state, {ui->lineAirportExtSearch}); });
  connect(ui->actionAirportSearchShowFuelParkOptions, &QAction::toggled, [=](bool state)
  { showHideLayoutElements({ui->horizontalLayoutAirportFuelParkSearch}, state, {ui->lineAirportFuelParkSearch}); });
  connect(ui->actionAirportSearchShowRunwayOptions, &QAction::toggled, [=](bool state)
  { showHideLayoutElements({ui->horizontalLayoutAirportRunwaySearch}, state, {ui->lineAirportRunwaySearch}); });
  connect(ui->actionAirportSearchShowAltOptions, &QAction::toggled, [=](bool state)
  { showHideLayoutElements({ui->horizontalLayoutAirportAltitudeSearch}, state, {ui->lineAirportAltSearch}); });
  connect(ui->actionAirportSearchShowDistOptions, &QAction::toggled, [=](bool state)
  { showHideLayoutElements({ui->horizontalLayoutAirportDistanceSearch}, state, {ui->lineAirportDistSearch}); });
  connect(ui->actionAirportSearchShowSceneryOptions, &QAction::toggled, [=](bool state)
  { showHideLayoutElements({ui->horizontalLayoutAirportScenerySearch}, state, {ui->lineAirportScenerySearch}); });
  /* *INDENT-ON* */

  // airport_id // file_id // ident // region // name // country // state // city
  // fuel_flags // has_avgas // has_jetfuel
  // has_tower_object // has_tower // is_closed
  // is_military // is_addon
  // num_boundary_fence
  // num_com
  // num_parking_gate // num_parking_ga_ramp // num_approach
  // num_runway_hard // num_runway_soft // num_runway_water // num_runway_light
  // num_runway_end_closed // num_runway_end_vasi // num_runway_end_als // num_runway_end_ils
  // num_apron // num_taxi_path
  // num_helipad
  // num_jetway
  // longest_runway_length // longest_runway_width // longest_runway_heading // longest_runway_surface
  // num_runways
  // largest_parking_ramp // largest_parking_gate
  // rating
  // scenery_local_path
  // bgl_filename
  // left_lonx // top_laty // right_lonx // bottom_laty
  // mag_var
  // tower_altitude // tower_lonx // tower_laty
  // altitude // lonx // laty
}

void SearchPaneList::preDatabaseLoad()
{
  if(airportSearchPane != nullptr)
    airportSearchPane->preDatabaseLoad();
}

void SearchPaneList::postDatabaseLoad()
{
  if(airportSearchPane != nullptr)
    airportSearchPane->postDatabaseLoad();
}

void SearchPaneList::showHideLayoutElements(const QList<QLayout *> layouts, bool visible,
                                            const QList<QWidget *>& otherWidgets)
{
  for(QWidget *w : otherWidgets)
    w->setVisible(visible);

  for(QLayout *layout : layouts)
    for(int i = 0; i < layout->count(); i++)
      layout->itemAt(i)->widget()->setVisible(visible);
}
