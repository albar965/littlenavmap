#include "searchpanelist.h"
#include "gui/mainwindow.h"
#include "table/searchpane.h"
#include "columnlist.h"
#include "ui_mainwindow.h"

SearchPaneList::SearchPaneList(MainWindow *parent, atools::sql::SqlDatabase *sqlDb)
  : QObject(parent), db(sqlDb), parentWidget(parent)
{

}

SearchPaneList::~SearchPaneList()
{
  delete airportSearchPane;
  delete airportColumns;
}

void SearchPaneList::createAirportSearch()
{
  Ui::MainWindow *ui = parentWidget->getUi();
  ui->checkBoxAirportAddonSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportApprSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportApprSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportClosedSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportIlsSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportLightSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportMilSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportTowerSearch->setCheckState(Qt::PartiallyChecked);
  ui->checkBoxAirportScenerySearch->setCheckState(Qt::PartiallyChecked);

  ui->toolButtonAirportSearch->addActions({ui->actionAirportSearchShowAllOptions,
                                           ui->actionAirportSearchShowExtOptions,
                                           ui->actionAirportSearchShowFuelParkOptions,
                                           ui->actionAirportSearchShowRunwayOptions,
                                           ui->actionAirportSearchShowAltOptions,
                                           ui->actionAirportSearchShowDistOptions,
                                           ui->actionAirportSearchShowSceneryOptions});
  ui->toolButtonAirportSearch->setArrowType(Qt::NoArrow);

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

  showHideLayoutElements({ui->gridLayoutAirportExtSearch}, false, {ui->lineAirportExtSearch});
  showHideLayoutElements({ui->horizontalLayoutAirportFuelParkSearch}, false, {ui->lineAirportFuelParkSearch});
  showHideLayoutElements({ui->horizontalLayoutAirportRunwaySearch}, false, {ui->lineAirportRunwaySearch});
  showHideLayoutElements({ui->horizontalLayoutAirportAltitudeSearch}, false, {ui->lineAirportAltSearch});
  showHideLayoutElements({ui->horizontalLayoutAirportDistanceSearch}, false, {ui->lineAirportDistSearch});
  showHideLayoutElements({ui->horizontalLayoutAirportScenerySearch}, false, {ui->lineAirportScenerySearch});

  airportColumns = new ColumnList("airport");

  // Default view column descriptors
  airportColumns->append(Column("airport_id", tr("ID")).hidden()).
  append(Column("ident", tr("ICAO")).canSort().canFilter().defaultCol().defaultSort()).
  append(Column("name", tr("Name")).canSort().canFilter().defaultCol()).
  append(Column("city", tr("City")).canSort().canFilter().defaultCol()).
  append(Column("state", tr("State")).canSort().canFilter().defaultCol()).
  append(Column("country", tr("Country")).canSort().canFilter().defaultCol()).
  append(Column("rating", tr("Rating")).canSort().canFilter().defaultCol()).
  append(Column("altitude", tr("Altitude")).canSort().canFilter().defaultCol()).
  append(Column("has_avgas", tr("Avgas")).canSort().canFilter().defaultCol()).
  append(Column("has_jetfuel", tr("Jetfuel")).canSort().canFilter().defaultCol()).
  append(Column("has_tower", tr("Tower")).canSort().canFilter().defaultCol()).
  append(Column("is_closed", tr("Closed")).canSort().canFilter().defaultCol()).
  append(Column("is_military", tr("Military")).canSort().canFilter().defaultCol()).
  append(Column("is_addon", tr("Addon")).canSort().canFilter().defaultCol()).
  append(Column("largest_parking_ramp", tr("Largest\nRamp")).canSort().canFilter().defaultCol()).
  append(Column("largest_parking_gate", tr("Largest\nGate")).canSort().canFilter().defaultCol()).
  append(Column("longest_runway_length", tr("Longest\nRunway Length")).canSort().canFilter().defaultCol()).
  append(Column("longest_runway_width", tr("Longest\nRunway Width")).canSort().canFilter().defaultCol()).
  append(Column("longest_runway_surface", tr("Longest\nRunway Surface")).canSort().canFilter().defaultCol()).
  append(Column("scenery_local_path", tr("Scenery")).canSort().canFilter().defaultCol()).
  append(Column("bgl_filename", tr("File")).canSort().canFilter().defaultCol());

  airportSearchPane = new SearchPane(parentWidget, parentWidget->getUi()->tableViewAirportSearch,
                                     airportColumns, db);

  airportSearchPane->addSearchWidget("ident", ui->lineEditAirportIcaoSearch);
  airportSearchPane->addSearchWidget("name", ui->lineEditAirportNameSearch);
  airportSearchPane->addSearchWidget("city", ui->lineEditAirportCitySearch);
  airportSearchPane->addSearchWidget("state", ui->lineEditAirportStateSearch);
  airportSearchPane->addSearchWidget("country", ui->lineEditAirportCountrySearch);

  airportSearchPane->connectSearchWidgets();

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

  // airport_id
  // file_id
  // ident
  // region
  // name
  // country
  // state
  // city
  // fuel_flags
  // has_avgas
  // has_jetfuel
  // has_tower_object
  // has_tower
  // is_closed
  // is_military
  // is_addon
  // num_boundary_fence
  // num_com
  // num_parking_gate
  // num_parking_ga_ramp
  // num_approach
  // num_runway_hard
  // num_runway_soft
  // num_runway_water
  // num_runway_light
  // num_runway_end_closed
  // num_runway_end_vasi
  // num_runway_end_als
  // num_runway_end_ils
  // num_apron
  // num_taxi_path
  // num_helipad
  // num_jetway
  // longest_runway_length
  // longest_runway_width
  // longest_runway_heading
  // longest_runway_surface
  // num_runways
  // largest_parking_ramp
  // largest_parking_gate
  // rating
  // scenery_local_path
  // bgl_filename
  // left_lonx
  // top_laty
  // right_lonx
  // bottom_laty
  // mag_var
  // tower_altitude
  // tower_lonx
  // tower_laty
  // altitude
  // lonx
  // laty

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
