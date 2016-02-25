#include "searchpanelist.h"
#include "gui/mainwindow.h"
#include "table/searchpane.h"
#include "column.h"
#include "columnlist.h"
#include "ui_mainwindow.h"
#include "map/navmapwidget.h"
#include <gui/widgetstate.h>

SearchPaneList::SearchPaneList(MainWindow *parent, atools::sql::SqlDatabase *sqlDb)
  : QObject(parent), db(sqlDb), parentWidget(parent)
{
  // Avoid stealing of Ctrl-C from other default menus
  parentWidget->getUi()->actionAirportSearchTableCopy->setShortcutContext(Qt::WidgetWithChildrenShortcut);
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
  saver.save({ui->tableViewAirportSearch,
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
              ui->actionAirportSearchShowSceneryOptions});
}

void SearchPaneList::restoreState()
{
  Ui::MainWindow *ui = parentWidget->getUi();
  atools::gui::WidgetState saver("SearchPaneAirport/Widget");
  saver.restore({ui->tableViewAirportSearch,
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
                 ui->actionAirportSearchShowSceneryOptions});
  ui->checkBoxAirportDistSearch->setChecked(false);
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
  airportColumns->
  append(Column("distance", tr("Distance")).virtualCol()).
  append(Column("airport_id", tr("ID")).hidden()).
  append(Column("ident", ui->lineEditAirportIcaoSearch, tr("ICAO")).canFilter().defaultSort()).
  append(Column("name", ui->lineEditAirportNameSearch, tr("Name")).canFilter()).
  append(Column("city", ui->lineEditAirportCitySearch, tr("City")).canFilter()).
  append(Column("state", ui->lineEditAirportStateSearch, tr("State")).canFilter()).
  append(Column("country", ui->lineEditAirportCountrySearch, tr("Country")).canFilter()).

  append(Column("rating", ui->checkBoxAirportScenerySearch, tr("Scenery\nRating")).cond("> 0", "== 0")).

  append(Column("altitude", tr("Altitude"))).
  append(Column("has_avgas", ui->checkBoxAirportAvgasSearch, tr("Avgas"))).
  append(Column("has_jetfuel", ui->checkBoxAirportJetASearch, tr("Jetfuel"))).
  append(Column("has_tower", ui->checkBoxAirportTowerSearch, tr("Tower"))).
  append(Column("is_closed", ui->checkBoxAirportClosedSearch, tr("Closed"))).
  append(Column("is_military", ui->checkBoxAirportMilSearch, tr("Military"))).
  append(Column("is_addon", ui->checkBoxAirportAddonSearch, tr("Addon"))).

  append(Column("num_runway_soft", ui->comboBoxAirportSurfaceSearch, tr("Soft\nRunways")).
         includesColName().indexCondMap(rwSurface)).

  append(Column("num_runway_hard", tr("Hard\nRunways"))).
  append(Column("num_runway_water", tr("Water\nRunways"))).
  append(Column("num_runway_light", ui->checkBoxAirportLightSearch, tr("Lights")).cond("> 0", "== 0")).
  append(Column("num_runway_end_ils", ui->checkBoxAirportIlsSearch, tr("ILS")).cond("> 0", "== 0")).
  append(Column("num_approach", ui->checkBoxAirportApprSearch, tr("Approach")).cond("> 0", "== 0")).

  append(Column("largest_parking_ramp", ui->comboBoxAirportRampSearch, tr("Largest\nRamp")).
         includesColName().indexCondMap(rampCondMap)).
  append(Column("largest_parking_gate", ui->comboBoxAirportGateSearch, tr("Largest\nGate")).
         indexCondMap(gateCondMap)).

  append(Column("num_parking_cargo", tr("Cargo Ramps"))).
  append(Column("num_parking_mil_cargo", tr("Mil Cargo"))).
  append(Column("num_parking_mil_combat", tr("Mil Combat"))).
  append(Column("longest_runway_length", tr("Longest\nRunway Length"))).
  append(Column("longest_runway_width", tr("Longest\nRunway Width"))).
  append(Column("longest_runway_surface", tr("Longest\nRunway Surface"))).
  append(Column("scenery_local_path", ui->lineEditAirportScenerySearch, tr("Scenery")).canFilter()).
  append(Column("bgl_filename", ui->lineEditAirportFileSearch, tr("File")).canFilter()).
  append(Column("lonx", tr("Longitude")).hidden()).
  append(Column("laty", tr("Latitude")).hidden())
  ;

  airportSearchPane = new SearchPane(parentWidget, parentWidget->getUi()->tableViewAirportSearch,
                                     airportColumns, db);

  // Runways
  airportColumns->assignMinMaxWidget("longest_runway_length",
                                     ui->spinBoxAirportRunwaysMinSearch,
                                     ui->spinBoxAirportRunwaysMaxSearch);
  // Altitude
  airportColumns->assignMinMaxWidget("altitude",
                                     ui->spinBoxAirportAltitudeMinSearch,
                                     ui->spinBoxAirportAltitudeMaxSearch);

  // Distance
  airportColumns->assignDistanceSearchWidgets(ui->pushButtonAirportDistSearch,
                                              ui->checkBoxAirportDistSearch,
                                              ui->comboBoxAirportDistDirectionSearch,
                                              ui->spinBoxAirportDistMinSearch,
                                              ui->spinBoxAirportDistMaxSearch);

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

  QMetaObject::Connection mo = connect(parentWidget->getMapWidget(), &NavMapWidget::markChanged,
                                       airportSearchPane, &SearchPane::markChanged);

  if(!mo)
    qWarning() << "connect failed";

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
