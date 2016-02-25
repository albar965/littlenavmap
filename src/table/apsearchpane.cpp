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

#include "table/apsearchpane.h"
#include "logging/loggingdefs.h"
#include "gui/tablezoomhandler.h"
#include "sql/sqldatabase.h"
#include "table/controller.h"
#include "gui/dialog.h"
#include "gui/mainwindow.h"
#include "table/column.h"
#include "ui_mainwindow.h"
#include "table/columnlist.h"
#include "geo/pos.h"
#include "gui/widgettools.h"
#include "gui/widgetstate.h"

#include <QMessageBox>
#include <QWidget>
#include <QTableView>
#include <QHeaderView>
#include <QMenu>
#include <QLineEdit>

ApSearchPane::ApSearchPane(MainWindow *parent, QTableView *tableView, ColumnList *columnList,
                           atools::sql::SqlDatabase *sqlDb)
  : SearchPane(parent, tableView, columnList, sqlDb)
{
  Ui::MainWindow *ui = parentWidget->getUi();

  // Avoid stealing of Ctrl-C from other default menus
  ui->actionAirportSearchTableCopy->setShortcutContext(Qt::WidgetWithChildrenShortcut);

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

  // Default view column descriptors
  columns->
  append(Column("distance", tr("Distance")).virtualCol()).
  append(Column("airport_id", tr("ID")).hidden()).
  append(Column("ident", ui->lineEditAirportIcaoSearch, tr("ICAO")).filter().defaultSort()).
  append(Column("name", ui->lineEditAirportNameSearch, tr("Name")).filter()).
  append(Column("city", ui->lineEditAirportCitySearch, tr("City")).filter()).
  append(Column("state", ui->lineEditAirportStateSearch, tr("State")).filter()).
  append(Column("country", ui->lineEditAirportCountrySearch, tr("Country")).filter()).

  append(Column("rating", ui->checkBoxAirportScenerySearch, tr("Scenery\nRating")).conditions("> 0",
                                                                                              "== 0")).

  append(Column("altitude", tr("Altitude"))).
  append(Column("has_avgas", ui->checkBoxAirportAvgasSearch, tr("Avgas"))).
  append(Column("has_jetfuel", ui->checkBoxAirportJetASearch, tr("Jetfuel"))).
  append(Column("has_tower", ui->checkBoxAirportTowerSearch, tr("Tower"))).
  append(Column("is_closed", ui->checkBoxAirportClosedSearch, tr("Closed"))).
  append(Column("is_military", ui->checkBoxAirportMilSearch, tr("Military"))).
  append(Column("is_addon", ui->checkBoxAirportAddonSearch, tr("Addon"))).

  append(Column("num_runway_soft", ui->comboBoxAirportSurfaceSearch, tr("Soft\nRunways")).
         includesName().indexCondMap(rwSurface)).

  append(Column("num_runway_hard", tr("Hard\nRunways"))).
  append(Column("num_runway_water", tr("Water\nRunways"))).
  append(Column("num_runway_light", ui->checkBoxAirportLightSearch, tr("Lights")).conditions("> 0", "== 0")).
  append(Column("num_runway_end_ils", ui->checkBoxAirportIlsSearch, tr("ILS")).conditions("> 0", "== 0")).
  append(Column("num_approach", ui->checkBoxAirportApprSearch, tr("Approach")).conditions("> 0", "== 0")).

  append(Column("largest_parking_ramp", ui->comboBoxAirportRampSearch, tr("Largest\nRamp")).
         includesName().indexCondMap(rampCondMap)).
  append(Column("largest_parking_gate", ui->comboBoxAirportGateSearch, tr("Largest\nGate")).
         indexCondMap(gateCondMap)).

  append(Column("num_parking_cargo", tr("Cargo Ramps"))).
  append(Column("num_parking_mil_cargo", tr("Mil Cargo"))).
  append(Column("num_parking_mil_combat", tr("Mil Combat"))).
  append(Column("longest_runway_length", tr("Longest\nRunway Length"))).
  append(Column("longest_runway_width", tr("Longest\nRunway Width"))).
  append(Column("longest_runway_surface", tr("Longest\nRunway Surface"))).
  append(Column("scenery_local_path", ui->lineEditAirportScenerySearch, tr("Scenery")).filter()).
  append(Column("bgl_filename", ui->lineEditAirportFileSearch, tr("File")).filter()).
  append(Column("lonx", tr("Longitude")).hidden()).
  append(Column("laty", tr("Latitude")).hidden())
  ;

  initViewAndController();
}

ApSearchPane::~ApSearchPane()
{

}

void ApSearchPane::connectSlots()
{
  SearchPane::connectSlots();

  Ui::MainWindow *ui = parentWidget->getUi();
  connect(ui->actionAirportSearchResetSearch, &QAction::triggered, this, &ApSearchPane::resetSearch);
  connect(ui->actionAirportSearchResetView, &QAction::triggered, this, &ApSearchPane::resetView);
  connect(ui->actionAirportSearchTableCopy, &QAction::triggered, this, &ApSearchPane::tableCopyCipboard);
  connect(ui->actionAirportSearchShowAll, &QAction::triggered, this, &ApSearchPane::loadAllRowsIntoView);

  // Runways
  columns->assignMinMaxWidget("longest_runway_length",
                              ui->spinBoxAirportRunwaysMinSearch,
                              ui->spinBoxAirportRunwaysMaxSearch);
  // Altitude
  columns->assignMinMaxWidget("altitude",
                              ui->spinBoxAirportAltitudeMinSearch,
                              ui->spinBoxAirportAltitudeMaxSearch);

  // Distance
  columns->assignDistanceSearchWidgets(ui->pushButtonAirportDistSearch,
                                       ui->checkBoxAirportDistSearch,
                                       ui->comboBoxAirportDistDirectionSearch,
                                       ui->spinBoxAirportDistMinSearch,
                                       ui->spinBoxAirportDistMaxSearch);

  // Connect widgets to the controller
  SearchPane::connectSearchWidgets();

  // Drop down menu actions
  using atools::gui::WidgetTools;
  /* *INDENT-OFF* */
  connect(ui->actionAirportSearchShowExtOptions, &QAction::toggled, [=](bool state)
  {WidgetTools::showHideLayoutElements({ui->gridLayoutAirportExtSearch}, state, {ui->lineAirportExtSearch}); });
  connect(ui->actionAirportSearchShowFuelParkOptions, &QAction::toggled, [=](bool state)
  { WidgetTools::showHideLayoutElements({ui->horizontalLayoutAirportFuelParkSearch}, state, {ui->lineAirportFuelParkSearch}); });
  connect(ui->actionAirportSearchShowRunwayOptions, &QAction::toggled, [=](bool state)
  { WidgetTools::showHideLayoutElements({ui->horizontalLayoutAirportRunwaySearch}, state, {ui->lineAirportRunwaySearch}); });
  connect(ui->actionAirportSearchShowAltOptions, &QAction::toggled, [=](bool state)
  { WidgetTools::showHideLayoutElements({ui->horizontalLayoutAirportAltitudeSearch}, state, {ui->lineAirportAltSearch}); });
  connect(ui->actionAirportSearchShowDistOptions, &QAction::toggled, [=](bool state)
  { WidgetTools::showHideLayoutElements({ui->horizontalLayoutAirportDistanceSearch}, state, {ui->lineAirportDistSearch}); });
  connect(ui->actionAirportSearchShowSceneryOptions, &QAction::toggled, [=](bool state)
  { WidgetTools::showHideLayoutElements({ui->horizontalLayoutAirportScenerySearch}, state, {ui->lineAirportScenerySearch}); });
  /* *INDENT-ON* */

}

void ApSearchPane::tableContextMenu(const QPoint& pos)
{
  Ui::MainWindow *ui = parentWidget->getUi();
  QString header, fieldData;
  bool columnCanFilter = false, columnCanGroup = false;

  QModelIndex index = controller->getModelIndexAt(pos);
  if(index.isValid())
  {
    const Column *columnDescriptor = controller->getColumn(index.column());
    Q_ASSERT(columnDescriptor != nullptr);
    columnCanFilter = columnDescriptor->isFilter();
    columnCanGroup = columnDescriptor->isGroup();

    if(columnCanGroup)
    {
      header = controller->getHeaderNameAt(index);
      Q_ASSERT(!header.isNull());
      // strip LF and other from header name
      header.replace("-\n", "").replace("\n", " ");
    }

    if(columnCanFilter)
      // Disabled menu items don't need any content
      fieldData = controller->getFieldDataAt(index);
  }
  else
    qDebug() << "Invalid index at" << pos;

  // Build the menu
  QMenu menu;

  menu.addAction(ui->actionAirportSearchSetMark);
  menu.addSeparator();

  menu.addAction(ui->actionAirportSearchTableCopy);
  ui->actionAirportSearchTableCopy->setEnabled(index.isValid());

  menu.addAction(ui->actionAirportSearchTableSelectAll);
  ui->actionAirportSearchTableSelectAll->setEnabled(controller->getTotalRowCount() > 0);

  menu.addSeparator();
  menu.addAction(ui->actionAirportSearchResetView);
  menu.addAction(ui->actionAirportSearchResetSearch);
  menu.addAction(ui->actionAirportSearchShowAll);

  QString actionFilterIncludingText, actionFilterExcludingText;
  actionFilterIncludingText = ui->actionAirportSearchFilterIncluding->text();
  actionFilterExcludingText = ui->actionAirportSearchFilterExcluding->text();

  // Add data to menu item text
  ui->actionAirportSearchFilterIncluding->setText(ui->actionAirportSearchFilterIncluding->text().arg(
                                                    fieldData));
  ui->actionAirportSearchFilterIncluding->setEnabled(index.isValid() && columnCanFilter);

  ui->actionAirportSearchFilterExcluding->setText(ui->actionAirportSearchFilterExcluding->text().arg(
                                                    fieldData));
  ui->actionAirportSearchFilterExcluding->setEnabled(index.isValid() && columnCanFilter);

  menu.addSeparator();
  menu.addAction(ui->actionAirportSearchFilterIncluding);
  menu.addAction(ui->actionAirportSearchFilterExcluding);
  menu.addSeparator();

  QAction *a = menu.exec(QCursor::pos());
  if(a != nullptr)
  {
    // A menu item was selected
    if(a == ui->actionAirportSearchFilterIncluding)
      controller->filterIncluding(index);
    else if(a == ui->actionAirportSearchFilterExcluding)
      controller->filterExcluding(index);
    else if(a == ui->actionAirportSearchTableSelectAll)
      controller->selectAll();
    // else if(a == ui->actionTableCopy) this is alread covered by the connected action
  }

  // Restore old menu texts
  ui->actionAirportSearchFilterIncluding->setText(actionFilterIncludingText);
  ui->actionAirportSearchFilterIncluding->setEnabled(true);

  ui->actionAirportSearchFilterExcluding->setText(actionFilterExcludingText);
  ui->actionAirportSearchFilterExcluding->setEnabled(true);

}

void ApSearchPane::saveState()
{
  atools::gui::WidgetState saver("SearchPaneAirport/Widget");
  saver.save(airportSearchWidgets);
}

void ApSearchPane::restoreState()
{
  Ui::MainWindow *ui = parentWidget->getUi();
  atools::gui::WidgetState saver("SearchPaneAirport/Widget");
  saver.restore(airportSearchWidgets);
  ui->checkBoxAirportDistSearch->setChecked(false);
}
