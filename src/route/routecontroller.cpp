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

#include "routecontroller.h"

#include "app/navapp.h"
#include "atools.h"
#include "common/constants.h"
#include "common/filecheck.h"
#include "common/formatter.h"
#include "common/mapcolors.h"
#include "common/symbolpainter.h"
#include "common/tabindexes.h"
#include "common/textpointer.h"
#include "common/unit.h"
#include "common/unit.h"
#include "common/unitstringtool.h"
#include "exception.h"
#include "export/csvexporter.h"
#include "fs/perf/aircraftperf.h"
#include "fs/pln/flightplanio.h"
#include "fs/sc/simconnectdata.h"
#include "fs/util/fsutil.h"
#include "geo/calculations.h"
#include "gui/actiontool.h"
#include "gui/dialog.h"
#include "gui/errorhandler.h"
#include "gui/helphandler.h"
#include "gui/itemviewzoomhandler.h"
#include "gui/tabwidgethandler.h"
#include "gui/tools.h"
#include "gui/treedialog.h"
#include "gui/widgetstate.h"
#include "mapgui/mapwidget.h"
#include "options/optiondata.h"
#include "parkingdialog.h"
#include "query/airportquery.h"
#include "query/airwaytrackquery.h"
#include "query/mapquery.h"
#include "query/procedurequery.h"
#include "route/customproceduredialog.h"
#include "route/flightplanentrybuilder.h"
#include "route/routealtitude.h"
#include "route/routecalcdialog.h"
#include "route/routecommand.h"
#include "route/routelabel.h"
#include "route/runwayselectiondialog.h"
#include "route/userwaypointdialog.h"
#include "routeextractor.h"
#include "routestring/routestringdialog.h"
#include "routestring/routestringreader.h"
#include "routestring/routestringwriter.h"
#include "routing/routefinder.h"
#include "routing/routenetwork.h"
#include "routing/routenetworkloader.h"
#include "settings/settings.h"
#include "track/trackcontroller.h"
#include "ui_mainwindow.h"
#include "util/contextsaver.h"
#include "util/htmlbuilder.h"

#include <QClipboard>
#include <QFile>
#include <QStandardItemModel>
#include <QInputDialog>
#include <QFileInfo>
#include <QTextTable>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QScrollBar>
#include <QStringBuilder>
#include <QUndoStack>

namespace rcol {
// Route table column indexes
enum RouteColumns
{
  /* Column indexes. Saved through view in RouteLabel::saveState() const
   * Update RouteController::routeColumns and RouteController::routeColumnDescription when adding new values */
  FIRST_COLUMN,
  IDENT = FIRST_COLUMN,
  REGION,
  NAME,
  PROCEDURE,
  AIRWAY_OR_LEGTYPE,
  RESTRICTION,
  TYPE,
  FREQ,
  RANGE,
  COURSE,
  COURSETRUE,
  DIST,
  REMAINING_DISTANCE,
  LEG_TIME,
  ETA,
  FUEL_WEIGHT,
  FUEL_VOLUME,
  WIND,
  WIND_HEAD_TAIL,
  ALTITUDE,
  SAFE_ALTITUDE,
  LATITUDE,
  LONGITUDE,
  MAGVAR,
  RECOMMENDED,
  REMARKS,
  LAST_COLUMN = REMARKS,

  /* Not column names but identifiers for the tree configuration dialog
   * Not saved directly. */
  HEADER_AIRPORTS = 1000,
  HEADER_TAKEOFF,
  HEADER_DEPARTURE,
  HEADER_ARRIVAL,
  HEADER_LAND,
  HEADER_DIST_TIME,
  HEADER_TAKEOFF_WIND,
  HEADER_LAND_WIND,

  FOOTER_SELECTION = 1500,
  FOOTER_ERROR
};

/* Default columns which are shown on first startup. Others are hidden. */
const static QSet<RouteColumns> DEFAULT_COLUMNS({
    IDENT,
    NAME,
    PROCEDURE,
    AIRWAY_OR_LEGTYPE,
    RESTRICTION,
    TYPE,
    FREQ,
    RANGE,
    COURSE,
    DIST,
    REMAINING_DISTANCE,
    LEG_TIME,
    ETA,
    FUEL_WEIGHT,
    FUEL_VOLUME,
    WIND,
    WIND_HEAD_TAIL,
    RECOMMENDED,
    REMARKS
  });
}

/* Maximum lines in flight plan and waypoint remarks for printing and HTML export */
const static int MAX_REMARK_LINES_HTML_AND_PRINT = 1000;
const static int MAX_REMARK_COLS_HTML_AND_PRINT = 200;

/* Do not update aircraft information more than every 0.1 seconds */
const static int MIN_SIM_UPDATE_TIME_MS = 100;
const static int ROUTE_ALT_CHANGE_DELAY_MS = 500;

using atools::fs::pln::Flightplan;
using atools::fs::pln::FlightplanEntry;
using atools::gui::ActionTool;
using namespace atools::geo;

namespace pln = atools::fs::pln;

RouteController::RouteController(QMainWindow *parentWindow, QTableView *tableView)
  : QObject(parentWindow), mainWindow(parentWindow), tableViewRoute(tableView)
{
  airportQuery = NavApp::getAirportQuerySim();

  routeFilenameDefault = atools::settings::Settings::getConfigFilename(lnm::ROUTE_DEFAULT_SUFFIX);

  dialog = new atools::gui::Dialog(mainWindow);

  routeColumns = QList<QString>({
    tr("Ident"),
    tr("Region"),
    tr("Name"),
    tr("Procedure"),
    tr("Airway or\nProcedure"),
    tr("Restriction\n%alt%/%speed%/angle"),
    tr("Type"),
    tr("Freq.\nMHz/kHz/Cha."),
    tr("Range\n%dist%"),
    tr("Course\n°M"),
    tr("Course\n°T"),
    tr("Distance\n%dist%"),
    tr("Remaining\n%dist%"),
    tr("Leg Time\nhh:mm"),
    tr("ETA\nhh:mm"),
    tr("Fuel Rem.\n%weight%"),
    tr("Fuel Rem.\n%volume%"),
    tr("Wind\n°M/%speed%"),
    tr("Head- or Tailwind\n%speed%"),
    tr("Altitude\n%alt%"),
    tr("Leg Safe Alt.\n%alt%"),
    tr("Latitude"),
    tr("Longitude"),
    tr("Mag.\nDecl.°"),
    tr("Related\nIdent/Freq./Dist./Bearing"),
    tr("Remarks")});

  routeColumnDescription = QList<QString>({
    tr("ICAO ident of the navaid or airport."),
    tr("Two letter ICAO region code of a navaid."),
    tr("Name of airport or navaid."),
    tr("Either SID, SID transition, STAR, STAR transition, transition,\n"
       "approach or missed plus the name of the procedure."),
    tr("Contains the airway name for en route legs or procedure instruction."),
    tr("Minimum and maximum altitude for en route airway segments.\n"
       "Procedure altitude restriction, speed limit or\n"
       "required descent flight path angle."),
    tr("Type of a radio navaid. Shows ILS, LOC or other for\n"
       "approaches like ILS or localizer on the last runway leg."),
    tr("Frequency or channel of a radio navaid.\n"
       "Also shows ILS or localizer frequency for corresponding approaches\n"
       "on the last runway leg."),
    tr("Range of a radio navaid if available."),
    tr("Magnetic start course of the great circle route\n"
       "connecting the two waypoints of the leg.\n"
       "Does not consider VOR calibrated declination."),
    tr("True start course of the great circle route connecting\n"
       "the two waypoints of the leg."),
    tr("Length of the flight plan leg."),
    tr("Remaining distance to destination airport or procedure end point.\n"
       "Static value. Does not update while flying."),
    tr("Flying time for this leg.\n"
       "Calculated based on the selected aircraft performance profile.\n"
       "Static value. Does not update while flying."),
    tr("Estimated time of arrival.\n"
       "Calculated based on the selected aircraft performance profile.\n"
       "Static value. Does not update while flying."),
    tr("Fuel weight remaining at waypoint.\n"
       "Calculated based on the aircraft performance profile.\n"
       "Static value. Does not update while flying."),
    tr("Fuel volume remaining at waypoint.\n"
       "Calculated based on the aircraft performance profile.\n"
       "Static value. Does not update while flying."),
    tr("Wind direction and speed at waypoint."),
    tr("Head- or tailwind at waypoint."),
    tr("Altitude at waypoint\n"
       "Calculated based on the aircraft performance profile."),
    tr("Safe altitude for a flight plan leg depending on ground elevation\n"
       "and options on page \"Flight Plan\"."),
    tr("Waypoint latitude in format as selected in options."),
    tr("Waypoint longitude in format as selected in options."),
    tr("Magnetic declination at waypoint or VOR calibrated declination."),
    tr("Related or recommended navaid for procedure legs.\n"
       "Shown with navaid ident, navaid frequency, distance and bearing defining a fix."),
    tr("Turn instructions, flyover, related navaid for procedure legs or\n"
       "or remarks entered by user.")
  });

  flightplanIO = new atools::fs::pln::FlightplanIO();

  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::adjustSelectionColors(ui->tableViewRoute);
  tabHandlerRoute = new atools::gui::TabWidgetHandler(ui->tabWidgetRoute, {}, QIcon(":/littlenavmap/resources/icons/tabbutton.svg"),
                                                      tr("Open or close tabs"));
  tabHandlerRoute->init(rc::TabRouteIds, lnm::ROUTEWINDOW_WIDGET_TABS);

  // Update units
  units = new UnitStringTool();
  units->init({ui->spinBoxRouteAlt, ui->spinBoxAircraftPerformanceWindSpeed, ui->spinBoxAircraftPerformanceWindAlt});

  // Set default table cell and font size to avoid Qt overly large cell sizes
  zoomHandler = new atools::gui::ItemViewZoomHandler(tableViewRoute);

  entryBuilder = new FlightplanEntryBuilder();

  symbolPainter = new SymbolPainter();
  routeLabel = new RouteLabel(mainWindow, route);

  // Use saved font size for table view
  zoomHandler->zoomPercent(OptionData::instance().getGuiRouteTableTextSize());
  updateRemarksFont();

  tableViewRoute->setContextMenuPolicy(Qt::CustomContextMenu);

  // Create flight plan calculation caches ===================================
  routeNetworkRadio = new atools::routing::RouteNetwork(atools::routing::SOURCE_RADIO);
  routeNetworkAirway = new atools::routing::RouteNetwork(atools::routing::SOURCE_AIRWAY);

  // Do not use a parent to allow the window moving to back
  routeCalcDialog = new RouteCalcDialog(nullptr);

  // Set up undo/redo framework ========================================
  undoStack = new QUndoStack(mainWindow);
  undoStack->setUndoLimit(ROUTE_UNDO_LIMIT);

  undoAction = undoStack->createUndoAction(mainWindow, tr("&Undo Flight Plan"));
  undoAction->setIcon(QIcon(":/littlenavmap/resources/icons/undo.svg"));
  undoAction->setShortcut(QKeySequence(tr("Ctrl+Z")));

  redoAction = undoStack->createRedoAction(mainWindow, tr("&Redo Flight Plan"));
  redoAction->setIcon(QIcon(":/littlenavmap/resources/icons/redo.svg"));
  redoAction->setShortcut(QKeySequence(tr("Ctrl+Y")));

  connect(redoAction, &QAction::triggered, this, &RouteController::redoTriggered);
  connect(undoAction, &QAction::triggered, this, &RouteController::undoTriggered);

  ui->toolBarRoute->insertAction(ui->actionRouteSelectParking, undoAction);
  ui->toolBarRoute->insertAction(ui->actionRouteSelectParking, redoAction);

  // Need to add the undo/redo actions to all widgets and tabs but userpoint and logbook search to avoid collision
  // with other undo actions
  // ui->dockWidgetRoute->insertActions(ui->actionRouteSelectParking, {undoAction, redoAction});
  // ui->dockWidgetRouteCalc->insertActions(ui->actionRouteSelectParking, {undoAction, redoAction});
  // ui->dockWidgetInformation->insertActions(ui->actionRouteSelectParking, {undoAction, redoAction});
  // ui->dockWidgetAircraft->insertActions(ui->actionRouteSelectParking, {undoAction, redoAction});
  // ui->dockWidgetProfile->insertActions(ui->actionRouteSelectParking, {undoAction, redoAction});
  // ui->dockWidgetMap->insertActions(ui->actionRouteSelectParking, {undoAction, redoAction});
  // ui->tabAirportSearch->insertActions(ui->actionRouteSelectParking, {undoAction, redoAction});
  // ui->tabProcedureSearch->insertActions(ui->actionRouteSelectParking, {undoAction, redoAction});
  // ui->tabNavIdSearch->insertActions(ui->actionRouteSelectParking, {undoAction, redoAction});
  // ui->tabOnlineCenterSearch->insertActions(ui->actionRouteSelectParking, {undoAction, redoAction});
  // ui->tabOnlineClientSearch->insertActions(ui->actionRouteSelectParking, {undoAction, redoAction});
  // ui->tabOnlineServerSearch->insertActions(ui->actionRouteSelectParking, {undoAction, redoAction});
  // undoAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  // redoAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  ui->menuRoute->insertActions(ui->actionRouteSelectParking, {undoAction, redoAction});
  ui->menuRoute->insertSeparator(ui->actionRouteSelectParking);

  // Top widgets ================================================================================
  connect(ui->spinBoxRouteAlt, QOverload<int>::of(&QSpinBox::valueChanged), this, &RouteController::routeAltChanged);
  connect(ui->comboBoxRouteType, QOverload<int>::of(&QComboBox::activated), this, &RouteController::routeTypeChanged);

  // View ================================================================================
  connect(tableViewRoute, &QTableView::doubleClicked, this, &RouteController::doubleClick);
  connect(tableViewRoute, &QTableView::customContextMenuRequested, this, &RouteController::tableContextMenu);
  connect(this, &RouteController::routeChanged, this, &RouteController::updateRemarkWidget);
  connect(this, &RouteController::routeChanged, this, &RouteController::updateRemarkHeader);
  connect(ui->plainTextEditRouteRemarks, &QPlainTextEdit::textChanged, this, &RouteController::remarksTextChanged);

  // Update route altitude and elevation profile with a delay ====================
  connect(&routeAltDelayTimer, &QTimer::timeout, this, &RouteController::routeAltChangedDelayed);
  routeAltDelayTimer.setSingleShot(true);

  // Clear selection after inactivity
  // Or move active to top after inactivity (no scrolling)
  connect(&tableCleanupTimer, &QTimer::timeout, this, &RouteController::cleanupTableTimeout);
  tableCleanupTimer.setInterval(OptionData::instance().getSimCleanupTableTime() * 1000);
  tableCleanupTimer.setSingleShot(true);

  // Restart timer when scrolling or holding the slider
  connect(tableViewRoute->verticalScrollBar(), &QScrollBar::valueChanged, this, &RouteController::updateCleanupTimer);
  connect(tableViewRoute->horizontalScrollBar(), &QScrollBar::valueChanged, this, &RouteController::updateCleanupTimer);
  connect(tableViewRoute->verticalScrollBar(), &QScrollBar::sliderPressed, this, &RouteController::updateCleanupTimer);
  connect(tableViewRoute->horizontalScrollBar(), &QScrollBar::sliderPressed, this, &RouteController::updateCleanupTimer);
  connect(tableViewRoute->verticalScrollBar(), &QScrollBar::sliderReleased, this, &RouteController::updateCleanupTimer);
  connect(tableViewRoute->horizontalScrollBar(), &QScrollBar::sliderReleased, this, &RouteController::updateCleanupTimer);

  // set up table view
  tableViewRoute->horizontalHeader()->setSectionsMovable(true);
  tableViewRoute->verticalHeader()->setSectionsMovable(false);
  tableViewRoute->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

  model = new QStandardItemModel();
  QItemSelectionModel *m = tableViewRoute->selectionModel();
  tableViewRoute->setModel(model);
  delete m;

  // Avoid stealing of keys from other default menus
  ui->actionRouteLegDown->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteLegUp->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteDeleteLeg->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteShowInformation->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteShowApproaches->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteDirectTo->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteShowApproachCustom->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteShowDepartureCustom->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteShowOnMap->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteTableSelectNothing->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteTableSelectAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteActivateLeg->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteSetMark->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteResetView->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteTableCopy->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteEditUserWaypoint->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  // Add to dock handler to enable auto raise and closing on exit
  NavApp::addDialogToDockHandler(routeCalcDialog);

  // Add action/shortcuts to table view
  tableViewRoute->addActions({ui->actionRouteLegDown, ui->actionRouteLegUp, ui->actionRouteDeleteLeg,
                              ui->actionRouteTableCopy, ui->actionRouteShowInformation,
                              ui->actionRouteShowApproaches, ui->actionRouteDirectTo, ui->actionRouteShowApproachCustom,
                              ui->actionRouteShowDepartureCustom, ui->actionRouteShowOnMap, ui->actionRouteTableSelectNothing,
                              ui->actionRouteTableSelectAll, ui->actionRouteActivateLeg, ui->actionRouteResetView, ui->actionRouteSetMark,
                              ui->actionRouteEditUserWaypoint});

  if(tableViewRoute->selectionModel() != nullptr)
    connect(tableViewRoute->selectionModel(), &QItemSelectionModel::selectionChanged, this, &RouteController::tableSelectionChanged);

  // Connect actions - actions without shortcut key are used in the context menu method directly
  connect(ui->actionRouteTableCopy, &QAction::triggered, this, &RouteController::tableCopyClipboardTriggered);
  connect(ui->actionRouteLegDown, &QAction::triggered, this, &RouteController::moveSelectedLegsDownTriggered);
  connect(ui->actionRouteLegUp, &QAction::triggered, this, &RouteController::moveSelectedLegsUpTriggered);
  connect(ui->actionRouteDeleteLeg, &QAction::triggered, this, &RouteController::deleteSelectedLegsTriggered);
  connect(ui->actionRouteEditUserWaypoint, &QAction::triggered, this, &RouteController::editUserWaypointTriggered);

  connect(ui->actionRouteShowInformation, &QAction::triggered, this, &RouteController::showInformationTriggered);
  connect(ui->actionRouteShowApproaches, &QAction::triggered, this, &RouteController::showProceduresTriggered);
  connect(ui->actionRouteShowApproachCustom, &QAction::triggered, this, &RouteController::showCustomApproachRouteTriggered);
  connect(ui->actionRouteShowDepartureCustom, &QAction::triggered, this, &RouteController::showCustomDepartureRouteTriggered);
  connect(ui->actionRouteShowOnMap, &QAction::triggered, this, &RouteController::showOnMapTriggered);
  connect(ui->actionRouteDirectTo, &QAction::triggered, this, &RouteController::directToTriggered);

  connect(ui->dockWidgetRoute, &QDockWidget::visibilityChanged, this, &RouteController::dockVisibilityChanged);
  connect(ui->actionRouteTableSelectNothing, &QAction::triggered, this, &RouteController::clearTableSelection);
  connect(ui->actionRouteTableSelectAll, &QAction::triggered, this, &RouteController::selectAllTriggered);
  connect(ui->pushButtonRouteClearSelection, &QPushButton::clicked, this, &RouteController::clearTableSelection);
  connect(ui->pushButtonRouteHelp, &QPushButton::clicked, this, &RouteController::helpClicked);
  connect(ui->actionRouteActivateLeg, &QAction::triggered, this, &RouteController::activateLegTriggered);
  connect(ui->actionRouteDisplayOptions, &QAction::triggered, this, &RouteController::routeTableOptions);
  connect(ui->pushButtonRouteSettings, &QPushButton::clicked, this, &RouteController::routeTableOptions);

  connect(this, &RouteController::routeChanged, routeCalcDialog, &RouteCalcDialog::routeChanged);
  connect(routeCalcDialog, &RouteCalcDialog::calculateClicked, this, &RouteController::calculateRoute);
  connect(routeCalcDialog, &RouteCalcDialog::calculateDirectClicked, this, &RouteController::calculateDirect);
  connect(routeCalcDialog, &RouteCalcDialog::calculateReverseClicked, this, &RouteController::reverseRoute);
  connect(routeCalcDialog, &RouteCalcDialog::downloadTrackClicked, NavApp::getTrackController(), &TrackController::startDownload);

  connect(routeLabel, &RouteLabel::flightplanLabelLinkActivated, this, &RouteController::flightplanLabelLinkActivated);

  connect(this, &RouteController::routeChanged, this, &RouteController::updateFooterErrorLabel);
  connect(this, &RouteController::routeAltitudeChanged, this, &RouteController::updateFooterErrorLabel);

  // UI editor cannot deal with line breaks - set text here
  ui->textBrowserViewRoute->setPlaceholderText(
    tr("No flight plan.\n\n"
       "Right-click the center of an airport symbol on the map and select it as departure/destination from the context menu, "
       "or use the airport search (press \"F4\") to select departure/destination from the context menu of the search result table."));
  updatePlaceholderWidget();
}

RouteController::~RouteController()
{
  NavApp::removeDialogFromDockHandler(routeCalcDialog);
  routeAltDelayTimer.stop();

  ATOOLS_DELETE_LOG(routeCalcDialog);
  ATOOLS_DELETE_LOG(tabHandlerRoute);
  ATOOLS_DELETE_LOG(units);
  ATOOLS_DELETE_LOG(entryBuilder);
  ATOOLS_DELETE_LOG(model);
  ATOOLS_DELETE_LOG(undoStack);
  ATOOLS_DELETE_LOG(routeNetworkRadio);
  ATOOLS_DELETE_LOG(routeNetworkAirway);
  ATOOLS_DELETE_LOG(zoomHandler);
  ATOOLS_DELETE_LOG(symbolPainter);
  ATOOLS_DELETE_LOG(routeLabel);
  ATOOLS_DELETE_LOG(flightplanIO);
  ATOOLS_DELETE_LOG(dialog);
}

void RouteController::undoTriggered()
{
  NavApp::setStatusMessage(QString(tr("Undo flight plan change.")));
}

void RouteController::redoTriggered()
{
  NavApp::setStatusMessage(QString(tr("Redo flight plan change.")));
}

/* Ctrl-C - copy selected table contents in CSV format to clipboard */
void RouteController::tableCopyClipboardTriggered()
{
  qDebug() << "RouteController::tableCopyClipboard";

  const Route& rt = route;
  const QStandardItemModel *mdl = model;

  // Use callback to get data to avoid truncated remarks
  auto dataFunc = [&rt, &mdl](int row, int column) -> QVariant {
                    if(column == rcol::REMARKS)
                      // Full remarks
                      return rt.value(row).getComment();
                    else
                      // Formatted view content
                      return mdl->data(mdl->index(row, column));
                  };

  QString csv;
  int exported = CsvExporter::selectionAsCsv(tableViewRoute, true /* rows */, true /* header */, csv, {}, nullptr, dataFunc);

  if(!csv.isEmpty())
  {
    QApplication::clipboard()->setText(csv);
    NavApp::setStatusMessage(QString(tr("Copied %1 entries as CSV to clipboard.")).arg(exported));
  }
}

void RouteController::flightplanTableAsTextTable(QTextCursor& cursor, const QBitArray& selectedCols,
                                                 float fontPointSize) const
{
  // Check if model is already initailized
  if(model->rowCount() == 0)
    return;

  int numCols = selectedCols.count(true);

  // Prepare table format ===================================
  QTextTableFormat fmt;
  fmt.setHeaderRowCount(1);
  fmt.setCellPadding(1);
  fmt.setCellSpacing(0);
  fmt.setBorder(2);
  fmt.setBorderBrush(Qt::lightGray);
  fmt.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
  QTextTable *table = cursor.insertTable(model->rowCount() + 1, numCols, fmt);

  // Cell alignment formats ===================================
  QTextBlockFormat alignRight;
  alignRight.setAlignment(Qt::AlignRight);
  QTextBlockFormat alignLeft;
  alignLeft.setAlignment(Qt::AlignLeft);

  // Text size and alternating background formats ===================================
  QTextCharFormat altFormat1 = table->cellAt(0, 0).format();
  altFormat1.setFontPointSize(fontPointSize);
  altFormat1.setBackground(mapcolors::mapPrintRowColor);

  QTextCharFormat altFormat2 = altFormat1;
  altFormat2.setBackground(mapcolors::mapPrintRowColorAlt);

  // Header font and background ================
  QTextCharFormat headerFormat = altFormat1;
  headerFormat.setFontWeight(QFont::Bold);
  headerFormat.setBackground(mapcolors::mapPrintHeaderColor);

  // Fill header =====================================================================
  // Table header from GUI widget
  QHeaderView *header = tableViewRoute->horizontalHeader();

  int cellIdx = 0;
  for(int viewCol = 0; viewCol < model->columnCount(); viewCol++)
  {
    // Convert view position to model position - needed to keep order
    int logicalCol = header->logicalIndex(viewCol);

    if(logicalCol == -1)
      continue;

    if(!selectedCols.at(logicalCol))
      // Ignore if not selected in the print dialog
      continue;

    table->cellAt(0, cellIdx).setFormat(headerFormat);
    cursor.setPosition(table->cellAt(0, cellIdx).firstPosition());

    QString txt = model->headerData(logicalCol, Qt::Horizontal).toString().replace("-\n", "").replace("\n", " ");
    cursor.insertText(txt);

    cellIdx++;
  }

  // Fill table =====================================================================
  for(int row = 0; row < model->rowCount(); row++)
  {
    cellIdx = 0;
    for(int viewCol = 0; viewCol < model->columnCount(); viewCol++)
    {
      int logicalCol = header->logicalIndex(viewCol);
      if(logicalCol == -1)
        continue;

      if(!selectedCols.at(logicalCol))
        // Ignore if not selected in the print dialog
        continue;

      const QStandardItem *item = model->item(row, logicalCol);

      if(item != nullptr)
      {
        // Alternating background =============================
        QTextCharFormat textFormat = (row % 2) == 0 ? altFormat1 : altFormat2;

        // Determine font color base on leg =============
        const RouteLeg& leg = route.value(row);
        if(leg.isAlternate())
          textFormat.setForeground(mapcolors::routeAlternateTableColor);
        else if(leg.isAnyProcedure())
          textFormat.setForeground(leg.getProcedureLeg().isMissed() ?
                                   mapcolors::routeProcedureMissedTableColor :
                                   mapcolors::routeProcedureTableColor);
        else if((logicalCol == rcol::IDENT && leg.getMapType() == map::INVALID) ||
                (logicalCol == rcol::AIRWAY_OR_LEGTYPE && leg.isRoute() && leg.isAirwaySetAndInvalid(route.getCruiseAltitudeFt())))
          textFormat.setForeground(Qt::red);
        else
          textFormat.setForeground(Qt::black);

        if(logicalCol == rcol::IDENT)
          // Make ident bold
          textFormat.setFontWeight(QFont::Bold);

        if(logicalCol == rcol::REMARKS)
          // Make remarks small
          textFormat.setFontPointSize(textFormat.fontPointSize() * 0.8);

        table->cellAt(row + 1, cellIdx).setFormat(textFormat);
        cursor.setPosition(table->cellAt(row + 1, cellIdx).firstPosition());

        // Assign alignment to cell
        if(item->textAlignment() == Qt::AlignRight)
          cursor.setBlockFormat(alignRight);
        else
          cursor.setBlockFormat(alignLeft);

        if(logicalCol == rcol::REMARKS)
          cursor.insertText(atools::elideTextLinesShort(leg.getComment(), MAX_REMARK_LINES_HTML_AND_PRINT,
                                                        MAX_REMARK_COLS_HTML_AND_PRINT, true, true));
        else
          cursor.insertText(item->text());
      }
      cellIdx++;
    }
  }

  // Move cursor after table
  cursor.setPosition(table->lastPosition() + 1);
}

void RouteController::flightplanHeaderPrint(atools::util::HtmlBuilder& html, bool titleOnly) const
{
  routeLabel->buildPrintText(html, titleOnly);

  if(!route.getFlightplanConst().getComment().isEmpty())
  {
    html.p().b(tr("Flight Plan Remarks")).pEnd();
    html.table(1, 2, 0, 0, html.getRowBackColor());
    html.tr().td().text(atools::elideTextLinesShort(route.getFlightplanConst().getComment(), MAX_REMARK_LINES_HTML_AND_PRINT,
                                                    MAX_REMARK_COLS_HTML_AND_PRINT, true, true),
                        atools::util::html::SMALL).tdEnd().trEnd();
    html.tableEnd().br();
  }
}

QString RouteController::getFlightplanTableAsHtmlDoc(float iconSizePixel) const
{
  // Embedded style sheet =================================
  QString css("table { border-collapse: collapse; } "
              "th, td { border-right: 1px solid #aaa; padding: 0px 3px 0px 3px; white-space: nowrap; font-size: 0.9em; } "
              "th { white-space: normal; padding: 3px 3px 3px 3px; font-size: 0.95em; } "
              "tr:hover {background-color: #c8c8c8; } ");

  // Metadata in header =================================
  QStringList headerLines({"<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\" />",
                           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />"});

  atools::util::HtmlBuilder html(true);
  html.doc(tr("%1 - %2").arg(QCoreApplication::applicationName()).arg(QFileInfo(routeFilename).fileName()),
           css, QString() /* bodyStyle */, headerLines);
  html.text(NavApp::getRouteController()->getFlightplanTableAsHtml(iconSizePixel, true /* print */),
            atools::util::html::NO_ENTITIES);

  // Add footer ==============================
  html.p().small(tr("%1 Version %2 (revision %3) on %4 ").
                 arg(QCoreApplication::applicationName()).
                 arg(QApplication::applicationVersion()).
                 arg(GIT_REVISION_LITTLENAVMAP).
                 arg(QLocale().toString(QDateTime::currentDateTime()))).pEnd();
  html.docEnd();
  return html.getHtml();
}

QString RouteController::getFlightplanTableAsHtml(float iconSizePixel, bool print) const
{
  qDebug() << Q_FUNC_INFO;

  // Check if model is already initailized
  if(model->rowCount() == 0)
    return QString();

  using atools::util::HtmlBuilder;

  atools::util::HtmlBuilder html(mapcolors::webTableBackgroundColor, mapcolors::webTableAltBackgroundColor);
  int minColWidth = tableViewRoute->horizontalHeader()->minimumSectionSize() + 1;

  routeLabel->buildHtmlText(html);

  if(print && !route.getFlightplanConst().getComment().isEmpty())
  {
    html.p().b(tr("Flight Plan Remarks")).pEnd();
    html.table(1, 2, 0, 0, html.getRowBackColor());
    html.tr().td().text(atools::elideTextLinesShort(route.getFlightplanConst().getComment(), MAX_REMARK_LINES_HTML_AND_PRINT,
                                                    MAX_REMARK_COLS_HTML_AND_PRINT, true, true),
                        atools::util::html::SMALL).tdEnd().trEnd();
    html.tableEnd().br();
  }

  html.table();

  // Table header
  QHeaderView *header = tableViewRoute->horizontalHeader();
  html.tr(Qt::lightGray);
  html.th(QString()); // Icon
  for(int viewCol = 0; viewCol < model->columnCount(); viewCol++)
  {
    // Convert view position to model position - needed to keep order
    int logicalCol = header->logicalIndex(viewCol);

    if(logicalCol == -1)
      continue;

    if(!tableViewRoute->isColumnHidden(logicalCol) && tableViewRoute->columnWidth(logicalCol) > minColWidth)
      html.th(model->headerData(logicalCol, Qt::Horizontal).
              toString().replace("-\n", "-<br/>").replace("\n", "<br/>"), atools::util::html::NO_ENTITIES);
  }
  html.trEnd();

  int nearestLegIndex = route.getActiveLegIndexCorrected();

  // Table content
  for(int row = 0; row < model->rowCount(); row++)
  {
    // First column is icon
    html.tr(nearestLegIndex != row ? QColor() : mapcolors::nextWaypointColor);
    const RouteLeg& leg = route.value(row);

    if(iconSizePixel > 0.f)
    {
      int sizeInt = atools::roundToInt(iconSizePixel);

      html.td();
      html.img(iconForLeg(leg, atools::roundToInt(iconSizePixel)), QString(), QString(), QSize(sizeInt, sizeInt));
      html.tdEnd();
    }

    // Rest of columns
    for(int viewCol = 0; viewCol < model->columnCount(); viewCol++)
    {
      int logicalCol = header->logicalIndex(viewCol);
      if(logicalCol == -1)
        continue;

      if(!tableViewRoute->isColumnHidden(logicalCol) && tableViewRoute->columnWidth(logicalCol) > minColWidth)
      {
        QStandardItem *item = model->item(row, logicalCol);

        if(item != nullptr)
        {
          QColor color(Qt::black);
          if(leg.isAlternate())
            color = mapcolors::routeAlternateTableColor;
          else if(leg.isAnyProcedure())
            color = leg.getProcedureLeg().isMissed() ?
                    mapcolors::routeProcedureMissedTableColor :
                    mapcolors::routeProcedureTableColor;
          else if((logicalCol == rcol::IDENT && leg.getMapType() == map::INVALID) ||
                  (logicalCol == rcol::AIRWAY_OR_LEGTYPE && leg.isRoute() &&
                   leg.isAirwaySetAndInvalid(route.getCruiseAltitudeFt())))
            color = Qt::red;

          atools::util::html::Flags flags = atools::util::html::NONE;
          // Make ident column text bold
          if(logicalCol == rcol::IDENT)
            flags |= atools::util::html::BOLD;

          // Remarks in small text
          if(logicalCol == rcol::REMARKS)
            flags |= atools::util::html::SMALL;

          // Take over alignment from model
          if(item->textAlignment().testFlag(Qt::AlignRight))
            flags |= atools::util::html::ALIGN_RIGHT;

          if(logicalCol == rcol::REMARKS)
            html.td(atools::elideTextLinesShort(leg.getComment(), MAX_REMARK_LINES_HTML_AND_PRINT,
                                                MAX_REMARK_COLS_HTML_AND_PRINT, true, true), flags, color);
          else
            html.td(item->text(), flags, color);
        }
        else
          html.td(QString());
      }
    }
    html.trEnd();
  }
  html.tableEnd();
  return html.getHtml();
}

void RouteController::routeStringToClipboard() const
{
  // Create string from the current flight plan using current settings and not from the dialog
  QString str = RouteStringWriter().createStringForRoute(route, NavApp::getRouteCruiseSpeedKts(),
                                                         RouteStringDialog::getOptionsFromSettings() | rs::ALT_AND_SPEED_METRIC);

  if(!str.isEmpty())
    QApplication::clipboard()->setText(str);

  NavApp::setStatusMessage(QString(tr("Flight plan string to clipboard.")));
}

void RouteController::aircraftPerformanceChanged()
{
  qDebug() << Q_FUNC_INFO;

  // Get type, speed and cruise altitude from widgets
  updateTableHeaders(); // Update lbs/gal for fuel
  updateFlightplanFromWidgets();

  // Needs to be called with empty route as well to update the error messages
  route.calculateLegAltitudes();

  updateModelTimeFuelWindAlt();
  updateModelHighlightsAndErrors();
  highlightNextWaypoint(route.getActiveLegIndexCorrected());

  routeLabel->updateHeaderLabel();
  routeLabel->updateFooterSelectionLabel();

  // Emit also for empty route to catch performance changes
  emit routeChanged(true /* geometryChanged */);
}

void RouteController::windUpdated()
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  if(!route.isEmpty())
  {
    // Get type, speed and cruise altitude from widgets
    route.calculateLegAltitudes();

    updateModelTimeFuelWindAlt();
    updateModelHighlightsAndErrors();
    highlightNextWaypoint(route.getActiveLegIndexCorrected());
  }
  routeLabel->updateHeaderLabel();
  routeLabel->updateFooterSelectionLabel();

  // Emit also for empty route to catch performance changes
  emit routeChanged(false /* geometryChanged */);
}

void RouteController::weatherUpdated()
{
  routeLabel->updateHeaderLabel();
}

/* Spin box altitude has changed value */
void RouteController::routeAltChanged()
{
  RouteCommand *undoCommand = nullptr;

  if(!route.isEmpty() /*&& route.getFlightplan().canSaveAltitude()*/)
    undoCommand = preChange(tr("Change Altitude"));

  // Get type, speed and cruise altitude from widgets
  updateFlightplanFromWidgets();

  // Transfer cruise altitude from flight plan window to calculation window
  routeCalcDialog->setCruisingAltitudeFt(route.getCruiseAltitudeFt());

  postChange(undoCommand);

  routeLabel->updateHeaderLabel();
  routeLabel->updateFooterSelectionLabel();

  NavApp::updateWindowTitle();

  // Calls RouteController::routeAltChangedDelayed
  routeAltDelayTimer.start(ROUTE_ALT_CHANGE_DELAY_MS);
}

void RouteController::routeAltChangedDelayed()
{
  if(atools::gui::Application::isShuttingDown())
    return;

  route.calculateLegAltitudes();

  // Update performance
  updateModelTimeFuelWindAlt();
  updateModelHighlightsAndErrors();
  highlightNextWaypoint(route.getActiveLegIndexCorrected());

  routeLabel->updateHeaderLabel();
  routeLabel->updateFooterSelectionLabel();

  // Delay change to avoid hanging spin box when profile updates
  emit routeAltitudeChanged(route.getCruiseAltitudeFt());
}

/* Combo box route type has value changed */
void RouteController::routeTypeChanged()
{
  RouteCommand *undoCommand = nullptr;

  if(!route.isEmpty() /*&& route.getFlightplan().canSaveFlightplanType()*/)
    undoCommand = preChange(tr("Change Type"));

  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  postChange(undoCommand);

  NavApp::updateWindowTitle();

  if(!route.isEmpty())
  {
    emit routeChanged(false /* geometryChanged */);
    Ui::MainWindow *ui = NavApp::getMainUi();
    NavApp::setStatusMessage(tr("Flight plan type changed to %1.").arg(ui->comboBoxRouteType->currentText()));
  }
}

bool RouteController::selectDepartureParking()
{
  qDebug() << Q_FUNC_INFO;

  const map::MapAirport& airport = route.getDepartureAirportLeg().getAirport();
  ParkingDialog parkingDialog(mainWindow, airport);

  int result = parkingDialog.exec();
  parkingDialog.hide();

  if(result == QDialog::Accepted)
  {
    if(parkingDialog.isAirportSelected())
    {
      // Clear start and parking and select airport
      routeClearParkingAndStart();
      return true; // Has changed
    }
    else
    {
      // Set either start of parking
      map::MapParking parking = parkingDialog.getSelectedParking();
      if(parking.isValid())
      {
        routeSetParking(parking);
        return true;
      }
      else
      {
        map::MapStart start = parkingDialog.getSelectedStartPosition();
        if(start.isValid())
        {
          routeSetStartPosition(start);
          return true;
        }
      }
    }
  }
  return false;
}

void RouteController::updateTableHeaders()
{
  using atools::fs::perf::AircraftPerf;

  QList<QString> routeHeaders(routeColumns);

  for(QString& str : routeHeaders)
    str = Unit::replacePlaceholders(str);

  model->setHorizontalHeaderLabels(routeHeaders);
}

void RouteController::saveState() const
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  atools::gui::WidgetState(lnm::ROUTE_VIEW).save(
    QList<const QObject *>({ui->comboBoxRouteType, ui->spinBoxRouteAlt, ui->actionRouteFollowSelection}));

  // Use own state for table
  atools::gui::WidgetState(lnm::ROUTE_VIEW_TABLE).save(tableViewRoute);

  atools::settings::Settings& settings = atools::settings::Settings::instance();
  settings.setValue(lnm::ROUTE_FILENAME, routeFilename);

  routeLabel->saveState();
  tabHandlerRoute->saveState();
  routeCalcDialog->saveState();
}

void RouteController::restoreState()
{
  tabHandlerRoute->restoreState();
  routeCalcDialog->restoreState();
  updateTableHeaders();

  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState state(lnm::ROUTE_VIEW, true, true);
  state.restore({ui->comboBoxRouteType, ui->spinBoxRouteAlt, ui->actionRouteFollowSelection});

  // Use own state for table
  atools::gui::WidgetState stateTable(lnm::ROUTE_VIEW_TABLE, true, true);
  if(stateTable.contains(tableViewRoute))
    stateTable.restore(tableViewRoute);
  else
  {
    // Hide non-default columns on first startup
    QHeaderView *header = tableViewRoute->horizontalHeader();
    for(int col = rcol::LAST_COLUMN; col >= rcol::FIRST_COLUMN; col--)
      header->setSectionHidden(col, !rcol::DEFAULT_COLUMNS.contains(static_cast<rcol::RouteColumns>(col)));
  }

  routeLabel->restoreState();

  try
  {
    QString cmdLineFlightplanFile, cmdLineFlightplanDescr;

    // Load plan from command line or last used =============================================
    fc::fromStartupProperties(NavApp::getStartupOptionsConst(), &cmdLineFlightplanFile, &cmdLineFlightplanDescr);

    if(!cmdLineFlightplanFile.isEmpty())
    {
      // Load from file from command line ===================================================
      QString message = atools::checkFileMsg(cmdLineFlightplanFile);
      if(message.isEmpty())
      {
        if(atools::fs::pln::FlightplanIO::isFlightplanFile(cmdLineFlightplanFile))
        {
          // Load file silently without change and without correction
          if(!loadFlightplan(cmdLineFlightplanFile, false /* correctAndWarn */))
            // Cannot be loaded - clear current filename
            clearFlightplan();
          // else Centered in MainWindow::mainWindowShownDelayed()
        }
        else
        {
          // Not a flight plan file
          clearFlightplan();
          dialog->warning(tr("File \"%1\" is a not supported flight plan format or not a flight plan.").arg(cmdLineFlightplanFile));
        }
      }
      else
      {
        // No file or not readable
        clearFlightplan();
        dialog->warning(message);
      }
    }
    else if(!cmdLineFlightplanDescr.isEmpty())
      // Parse route description from command line ===================================================
      loadFlightplanRouteStr(cmdLineFlightplanDescr);
    else
    {
      // Nothing given on command line ==================================
      if(OptionData::instance().getFlags().testFlag(opts::STARTUP_LOAD_ROUTE))
      {
        if(!NavApp::isSafeMode())
        {
          atools::settings::Settings& settings = atools::settings::Settings::instance();
          QString lastUsedFlightplanFile = settings.valueStr(lnm::ROUTE_FILENAME);
          QString flightplanFile = lastUsedFlightplanFile;
          bool changed = false, defaultLoad = false;

          if(atools::checkFile(Q_FUNC_INFO, routeFilenameDefault, false /* warn */))
          {
            // Default file "ABarthel/little_navmap.lnmpln" in settings exists from last save - load it and set new file to changed
            flightplanFile = routeFilenameDefault;
            changed = defaultLoad = true;
          }

          if(!flightplanFile.isEmpty())
          {
            Flightplan fp;
            atools::fs::pln::FileFormat format = flightplanIO->load(fp, flightplanFile);

            // Do not warn on missing altitude after loading - do not change altitude
            loadFlightplanInternal(fp, format, flightplanFile, changed, false /* adjustAltitude */, false /* undo */,
                                   false /* warnAltitude */, true /* correctProfile */);

            if(defaultLoad && settings.contains(lnm::ROUTE_DEFAULT_FILE_LNMPLN))
              // Restore LNMPLN status after loading since this set the status to true
              route.getFlightplan().setLnmFormat(settings.valueBool(lnm::ROUTE_DEFAULT_FILE_LNMPLN));

            routeFilename = lastUsedFlightplanFile;
          }
        }
      }
    }
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    clearFlightplan();
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    clearFlightplan();
  }

  if(route.isEmpty())
    updateFlightplanFromWidgets();

  units->update();

  connect(NavApp::getRouteTabHandler(), &atools::gui::TabWidgetHandler::tabOpened, this, &RouteController::updateRouteTabChangedStatus);
}

void RouteController::resetWindowLayout()
{
  routeCalcDialog->resetWindowLayout();
}

void RouteController::clearFlightplan()
{
  routeFilename.clear();
  fileDepartureIdent.clear();
  fileDestinationIdent.clear();
  fileIfrVfr = pln::VFR;
  fileCruiseAltFt = 0.f;
  route.clear();
}

void RouteController::getSelectedRouteLegs(QList<int>& selLegIndexes) const
{
  if(NavApp::getMainUi()->dockWidgetRoute->isVisible() && tableViewRoute->selectionModel() != nullptr)
  {
    for(const QItemSelectionRange& rng : tableViewRoute->selectionModel()->selection())
    {
      for(int row = rng.top(); row <= rng.bottom(); ++row)
        selLegIndexes.append(row);
    }
  }
}

void RouteController::newFlightplan()
{
  qDebug() << "newFlightplan";

  clearRouteAndUndo();
  clearAllErrors();
  clearTableSelection();

  // Avoid warning when saving
  route.getFlightplan().setLnmFormat(true);

  // Copy current alt and type from widgets to flightplan
  updateFlightplanFromWidgets();

  route.createRouteLegsFromFlightplan();
  route.updateAll();
  route.calculateLegAltitudes();
  route.updateRouteCycleMetadata();
  route.updateAircraftPerfMetadata();

  updateTableModelAndErrors();
  updateActions();
  remarksFlightplanToWidget();

  emit routeChanged(true /* geometryChanged */, true /* newFlightPlan */);
}

void RouteController::routeNewFromAirports(const map::MapAirport& departure, const map::MapAirport& destination)
{
  RouteCommand *undoCommand = preChange(tr("Set Departure and Destination"));
  NavApp::showFlightplan();

  routeSetDepartureInternal(departure);
  routeSetDestinationInternal(destination);

  route.updateAll();
  route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
  route.calculateLegAltitudes();
  route.updateDepartureAndDestination(false /* clearInvalidStart */);

  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  updateActiveLeg();
  updateTableModelAndErrors();
  updateActions();

  postChange(undoCommand);
  emit routeChanged(true /* geometryChanged */);
  NavApp::setStatusMessage(tr("New flight plan set."));
}

void RouteController::loadFlightplanInternal(atools::fs::pln::Flightplan flightplan, atools::fs::pln::FileFormat format,
                                             const QString& filename, bool changed, bool adjustAltitude, bool undo, bool warnAltitude,
                                             bool correctProfile)
{
  qDebug() << Q_FUNC_INFO << filename << "format" << format << "changed" << changed
           << "adjustAltitude" << adjustAltitude
           << "warnAltitude" << warnAltitude // Loaded manually - show warning dialog
           << "correctProfile" << correctProfile;

#ifdef DEBUG_INFORMATION_ROUTE
  qDebug() << flightplan;
#endif

  // Input flight plan needs cruise altitude in feet
  // Stored plan uses local unit (meter or feet)

  clearAllErrors();
  clearTableSelection();

  RouteCommand *undoCommand = nullptr;
  if(undo)
    undoCommand = preChange(tr("Flight Plan Changed"));

  // Keep this since it is overwritten by widgets later
  pln::FlightplanType loadedFlightplanType = flightplan.getFlightplanType();

  // Change altitude based on airways and procedures later if true
  bool adjustAltAfterLoad = false;

  if(format == atools::fs::pln::FLP || format == atools::fs::pln::GARMIN_GFP)
  {
    // FLP and GFP are a sort of route string
    // New waypoints along airways have to be inserted and waypoints have to be resolved without coordinate backup

    // Create a route string
    QStringList routeString;
    for(int i = 0; i < flightplan.size(); i++)
    {
      const FlightplanEntry& entry = flightplan.at(i);
      if(!entry.getAirway().isEmpty())
        routeString.append(entry.getAirway());
      routeString.append(entry.getIdent());
    }
    qInfo() << "FLP/GFP generated route string" << routeString;

    // All is valid except the waypoint entries
    flightplan.clearAll();

    // Use route string to overwrite the current incomplete flight plan object
    RouteStringReader rs(entryBuilder);
    rs.setPlaintextMessages(true);
    bool ok = rs.createRouteFromString(routeString.join(" "), rs::NONE, &flightplan);
    qInfo() << "createRouteFromString messages" << rs.getAllMessages();

    if(!ok)
    {
      dialog->warning(tr("Loading of FLP flight plan failed:<br/><br/>") % rs.getAllMessages().join("<br/>"));
      return;

    }
    else if(!rs.getAllMessages().isEmpty())
      dialog->showInfoMsgBox(lnm::ACTIONS_SHOW_LOAD_FLP_WARN,
                             tr("Warnings while loading FLP flight plan file:<br/><br/>") %
                             rs.getAllMessages().join("<br/>"),
                             tr("Do not &show this dialog again."));

    adjustAltAfterLoad = true; // Change altitude based on airways and procedures later
  }
  else if(format == atools::fs::pln::FMS11 || format == atools::fs::pln::FMS3 || format == atools::fs::pln::FSC_PLN ||
          format == atools::fs::pln::FLIGHTGEAR || format == atools::fs::pln::GARMIN_FPL)
  {
    if( // Cruise either too low or ...
      atools::almostEqual(flightplan.getCruiseAltitudeFt(), 0.f) || // Still in ft
      // ... X-Plane with not enough waypoints
      ((format == atools::fs::pln::FMS3 || format == atools::fs::pln::FMS11) && flightplan.size() <= 2))
      // FMS with only two waypoints cannot determine altitude - adjust later
      adjustAltAfterLoad = true; // Change altitude based on airways and procedures later
  }

  if(undo)
    // Do not clear undo flags - only replace plan and allow undo/redo
    clearRoute();
  else
    // Clear everything for a new plan
    clearRouteAndUndo();

  if(changed)
    // No clean state in stack
    undoIndexClean = -1;

  if(!undo)
    routeFilename = filename;

  // FSX and P3D use this field for helipad or runway numbers where the latter one is zero prefixed.
  // MSFS has no helipad number and we need a zero prefix added to the runway number to allow database queries
  if(format == atools::fs::pln::MSFS_PLN)
    flightplan.setDepartureParkingName(atools::fs::util::runwayNamePrefixZero(flightplan.getDepartureParkingName()));

  // Assign plan to route and get reference to it
  route.setFlightplan(flightplan);
  Flightplan& routeFlightplan = route.getFlightplan();

  route.createRouteLegsFromFlightplan();

  // test and error after undo/redo and switch

  // These formats have transition waypoints which might match or not on import and require a cleanup
  bool specialProcedureHandling = format == atools::fs::pln::MSFS_PLN || format == atools::fs::pln::FSC_PLN;

  loadProceduresFromFlightplan(false /* clearOldProcedureProperties */, specialProcedureHandling /* cleanupRoute */,
                               specialProcedureHandling /* autoresolveTransition */);

  // Update type if loaded file does not support a type
  if(loadedFlightplanType == atools::fs::pln::NO_TYPE)
  {
    if(route.hasAirways() || route.hasAnyRealProcedure())
      routeFlightplan.setFlightplanType(atools::fs::pln::IFR);
    else
      routeFlightplan.setFlightplanType(atools::fs::pln::VFR);
    updateComboBoxFromFlightplanType();
  }

  route.updateAll(); // Removes alternate property if not resolvable

  // Update airway objects in route legs only - leave cruise altitude as is
  route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);

  // Calculate elevation profile
  route.calculateLegAltitudes();

  bool showWarnDialog = false;
  // Check if profile is valid, has to be adjusted and warnings (= manual load) are required
  if((!route.isValidProfile() && correctProfile) || adjustAltAfterLoad || adjustAltitude)
  {
    // Remember the original cruise altitude before adapting
    float origCruiseAlt = route.getCruiseAltitudeFt();

    // Update cruise altitude in local units based on procedures and airway restriction
    // Corrects cruise altitude if adjustRouteAltitude is true
    route.updateAirwaysAndAltitude(true /* adjustRouteAltitude */);

    // Calculate profile
    route.calculateLegAltitudes();

    if(!route.isValidProfile())
    {
      // Profile not valid or adjustement requested
      route.updateAirwaysAndAltitude(true /* adjustRouteAltitude */);
      route.calculateLegAltitudes();

      // Check if valid and use higher cruise if still violating altitudes
      if(!route.isValidProfile())
      {
        // Try again with plus 2000 meter or feet
        route.setCruiseAltitudeFt(route.getCruiseAltitudeFt() + Unit::rev(2000.f, Unit::altFeetF));
        route.calculateLegAltitudes();

        if(!route.isValidProfile())
        {
          // If all else fails round 2000 ft up to the max of departure and destination altitude
          float alt = std::max(route.getDepartureAirportLeg().getAltitude(), route.getDestinationAirportLeg().getAltitude());

          if(alt < map::INVALID_ALTITUDE_VALUE / 2.f)
          {
            alt = atools::roundToNearest(alt + 2000.f, 2000.f);

            // Try again with plus 2000 meter or feet
            route.setCruiseAltitudeFt(Unit::rev(alt, Unit::altFeetF));
            route.calculateLegAltitudes();
          }
        }
      }
    }

    // Test if cruise altitude did change
    if(atools::almostNotEqual(origCruiseAlt, route.getCruiseAltitudeFt(), 1.f))
    {
      if(undoCommand == nullptr)
      {
        // Create late undo command if not already done
        float newCruise = route.getCruiseAltitudeFt();

        // Set the old altitude for the undo command to store
        route.setCruiseAltitudeFt(origCruiseAlt);
        undoCommand = preChange(tr("Flight Plan Changed"));

        // Assign new cruise altitude again
        route.setCruiseAltitudeFt(newCruise);
      }
      // else there is an undo step anyway which has the altitude change included now

      // Show dialog if requested and if something has changed
      showWarnDialog = warnAltitude;
    }
  }

  if(showWarnDialog)
  {
    // Let user know
    dialog->showInfoMsgBox(lnm::ACTIONS_SHOW_LOAD_ALT_WARN,
                           tr("Cruising altitude from flight plan is not valid. "
                              "It may conflict with airport elevation, "
                              "procedure and/or airway restrictions.<br/><br/>"
                              "Applied best guess for cruising altitude, trying to avoid errors.<br/>"
                              "Note that you can undo this change.<br/><br/>"
                              "Adjust the altitude manually as needed."), tr("Do not &show this dialog again."));
  }

  // Save values for checking filename match when doing save
  fileDepartureIdent = routeFlightplan.getDepartureIdent();
  fileDestinationIdent = routeFlightplan.getDestinationIdent();
  fileIfrVfr = routeFlightplan.getFlightplanType();
  fileCruiseAltFt = route.getCruiseAltitudeFt();

  // Get number from user waypoint from user defined waypoint in fs flight plan
  entryBuilder->setCurUserpointNumber(route.getNextUserWaypointNumber());

  remarksFlightplanToWidget();
  updateTableModelAndErrors();
  updateActions();
  routeCalcDialog->setCruisingAltitudeFt(route.getCruiseAltitudeFt());

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << route;
#endif

  postChange(undoCommand);

  emit routeChanged(true /* geometryChanged */, true /* newFlightPlan */);
}

void RouteController::loadProceduresFromFlightplan(bool clearOldProcedureProperties, bool cleanupRoute, bool autoresolveTransition)
{
  if(route.isEmpty())
    return;

  route.updateIndicesAndOffsets();

  QStringList errors;
  proc::MapProcedureLegs arrival, departure, star;
  NavApp::getProcedureQuery()->getLegsForFlightplanProperties(route.getFlightplanConst().getPropertiesConst(),
                                                              route.getDepartureAirportLeg().getAirport(),
                                                              route.getDestinationAirportLeg().getAirport(),
                                                              arrival, star, departure, errors, autoresolveTransition);
  errors.removeDuplicates();
  procedureErrors = errors;

  // SID/STAR with multiple runways are already assigned
  route.setSidProcedureLegs(departure);
  route.setStarProcedureLegs(star);
  route.setArrivalProcedureLegs(arrival);
  route.updateProcedureLegs(entryBuilder, clearOldProcedureProperties, cleanupRoute);
}

bool RouteController::loadFlightplanLnmStr(const QString& string)
{
  qDebug() << Q_FUNC_INFO;

  Flightplan fp;
  try
  {
    // Will throw an exception if something goes wrong
    flightplanIO->loadLnmStr(fp, string);

    loadFlightplanInternal(fp, atools::fs::pln::LNM_PLN, QString(),
                           false /*changed*/, false /* adjustAltitude */, false /* undo */, false /* warnAltitude */,
                           true /* correctProfile */);
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    return false;
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    return false;
  }
  return true;
}

void RouteController::loadFlightplanRouteStr(const QString& routeString)
{
  qDebug() << Q_FUNC_INFO << routeString;

  RouteStringReader reader(entryBuilder);
  reader.setPlaintextMessages(true);

  Flightplan fp;

  // true if an altitude item is included in string
  bool altIncluded = false;

  // Use settings from dialog
  if(reader.createRouteFromString(routeString, RouteStringDialog::getOptionsFromSettings(), &fp, nullptr, nullptr, &altIncluded))
    loadFlightplanInternal(fp, atools::fs::pln::LNM_PLN, QString(),
                           false /* changed */, !altIncluded /* adjustAltitude */, false /* undo */, false /* warnAltitude */,
                           true /* correctProfile */);

  if(reader.hasErrorMessages() || reader.hasWarningMessages())
    dialog->warning(tr("<p>Errors reading flight plan route description<br/><b>%1</b><br/>from command line:</p><p>%2</p>").
                    arg(routeString).arg(reader.getAllMessages().join("<br>")));
}

bool RouteController::loadFlightplan(const QString& filename, bool correctAndWarn)
{
  qDebug() << Q_FUNC_INFO << filename;
  Flightplan fp;
  try
  {
    qDebug() << Q_FUNC_INFO << "loadFlightplan" << filename;
    // Will throw an exception if something goes wrong
    atools::fs::pln::FileFormat format = flightplanIO->load(fp, filename);
    loadFlightplanInternal(fp, format, filename,
                           false /*changed*/, false /* adjustAltitude */, false /* undo */, correctAndWarn /* warnAltitude */,
                           correctAndWarn /* correctProfile */);
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    return false;
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    return false;
  }
  return true;
}

bool RouteController::insertFlightplan(const QString& filename, int insertBefore)
{
  qDebug() << Q_FUNC_INFO << filename << insertBefore;

  try
  {
    Flightplan flightplan;
    pln::Flightplan& routePlan = route.getFlightplan();

    // Ignore events triggering follow due to selection changes
    atools::util::ContextSaverBool saver(ignoreFollowSelection);

    // Will throw an exception if something goes wrong
    flightplanIO->load(flightplan, filename);
    RouteCommand *undoCommand = preChange(insertBefore >= route.getDestinationAirportLegIndex() ? tr("Insert") : tr("Append"));

    bool beforeDestInsert = false, beforeDepartPrepend = false, afterDestAppend = false, middleInsert = false;
    int insertPosSelection = insertBefore;

    if(insertBefore >= route.getSizeWithoutAlternates())
    {
      // Append ================================================================
      afterDestAppend = true;

      // Remove arrival legs and properties
      route.removeProcedureLegs(proc::PROCEDURE_ARRIVAL_ALL);

      // Remove alternates and properties
      route.removeAlternateLegs();

      // Start of selection without arrival procedures
      insertPosSelection = route.size();

      // Append flight plan to current flightplan object - route is updated later
      for(const FlightplanEntry& entry : qAsConst(flightplan))
        routePlan.append(entry);

      // Appended after destination airport
      routePlan.setDestinationName(flightplan.getDestinationName());
      routePlan.setDestinationIdent(flightplan.getDestinationIdent());
      routePlan.setDestinationPosition(flightplan.getDestinationPosition());

      // Copy any STAR and arrival procedure properties from the loaded plan
      atools::fs::pln::copyArrivalProcedureProperties(routePlan.getProperties(), flightplan.getPropertiesConst());
      atools::fs::pln::copyStarProcedureProperties(routePlan.getProperties(), flightplan.getPropertiesConst());
    }
    else
    {
      // Insert ================================================================
      if(insertBefore == 0)
      {
        // Insert before departure ==============
        beforeDepartPrepend = true;

        // Remove departure legs and properties
        route.removeProcedureLegs(proc::PROCEDURE_DEPARTURE);

        // Added before departure airport
        routePlan.setDepartureName(flightplan.getDepartureName());
        routePlan.setDepartureIdent(flightplan.getDepartureIdent());
        routePlan.setDeparturePosition(flightplan.getDeparturePosition(), flightplan.constFirst().getAltitude());
        routePlan.setDepartureParkingPosition(flightplan.getDepartureParkingPosition(),
                                              flightplan.getDepartureParkingPosition().getAltitude(),
                                              flightplan.getDepartureParkingHeading());
        routePlan.setDepartureParkingName(flightplan.getDepartureParkingName());
        routePlan.setDepartureParkingType(flightplan.getDepartureParkingType());

        // Copy SID properties from source
        atools::fs::pln::copySidProcedureProperties(routePlan.getProperties(), flightplan.getPropertiesConst());

        route.eraseAirwayFlightplan(1);
      }
      else if(insertBefore >= route.getSizeWithoutAlternates() - 1)
      {
        // Insert before destination ==============
        beforeDestInsert = true;

        // Remove all arrival legs and properties
        route.removeProcedureLegs(proc::PROCEDURE_ARRIVAL_ALL);

        // Correct insert position and start of selection for removed arrival legs
        insertBefore = route.getSizeWithoutAlternates() - 1;
        insertPosSelection = insertBefore;

        // No procedures taken from the loaded plan
      }
      else
      {
        // Insert in the middle ==============
        middleInsert = true;

        // No airway at start leg
        route.eraseAirwayFlightplan(insertBefore);

        // No procedures taken from the loaded plan
      }

      // Copy new legs to the flightplan object only - update route later
      for(auto it = flightplan.rbegin(); it != flightplan.rend(); ++it)
        routePlan.insert(insertBefore, *it);
    }

    // Clear procedure legs from the flightplan object
    routePlan.removeIsNoSaveEntries();

    // Clear procedure structures
    route.clearProcedures(proc::PROCEDURE_ALL);

    // Clear procedure legs from route object only since indexes are not consistent now
    route.clearProcedureLegs(proc::PROCEDURE_ALL, true /* clear route */, false /* clear flight plan */);

    // Rebuild all legs from flightplan object and properties
    route.createRouteLegsFromFlightplan();

    // Load procedures and add legs
    loadProceduresFromFlightplan(true /* clearOldProcedureProperties */, false /* cleanupRoute */, false /* autoresolveTransition */);
    route.updateAll();
    route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
    route.calculateLegAltitudes();

    updateActiveLeg();
    updateTableModelAndErrors();

    postChange(undoCommand);

    // Select newly imported flight plan legs
    if(afterDestAppend)
      selectRange(insertPosSelection, route.size() - 1);
    else if(beforeDepartPrepend)
      selectRange(0, flightplan.size() + route.getLastIndexOfDepartureProcedure() - 1);
    else if(beforeDestInsert)
      selectRange(insertPosSelection, route.size() - 2 - route.getNumAlternateLegs());
    else if(middleInsert)
      selectRange(insertPosSelection, insertPosSelection + flightplan.size() - 1);

    updateActions();

    emit routeChanged(true /* geometryChanged */);
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    return false;
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    return false;
  }
  return true;
}

void RouteController::saveFlightplanLnmExported(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;

  // Change name to exported filename
  routeFilename = filename;

  fileDepartureIdent = route.getFlightplanConst().getDepartureIdent();
  fileDestinationIdent = route.getFlightplanConst().getDestinationIdent();
  fileIfrVfr = route.getFlightplanConst().getFlightplanType();
  fileCruiseAltFt = route.getCruiseAltitudeFt();

  // Set format to original route since it is saved as LNM now
  route.getFlightplan().setLnmFormat(true);

  // Set clean undo state index since QUndoStack only returns weird values
  undoIndexClean = undoIndex;
  undoStack->setClean();

  updateRemarkHeader();
  NavApp::updateWindowTitle();
}

void RouteController::saveFlightplanLnmToFileQuiet(const QString& filename)
{
  saveFlightplanLnmInternal(filename, true /* silentShutdown */, false /* clearUndo */);
}

void RouteController::saveFlightplanLnmDefaultShutdown()
{
  qDebug() << Q_FUNC_INFO << routeFilenameDefault;

  if(OptionData::instance().getFlags().testFlag(opts::STARTUP_LOAD_ROUTE) && !route.isEmpty() && hasChanged())
  {
    // Have to save the LNMPLN status separately since it is reset when saving to the default routeFilenameDefault
    // Need to overwriting FMS, PLN, etc. with LNMPLN
    atools::settings::Settings::instance().setValue(lnm::ROUTE_DEFAULT_FILE_LNMPLN, isLnmFormatFlightplan());

    // Save plan to default file
    saveFlightplanLnmInternal(routeFilenameDefault, true /* silentShutdown */, true /* clearUndo */);
  }
  else if(QFileInfo::exists(routeFilenameDefault))
  {
    // Clear LNMPLN status
    atools::settings::Settings::instance().remove(lnm::ROUTE_DEFAULT_FILE_LNMPLN);

    // No flight plan - delete to avoid reloading
    bool deleted = QFile::remove(routeFilenameDefault);
    qDebug() << Q_FUNC_INFO << "Deleted" << routeFilenameDefault << deleted;
  }
}

bool RouteController::saveFlightplanLnmAs(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;
  bool success = saveFlightplanLnmInternal(filename, false /* silentShutdown */, true /* clearUndo */);

  if(success)
    // Keep filename only if process was not canceled out or failed otherwise
    routeFilename = filename;

  return success;
}

Route RouteController::getRouteForSelection() const
{
  QList<int> rows = getSelectedRows(false /* reverse */);
  Route saveRoute(route);
  if(!rows.isEmpty())
    saveRoute.removeAllExceptRange(rows.constFirst(), rows.constLast());
  saveRoute.updateIndicesAndOffsets();
  saveRoute.getFlightplan().adjustDepartureAndDestination(true /* force */); // Fill departure and destination fields
  return saveRoute;
}

bool RouteController::saveFlightplanLnmAsSelection(const QString& filename)
{
  // Range must not contains procedures or alternates.
  QList<int> rows = getSelectedRows(false /* reverse */);
  qDebug() << Q_FUNC_INFO << filename << rows;

  if(!rows.isEmpty())
    return saveFlightplanLnmSelectionAs(filename, rows.constFirst(), rows.constLast());
  else
    return false;
}

bool RouteController::saveFlightplanLnm()
{
  qDebug() << Q_FUNC_INFO << routeFilename;
  return saveFlightplanLnmInternal(routeFilename, false /* silentShutdown */, true /* clearUndo */);
}

bool RouteController::saveFlightplanLnmSelectionAs(const QString& filename, int from, int to) const
{
  try
  {
    // Create a copy
    Route saveRoute(route);
    saveRoute.updateRouteCycleMetadata();

    // Remove all legs except the ones to save
    saveRoute.removeAllExceptRange(from, to);
    saveRoute.updateIndicesAndOffsets();

    // Procedures are never saved for a selection
    saveRoute.removeAllProcedureLegs();

    // Get plan without alternates and all altitude values erased
    Flightplan saveplan = saveRoute.zeroedAltitudes().adjustedToOptions(rf::DEFAULT_OPTS_LNMPLN_SAVE_SELECTED).getFlightplanConst();
    saveplan.adjustDepartureAndDestination(true /* force */);

    // Remove original comment
    saveplan.setComment(QString());

    // Add performance file type and name ===============
    saveRoute.updateAircraftPerfMetadata();

    // Save LNMPLN - Will throw an exception if something goes wrong
    flightplanIO->saveLnm(saveplan, filename);
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    return false;
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    return false;
  }
  return true;
}

bool RouteController::saveFlightplanLnmInternal(const QString& filename, bool silentShutdown, bool clearUndo)
{
  qDebug() << Q_FUNC_INFO << filename << silentShutdown;

  try
  {
    // Remember to send out signal
    bool routeHasChanged = false;

    // Update AIRAC cycle on original route ===============
    route.updateRouteCycleMetadata();

    // Add performance file type and name on original route  ===============
    route.updateAircraftPerfMetadata();

    // Check if any procedures were not loaded from the database ============================
    proc::MapProcedureTypes missingProcedures = route.getMissingProcedures();
    if(missingProcedures != proc::PROCEDURE_NONE)
    {
      qDebug() << Q_FUNC_INFO << "missingProcedures" << missingProcedures;

      if(!silentShutdown)
      {
        // Ask before saving file
        int result = dialog->showQuestionMsgBox(lnm::ACTIONS_SHOW_SAVE_LNMPLN_WARNING,
                                                tr("<p>One or more procedures in the flight plan are not valid.</p>"
                                                     "<ul><li>%1</li></ul>"
                                                       "<p>These will be dropped when saving.</p>"
                                                         "<p>Save flight plan now and drop invalid procedures?</p>").
                                                arg(procedureErrors.join(tr("</li><li>"))),
                                                tr("Do not &show this dialog again and save without warning."),
                                                QMessageBox::Cancel | QMessageBox::Save, QMessageBox::Cancel, QMessageBox::Save);

        if(result == QMessageBox::Cancel)
          return false;
        else if(result == QMessageBox::Help)
        {
          atools::gui::HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl % "FLIGHTPLANSAVE.html", lnm::helpLanguageOnline());
          return false;
        }
      }

      // Remove all missing procedures and flight plan legs as well as properties in plan
      route.removeProcedureLegs(missingProcedures);

      // Reload from database also to update the error message
      loadProceduresFromFlightplan(true /* clearOldProcedureProperties */, false /* cleanupRoute */, false /* autoresolveTransition */);

      // Copy loaded procedures back to properties to ensure that only valid ones are saved
      // Additionally remove duplicate waypoints
      route.updateProcedureLegs(entryBuilder, true /* clearOldProcedureProperties */, true /* cleanup route */);

      // Have to rebuild all after modification above
      route.updateAll();
      route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
      route.calculateLegAltitudes();
      route.updateDepartureAndDestination(false /* clearInvalidStart */); // Keep start/parking which was not found in database

      routeHasChanged = true;
    }

    // Create a copy which allows to change altitude
    // Copy altitudes to flight plan entries
    Flightplan flightplanCopy = route.updatedAltitudes().adjustedToOptions(rf::DEFAULT_OPTS_LNMPLN).getFlightplanConst();

    // Remember type, departure and destination for filename checking (save/saveAs)
    fileIfrVfr = flightplanCopy.getFlightplanType();
    fileCruiseAltFt = route.getCruiseAltitudeFt();
    fileDepartureIdent = flightplanCopy.getDepartureIdent();
    fileDestinationIdent = flightplanCopy.getDestinationIdent();

    // Set cruise altitude on plan copy in feet instead of local units
    flightplanCopy.setCruiseAltitudeFt(route.getCruiseAltitudeFt());

    // Save LNMPLN - Will throw an exception if something goes wrong
    flightplanIO->saveLnm(flightplanCopy, filename);

    // Set format to original route since it is saved as LNM now
    route.getFlightplan().setLnmFormat(true);

    if(clearUndo)
    {
      // Set clean undo state index since QUndoStack only returns weird values
      undoIndexClean = undoIndex;
      undoStack->setClean();
    }

    if(!silentShutdown)
    {
      updateRemarkHeader();
      NavApp::updateWindowTitle();

      if(routeHasChanged)
        emit routeChanged(false /* geometryChanged */, false /* newFlightPlan */);
    }

    qDebug() << "saveFlightplan undoIndex" << undoIndex << "undoIndexClean" << undoIndexClean;
  }
  catch(atools::Exception& e)
  {
    atools::gui::ErrorHandler(mainWindow).handleException(e);
    return false;
  }
  catch(...)
  {
    atools::gui::ErrorHandler(mainWindow).handleUnknownException();
    return false;
  }
  return true;
}

void RouteController::calculateDirect()
{
  qDebug() << Q_FUNC_INFO;

  // Stop any background tasks
  beforeRouteCalc();

  RouteCommand *undoCommand = preChange(tr("Direct Calculation"));

  clearTableSelection();
  route.removeRouteLegs();

  route.updateAll();
  route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
  route.calculateLegAltitudes();

  updateTableModelAndErrors();
  updateActions();
  postChange(undoCommand);
  emit routeChanged(true /* geometryChanged */);
  NavApp::setStatusMessage(tr("Calculated direct flight plan."));
}

void RouteController::beforeRouteCalc()
{
  routeAltDelayTimer.stop();
  emit preRouteCalc();
}

void RouteController::calculateRouteWindowSelection()
{
  qDebug() << Q_FUNC_INFO;
  routeCalcDialog->showForSelectionCalculation();
  routeCalcDialog->setCruisingAltitudeFt(route.getCruiseAltitudeFt());
  calculateRouteWindowShow();
}

void RouteController::calculateRouteWindowFull()
{
  qDebug() << Q_FUNC_INFO;
  routeCalcDialog->showForFullCalculation();
  routeCalcDialog->setCruisingAltitudeFt(route.getCruiseAltitudeFt());
  calculateRouteWindowShow();
}

void RouteController::calculateRouteWindowShow()
{
  qDebug() << Q_FUNC_INFO;

  // Always show - do not toggle
  routeCalcDialog->show();
  routeCalcDialog->raise();
  routeCalcDialog->activateWindow();

  // Transfer cruise altitude from flight plan window to calculation window
  routeCalcDialog->setCruisingAltitudeFt(route.getCruiseAltitudeFt());
}

void RouteController::calculateRoute()
{
  qDebug() << Q_FUNC_INFO;

  atools::routing::RouteNetwork *net = nullptr;
  QString command;
  atools::routing::Modes mode = atools::routing::MODE_NONE;
  bool fetchAirways = false;

  // Ignore events triggering follow due to selection changes
  atools::util::ContextSaverBool saver(ignoreFollowSelection);

  // Build configuration for route finder =======================================
  if(routeCalcDialog->getRoutingType() == rd::AIRWAY)
  {
    net = routeNetworkAirway;
    fetchAirways = true;

    // Airway preference =======================================
    switch(routeCalcDialog->getAirwayRoutingType())
    {
      case rd::BOTH:
        command = tr("Airway Flight Plan Calculation");
        mode = atools::routing::MODE_AIRWAY_WAYPOINT;
        break;

      case rd::VICTOR:
        command = tr("Low altitude airway Flight Plan Calculation");
        mode = atools::routing::MODE_VICTOR_WAYPOINT;
        break;

      case rd::JET:
        command = tr("High altitude airway Flight Plan Calculation");
        mode = atools::routing::MODE_JET_WAYPOINT;
        break;
    }

    // Airway/waypoint preference =======================================
    int pref = routeCalcDialog->getAirwayWaypointPreference();
    if(pref == RouteCalcDialog::AIRWAY_WAYPOINT_PREF_MIN)
      mode &= ~atools::routing::MODE_WAYPOINT;
    else if(pref == RouteCalcDialog::AIRWAY_WAYPOINT_PREF_MAX)
      mode &= ~atools::routing::MODE_AIRWAY;

    // RNAV setting
    if(routeCalcDialog->isAirwayNoRnav())
      mode |= atools::routing::MODE_NO_RNAV;

    // Use tracks like NAT or PACOTS
    if(routeCalcDialog->isUseTracks())
      mode |= atools::routing::MODE_TRACK;
  }
  else if(routeCalcDialog->getRoutingType() == rd::RADIONNAV)
  {
    // Radionav settings ========================================
    command = tr("Radionnav Flight Plan Calculation");
    fetchAirways = false;
    net = routeNetworkRadio;
    mode = atools::routing::MODE_RADIONAV_VOR;
    if(routeCalcDialog->isRadionavNdb())
      mode |= atools::routing::MODE_RADIONAV_NDB;
  }

  if(!net->isLoaded())
  {
    atools::routing::RouteNetworkLoader loader(NavApp::getDatabaseNav(), NavApp::getDatabaseTrack());
    loader.load(net);
  }

  atools::routing::RouteFinder routeFinder(net);
  routeFinder.setCostFactorForceAirways(routeCalcDialog->getAirwayPreferenceCostFactor());

  int fromIdx = -1, toIdx = -1;
  if(routeCalcDialog->isCalculateSelection())
  {
    fromIdx = routeCalcDialog->getRouteRangeFromIndex();
    toIdx = routeCalcDialog->getRouteRangeToIndex();

    // Disable certain optimizations in route finder - use nearest underlying point as start for departure position
    mode |= atools::routing::MODE_POINT_TO_POINT;
  }

  if(route.hasAnySidProcedure())
    // Disable certain optimizations in route finder - use nearest underlying point as start for departure position
    mode |= atools::routing::MODE_POINT_TO_POINT;

  if(calculateRouteInternal(&routeFinder, command, fetchAirways, routeCalcDialog->getCruisingAltitudeFt(), fromIdx, toIdx, mode))
    NavApp::setStatusMessage(tr("Calculated flight plan."));
  else
    NavApp::setStatusMessage(tr("No route found."));

  routeCalcDialog->updateWidgets();
}

void RouteController::clearAirwayNetworkCache()
{
  routeNetworkAirway->clear();
}

/* Calculate a flight plan to all types */
bool RouteController::calculateRouteInternal(atools::routing::RouteFinder *routeFinder, const QString& commandName,
                                             bool fetchAirways, float altitudeFt, int fromIndex, int toIndex,
                                             atools::routing::Modes mode)
{
  qDebug() << Q_FUNC_INFO;
  bool calcRange = fromIndex != -1 && toIndex != -1;
  int oldRouteSize = route.size();

  // Stop any background tasks
  beforeRouteCalc();

  Flightplan& flightplan = route.getFlightplan();

  // Load network from database if not already done
  QGuiApplication::setOverrideCursor(Qt::WaitCursor);

  Pos departurePos, destinationPos;

  if(calcRange)
  {
    fromIndex = std::max(route.getLastIndexOfDepartureProcedure(), fromIndex);
    toIndex = std::min(route.getDestinationIndexBeforeProcedure(), toIndex);

    departurePos = route.value(fromIndex).getPosition();
    destinationPos = route.value(toIndex).getPosition();
  }
  else
  {
    departurePos = route.getLastLegOfDepartureProcedure().getPosition();
    destinationPos = route.getDestinationBeforeProcedure().getPosition();
  }

  // ===================================================================
  // Set up a progress dialog which shows for all calculations taking more than half a second
  QProgressDialog progress(tr("Calculating Flight Plan ..."), tr("Cancel"), 0, 0, routeCalcDialog);
  progress.setWindowTitle(tr("Little Navmap - Calculating Flight Plan"));
  progress.setWindowFlags(progress.windowFlags() & ~Qt::WindowContextHelpButtonHint);
  progress.setWindowModality(Qt::ApplicationModal);
  progress.setMinimumDuration(500);

  bool dialogShown = false, canceled = false;
  routeFinder->setProgressCallback([&progress, &canceled, &dialogShown](int distToDest, int currentDistToDest) -> bool
  {
    QApplication::processEvents();
    progress.setMaximum(distToDest);
    progress.setValue(distToDest - currentDistToDest);
    canceled = progress.wasCanceled();

    if(!dialogShown && progress.isVisible())
    {
      // Dialog is shown - remove wait cursor
      dialogShown = true;
      QGuiApplication::restoreOverrideCursor();
    }

    return !canceled;
  });

  // Calculate the route - calls above lambda ================================================
  bool found = routeFinder->calculateRoute(departurePos, destinationPos, atools::roundToInt(altitudeFt), mode);

  if(!dialogShown)
    QGuiApplication::restoreOverrideCursor();

  qDebug() << Q_FUNC_INFO << "found" << found << "canceled" << canceled;

  // Hide dialog
  progress.reset();

  // Create wait cursor if calculation takes too long
  QGuiApplication::setOverrideCursor(Qt::WaitCursor);

  float distance = 0.f;
  QVector<RouteEntry> calculatedRoute;

  if(found && !canceled)
  {
    // A route was found

    // Fetch waypoints
    RouteExtractor extractor(routeFinder);
    extractor.extractRoute(calculatedRoute, distance);
    qDebug() << Q_FUNC_INFO << "Extracted size" << calculatedRoute.size();

    found = calculatedRoute.size() > 0;
  }

  if(found && !canceled)
  {
    // Compare to direct connection and check if route is too long
    float directDistance = departurePos.distanceMeterTo(destinationPos);
    float ratio = distance / directDistance;
    qDebug() << "route distance" << QString::number(distance, 'f', 0)
             << "direct distance" << QString::number(directDistance, 'f', 0) << "ratio" << ratio;

    if(ratio < MAX_DISTANCE_DIRECT_RATIO)
    {
      // Start undo
      RouteCommand *undoCommand = preChange(commandName);
      int numAlternateLegs = route.getNumAlternateLegs();

      if(calcRange)
      {
        flightplan[toIndex].setAirway(QString());
        flightplan[toIndex].setFlag(atools::fs::pln::entry::TRACK, false);
        flightplan.erase(flightplan.begin() + fromIndex + 1, flightplan.begin() + toIndex);
      }
      else
        // Erase all but start and destination
        flightplan.erase(flightplan.begin() + 1, flightplan.end() - numAlternateLegs - 1);

      int idx = 1;
      // Create flight plan entries - will be copied later to the route map objects
      for(const RouteEntry& routeEntry : qAsConst(calculatedRoute))
      {
        FlightplanEntry flightplanEntry;
        entryBuilder->buildFlightplanEntry(routeEntry.ref.id, atools::geo::EMPTY_POS, routeEntry.ref.objType,
                                           flightplanEntry, fetchAirways);
        if(fetchAirways && routeEntry.airwayId != -1)
          // Get airway by id - needed to fetch the name first
          updateFlightplanEntryAirway(routeEntry.airwayId, flightplanEntry);

        if(calcRange)
          flightplan.insert(flightplan.begin() + fromIndex + idx, flightplanEntry);
        else
          flightplan.insert(flightplan.end() - numAlternateLegs - 1, flightplanEntry);
        idx++;
      }

      // Remove procedure points from flight plan
      flightplan.removeProcedureEntries();

      // Copy flight plan to route object
      route.createRouteLegsFromFlightplan();

      // Reload procedures from properties
      loadProceduresFromFlightplan(true /* clearOldProcedureProperties */, false /* cleanupRoute */, false /* autoresolveTransition */);
      QGuiApplication::restoreOverrideCursor();

      // Remove duplicates in flight plan and route
      route.updateAll();

      // Set altitude in local units
      flightplan.setCruiseAltitudeFt(altitudeFt);

      route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);

      updateActiveLeg();

      route.calculateLegAltitudes();

      // Remove all airways violation restrictions during climb or descent
      clearAirwayViolations();

      updateTableModelAndErrors();
      updateActions();

      postChange(undoCommand);
      NavApp::updateWindowTitle();

#ifdef DEBUG_INFORMATION
      qDebug() << flightplan;
#endif

      NavApp::updateErrorLabel();

      if(calcRange)
      {
        // will also update route window
        int newToIndex = toIndex - (oldRouteSize - route.size());
        selectRange(fromIndex, newToIndex);
      }

      tableSelectionChanged(QItemSelection(), QItemSelection());

      emit routeChanged(true /* geometryChanged */);
    }
    else
      // Too long
      found = false;
  }

  QGuiApplication::restoreOverrideCursor();
  if(!found && !canceled)
    // Use routeCalcDialog as parent to avoid main raising in front
    atools::gui::Dialog(routeCalcDialog).showInfoMsgBox(lnm::ACTIONS_SHOW_ROUTE_ERROR,
                                                        tr("Cannot calculate flight plan.\n\n"
                                                           "Try another calculation type,\n"
                                                           "change the cruise altitude or\n"
                                                           "create the flight plan manually."),
                                                        tr("Do not &show this dialog again."));
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << route;
#endif

  return found;
}

void RouteController::adjustFlightplanAltitude()
{
  qDebug() << Q_FUNC_INFO;

  if(route.isEmpty())
    return;

  Flightplan& flightplan = route.getFlightplan();
  // Convert feet to local unit for adjusting
  float adjustedAltitudeLocal = route.getAdjustedAltitude(Unit::altFeetF(flightplan.getCruiseAltitudeFt()));

  if(atools::almostNotEqual(adjustedAltitudeLocal, Unit::altFeetF(flightplan.getCruiseAltitudeFt())))
  {
    RouteCommand *undoCommand = nullptr;

    undoCommand = preChange(tr("Adjust altitude"));
    // Convert local unit back to feet
    flightplan.setCruiseAltitudeFt(Unit::rev(adjustedAltitudeLocal, Unit::altFeetF));

    updateTableModelAndErrors();

    // Need to update again after updateAll and altitude change
    route.calculateLegAltitudes();

    postChange(undoCommand);

    NavApp::updateWindowTitle();
    NavApp::updateErrorLabel();

    if(!route.isEmpty())
      emit routeAltitudeChanged(route.getCruiseAltitudeFt());

    NavApp::setStatusMessage(tr("Adjusted flight plan altitude."));
  }
}

void RouteController::showInRoute(int index)
{
  qDebug() << Q_FUNC_INFO << index;

  if(index >= 0 && index < model->rowCount())
  {
    // Ignore events triggering follow due to selection changes
    atools::util::ContextSaverBool saver(ignoreFollowSelection);

    // Triggers tableSelectionChanged() immediately
    tableViewRoute->selectRow(index);
  }
}

void RouteController::reverseRoute()
{
  qDebug() << Q_FUNC_INFO;

  RouteCommand *undoCommand = preChange(tr("Reverse"));

  // Destination will be departure - check if valid
  bool departureIsAirport = route.hasValidDestination();

  clearTableSelection();

  // Remove all procedures and properties ===============================
  route.removeProcedureLegs(proc::PROCEDURE_ALL);
  route.removeAlternateLegs();

  // Reverse plan ===============================
  atools::fs::pln::Flightplan& flightplan = route.getFlightplan();
  std::reverse(flightplan.begin(), flightplan.end());

  // Update departure and destination ===============================
  flightplan.adjustDepartureAndDestination(true /* force */);

  // Overwrite parking position with airport position
  flightplan.setDepartureParkingPosition(flightplan.constFirst().getPosition(),
                                         atools::fs::pln::INVALID_ALTITUDE, atools::fs::pln::INVALID_HEADING);
  flightplan.setDepartureParkingName(QString());
  flightplan.setDepartureParkingType(departureIsAirport ? atools::fs::pln::AIRPORT : atools::fs::pln::NO_POS);

  // Erase all airways to avoid wrong direction travel against one way
  for(atools::fs::pln::FlightplanEntry& entry : flightplan)
    entry.setAirway(QString());
  flightplan.getProperties().remove(atools::fs::pln::PROCAIRWAY);

  // Rebuild route from flight plan ========================================
  route.createRouteLegsFromFlightplan();
  route.updateAll();
  route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
  route.calculateLegAltitudes();

  updateActiveLeg();
  updateTableModelAndErrors();
  updateActions();

  postChange(undoCommand);
  emit routeChanged(true /* geometryChanged */);
  NavApp::setStatusMessage(tr("Reversed flight plan."));
}

void RouteController::preDatabaseLoad()
{
  loadingDatabaseState = true;
  routeAltDelayTimer.stop();

  // Reset active to avoid crash when indexes change
  route.resetActive();
  highlightNextWaypoint(route.getActiveLegIndex());

  tempFlightplanStr.clear();
  Flightplan flightplan = route.updatedAltitudes().adjustedToOptions(rf::DEFAULT_OPTS_LNMPLN).getFlightplanConst();
  if(!flightplan.isEmpty())
    tempFlightplanStr = flightplanIO->saveLnmStr(flightplan);

#ifdef DEBUG_INFORMATION
  atools::strToFile(atools::settings::Settings::getConfigFilename("_debug.lnmpln"), tempFlightplanStr);
#endif
  routeCalcDialog->preDatabaseLoad();
}

void RouteController::postDatabaseLoad()
{
  // Clear routing caches
  routeNetworkRadio->clear();
  routeNetworkAirway->clear();
  clearAllErrors();

  Flightplan flightplan;

  if(!tempFlightplanStr.isEmpty())
  {
    flightplanIO->loadLnmStr(flightplan, tempFlightplanStr);
    flightplan.setLnmFormat(route.getFlightplan().isLnmFormat());
  }

  // Assign plan to route and get reference to it
  route.setFlightplan(flightplan);
  route.createRouteLegsFromFlightplan();
  loadProceduresFromFlightplan(false /* clearOldProcedureProperties */, false /* cleanupRoute */, false /* autoresolveTransition */);
  route.updateAll(); // Removes alternate property if not resolvable
  route.updateRouteCycleMetadata();
  route.updateAircraftPerfMetadata();

  // Update cruise altitude in local units based on procedures and airway restriction
  // Corrects cruise altitude if adjustRouteAltitude is true
  route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
  route.calculateLegAltitudes();
  updateActiveLeg();

  // Get number from user waypoint from user defined waypoint in fs flight plan
  entryBuilder->setCurUserpointNumber(route.getNextUserWaypointNumber());

  updateTableModelAndErrors();
  updateActions();
  NavApp::updateErrorLabel();

  routeCalcDialog->postDatabaseLoad();

  NavApp::updateWindowTitle();
  loadingDatabaseState = false;
}

/* Double click into table view */
void RouteController::doubleClick(const QModelIndex& index)
{
  qDebug() << Q_FUNC_INFO;
  if(index.isValid())
  {
    qDebug() << "mouseDoubleClickEvent";
    showAtIndex(index.row(), true /* info */, true /* map */, true /* doubleClick */);
  }
}

void RouteController::showAtIndex(int index, bool info, bool map, bool doubleClick)
{
  if(index >= 0 && index < map::INVALID_INDEX_VALUE)
  {
    const RouteLeg& routeLeg = route.value(index);
    if(routeLeg.isValid())
    {
      if(map)
      {
        if(routeLeg.getMapType() == map::AIRPORT)
          emit showRect(routeLeg.getAirport().bounding, doubleClick);
        else
          emit showPos(routeLeg.getPosition(), 0.f, doubleClick);
      }

      if(info)
        showInformationInternal(routeLeg);
    }
  }
}

void RouteController::updateActions()
{
  // Disable all per default
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->actionRouteLegUp->setEnabled(false);
  ui->actionRouteLegDown->setEnabled(false);
  ui->actionRouteDeleteLeg->setEnabled(false);
  ui->actionRouteConvertProcedure->setEnabled(false);

  // Empty - nothing to move or delete
  if(model->rowCount() == 0)
    return;

  // One or more legs - check for details like procedures or procedure boundary
  QItemSelectionModel *selectionModel = tableViewRoute->selectionModel();
  if(selectionModel != nullptr && selectionModel->hasSelection())
  {
    bool containsProc, containsAlternate, moveDownTouchesProc, moveUpTouchesProc, moveDownTouchesAlt, moveUpTouchesAlt, moveDownLeavesAlt,
         moveUpLeavesAlt;
    // Ordered from top to bottom
    QList<int> rows = getSelectedRows(false /* reverse */);
    route.selectionFlagsProcedures(rows, containsProc, moveDownTouchesProc, moveUpTouchesProc);
    route.selectionFlagsAlternate(rows, containsAlternate, moveDownTouchesAlt, moveUpTouchesAlt, moveDownLeavesAlt, moveUpLeavesAlt);

    if(rows.size() == 1 && containsAlternate)
    {
      ui->actionRouteLegUp->setEnabled(!moveUpLeavesAlt);
      ui->actionRouteLegDown->setEnabled(!moveDownLeavesAlt);
      ui->actionRouteDeleteLeg->setEnabled(true);
    }
    else if(model->rowCount() > 1)
    {
      ui->actionRouteDeleteLeg->setEnabled(true);
      ui->actionRouteConvertProcedure->setEnabled(containsProc);

      if(selectionModel->hasSelection())
      {
        ui->actionRouteLegUp->setEnabled(!selectionModel->isRowSelected(0, QModelIndex()) &&
                                         !containsProc && !containsAlternate && !moveUpTouchesProc && !moveUpTouchesAlt);

        ui->actionRouteLegDown->setEnabled(!selectionModel->isRowSelected(model->rowCount() - 1, QModelIndex()) &&
                                           !containsProc && !containsAlternate && !moveDownTouchesProc &&
                                           !moveDownTouchesAlt);
      }
    }
    else if(model->rowCount() == 1)
      // Only one waypoint - nothing to move
      ui->actionRouteDeleteLeg->setEnabled(true);
  }
}

void RouteController::showInformationTriggered()
{
  qDebug() << Q_FUNC_INFO;
  QModelIndex index = tableViewRoute->currentIndex();
  if(index.isValid())
    showInformationInternal(route.value(index.row()));
}

void RouteController::showInformationInternal(const RouteLeg& routeLeg)
{
  if(routeLeg.isAnyProcedure())
  {
    if(routeLeg.getProcedureLeg().navaids.hasTypes(map::AIRPORT | map::WAYPOINT | map::VOR | map::NDB))
      emit showInformation(routeLeg.getProcedureLeg().navaids);
  }
  else
  {
    map::MapResult result;
    NavApp::getMapQueryGui()->getMapObjectById(result, routeLeg.getMapType(), map::AIRSPACE_SRC_NONE,
                                               routeLeg.getId(), false /* airport from nav database */);
    emit showInformation(result);
  }
}

void RouteController::showProceduresTriggered()
{
  QModelIndex index = tableViewRoute->currentIndex();
  if(index.isValid())
  {
    const RouteLeg& routeLeg = route.value(index.row());

    if(routeLeg.isValidWaypoint() && routeLeg.getMapType() == map::AIRPORT)
    {
      bool departureFilter, arrivalFilter;
      route.getAirportProcedureFlags(routeLeg.getAirport(), index.row(), departureFilter, arrivalFilter);
      emit showProcedures(routeLeg.getAirport(), departureFilter, arrivalFilter);
    }
  }
}

/* From context menu */
void RouteController::showOnMapTriggered()
{
  QModelIndex index = tableViewRoute->currentIndex();
  if(index.isValid())
  {
    const RouteLeg& routeLeg = route.value(index.row());

    if(routeLeg.getMapType() == map::AIRPORT)
      emit showRect(routeLeg.getAirport().bounding, false);
    else
      emit showPos(routeLeg.getPosition(), 0.f, false);

    if(routeLeg.getMapType() == map::AIRPORT)
      NavApp::setStatusMessage(tr("Showing airport on map."));
    else
      NavApp::setStatusMessage(tr("Showing navaid on map."));
  }
}

void RouteController::routeTableOptions()
{
  qDebug() << Q_FUNC_INFO;

  atools::gui::TreeDialog treeDialog(mainWindow, QCoreApplication::applicationName() % tr(" - Flight Plan Table"),
                                     tr("Select header lines and flight plan table columns to show.\n"
                                        "You can move and resize columns by clicking into the flight plan column headers."),
                                     lnm::ROUTE_FLIGHTPLAN_COLUMS_DIALOG, "FLIGHTPLAN.html#flight-plan-table-columns",
                                     true /* showExpandCollapse */);

  treeDialog.setHelpOnlineUrl(lnm::helpOnlineUrl);
  treeDialog.setHelpLanguageOnline(lnm::helpLanguageOnline());
  treeDialog.setHeader({tr("Column Name"), tr("Description")});

  // Add header options to tree ========================================
  QTreeWidgetItem *headerItem = treeDialog.addTopItem1(tr("Flight Plan Table Header"));
  treeDialog.addItem2(headerItem, rcol::HEADER_AIRPORTS, tr("Airports"),
                      tr("Departure and destination airports as links to show them on the map and in information."),
                      routeLabel->isFlag(routelabel::HEADER_AIRPORTS));

  treeDialog.addItem2(headerItem, rcol::HEADER_DIST_TIME, tr("Distance and Time"), tr("Flight plan total distance and flight time."),
                      routeLabel->isFlag(routelabel::HEADER_DISTTIME));

  treeDialog.addItem2(headerItem, rcol::HEADER_TAKEOFF, tr("Takeoff Runway"),
                      tr("Departure runway heading and length."), routeLabel->isFlag(routelabel::HEADER_RUNWAY_TAKEOFF));

  treeDialog.addItem2(headerItem, rcol::HEADER_TAKEOFF_WIND, tr("Takeoff Wind"),
                      tr("Headwind on takeoff runway indicated by %1 and tailwind by %2\n"
                         "as well as crosswind (%3 or %4).\n"
                         "The wind information is taken from the selected source in\n"
                         "menu \"Weather\" -> \"Airport Weather Source\".\n"
                         "Wind is taken from nearest airport if not available.").
                      arg(TextPointer::getWindPointerSouth()).arg(TextPointer::getWindPointerNorth()).
                      arg(TextPointer::getWindPointerEast()).arg(TextPointer::getWindPointerWest()),
                      routeLabel->isFlag(routelabel::HEADER_RUNWAY_TAKEOFF_WIND));

  treeDialog.addItem2(headerItem, rcol::HEADER_DEPARTURE, tr("Departure"),
                      tr("SID information."), routeLabel->isFlag(routelabel::HEADER_DEPARTURE));

  treeDialog.addItem2(headerItem, rcol::HEADER_ARRIVAL, tr("Arrival and Approach"),
                      tr("STAR and approach procedure information."), routeLabel->isFlag(routelabel::HEADER_ARRIVAL));

  treeDialog.addItem2(headerItem, rcol::HEADER_LAND, tr("Landing Runway"),
                      tr("Destination runway heading, available distance for landing, elevation and facilities."),
                      routeLabel->isFlag(routelabel::HEADER_RUNWAY_LAND));

  treeDialog.addItem2(headerItem, rcol::HEADER_LAND_WIND, tr("Landing Wind"),
                      tr("Destination runway heading, available distance for landing, elevation and facilities."),
                      tr("Headwind on landing runway indicated by %1 and tailwind by %2\n"
                         "as well as crosswind (%3 or %4).\n"
                         "The wind information is taken from the selected source in\n"
                         "menu \"Weather\" -> \"Airport Weather Source\".\n"
                         "Wind is taken from nearest airport if not available.").
                      arg(TextPointer::getWindPointerSouth()).arg(TextPointer::getWindPointerNorth()).
                      arg(TextPointer::getWindPointerEast()).arg(TextPointer::getWindPointerWest()),
                      routeLabel->isFlag(routelabel::HEADER_RUNWAY_LAND_WIND));

  // Add footer options to tree ========================================
  QTreeWidgetItem *footerItem = treeDialog.addTopItem1(tr("Flight Plan Table Footer"));
  treeDialog.addItem2(footerItem, rcol::FOOTER_SELECTION, tr("Selected Flight Plan Legs"),
                      tr("Show distance, time and fuel for selected flight plan legs."),
                      routeLabel->isFlag(routelabel::FOOTER_SELECTION));
  treeDialog.addItem2(footerItem, rcol::FOOTER_ERROR, tr("Error Messages"),
                      tr("Show error messages for flight plan elements like missing airports and more.\n"
                         "It is strongly recommended to keep the error label enabled."),
                      routeLabel->isFlag(routelabel::FOOTER_ERROR));

  // Add column names and description texts to tree ====================
  QTreeWidgetItem *tableItem = treeDialog.addTopItem1(tr("Flight plan table columns"));
  QHeaderView *header = tableViewRoute->horizontalHeader();
  for(int col = rcol::FIRST_COLUMN; col <= rcol::LAST_COLUMN; col++)
  {
    QString textCol1 = Unit::replacePlaceholders(routeColumns.at(col)).replace("\n", " ");
    QString textCol2 = routeColumnDescription.at(col);
    treeDialog.addItem2(tableItem, col, textCol1, textCol2, !header->isSectionHidden(col));
  }
  treeDialog.restoreState(false /* restoreCheckState */, true /* restoreExpandState */);

  if(treeDialog.exec() == QDialog::Accepted)
  {
    treeDialog.saveState(false /* saveCheckState */, true /* saveExpandState */);

    // Hidden sections are saved with the view
    for(int col = rcol::LAST_COLUMN; col >= rcol::FIRST_COLUMN; col--)
      header->setSectionHidden(col, !treeDialog.isItemChecked(col));

    // Set label options ==========================
    routeLabel->setFlag(routelabel::HEADER_AIRPORTS, treeDialog.isItemChecked(rcol::HEADER_AIRPORTS));
    routeLabel->setFlag(routelabel::HEADER_DISTTIME, treeDialog.isItemChecked(rcol::HEADER_DIST_TIME));

    routeLabel->setFlag(routelabel::HEADER_RUNWAY_TAKEOFF, treeDialog.isItemChecked(rcol::HEADER_TAKEOFF));
    routeLabel->setFlag(routelabel::HEADER_RUNWAY_TAKEOFF_WIND, treeDialog.isItemChecked(rcol::HEADER_TAKEOFF_WIND));
    routeLabel->setFlag(routelabel::HEADER_DEPARTURE, treeDialog.isItemChecked(rcol::HEADER_DEPARTURE));

    routeLabel->setFlag(routelabel::HEADER_ARRIVAL, treeDialog.isItemChecked(rcol::HEADER_ARRIVAL));
    routeLabel->setFlag(routelabel::HEADER_RUNWAY_LAND, treeDialog.isItemChecked(rcol::HEADER_LAND));
    routeLabel->setFlag(routelabel::HEADER_RUNWAY_LAND_WIND, treeDialog.isItemChecked(rcol::HEADER_LAND_WIND));

    routeLabel->setFlag(routelabel::FOOTER_SELECTION, treeDialog.isItemChecked(rcol::FOOTER_SELECTION));
    routeLabel->setFlag(routelabel::FOOTER_ERROR, treeDialog.isItemChecked(rcol::FOOTER_ERROR));

    // Update all =====================
    updateModelTimeFuelWindAlt();
    updateModelHighlightsAndErrors();

    routeLabel->updateHeaderLabel();
    routeLabel->updateFooterSelectionLabel();
    routeLabel->updateFooterErrorLabel();

    highlightNextWaypoint(route.getActiveLegIndexCorrected());
  }
}

void RouteController::activateLegTriggered()
{
  QModelIndex index = tableViewRoute->currentIndex();
  if(index.isValid())
    activateLegManually(index.row());
}

void RouteController::helpClicked()
{
  atools::gui::HelpHandler::openHelpUrlWeb(mainWindow, lnm::helpOnlineUrl % "FLIGHTPLAN.html", lnm::helpLanguageOnline());
}

void RouteController::selectAllTriggered()
{
  tableViewRoute->selectAll();
}

bool RouteController::canCalcSelection()
{
  // Check if selected rows contain a procedure or if a procedure is between first and last selection
  if(selectedRows.size() > 1)
    return route.canCalcSelection(selectedRows.constFirst(), selectedRows.constLast());

  return false;
}

bool RouteController::canSaveSelection()
{
  // Check if selected rows contain a procedure or if a procedure is between first and last selection
  if(selectedRows.size() > 1)
    return route.canSaveSelection(selectedRows.constFirst(), selectedRows.constLast());

  return false;
}

void RouteController::tableContextMenu(const QPoint& pos)
{
  static const int NAVAID_NAMES_ELIDE = 15;

  Ui::MainWindow *ui = NavApp::getMainUi();
  contextMenuOpen = true;
  QPoint menuPos = QCursor::pos();
  // Use widget center if position is not inside widget
  if(!tableViewRoute->rect().contains(tableViewRoute->mapFromGlobal(QCursor::pos())))
    menuPos = tableViewRoute->mapToGlobal(tableViewRoute->rect().center());

  // Move menu position off the cursor to avoid accidental selection on touchpads
  menuPos += QPoint(3, 3);

  qDebug() << "tableContextMenu";

  const static QList<QAction *> ACTIONS({ui->actionRouteShowInformation, ui->actionRouteShowApproaches, ui->actionRouteShowApproachCustom,
                                         ui->actionRouteShowDepartureCustom, ui->actionRouteShowOnMap, ui->actionRouteActivateLeg,
                                         ui->actionRouteDirectTo, ui->actionRouteFollowSelection, ui->actionRouteLegUp,
                                         ui->actionRouteLegDown, ui->actionRouteDeleteLeg, ui->actionRouteEditUserWaypoint,
                                         ui->actionRouteInsert, ui->actionRouteTableAppend, ui->actionRouteSaveSelection,
                                         ui->actionRouteCalcSelected, ui->actionMapRangeRings, ui->actionMapNavaidRange,
                                         ui->actionMapTrafficPattern, ui->actionMapHold, ui->actionMapAirportMsa,
                                         ui->actionRouteTableCopy, ui->actionRouteTableSelectAll, ui->actionRouteTableSelectNothing,
                                         ui->actionRouteResetView, ui->actionRouteDisplayOptions, ui->actionRouteSetMark,
                                         ui->actionRouteMarkAddon, ui->actionRouteConvertProcedure});

  // Save text which will be changed below - Re-enable actions on exit to allow keystrokes
  atools::gui::ActionTool actionTool(ACTIONS);

  // Get table index ===========================================================
  QModelIndex index = tableViewRoute->indexAt(pos);
  QItemSelectionModel *selectionModel = tableViewRoute->selectionModel();
  if(selectionModel != nullptr && model->rowCount() > 0)
  {
    if(index.isValid())
      // Click spot is valid - set current but do not update selection
      selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate | QItemSelectionModel::Rows);
    else
    {
      // Invalid click spot - fall back to selection and get first field there
      // Update current for ...Triggered() methods called by actions
      QList<int> rows = atools::gui::selectedRows(tableViewRoute->selectionModel(), false /* reverse */);
      if(rows.size() == 1)
      {
        // Move current to one selected row
        index = model->index(rows.constFirst(), 0);
        selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate | QItemSelectionModel::Rows);
      }
      else
        // Use current - keep all disabled if this is not valid
        index = tableViewRoute->currentIndex();
    }
  }

  const RouteLeg *routeLeg = nullptr, *prevRouteLeg = nullptr;
  int row = -1;
  if(index.isValid())
  {
    row = index.row();
    routeLeg = &route.value(row);
    if(index.row() > 0)
      prevRouteLeg = &route.value(row - 1);
  }

  QString objectText, airportText;
  if(routeLeg != nullptr)
  {
    objectText = routeLeg->getDisplayText(NAVAID_NAMES_ELIDE);
    if(routeLeg->getMapType() == map::AIRPORT)
      airportText = objectText;
  }

  QMenu menu;
  menu.setToolTipsVisible(NavApp::isMenuToolTipsVisible());

  // Enable/disable move, delete and direct to
  updateActions();

  ui->actionRouteTableCopy->setEnabled(index.isValid());

  bool canInsert = false;

  ui->actionRouteShowApproachCustom->setEnabled(false);
  ui->actionRouteShowDepartureCustom->setEnabled(false);
  ui->actionRouteShowApproaches->setEnabled(false);
  ui->actionRouteEditUserWaypoint->setEnabled(false);
  ui->actionRouteShowInformation->setEnabled(false);
  ui->actionRouteMarkAddon->setEnabled(false);
  ui->actionRouteConvertProcedure->setEnabled(false);

  // Collect information for selected leg =======================================================================
  proc::MapProcedureTypes baseType = proc::PROCEDURE_NONE;

  // Menu above a row
  map::MapResult msaResult;
  if(routeLeg != nullptr)
  {
    // Get base type with transitions removed
    baseType = proc::MapProcedureLegs::getProcedureTypeBase(routeLeg->getProcedureType());

    if(routeLeg->getVor().isValid() || routeLeg->getNdb().isValid() || routeLeg->getWaypoint().isValid() || routeLeg->isAirport())
      NavApp::getMapQueryGui()->getMapObjectByIdent(msaResult, map::AIRPORT_MSA, routeLeg->getIdent(),
                                                    routeLeg->getRegion(), QString(), routeLeg->getPosition());

    if(routeLeg->isAnyProcedure())
    {
      if(routeLeg->getProcedureLeg().navaids.hasTypes(map::AIRPORT | map::WAYPOINT | map::VOR | map::NDB))
        ui->actionRouteShowInformation->setEnabled(true);

      ui->actionRouteConvertProcedure->setEnabled(true);

      // Get two names in case of converting approach. Then also convert the STAR as well
      QString procName1, procName2;
      if(baseType == proc::PROCEDURE_APPROACH && route.hasAnyStarProcedure())
        procName2 = route.getProcedureLegText(proc::PROCEDURE_STAR, false /* includeRunway */,
                                              true /* missedAsApproach */, true /* transitionAsProcedure */);

      procName1 = route.getProcedureLegText(baseType, false /* includeRunway */,
                                            true /* missedAsApproach */, true /* transitionAsProcedure */);

      ActionTool::setText(ui->actionRouteConvertProcedure, true, atools::strJoin({procName1, procName2}, tr(", "), tr(" and ")));
    }
    else
    {
      ui->actionRouteConvertProcedure->setText(ui->actionRouteConvertProcedure->text().arg(tr("Procedure")));
      ui->actionRouteShowInformation->setEnabled(routeLeg->isValidWaypoint() && routeLeg->getMapType() != map::USERPOINTROUTE &&
                                                 routeLeg->getMapType() != map::INVALID);
    }

    if(routeLeg->isValidWaypoint())
    {
      if(prevRouteLeg == nullptr)
        // allow to insert before first one
        canInsert = true;
      else if(prevRouteLeg->isRoute() && routeLeg->isAnyProcedure() && routeLeg->getProcedureType() & proc::PROCEDURE_ARRIVAL_ALL)
        // Insert between enroute waypoint and approach or STAR
        canInsert = true;
      else if(routeLeg->isRoute() && prevRouteLeg->isAnyProcedure() && prevRouteLeg->getProcedureType() & proc::PROCEDURE_DEPARTURE)
        // Insert between SID and waypoint
        canInsert = true;
      else
        canInsert = routeLeg->isRoute();
    }

    // Prepare procedure menus ==============================================================================

    if(routeLeg->isValidWaypoint() && routeLeg->getMapType() == map::AIRPORT)
    {
      ui->actionRouteMarkAddon->setEnabled(true);
      ActionTool::setText(ui->actionRouteMarkAddon, true, objectText);

      bool departureFilter, arrivalFilter, hasDeparture, hasAnyArrival, airportDeparture, airportDestination, airportRoundTrip;
      route.getAirportProcedureFlags(routeLeg->getAirport(), row, departureFilter, arrivalFilter, hasDeparture, hasAnyArrival,
                                     airportDeparture, airportDestination, airportRoundTrip);

      if(hasAnyArrival || hasDeparture)
      {
        if(airportDeparture && !airportRoundTrip)
        {
          if(hasDeparture)
          {
            ui->actionRouteShowApproaches->setText(tr("Show Departure &Procedures for %1"));
            ActionTool::setText(ui->actionRouteShowApproaches, true, objectText);
          }
          else
            ui->actionRouteShowApproaches->setText(tr("Show procedures (no departure procedure)"));
        }
        else if(airportDestination && !airportRoundTrip)
        {
          if(hasAnyArrival)
          {
            ui->actionRouteShowApproaches->setText(tr("Show Arrival/Approach &Procedures for %1"));
            ActionTool::setText(ui->actionRouteShowApproaches, true, objectText);
          }
          else
            ui->actionRouteShowApproaches->setText(tr("Show &Procedures (no arrival/approch procedure)"));
        }
        else
        {
          ui->actionRouteShowApproaches->setText(tr("Show Procedures for %1"));
          ActionTool::setText(ui->actionRouteShowApproaches, true, objectText);
        }
      }
      else
        ui->actionRouteShowApproaches->setText(tr("Show Procedures (no procedure)"));

      if(airportDestination)
      {
        ui->actionRouteShowApproachCustom->setText(tr("Select Destination &Runway for %1 ..."));
        ui->actionRouteShowApproachCustom->setEnabled(true);
      }
      else if(!airportDeparture)
      {
        ui->actionRouteShowApproachCustom->setText(tr("Select &Runway and use %1 as Destination ..."));
        ui->actionRouteShowApproachCustom->setEnabled(true);
      }

      if(airportDeparture)
      {
        ui->actionRouteShowDepartureCustom->setText(tr("Select &Departure Runway for %1 ..."));
        ui->actionRouteShowDepartureCustom->setEnabled(true);
      }
      else if(!airportDestination)
      {
        ui->actionRouteShowDepartureCustom->setText(tr("Select Runway and use %1 as &Departure ..."));
        ui->actionRouteShowDepartureCustom->setEnabled(true);
      }
    }
    else
    {
      ui->actionRouteShowApproaches->setText(tr("Show &Procedures"));
      ui->actionRouteShowApproachCustom->setText(tr("Select Destination &Runway for %1 ..."));
      ui->actionRouteShowDepartureCustom->setText(tr("Select &Departure Runway for %1 ..."));
    }

    if(routeLeg->getAirport().noRunways())
    {
      QString norw = routeLeg->isAirport() ? tr(" (no runway)") : QString();
      ActionTool::setText(ui->actionRouteShowApproachCustom, false, QString(), norw);
      ActionTool::setText(ui->actionRouteShowDepartureCustom, false, QString(), norw);
    }
    else
    {
      ActionTool::setText(ui->actionRouteShowApproachCustom, objectText);
      ActionTool::setText(ui->actionRouteShowDepartureCustom, objectText);
    }

    ui->actionRouteShowOnMap->setEnabled(true);
    ui->actionMapRangeRings->setEnabled(true);
    ui->actionRouteSetMark->setEnabled(true);

#ifdef DEBUG_MOVING_AIRPLANE
    ui->actionRouteActivateLeg->setEnabled(routeLeg->isValidWaypoint());
#else
    ui->actionRouteActivateLeg->setEnabled(routeLeg->isValid() && NavApp::isConnected());
#endif
  }
  else
  {
    ui->actionRouteShowInformation->setEnabled(false);
    ui->actionRouteShowApproaches->setEnabled(false);
    ui->actionRouteActivateLeg->setEnabled(false);
    ui->actionRouteShowOnMap->setEnabled(false);
    ui->actionMapRangeRings->setEnabled(false);
    ui->actionRouteSetMark->setEnabled(false);
  }

  ui->actionRouteSaveSelection->setEnabled(canSaveSelection());
  ui->actionRouteTableAppend->setEnabled(!route.isEmpty());
  ui->actionRouteInsert->setEnabled(canInsert);
  ui->actionRouteCalcSelected->setEnabled(canCalcSelection());
  ui->actionRouteTableSelectNothing->setEnabled(tableViewRoute->selectionModel() == nullptr ?
                                                false : tableViewRoute->selectionModel()->hasSelection());
  ui->actionRouteTableSelectAll->setEnabled(!route.isEmpty());

  // Edit position ======================================
  ui->actionRouteEditUserWaypoint->setText(tr("Edit &Position or Remarks ..."));
  if(routeLeg != nullptr)
  {
    if(routeLeg->getMapType() == map::USERPOINTROUTE)
    {
      ui->actionRouteEditUserWaypoint->setEnabled(true);
      ui->actionRouteEditUserWaypoint->setText(tr("&Edit %1 ..."));
      ui->actionRouteEditUserWaypoint->setToolTip(tr("Edit name and coordinates of user defined flight plan position"));
      ui->actionRouteEditUserWaypoint->setStatusTip(ui->actionRouteEditUserWaypoint->toolTip());
    }
    else if(route.canEditComment(row))
    {
      ui->actionRouteEditUserWaypoint->setEnabled(true);
      ui->actionRouteEditUserWaypoint->setText(tr("Edit &Remarks for %1 ..."));
      ui->actionRouteEditUserWaypoint->setToolTip(tr("Edit remarks for selected flight plan leg"));
      ui->actionRouteEditUserWaypoint->setStatusTip(ui->actionRouteEditUserWaypoint->toolTip());
    }
  }

  // Get selected VOR and NDB and check if ranges can be shown ====================================
  // If there are any radio navaids in the selected list enable range menu item
  int numRadioNavaids = 0;
  QString navaidText;
  for(int idx : qAsConst(selectedRows))
  {
    const RouteLeg& leg = route.value(idx);
    if((leg.getVor().isValid() && leg.getVor().range > 0) || (leg.getNdb().isValid() && leg.getNdb().range > 0))
    {
      navaidText = leg.getDisplayText();
      numRadioNavaids++;
    }
  }

  // Direct ==============================================================================
  if(!selectedRows.isEmpty())
  {
    bool disableDirect = true;
    QString suffix = proc::procedureTextSuffixDirectTo(route, selectedRows.constFirst(), &routeLeg->getAirport(), &disableDirect);
    ui->actionRouteDirectTo->setDisabled(disableDirect);
    ActionTool::setText(ui->actionRouteDirectTo, !disableDirect, objectText, suffix);
  }
  else
    ui->actionRouteDirectTo->setDisabled(true);

  // Add marks ==============================================================================
  ActionTool::setText(ui->actionMapRangeRings, routeLeg != nullptr, objectText);

  if(numRadioNavaids == 0)
    ui->actionMapNavaidRange->setEnabled(false);
  else if(numRadioNavaids == 1)
    ActionTool::setText(ui->actionMapNavaidRange, routeLeg != nullptr, navaidText);
  else if(numRadioNavaids > 1)
    ActionTool::setText(ui->actionMapNavaidRange, routeLeg != nullptr, tr("selected navaids"));

  ActionTool::setText(ui->actionMapHold, routeLeg != nullptr, objectText);

  ui->actionMapAirportMsa->setEnabled(msaResult.hasAirportMsa());

  if(routeLeg != nullptr && routeLeg->isAirport())
    ActionTool::setText(ui->actionMapTrafficPattern, !routeLeg->getAirport().noRunways(), objectText, tr(" (no runway)"));
  else
    ActionTool::setText(ui->actionMapTrafficPattern, false);

  // build menu ====================================================================

  // Replace any left over placeholders in action text
  actionTool.finishTexts(objectText);

  menu.addAction(ui->actionRouteShowInformation);
  menu.addAction(ui->actionRouteShowOnMap);
  menu.addSeparator();

  menu.addAction(ui->actionRouteShowDepartureCustom);
  menu.addAction(ui->actionRouteShowApproachCustom);
  menu.addSeparator();

  menu.addAction(ui->actionRouteShowApproaches);
  menu.addSeparator();

  menu.addAction(ui->actionRouteActivateLeg);
  menu.addSeparator();

  menu.addAction(undoAction);
  menu.addAction(redoAction);
  menu.addSeparator();

  menu.addAction(ui->actionRouteLegUp);
  menu.addAction(ui->actionRouteLegDown);
  menu.addAction(ui->actionRouteDeleteLeg);
  menu.addAction(ui->actionRouteEditUserWaypoint);
  menu.addSeparator();

  menu.addAction(ui->actionRouteConvertProcedure);
  menu.addSeparator();

  menu.addAction(ui->actionRouteDirectTo);
  menu.addSeparator();

  menu.addAction(ui->actionRouteInsert);
  menu.addAction(ui->actionRouteTableAppend);
  menu.addAction(ui->actionRouteSaveSelection);
  menu.addSeparator();

  menu.addAction(ui->actionRouteCalcSelected);
  menu.addSeparator();

  menu.addAction(ui->actionMapRangeRings);
  menu.addAction(ui->actionMapNavaidRange);
  menu.addAction(ui->actionMapTrafficPattern);
  menu.addAction(ui->actionMapHold);
  menu.addAction(ui->actionMapAirportMsa);
  menu.addSeparator();

  menu.addAction(ui->actionRouteMarkAddon);
  menu.addSeparator();

  menu.addAction(ui->actionRouteFollowSelection);
  menu.addSeparator();

  menu.addAction(ui->actionRouteTableCopy);
  menu.addAction(ui->actionRouteTableSelectAll);
  menu.addAction(ui->actionRouteTableSelectNothing);
  menu.addSeparator();

  menu.addAction(ui->actionRouteResetView);
  menu.addSeparator();

  menu.addAction(ui->actionRouteSetMark);
  menu.addSeparator();

  menu.addAction(ui->actionRouteDisplayOptions);

  // Execute menu =========================================================================================
  QAction *action = menu.exec(menuPos);
  if(action != nullptr)
    qDebug() << Q_FUNC_INFO << "selected" << action->text();
  else
    qDebug() << Q_FUNC_INFO << "no action selected";

  if(action != nullptr)
  {
    if(action == ui->actionRouteResetView)
    {
      for(int col = rcol::FIRST_COLUMN; col <= rcol::LAST_COLUMN; col++)
        tableViewRoute->showColumn(col);

      // Reorder columns to match model order
      QHeaderView *header = tableViewRoute->horizontalHeader();
      for(int i = 0; i < header->count(); i++)
        header->moveSection(header->visualIndex(i), i);

      tableViewRoute->resizeColumnsToContents();
      NavApp::setStatusMessage(tr("Table view reset to defaults."));
    }
    else if(action == ui->actionRouteSetMark && routeLeg != nullptr)
      emit changeMark(routeLeg->getPosition());
    else if(action == ui->actionMapRangeRings && routeLeg != nullptr)
      NavApp::getMapWidgetGui()->addRangeMark(routeLeg->getPosition(), true /* showDialog */);
    else if(action == ui->actionMapTrafficPattern && routeLeg != nullptr)
      NavApp::getMapWidgetGui()->addPatternMark(routeLeg->getAirport());
    else if(action == ui->actionMapHold && routeLeg != nullptr)
    {
      map::MapResult result;
      NavApp::getMapQueryGui()->getMapObjectById(result, routeLeg->getMapType(), map::AIRSPACE_SRC_NONE,
                                                 routeLeg->getId(), false /* airport from nav*/);

      if(!result.isEmpty(map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT))
        NavApp::getMapWidgetGui()->addHold(result, atools::geo::EMPTY_POS);
      else
        NavApp::getMapWidgetGui()->addHold(result, routeLeg->getPosition());
    }
    else if(action == ui->actionMapNavaidRange)
    {
      // Show range rings for all radio navaids
      for(int idx : qAsConst(selectedRows))
      {
        const RouteLeg& routeLegSel = route.value(idx);
        if(routeLegSel.getNdb().isValid() || routeLegSel.getVor().isValid())
        {
          map::MapTypes type = routeLegSel.getMapType();
          if(routeLegSel.isAnyProcedure())
          {
            if(routeLegSel.getNdb().isValid())
              type = map::NDB;
            if(routeLegSel.getVor().isValid())
              type = map::VOR;
          }

          if(routeLegSel.getRange() > 0)
            NavApp::getMapWidgetGui()->addNavRangeMark(routeLegSel.getPosition(), type, routeLegSel.getDisplayIdent(),
                                                       routeLegSel.getFrequencyOrChannel(), routeLegSel.getRange());
        }
      }
    }
    else if(action == ui->actionRouteConvertProcedure)
      convertProcedure(baseType);
    else if(action == ui->actionRouteMarkAddon)
    {
      QModelIndex curIndex = tableViewRoute->currentIndex();
      if(curIndex.isValid())
      {
        map::MapAirport airport = route.value(curIndex.row()).getAirport();
        if(airport.isValid())
          emit addUserpointFromMap(map::MapResult::createFromMapBase(&airport), airport.position, true /* airportAddon */);
      }
    }
    else if(action == ui->actionMapAirportMsa)
      emit addAirportMsa(msaResult.airportMsa.value(0));
    else if(action == ui->actionRouteInsert)
      emit routeInsert(row);
    else if(action == ui->actionRouteCalcSelected)
      calculateRouteWindowSelection();
    // else if(action == ui->actionMapHideRangeRings)
    // NavApp::getMapWidget()->clearRangeRingsAndDistanceMarkers(); // Connected directly
    // else if(action == ui->actionRouteEditUserWaypoint)
    // editUserWaypointName(index.row());
    // else if(action == ui->actionRouteTableAppend) // Done by signal from action
    // emit routeAppend();
    // Other actions emit signals directly
  }
  contextMenuOpen = false;

  // Restart the timers for clearing the selection or moving the active to top
  updateCleanupTimer();
}

void RouteController::activateLegManually(int index)
{
  qDebug() << Q_FUNC_INFO << index;
  route.setActiveLeg(index);
  highlightNextWaypoint(route.getActiveLegIndex());
  // Use geometry changed flag to force redraw
  emit routeChanged(true /* geometryChanged */);
}

void RouteController::resetActiveLeg()
{
  qDebug() << Q_FUNC_INFO;
  route.resetActive();
  highlightNextWaypoint(route.getActiveLegIndex());
  // Use geometry changed flag to force redraw
  emit routeChanged(true /* geometryChanged */);
}

void RouteController::updateActiveLeg()
{
  route.updateActiveLegAndPos(true /* force update */, aircraft.isFlying());
}

void RouteController::editUserWaypointTriggered()
{
  QModelIndex index = tableViewRoute->currentIndex();
  if(index.isValid())
    editUserWaypointName(tableViewRoute->currentIndex().row());
}

void RouteController::editUserWaypointName(int index)
{
  qDebug() << Q_FUNC_INFO << "index" << index;

  if(index >= 0 && route.canEditComment(index))
  {
    UserWaypointDialog waypointDialog(mainWindow, route.value(index).getFlightplanEntry());
    if(waypointDialog.exec() == QDialog::Accepted)
    {
      RouteCommand *undoCommand = nullptr;

      // Ignore events triggering follow due to selection changes
      atools::util::ContextSaverBool saver(ignoreFollowSelection);

      // if(route.getFlightplan().canSaveUserWaypointName())
      undoCommand = preChange(tr("Waypoint Change"));

      route.getFlightplan()[index] = waypointDialog.getEntry();

      route.updateAll();
      route.calculateLegAltitudes();

      updateActiveLeg();
      updateTableModelAndErrors();
      updateActions();

      postChange(undoCommand);
      emit routeChanged(true /* geometryChanged */);
      NavApp::setStatusMessage(tr("Changed waypoint in flight plan."));
    }
  }
}

void RouteController::shownMapFeaturesChanged(map::MapTypes types)
{
  route.setShownMapFeatures(types);
}

/* Hide or show map highlights if dock visibility changes */
void RouteController::dockVisibilityChanged(bool visible)
{
  Q_UNUSED(visible)

  // Avoid spurious events that appear on shutdown and cause crashes
  if(!atools::gui::Application::isShuttingDown())
    // Visible - send update to show map highlights
    // Not visible - send update to hide highlights
    tableSelectionChanged(QItemSelection(), QItemSelection());
}

bool RouteController::canCleanupTable()
{
  return !contextMenuOpen && !tableViewRoute->horizontalScrollBar()->isSliderDown() && !tableViewRoute->verticalScrollBar()->isSliderDown();
}

void RouteController::cleanupTableTimeout()
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  if(atools::gui::Application::isShuttingDown())
    return;

  if(NavApp::isConnectedAndAircraftFlying())
  {
    if(!canCleanupTable())
      // Cannot clean up now - restart timers
      updateCleanupTimer();
    else
    {
      opts2::Flags2 flags2 = OptionData::instance().getFlags2();
      if(hasTableSelection() && flags2.testFlag(opts2::ROUTE_CLEAR_SELECTION))
      {
        // Ignore events triggering follow due to selection changes
        atools::util::ContextSaverBool saver(ignoreFollowSelection);

        // Clear selection and move cursor to current row and first column
        QModelIndex idx = tableViewRoute->model()->index(tableViewRoute->currentIndex().row(),
                                                         tableViewRoute->horizontalHeader()->logicalIndex(0));

        if(tableViewRoute->selectionModel() != nullptr)
          tableViewRoute->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::Clear);
      }

      if(flags2.testFlag(opts2::ROUTE_CENTER_ACTIVE_LEG))
        // Move active to second row
        scrollToActive();
    }
  }
}

void RouteController::clearTableSelection()
{
  tableViewRoute->clearSelection();
}

bool RouteController::hasTableSelection()
{
  return tableViewRoute->selectionModel() == nullptr ? false : tableViewRoute->selectionModel()->hasSelection();
}

void RouteController::updateCleanupTimer()
{
  if(NavApp::isConnectedAndAircraftFlying())
  {
    opts2::Flags2 flags2 = OptionData::instance().getFlags2();
    if((hasTableSelection() && flags2.testFlag(opts2::ROUTE_CLEAR_SELECTION)) ||
       flags2.testFlag(opts2::ROUTE_CENTER_ACTIVE_LEG))
    {
#ifdef DEBUG_INFORMATION
      qDebug() << Q_FUNC_INFO << "tableCleanupTimer.start()";
#endif
      tableCleanupTimer.start();
    }
  }
}

void RouteController::tableSelectionChanged(const QItemSelection&, const QItemSelection&)
{
  // Get selected rows in ascending order
  selectedRows.clear();
  selectedRows = getSelectedRows(false /* reverse */);

  updateActions();
  QItemSelectionModel *selectionModel = tableViewRoute->selectionModel();

  if(selectionModel == nullptr || modelUpdatesBlocked)
    return;

  int selectedRowSize = 0;
  if(selectionModel != nullptr && selectionModel->hasSelection())
    selectedRowSize = selectionModel->selectedRows().size();

#ifdef DEBUG_INFORMATION
  if(selectionModel != nullptr && selectionModel->hasSelection())
  {
    int r = selectionModel->currentIndex().row();
    if(r != -1)
      qDebug() << r << "#" << route.value(r);
  }
#endif

  NavApp::getMainUi()->pushButtonRouteClearSelection->setEnabled(selectionModel != nullptr && selectionModel->hasSelection());

  routeCalcDialog->selectionChanged();
  routeLabel->updateFooterSelectionLabel();

  emit routeSelectionChanged(selectedRowSize, model->rowCount());

  updateCleanupTimer();

  // Ignore follow selection if its origin is from showInRoute()
  if(!ignoreFollowSelection && NavApp::getMainUi()->actionRouteFollowSelection->isChecked() &&
     selectionModel->currentIndex().isValid() && selectionModel->isSelected(selectionModel->currentIndex()))
    emit showPos(route.value(selectionModel->currentIndex().row()).getPosition(), map::INVALID_DISTANCE_VALUE, false /* doubleClick */);
}

void RouteController::changeRouteUndo(const atools::fs::pln::Flightplan& newFlightplan)
{
  // Keep our own index as a workaround
  undoIndex--;

  qDebug() << "changeRouteUndo undoIndex" << undoIndex << "undoIndexClean" << undoIndexClean;
  changeRouteUndoRedo(newFlightplan);
}

void RouteController::changeRouteRedo(const atools::fs::pln::Flightplan& newFlightplan)
{
  // Keep our own index as a workaround
  undoIndex++;
  qDebug() << "changeRouteRedo undoIndex" << undoIndex << "undoIndexClean" << undoIndexClean;
  changeRouteUndoRedo(newFlightplan);
}

void RouteController::changeRouteUndoRedo(const atools::fs::pln::Flightplan& newFlightplan)
{
  int currentRow = tableViewRoute->currentIndex().isValid() ? tableViewRoute->currentIndex().row() : -1;

  // Ignore events triggering follow due to selection changes
  atools::util::ContextSaverBool saver(ignoreFollowSelection);

  clearAllErrors();
  route.clearAll();
  route.setFlightplan(newFlightplan);

  // Change format in plan according to last saved format
  route.createRouteLegsFromFlightplan();
  loadProceduresFromFlightplan(false /* clearOldProcedureProperties */, false /* cleanupRoute */, false /* autoresolveTransition */);
  route.updateAll();
  route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
  route.calculateLegAltitudes();
  remarksFlightplanToWidget();

  updateTableModelAndErrors();
  updateActions();

  // Clear all also removes active airaft position - assign again and select active leg
  route.setActivePos(aircraft.getPosition(), aircraft.getHeadingDegTrue());
  updateActiveLeg();
  highlightNextWaypoint(route.getActiveLegIndexCorrected());

  setCurrentRow(currentRow, false /* select */);

  emit routeChanged(true /* geometryChanged */);
}

void RouteController::styleChanged()
{
  tabHandlerRoute->styleChanged();
  updateModelHighlightsAndErrors();
  routeLabel->styleChanged();
  atools::gui::adjustSelectionColors(NavApp::getMainUi()->tableViewRoute);
  highlightNextWaypoint(route.getActiveLegIndexCorrected());
}

void RouteController::updateRemarksFont()
{
  QFont font = QApplication::font();
  font.setPointSizeF(font.pointSizeF() * OptionData::instance().getGuiRouteRemarksTextSize() / 100.f);
  NavApp::getMainUi()->plainTextEditRouteRemarks->setFont(font);
}

void RouteController::optionsChanged()
{
  zoomHandler->zoomPercent(OptionData::instance().getGuiRouteTableTextSize());

  updateRemarksFont();

  routeLabel->optionsChanged();

  routeCalcDialog->optionsChanged();

  tableCleanupTimer.setInterval(OptionData::instance().getSimCleanupTableTime() * 1000);

  updateTableHeaders();
  updateTableModelAndErrors();

  updateUnits();
  tableViewRoute->update();

  updateCleanupTimer();
}

void RouteController::fontChanged(const QFont& font)
{
  atools::gui::updateAllFonts(routeCalcDialog, font);
  zoomHandler->zoomPercent(OptionData::instance().getGuiRouteTableTextSize());
  updateTableModelAndErrors();
}

void RouteController::tracksChanged()
{
  // Need to save and reload flight plan
  preDatabaseLoad();
  postDatabaseLoad();
}

void RouteController::updateUnits()
{
  units->update();
}

bool RouteController::hasChanged() const
{
  return undoIndexClean == -1 || undoIndexClean != undoIndex;
}

float RouteController::getCruiseAltitudeWidget() const
{
  return Unit::rev(static_cast<float>(NavApp::getMainUi()->spinBoxRouteAlt->value()), Unit::altFeetF);
}

bool RouteController::isLnmFormatFlightplan() const
{
  return route.getFlightplanConst().isLnmFormat();
}

bool RouteController::doesLnmFilenameMatchRoute() const
{
  if(!routeFilename.isEmpty())
  {
    if(!(OptionData::instance().getFlags().testFlag(opts::GUI_AVOID_OVERWRITE_FLIGHTPLAN)))
      // User decided to ignore departure and destination change in flight plan
      return true;

    // Check if any parameters in the flight plan name have changed
    bool ok = true;
    const QString pattern = OptionData::instance().getFlightplanPattern();

    if(pattern.contains(atools::fs::pln::pattern::PLANTYPE))
      ok &= fileIfrVfr == route.getFlightplanConst().getFlightplanType();

    if(pattern.contains(atools::fs::pln::pattern::CRUISEALT))
      ok &= atools::almostEqual(fileCruiseAltFt, route.getCruiseAltitudeFt(), 10.f);

    if(pattern.contains(atools::fs::pln::pattern::DEPARTIDENT))
      ok &= fileDepartureIdent == route.getFlightplanConst().getDepartureIdent();

    if(pattern.contains(atools::fs::pln::pattern::DESTIDENT))
      ok &= fileDestinationIdent == route.getFlightplanConst().getDestinationIdent();

    return ok;
  }
  return false;
}

/* Called by action */
void RouteController::moveSelectedLegsDownTriggered()
{
  if(model->rowCount() <= 1)
    return;

  qDebug() << "Leg down";
  // Ignore events triggering follow due to selection changes
  atools::util::ContextSaverBool saver(ignoreFollowSelection);

  moveSelectedLegsInternal(MOVE_DOWN);
  tableSelectionChanged(QItemSelection(), QItemSelection());
}

/* Called by action */
void RouteController::moveSelectedLegsUpTriggered()
{
  if(model->rowCount() <= 1)
    return;

  qDebug() << "Leg up";
  // Ignore events triggering follow due to selection changes
  atools::util::ContextSaverBool saver(ignoreFollowSelection);

  moveSelectedLegsInternal(MOVE_UP);
  tableSelectionChanged(QItemSelection(), QItemSelection());
}

void RouteController::moveSelectedLegsInternal(MoveDirection direction)
{
  // Get the selected rows. Depending on move direction order can be reversed to ease moving
  QList<int> rows = getSelectedRows(direction == MOVE_DOWN /* reverse order */);

  if(!rows.isEmpty())
  {
    RouteCommand *undoCommand = preChange(tr("Move Waypoints"));

    QModelIndex curIdx = tableViewRoute->currentIndex();
    // Remove selection
    if(tableViewRoute->selectionModel() != nullptr)
      tableViewRoute->selectionModel()->clear();

    // Avoid callback selection changed which can result in crashes due to inconsistent route
    blockModel();
    for(int row : qAsConst(rows))
    {
      // Change flight plan
      route.getFlightplan().move(row, row + direction);
      route.move(row, row + direction);

      // Move row
      model->insertRow(row + direction, model->takeRow(row)); // Can send tableSelectionChanged() which has to be blocked
    }
    unBlockModel();

    int firstRow = rows.constFirst();
    int lastRow = rows.constLast();

    if(direction == MOVE_DOWN)
    {
      qDebug() << "Move down" << firstRow << "to" << lastRow;
      // Departure moved down and was replaced by something else jumping up

      // Erase airway names at start of the moved block - last is smaller here
      route.eraseAirwayFlightplan(lastRow);
      route.eraseAirwayFlightplan(lastRow + 1);

      // Erase airway name at end of the moved block
      route.eraseAirwayFlightplan(firstRow + 2);
    }
    else if(direction == MOVE_UP)
    {
      qDebug() << "Move up" << firstRow << "to" << lastRow;
      // Something moved up and departure jumped up

      // Erase airway name at start of the moved block - last is larger here
      route.eraseAirwayFlightplan(firstRow - 1);

      // Erase airway names at end of the moved block
      route.eraseAirwayFlightplan(lastRow);
      route.eraseAirwayFlightplan(lastRow + 1);
    }

    route.updateAll();
    route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
    route.calculateLegAltitudes();

    route.updateDepartureAndDestination(false /* clearInvalidStart */);
    // Get type and cruise altitude from widgets
    updateFlightplanFromWidgets();

    updateActiveLeg();
    updateTableModelAndErrors();

    // Restore current position at new moved position
    tableViewRoute->setCurrentIndex(model->index(curIdx.row() + direction, curIdx.column()));

    // Restore previous selection at new moved position
    selectList(rows, direction);

    updateActions();

    postChange(undoCommand);
    emit routeChanged(true /* geometryChanged */);
    NavApp::setStatusMessage(tr("Moved flight plan legs."));
  }
}

void RouteController::routeDelete(int index)
{
  deleteSelectedLegs({index});
}

void RouteController::deleteSelectedLegsTriggered()
{
  deleteSelectedLegs(getSelectedRows(true /* reverse */));
}

void RouteController::deleteSelectedLegs(const QList<int>& rows)
{
  if(!rows.isEmpty())
  {
    // Ignore events triggering follow due to selection changes
    atools::util::ContextSaverBool saver(ignoreFollowSelection);

    clearTableSelection();

    // Do not merge for procedure deletes
    proc::MapProcedureTypes procs = affectedProcedures(rows);
    RouteCommand *undoCommand = preChange(procs & proc::PROCEDURE_ALL ? tr("Delete Procedure") : tr("Delete Waypoints"));

    deleteSelectedLegsInternal(rows);
    updateActiveLeg();
    updateTableModelAndErrors();
    updateActions();

    setCurrentRow(rows.constLast(), true /* select */);
    tableSelectionChanged(QItemSelection(), QItemSelection());

    postChange(undoCommand);
    emit routeChanged(true /* geometryChanged */);
    NavApp::setStatusMessage(tr("Removed flight plan legs."));
  }
}

void RouteController::deleteSelectedLegsInternal(const QList<int>& rows)
{
  qDebug() << Q_FUNC_INFO << rows;

  if(!rows.isEmpty())
  {
    proc::MapProcedureTypes procs = affectedProcedures(rows);

    // Avoid callback selection changed which can result in crashes due to inconsistent route
    blockModel();
    for(int row : rows)
    {
      if(row == 0)
        route.clearDepartureStartAndParking();

      route.getFlightplan().removeAt(row);
      route.eraseAirwayFlightplan(row);
      route.removeLegAt(row);
      model->removeRow(row); // Can send tableSelectionChanged() which has to be blocked
    }
    unBlockModel();

    if(procs & proc::PROCEDURE_ALL)
    {
      // Remove dummy legs from flight plan and route
      route.removeProcedureLegs(procs);

      // Reload procedures from the database after deleting a transition.
      // This is needed since attached transitions can change procedures.
      route.reloadProcedures(procs);

      // Reload legs from procedures
      route.updateProcedureLegs(entryBuilder, true /* clearOldProcedureProperties */, true /* cleanup route */);
    }

    route.updateIndicesAndOffsets();
    if(route.getSizeWithoutAlternates() == 0)
    {
      // Remove alternates too if last leg was deleted
      route.clear();
      route.getFlightplan().clearAll();
    }

    route.updateAll();
    route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
    route.calculateLegAltitudes();

    route.updateDepartureAndDestination(false /* clearInvalidStart */);

    // Get type and cruise altitude from widgets
    updateFlightplanFromWidgets();
  }
}

void RouteController::setCurrentRow(int row, bool select)
{
  if(row < 0)
    row = 0;
  if(row > model->rowCount() - 1)
    row = model->rowCount() - 1;

  QItemSelectionModel::SelectionFlags flags = QItemSelectionModel::Rows | QItemSelectionModel::Current;
  if(select)
    flags |= QItemSelectionModel::Select;

  tableViewRoute->selectionModel()->setCurrentIndex(model->index(row, 0), flags);
}

void RouteController::convertProcedure(int routeIndex)
{
  convertProcedure(proc::MapProcedureLegs::getProcedureTypeBase(route.value(routeIndex).getProcedureType()));
}

void RouteController::convertProcedure(proc::MapProcedureTypes types)
{
  qDebug() << Q_FUNC_INFO << types;

  QString message = tr("<p>You can use this feature to allow editing of a procedure or to avoid problems with simulators or "
                         "add-ons when loading an exported flight plan.</p>"
                         "<p>Note that you can undo this conversion.</p>"
                           "<p>Converting a procedure to waypoints loses information:</p>"
                             "<ul>"
                               "<li>Several approach procedure leg types like holds and turns cannot be converted and will appear as straight lines.</li>"
                                 "<li>Speed and altitude restrictions are included as remarks and are not followed in the elevation profile.</li>"
                                 "</ul>");

  dialog->showWarnMsgBox(lnm::ACTIONS_SHOW_FLIGHTPLAN_WARN_CONVERT, message, tr("Do not &show this dialog again."));

  // Ignore events triggering follow due to selection changes
  atools::util::ContextSaverBool saver(ignoreFollowSelection);

  RouteCommand *undoCommand = preChange(tr("Convert Procedure"));

  // Also convert STAR if approach is converted
  if(types.testFlag(proc::PROCEDURE_APPROACH) && route.hasAnyStarProcedure())
    types |= proc::PROCEDURE_STAR;

  // Build conversion options from procedure types
  rf::RouteAdjustOptions opts = rf::RESTRICTIONS_TO_REMARKS;

  if(types.testFlag(proc::PROCEDURE_SID))
    opts |= rf::SAVE_SID_WP;

  if(types.testFlag(proc::PROCEDURE_STAR))
    opts |= rf::SAVE_STAR_WP;

  if(types.testFlag(proc::PROCEDURE_APPROACH))
    opts |= rf::SAVE_APPROACH_WP;

  // Convert flight plan
  route = route.adjustedToOptions(opts);

  // Reload other procedures
  loadProceduresFromFlightplan(false /* clearOldProcedureProperties */, false /* cleanupRoute */, false /* autoresolveTransition */);

  route.updateAll();
  route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
  route.calculateLegAltitudes();

  route.updateDepartureAndDestination(false /* clearInvalidStart */);

  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  updateActiveLeg();
  updateTableModelAndErrors();

  postChange(undoCommand);

  emit routeChanged(true /* geometryChanged */);

  NavApp::setStatusMessage(tr("Procedure converted."));
}

/* Get selected row numbers from the table model */
QList<int> RouteController::getSelectedRows(bool reverse) const
{
  return atools::gui::selectedRows(tableViewRoute->selectionModel(), reverse);
}

/* Select all columns of the given rows adding offset to each row index */
void RouteController::selectList(const QList<int>& rows, int offset)
{
  // Check if model is already initailized
  if(model->rowCount() == 0)
    return;

  QItemSelection newSel;

  for(int row : rows)
    // Need to select all columns
    newSel.append(QItemSelectionRange(model->index(row + offset, rcol::FIRST_COLUMN), model->index(row + offset, rcol::LAST_COLUMN)));

  if(tableViewRoute->selectionModel() != nullptr)
    tableViewRoute->selectionModel()->select(newSel, QItemSelectionModel::ClearAndSelect);
}

void RouteController::selectRange(int from, int to)
{
  // Check if model is already initailized
  if(model->rowCount() == 0)
    return;

  QItemSelection newSel;

  int maxRows = tableViewRoute->model()->rowCount();

  if(from < 0 || to < 0 || from > maxRows - 1 || to > maxRows - 1)
    qWarning() << Q_FUNC_INFO << "not in range from" << from << "to" << to << ", min 0 max" << maxRows;

  from = std::min(std::max(from, 0), maxRows);
  to = std::min(std::max(to, 0), maxRows);

  newSel.append(QItemSelectionRange(model->index(from, rcol::FIRST_COLUMN), model->index(to, rcol::LAST_COLUMN)));

  if(tableViewRoute->selectionModel() != nullptr)
    tableViewRoute->selectionModel()->select(newSel, QItemSelectionModel::ClearAndSelect);
}

void RouteController::routeSetHelipad(const map::MapHelipad& helipad)
{
  qDebug() << Q_FUNC_INFO << helipad.id;

  map::MapStart start;
  airportQuery->getStartById(start, helipad.startId);

  routeSetStartPosition(start);
}

void RouteController::routeClearParkingAndStart()
{
  qDebug() << Q_FUNC_INFO;

  RouteCommand *undoCommand = nullptr;

  undoCommand = preChange(tr("Set Start Position to Airport"));

  // Update the current airport which is new or the same as the one used by the parking spot
  route.setDepartureParking(map::MapParking());
  route.setDepartureStart(map::MapStart());
  route.updateAll();

  // Update flightplan copy
  pln::Flightplan& flightplan = route.getFlightplan();
  flightplan.setDepartureParkingName(QString());
  flightplan.setDepartureParkingType(atools::fs::pln::NO_POS);
  flightplan.setDepartureParkingPosition(atools::geo::EMPTY_POS, atools::fs::pln::INVALID_ALTITUDE, atools::fs::pln::INVALID_HEADING);

  route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
  route.calculateLegAltitudes();

  route.updateDepartureAndDestination(true /* clearInvalidStart */);
  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();
  updateTableModelAndErrors();
  updateActions();

  postChange(undoCommand);
  emit routeChanged(true /* geometryChanged */);

  NavApp::setStatusMessage(tr("Start postition set to airport."));
}

void RouteController::routeSetParking(const map::MapParking& parking)
{
  qDebug() << Q_FUNC_INFO << parking.id;

  RouteCommand *undoCommand = nullptr;

  undoCommand = preChange(tr("Set Start Position"));

  if(route.isEmpty() || route.getDepartureAirportLeg().getMapType() != map::AIRPORT ||
     route.getDepartureAirportLeg().getId() != parking.airportId)
  {
    // No route, no start airport or different airport
    map::MapAirport ap;
    airportQuery->getAirportById(ap, parking.airportId);
    routeSetDepartureInternal(ap);
    route.removeProcedureLegs(proc::PROCEDURE_DEPARTURE);
  }

  // Update the current airport which is new or the same as the one used by the parking spot
  route.setDepartureParking(parking);
  route.updateAll();

  // Update flightplan copy
  pln::Flightplan& flightplan = route.getFlightplan();
  flightplan.setDepartureParkingName(map::parkingNameForFlightplan(parking));
  flightplan.setDepartureParkingType(atools::fs::pln::PARKING);
  flightplan.setDepartureParkingPosition(parking.position, route.getDepartureAirportLeg().getAltitude(), parking.heading);

  route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
  route.calculateLegAltitudes();
  route.updateDepartureAndDestination(true /* clearInvalidStart */);

  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();
  updateTableModelAndErrors();
  updateActions();

  postChange(undoCommand);
  emit routeChanged(true /* geometryChanged */);

  NavApp::setStatusMessage(tr("Start position set to %1 parking %2.").arg(route.getDepartureAirportLeg().getDisplayIdent()).
                           arg(map::parkingNameOrNumber(parking)));
}

/* Set start position (runway, helipad) for departure */
void RouteController::routeSetStartPosition(map::MapStart start)
{
  qDebug() << "route set start id" << start.id;

  RouteCommand *undoCommand = preChange(tr("Set Start Position"));
  NavApp::showFlightplan();

  if(route.isEmpty() || route.getDepartureAirportLeg().getMapType() != map::AIRPORT ||
     route.getDepartureAirportLeg().getId() != start.airportId)
  {
    map::MapAirport airport;
    airportQuery->getAirportById(airport, start.airportId);
    routeSetDepartureInternal(airport);
    route.removeProcedureLegs(proc::PROCEDURE_DEPARTURE);
  }

  // Update the current airport which is new or the same as the one used by the parking spot
  route.setDepartureStart(start);
  route.updateAll();

  pln::Flightplan& flightplan = route.getFlightplan();
  flightplan.setDepartureParkingName(start.runwayName);
  if(route.hasDepartureRunway())
    flightplan.setDepartureParkingType(atools::fs::pln::RUNWAY);
  else if(route.hasDepartureHelipad())
    flightplan.setDepartureParkingType(atools::fs::pln::HELIPAD);
  else
    flightplan.setDepartureParkingType(atools::fs::pln::AIRPORT);

  flightplan.setDepartureParkingPosition(start.position, route.getDepartureAirportLeg().getAltitude(), start.heading);

  route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
  route.calculateLegAltitudes();
  route.updateDepartureAndDestination(true /* clearInvalidStart */);

  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();
  updateTableModelAndErrors();
  updateActions();

  postChange(undoCommand);
  emit routeChanged(true /* geometryChanged */);

  NavApp::setStatusMessage(tr("Start position set to %1 position %2.").
                           arg(route.getDepartureAirportLeg().getDisplayIdent()).
                           arg(start.runwayName));
}

void RouteController::routeSetDeparture(map::MapAirport airport)
{
  qDebug() << Q_FUNC_INFO << airport.id << airport.ident;

  if(!airport.isValid())
    return;

  // Ignore events triggering follow due to selection changes
  atools::util::ContextSaverBool saver(ignoreFollowSelection);

  RouteCommand *undoCommand = preChange(tr("Set Departure"));
  NavApp::showFlightplan();

  routeSetDepartureInternal(airport);

  route.removeProcedureLegs(proc::PROCEDURE_DEPARTURE);

  route.updateAll();
  route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
  route.calculateLegAltitudes();
  route.updateDepartureAndDestination(true /* clearInvalidStart */);

  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  updateActiveLeg();
  updateTableModelAndErrors();
  updateActions();

  postChange(undoCommand);
  emit routeChanged(true /* geometryChanged */);
  NavApp::setStatusMessage(tr("Departure set to %1.").arg(route.getDepartureAirportLeg().getDisplayIdent()));
}

/* Add departure and add best runway start position */
void RouteController::routeSetDepartureInternal(const map::MapAirport& airport)
{
  Flightplan& flightplan = route.getFlightplan();

  bool replaced = false;
  if(route.getSizeWithoutAlternates() > 1)
  {
    // Replace current departure
    const FlightplanEntry& first = flightplan.constFirst();
    if(first.getWaypointType() == pln::entry::AIRPORT && flightplan.getDepartureIdent() == first.getIdent())
    {
      // Replace first airport
      FlightplanEntry entry;
      entryBuilder->buildFlightplanEntry(airport, entry, false /* alternate */);
      flightplan.replace(0, entry);

      RouteLeg routeLeg(&flightplan);
      routeLeg.createFromAirport(0, airport, nullptr);
      route.replace(0, routeLeg);
      replaced = true;
    }
  }

  if(!replaced)
  {
    // Default is prepend
    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(airport, entry, false /* alternate */);
    flightplan.prepend(entry);

    RouteLeg routeLeg(&flightplan);
    routeLeg.createFromAirport(0, airport, nullptr);
    route.insert(0, routeLeg);
  }
}

void RouteController::routeSetDestination(map::MapAirport airport)
{
  qDebug() << Q_FUNC_INFO << airport.id << airport.ident;

  if(!airport.isValid())
    return;

  // Ignore events triggering follow due to selection changes
  atools::util::ContextSaverBool saver(ignoreFollowSelection);

  RouteCommand *undoCommand = preChange(tr("Set Destination"));
  NavApp::showFlightplan();

  routeSetDestinationInternal(airport);

  route.removeProcedureLegs(proc::PROCEDURE_ARRIVAL_ALL);

  route.updateAll();
  route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
  route.calculateLegAltitudes();
  route.updateDepartureAndDestination(false /* clearInvalidStart */);

  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  updateActiveLeg();
  updateTableModelAndErrors();
  updateActions();

  postChange(undoCommand);
  emit routeChanged(true /* geometryChanged */);
  NavApp::setStatusMessage(tr("Destination set to %1.").arg(airport.ident));
}

void RouteController::routeAddAlternate(map::MapAirport airport)
{
  qDebug() << Q_FUNC_INFO << airport.id << airport.ident;

  if(!airport.isValid())
    return;

  if(route.isAirportAlternate(airport.ident) || route.isAirportDestination(airport.ident))
    return;

  // Ignore events triggering follow due to selection changes
  atools::util::ContextSaverBool saver(ignoreFollowSelection);

  RouteCommand *undoCommand = preChange(tr("Add Alternate"));
  NavApp::showFlightplan();

  FlightplanEntry entry;
  entryBuilder->buildFlightplanEntry(airport, entry, true /* alternate */);

  Flightplan& flightplan = route.getFlightplan();
  flightplan.append(entry);

  const RouteLeg *lastLeg = nullptr;
  if(flightplan.size() > 1)
    // Set predecessor if route has entries
    lastLeg = &route.value(route.size() - 1);

  RouteLeg routeLeg(&flightplan);
  routeLeg.createFromAirport(flightplan.size() - 1, airport, lastLeg);
  routeLeg.setAlternate();
  route.append(routeLeg);

  route.updateAll();
  route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
  route.calculateLegAltitudes();
  route.updateDepartureAndDestination(false /* clearInvalidStart */);

  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  updateActiveLeg();
  updateTableModelAndErrors();

  postChange(undoCommand);
  emit routeChanged(true /* geometryChanged */);
  NavApp::setStatusMessage(tr("Alternate %1 added.").arg(airport.ident));
}

void RouteController::routeSetDestinationInternal(const map::MapAirport& airport)
{
  Flightplan& flightplan = route.getFlightplan();

  bool replaced = false;
  if(route.getSizeWithoutAlternates() > 1)
  {
    // Replace current destination
    int destIdx = route.getDestinationAirportLegIndex();
    if(destIdx != map::INVALID_INDEX_VALUE)
    {
      // Remove current destination
      const FlightplanEntry& last = flightplan.at(destIdx);
      if(last.getWaypointType() == pln::entry::AIRPORT &&
         flightplan.getDestinationIdent() == last.getIdent())
      {
        // Replace destination airport
        FlightplanEntry entry;
        entryBuilder->buildFlightplanEntry(airport, entry, false /* alternate */);
        flightplan.replace(destIdx, entry);

        RouteLeg routeLeg(&flightplan);
        const RouteLeg *lastLeg = destIdx > 1 ? &route.value(destIdx - 1) : nullptr;
        routeLeg.createFromAirport(destIdx, airport, lastLeg);
        route.replace(destIdx, routeLeg);
        replaced = true;
      }
    }
  }

  if(!replaced)
  {
    // Default is append
    int insertPos = route.size() - route.getNumAlternateLegs();

    FlightplanEntry entry;
    entryBuilder->buildFlightplanEntry(airport, entry, false /* alternate */);
    flightplan.insert(insertPos, entry);

    RouteLeg routeLeg(&flightplan);
    const RouteLeg *lastLeg = insertPos > 1 ? &route.value(insertPos - 1) : nullptr;
    routeLeg.createFromAirport(insertPos, airport, lastLeg);
    route.insert(insertPos, routeLeg);
  }
}

void RouteController::showCustomApproachMainMenu()
{
  // Called from main menu for current destination
  if(route.hasValidDestinationAndRunways())
    showCustomApproach(route.getDestinationAirportLeg().getAirport(), tr("Select runway for destination:"));
}

void RouteController::showCustomDepartureMainMenu()
{
  // Called from main menu for current departure
  if(route.hasValidDepartureAndRunways())
    showCustomDeparture(route.getDepartureAirportLeg().getAirport(), tr("Select runway for departure:"));
}

void RouteController::showCustomApproachRouteTriggered()
{
  QModelIndex index = tableViewRoute->currentIndex();

  // Allow for departure airport only or single airport plan where airport can be used both as departure and destination
  if(index.isValid() && (route.getDepartureAirportLegIndex() != index.row() || route.getSizeWithoutAlternates() == 1))
    showCustomApproach(route.value(index.row()).getAirport(), QString());
}

void RouteController::showCustomDepartureRouteTriggered()
{
  QModelIndex index = tableViewRoute->currentIndex();
  // Allow for destination airport only or single airport plan where airport can be used both as departure and destination
  if(index.isValid() && (route.getDestinationAirportLegIndex() != index.row() || route.getSizeWithoutAlternates() == 1))
    showCustomDeparture(route.value(index.row()).getAirport(), QString());
}

void RouteController::showCustomApproach(map::MapAirport airport, QString dialogHeader)
{
  // Also called from search menu
  qDebug() << Q_FUNC_INFO << airport.id << airport.ident;

  if(!airport.isValid() || airport.noRunways())
    return;

  if(dialogHeader.isEmpty())
  {
    // No header given. Build one based on airport relation to route (is destination, etc.)
    bool destination;
    proc::procedureFlags(route, &airport, nullptr, &destination);
    if(destination)
      dialogHeader = tr("Select runway for destination:");
    else
      dialogHeader = tr("Select runway and use airport as destination:");
  }

  // Get the simulator runway end id to pre-select runway end row in dialog =================================
  // Check if there is already a STAR or an approach to fetch current runway selection
  QList<map::MapRunwayEnd> runwayEnds;
  const MapQuery *mapQuery = NavApp::getMapQueryGui();
  if(!route.getApproachLegs().isEmpty())
  {
    if(route.hasCustomApproach())
      // Custom departure is already sim id
      runwayEnds.append(route.getApproachLegs().runwayEnd);
    else
      // Fetch sim id from procedure which uses nav id
      mapQuery->getRunwayEndByNameFuzzy(runwayEnds, route.getApproachRunwayName(), airport, false /* navData */);
  }
  else if(!route.getStarLegs().isEmpty())
    // Fetch sim id from procedure which uses nav id
    mapQuery->getRunwayEndByNameFuzzy(runwayEnds, route.getStarRunwayName(), airport, false /* navData */);

  int runwayEndId = -1;
  if(!runwayEnds.isEmpty())
    runwayEndId = runwayEnds.constFirst().id;

  // Show runway selection dialog to user
  CustomProcedureDialog procedureDialog(mainWindow, airport, false /* departureParam */, dialogHeader, runwayEndId);
  int result = procedureDialog.exec();

  if(result == QDialog::Accepted)
  {
    if(procedureDialog.isShowProceduresSelected())
      emit showProcedures(airport, false /* departureFilter */, true /* arrivalFilter */);
    else
    {
      map::MapRunway runway;
      map::MapRunwayEnd end;
      procedureDialog.getSelected(runway, end);
      qDebug() << Q_FUNC_INFO << runway.primaryName << runway.secondaryName << end.id << end.name;

      proc::MapProcedureLegs procedure;
      NavApp::getProcedureQuery()->createCustomApproach(procedure, airport, end, procedureDialog.getLegDistance(),
                                                        procedureDialog.getEntryAltitude(), procedureDialog.getLegOffsetAngle());
      routeAddProcedure(procedure);
    }
  }
}

void RouteController::showCustomDeparture(map::MapAirport airport, QString dialogHeader)
{
  // Also called from search menu
  qDebug() << Q_FUNC_INFO << airport.id << airport.ident;

  if(!airport.isValid() || airport.noRunways())
    return;

  if(dialogHeader.isEmpty())
  {
    // No header given. Build one based on airport relation to route (is departure, etc.)
    bool departure;
    proc::procedureFlags(route, &airport, &departure);
    if(departure)
      dialogHeader = tr("Select runway for departure:");
    else
      dialogHeader = tr("Select runway and use airport as departure:");
  }

  // Get the simulator runway end id to pre-select runway end row in dialog =================================
  // Check if there is already a SID to fetch current runway selection
  int runwayEndId = -1;
  if(!route.getSidLegs().isEmpty())
  {
    QList<map::MapRunwayEnd> runwayEnds;
    if(route.hasCustomDeparture())
      // Custom departure is already sim id
      runwayEnds.append(route.getSidLegs().runwayEnd);
    else
      // Fetch sim id from procedure which uses nav id
      NavApp::getMapQueryGui()->getRunwayEndByNameFuzzy(runwayEnds, route.getSidRunwayName(), airport, false /* navData */);

    if(!runwayEnds.isEmpty())
      runwayEndId = runwayEnds.constFirst().id;
  }

  // Show runway selection dialog to user
  CustomProcedureDialog procedureDialog(mainWindow, airport, true /* departureParam */, dialogHeader, runwayEndId);
  int result = procedureDialog.exec();

  if(result == QDialog::Accepted)
  {
    if(procedureDialog.isShowProceduresSelected())
      emit showProcedures(airport, true /* departureFilter */, false /* arrivalFilter */);
    else
    {
      map::MapRunway runway;
      map::MapRunwayEnd end;
      procedureDialog.getSelected(runway, end);

      qDebug() << Q_FUNC_INFO << runway.primaryName << runway.secondaryName << end.id << end.name;

      proc::MapProcedureLegs procedure;
      NavApp::getProcedureQuery()->createCustomDeparture(procedure, airport, end, procedureDialog.getLegDistance());
      routeAddProcedure(procedure);
    }
  }
}

void RouteController::updateRouteTabChangedStatus()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  // Update status in flight plan table
  int idxRoute = NavApp::getRouteTabHandler()->getIndexForId(rc::ROUTE);
  if(idxRoute != -1)
  {
    if(hasChanged())
    {
      if(!ui->tabWidgetRoute->tabText(idxRoute).endsWith(tr(" *")))
        ui->tabWidgetRoute->setTabText(idxRoute, ui->tabWidgetRoute->tabText(idxRoute) % tr(" *"));
    }
    else
      ui->tabWidgetRoute->setTabText(idxRoute, ui->tabWidgetRoute->tabText(idxRoute).replace(tr(" *"), QString()));
  }

  // Update status in remarks tab
  int idxRemark = NavApp::getRouteTabHandler()->getIndexForId(rc::REMARKS);
  if(idxRemark != -1)
  {
    // Add star only if remarks contain text
    if(hasChanged() && !ui->plainTextEditRouteRemarks->toPlainText().isEmpty())
    {
      if(!ui->tabWidgetRoute->tabText(idxRemark).endsWith(tr(" *")))
        ui->tabWidgetRoute->setTabText(idxRemark, ui->tabWidgetRoute->tabText(idxRemark) % tr(" *"));
    }
    else
      ui->tabWidgetRoute->setTabText(idxRemark, ui->tabWidgetRoute->tabText(idxRemark).replace(tr(" *"), QString()));
  }
}

void RouteController::routeAddProcedure(proc::MapProcedureLegs legs)
{
  qDebug() << Q_FUNC_INFO
           << legs.type << legs.procedureFixIdent << legs.suffix << legs.arincName
           << legs.transitionType << legs.transitionFixIdent;

  if(legs.isEmpty())
  {
    qWarning() << Q_FUNC_INFO << "empty procedure";
    return;
  }

  clearTableSelection();

  AirportQuery *airportQueryNav = NavApp::getAirportQueryNav(), *airportQuerySim = NavApp::getAirportQuerySim();
  ProcedureQuery *procedureQuery = NavApp::getProcedureQuery();
  MapQuery *mapQuery = NavApp::getMapQueryGui();
  map::MapAirport airportSim, airportNav;
  if(legs.isAnyCustom())
  {
    // Airport id in legs is from sim database - get airport and convert to nav database
    airportSim = airportQuerySim->getAirportById(legs.ref.airportId);
    airportNav = mapQuery->getAirportNav(airportSim);
  }
  else
  {
    // Airport id in legs is from nav database - get airport and convert to simulator database
    airportNav = airportQueryNav->getAirportById(legs.ref.airportId);
    airportSim = mapQuery->getAirportSim(airportNav);
  }

  // Adding approach or destination runway ==========================================================================
  if(legs.mapType & proc::PROCEDURE_APPROACH_ALL && route.hasAnyStarProcedure())
  {
    // STAR already assigned and adding approach - try to adjust STAR to approach =====================================
    QStringList starRunwaysNav;
    atools::fs::util::sidStarMultiRunways(airportQueryNav->getRunwayNames(airportNav.id), route.getStarLegs().arincName, &starRunwaysNav);

    // Check if the runway of an an already present STAR can be changed to match the approach
    if(atools::fs::util::runwayContains(starRunwaysNav, legs.runway, legs.isAnyCustom() /* fuzzy */))
    {
      // Adjust STAR runway to approach runway
      proc::MapProcedureLegs starLegs = route.getStarLegs();

      // Convert to matching list of STAR nav runway from sim runway
      QString starRunway = legs.isAnyCustom() ? atools::fs::util::runwayBestFit(legs.runway, starRunwaysNav) : legs.runway;
      procedureQuery->insertSidStarRunway(starLegs, starRunway);
      route.setStarProcedureLegs(starLegs);
    }
  }

  // Adding SID or STAR ==========================================================================
  // Do not check runways for custom deaparture
  QString sidStarRunwayNav;
  if(legs.mapType & proc::PROCEDURE_SID_STAR_ALL && !legs.isCustomDeparture())
  {
    // Get runways for all or parallel runway procedures ===============================
    QStringList sidStarRunwaysNav;
    atools::fs::util::sidStarMultiRunways(airportQueryNav->getRunwayNames(airportNav.id), legs.arincName, &sidStarRunwaysNav);

    if(!sidStarRunwaysNav.isEmpty())
    {
      // Approach already assigned and adding STAR - try to adjust STAR to approach =====================================
      // Check if an already present approach matches the new STAR runway
      QString apprRw = route.getApproachLegs().runway;
      if((legs.mapType & proc::PROCEDURE_STAR_ALL) && route.hasAnyApproachProcedure() &&
         atools::fs::util::runwayContains(sidStarRunwaysNav, apprRw, route.hasCustomApproach() /* fuzzy */))
        // No runway selection dialog
        sidStarRunwayNav = route.hasCustomApproach() ? atools::fs::util::runwayBestFit(apprRw, sidStarRunwaysNav) : apprRw;
      else
      {
        // Show runway selection dialog allowing the user to select a runway
        QString text = proc::procedureLegsText(legs, proc::PROCEDURE_NONE,
                                               false /* narrow */, true /* includeRunway*/, false /* missedAsApproach*/,
                                               false /* transitionAsProcedure */);

        // Get navdata runway end id for preselection =============================================
        QList<map::MapRunwayEnd> runwayEnds;

        // Adding a SID and there is already one
        if(legs.mapType & proc::PROCEDURE_SID_ALL && !route.getSidLegs().isEmpty())
        {
          if(route.hasCustomDeparture())
            // Fetch nav id from procedure which uses sim id
            mapQuery->getRunwayEndByNameFuzzy(runwayEnds, route.getSidRunwayName(), airportNav, true /* navData */);
          else
            // Custom departure is already sim id
            runwayEnds.append(route.getSidLegs().runwayEnd);
        }
        else if(legs.mapType & proc::PROCEDURE_STAR_ALL)
        {
          if(!route.getApproachLegs().isEmpty())
          {
            if(route.hasCustomApproach())
              // Fetch nav id from procedure which uses sim id
              mapQuery->getRunwayEndByNameFuzzy(runwayEnds, route.getApproachRunwayName(), airportNav, true /* navData */);
            else
              // Custom approach is already sim id
              runwayEnds.append(route.getApproachLegs().runwayEnd);
          }
          else if(!route.getStarLegs().isEmpty())
            runwayEnds.append(route.getStarLegs().runwayEnd);
        }

        int runwayEndId = -1;
        if(!runwayEnds.isEmpty())
          runwayEndId = runwayEnds.constFirst().id;

        RunwaySelectionDialog runwaySelectionDialog(mainWindow, airportNav, sidStarRunwaysNav, text, true /* navdata */, runwayEndId);
        if(runwaySelectionDialog.exec() == QDialog::Accepted)
          sidStarRunwayNav = runwaySelectionDialog.getSelectedName();
        else
          // Cancel out
          return;
      }
    }
  }

  // Raise window if requested
  if(route.isEmpty())
    NavApp::showFlightplan();

  RouteCommand *undoCommand = nullptr;

  // Ignore events triggering follow due to selection changes
  atools::util::ContextSaverBool saver(ignoreFollowSelection);

  undoCommand = preChange(tr("Add Procedure"));

  // Inserting new ones does not produce errors - only loading
  procedureErrors.clear();

  if(legs.mapType & proc::PROCEDURE_STAR || legs.mapType & proc::PROCEDURE_APPROACH_ALL_MISSED)
  {
    if(route.isEmpty() || route.getDestinationAirportLeg().getMapType() != map::AIRPORT ||
       route.getDestinationAirportLeg().getId() != airportSim.id)
    {
      // No route, no destination airport or different airport
      route.removeProcedureLegs(proc::PROCEDURE_ARRIVAL_ALL);
      routeSetDestinationInternal(airportSim);
    }

    // Will take care of the flight plan entries too
    if(legs.mapType & proc::PROCEDURE_STAR)
    {
      // Assign runway for SID/STAR than can have multiple runways
      procedureQuery->insertSidStarRunway(legs, sidStarRunwayNav);
      route.setStarProcedureLegs(legs);
    }

    if(legs.mapType & proc::PROCEDURE_APPROACH_ALL_MISSED)
      route.setArrivalProcedureLegs(legs);

    route.updateProcedureLegs(entryBuilder, true /* clearOldProcedureProperties */, true /* cleanup route */);
  }
  else if(legs.mapType & proc::PROCEDURE_DEPARTURE)
  {
    if(route.isEmpty() || route.getDepartureAirportLeg().getMapType() != map::AIRPORT ||
       route.getDepartureAirportLeg().getId() != airportSim.id)
    {
      // No route, no departure airport or different airport
      route.removeProcedureLegs(proc::PROCEDURE_DEPARTURE);
      routeSetDepartureInternal(airportSim);
      route.clearDepartureStartAndParking();
    }

    // Assign runway for SID/STAR than can have multiple runways
    // Not needed for departure runway selection
    if(!legs.isCustomDeparture())
      procedureQuery->insertSidStarRunway(legs, sidStarRunwayNav);

    route.setSidProcedureLegs(legs);

    // Will take care of the flight plan entries too
    route.updateProcedureLegs(entryBuilder, true /* clearOldProcedureProperties */, true /* cleanup route */);
  }
  route.updateAll();
  route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
  route.calculateLegAltitudes();
  route.updateDepartureAndDestination(false /* clearInvalidStart */);

  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  updateActiveLeg();
  updateTableModelAndErrors();
  updateActions();

  postChange(undoCommand);

  qDebug() << Q_FUNC_INFO << route.getFlightplanConst().getProperties();

  emit routeChanged(true /* geometryChanged */);

  NavApp::setStatusMessage(tr("Added procedure to flight plan."));
}

void RouteController::directToTriggered()
{
  QModelIndex index = tableViewRoute->currentIndex();
  if(index.isValid())
    routeDirectTo(-1, atools::geo::EMPTY_POS, map::NONE, index.row());
}

void RouteController::routeDirectTo(int id, const atools::geo::Pos& userPos, map::MapTypes type, int legIndexDirectTo)
{
  qDebug() << Q_FUNC_INFO << "id" << id << "userPos" << userPos << "type" << type << "legIndexDirectTo" << legIndexDirectTo
           << "route.getActiveLegIndex()" << route.getActiveLegIndex();

  bool disable = false;
  proc::procedureTextSuffixDirectTo(route, legIndexDirectTo, &route.value(legIndexDirectTo).getAirport(), &disable);
  const Pos& userAicraftPos = NavApp::getUserAircraftPos();
  if(!disable)
  {
    // Ignore events triggering follow due to selection changes
    atools::util::ContextSaverBool saver(ignoreFollowSelection);

    RouteCommand *undoCommand = preChange(tr("Direct to"));
    updateActiveLeg();

    if(route.getSizeWithoutAlternates() == 0)
    {
      // Empty route case ============================================================
      // Append positions to empty flight plan
      // Current aircraft position
      routeAddInternal(-1, userAicraftPos, map::USERPOINTROUTE, legindex::APPEND, true /* presentPos */);

      // Clicked position or feature
      routeAddInternal(id, userPos, type, legindex::APPEND);
    }
    else if(route.getSizeWithoutAlternates() == 1)
    {
      // One destination only case ============================================================
      // Clicked position or feature
      if(route.getDestinationAirportLegIndex() != legIndexDirectTo)
        // Prepend only if not destination airport
        routeAddInternal(id, userPos, type, legindex::PREPEND);

      // Current aircraft position - prepend before next
      routeAddInternal(-1, userAicraftPos, map::USERPOINTROUTE, legindex::PREPEND, true /* presentPos */);
    }
    else
    {
      // Normal flight plan with legs =================================================================
      if(legIndexDirectTo != -1 && legIndexDirectTo < map::INVALID_INDEX_VALUE)
      {
        // To route leg: id 482 userPos Pos(-120.370773,49.381542,0.000000) type VOR legIndexDirectTo 15
        // legIndexDirectTo leg index which will be connect from PPOS to legIndexDirectTo

        // Index of active leg - departure airport is leg 0, first leg is index 1
        int actIdx = route.getActiveLegIndex();
        QList<int> rowsToDelete;
        bool addPresentPos = false;

        if(route.value(legIndexDirectTo).isAlternate())
        {
#ifdef DEBUG_INFORMATION
          qDebug() << Q_FUNC_INFO << "forward to alternate";
#endif
          // Direct to is an alterate ======================================
          // Remember old alternate
          const RouteLeg& altDest = route.value(legIndexDirectTo);
          Pos pos = altDest.getPosition();
          map::MapTypes altType = altDest.getMapType();
          int altId = altDest.getId();

          // Put alternate to delete on top of list
          rowsToDelete.append(legIndexDirectTo);

          if(route.getSizeWithoutAlternates() > 1)
          {
            // Other legs to delete including procedures in reverse order
            int deleteFromIdx = route.getDestinationAirportLegIndex();
            for(int i = deleteFromIdx; i >= actIdx; i--)
              rowsToDelete.append(i);
          }

          // Delete legs
          deleteSelectedLegsInternal(rowsToDelete);

          // Append previously saved alternate as new destination
          routeAddInternal(altId, pos, altType, legindex::APPEND, true /* presentPos */);

          addPresentPos = true;
        } // if(route.getLegAt(legIndexDirectTo).isAlternate())
        else
        {
          // Direct to is an intermediate waypoint or the destination ======================================

          if(legIndexDirectTo == actIdx)
          {
#ifdef DEBUG_INFORMATION
            qDebug() << Q_FUNC_INFO << "forward legIndexDirectTo == actIdx";
#endif
            // Insert current position for user aircraft into nearest leg
            addPresentPos = true;
          }
          else if(legIndexDirectTo > actIdx)
          {
#ifdef DEBUG_INFORMATION
            qDebug() << Q_FUNC_INFO << "forward legIndexDirectTo > actIdx";
#endif

            // Direct to waypoint ahead ==================
            for(int i = legIndexDirectTo - 1; i >= actIdx; i--)
              rowsToDelete.append(i);

#ifdef DEBUG_INFORMATION
            qDebug() << Q_FUNC_INFO << "forward legIndexDirectTo > actIdx" << rowsToDelete;
#endif

            // Delete legs until direct to including current one
            // Do not delete if direct to is current leg
            deleteSelectedLegsInternal(rowsToDelete);
            addPresentPos = true;
          }
          // else Direct to waypoint back - invalid
        }

        // Insert current position for user aircraft into nearest leg
        if(addPresentPos)
          routeAddInternal(-1, userAicraftPos, map::USERPOINTROUTE, legindex::AUTO, true /* presentPos */);
      }
      else // if(id == -1 && userPos.isValid() && type == map::NONE)
      {
        // To any pos: id -1 userPos Pos(-120.346367,49.243427,0.000000) type NONE legIndexDirectTo -1
        // To any airport or navaid: id 10366 userPos Pos(-120.513565,49.468750,0.000000) type AIRPORT legIndexDirectTo -1
        int insertIndex = routeAddInternal(-1, userAicraftPos, map::USERPOINTROUTE, legindex::AUTO, true /* presentPos */);
        routeAddInternal(id, userPos, type, insertIndex);
      }
    } // else if(route.getSizeWithoutAlternates() == 1)

    updateActiveLeg();
    updateTableModelAndErrors();
    updateActions();
    tableSelectionChanged(QItemSelection(), QItemSelection());

    postChange(undoCommand);
    emit routeChanged(true /* geometryChanged */);
    NavApp::setStatusMessage(tr("Changed flight plan for direct to."));
  }
}

void RouteController::routeAdd(int id, atools::geo::Pos userPos, map::MapTypes type, int legIndex)
{
  qDebug() << Q_FUNC_INFO << "user pos" << userPos << "id" << id << "type" << type << "leg index" << legIndex;

  // Ignore events triggering follow due to selection changes
  atools::util::ContextSaverBool saver(ignoreFollowSelection);

  RouteCommand *undoCommand = preChange(tr("Add Waypoint"));

  if(route.isEmpty())
    NavApp::showFlightplan();

  routeAddInternal(id, userPos, type, legIndex);
  updateActiveLeg();
  updateTableModelAndErrors();
  updateActions();

  postChange(undoCommand);
  emit routeChanged(true /* geometryChanged */);
  NavApp::setStatusMessage(tr("Added waypoint to flight plan."));
}

int RouteController::routeAddInternal(int id, atools::geo::Pos userPos, map::MapTypes type, int legIndex, bool presentPos)
{
  QString userpointIdent;
  if(presentPos)
  {
    // Prepare for direct to insertion
    userpointIdent = tr("PPOS");

    // Look into flight plan for an PPOS at the same position - return index if found
    for(int i = 0; i < route.size(); i++)
    {
      const RouteLeg& leg = route.value(i);
      if(leg.getMapType() == map::USERPOINTROUTE && leg.getIdent() == userpointIdent &&
         leg.getPosition().almostEqual(userPos, atools::geo::Pos::POS_EPSILON_1NM))
        return i;
    }
  }

  qDebug() << Q_FUNC_INFO << "id" << id << "userPos" << userPos << "type" << type
           << "legIndex" << legIndex << "userpointIdent" << userpointIdent;

  FlightplanEntry entry;
  entryBuilder->buildFlightplanEntry(id, userPos, type, entry, -1);

  Flightplan& flightplan = route.getFlightplan();
  int insertIndex = calculateInsertIndex(entry.getPosition(), legIndex);
  flightplan.insert(insertIndex, entry);
  route.eraseAirwayFlightplan(insertIndex);
  route.eraseAirwayFlightplan(insertIndex + 1);

  const RouteLeg *lastLeg = nullptr;

  if(flightplan.isEmpty() && insertIndex > 0)
    lastLeg = &route.value(insertIndex - 1);
  RouteLeg routeLeg(&flightplan);
  routeLeg.createFromDatabaseByEntry(insertIndex, lastLeg);
  if(!userpointIdent.isEmpty() && routeLeg.getFlightplanEntry() != nullptr)
    routeLeg.getFlightplanEntry()->setIdent(userpointIdent);

  route.insert(insertIndex, routeLeg);

  proc::MapProcedureTypes procs = proc::PROCEDURE_NONE;

  if(legIndex == map::INVALID_INDEX_VALUE)
    procs = proc::PROCEDURE_ARRIVAL_ALL;
  else
    procs = affectedProcedures({insertIndex});

  route.removeProcedureLegs(procs);

  // Reload procedures from the database after deleting a transition.
  // This is needed since attached transitions can change procedures.
  route.reloadProcedures(procs);

  route.updateAll();
  route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
  route.calculateLegAltitudes();

  route.updateDepartureAndDestination(false /* clearInvalidStart */);
  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  return insertIndex;
}

void RouteController::routeReplace(int id, atools::geo::Pos userPos, map::MapTypes type, int legIndex)
{
  qDebug() << Q_FUNC_INFO << "user pos" << userPos << "id" << id << "type" << type << "leg index" << legIndex;
  const FlightplanEntry oldEntry = route.getFlightplanConst().at(legIndex);
  bool alternate = oldEntry.getFlags() & atools::fs::pln::entry::ALTERNATE;

  if(alternate && !(type & map::AIRPORT))
    return;

  // Ignore events triggering follow due to selection changes
  atools::util::ContextSaverBool saver(ignoreFollowSelection);

  RouteCommand *undoCommand = preChange(tr("Change Waypoint"));

  FlightplanEntry entry;
  entryBuilder->buildFlightplanEntry(id, userPos, type, entry, -1);

  if(alternate)
    entry.setFlag(atools::fs::pln::entry::ALTERNATE);

  // Transfer user properties
  if(oldEntry.getWaypointType() == atools::fs::pln::entry::USER && entry.getWaypointType() == atools::fs::pln::entry::USER)
  {
    entry.setIdent(oldEntry.getIdent());
    entry.setRegion(oldEntry.getRegion());
    entry.setName(oldEntry.getName());
    entry.setComment(oldEntry.getComment());
  }

  Flightplan& flightplan = route.getFlightplan();

  flightplan.replace(legIndex, entry);

  const RouteLeg *lastLeg = nullptr;
  if(alternate)
    lastLeg = &route.getDestinationAirportLeg();
  else if(legIndex > 0 && !route.isFlightplanEmpty())
    // Get predecessor of replaced entry
    lastLeg = &route.value(legIndex - 1);

  RouteLeg routeLeg(&flightplan);
  routeLeg.createFromDatabaseByEntry(legIndex, lastLeg);

  route.replace(legIndex, routeLeg);
  route.eraseAirwayFlightplan(legIndex);
  route.eraseAirwayFlightplan(legIndex + 1);

  if(legIndex == route.getDestinationAirportLegIndex())
    route.removeProcedureLegs(proc::PROCEDURE_ARRIVAL_ALL);

  if(legIndex == 0)
    route.removeProcedureLegs(proc::PROCEDURE_DEPARTURE);

  route.updateAll();
  route.updateAirwaysAndAltitude(false /* adjustRouteAltitude */);
  route.calculateLegAltitudes();

  route.updateDepartureAndDestination(false /* clearInvalidStart */);
  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  updateActiveLeg();
  updateTableModelAndErrors();
  updateActions();

  postChange(undoCommand);
  emit routeChanged(true /* geometryChanged */);
  NavApp::setStatusMessage(tr("Replaced waypoint in flight plan."));
}

int RouteController::calculateInsertIndex(const atools::geo::Pos& pos, int legIndex)
{
  Flightplan& flightplan = route.getFlightplan();

  int insertIndex = -1;
  if(legIndex == legindex::APPEND)
    insertIndex = route.getSizeWithoutAlternates();
  else if(legIndex == legindex::PREPEND)
    insertIndex = 0;
  else if(legIndex == legindex::AUTO)
  {
    if(flightplan.isEmpty())
      // First is  departure
      insertIndex = 0;
    else if(flightplan.size() == 1)
      // Keep first as departure
      insertIndex = 1;
    else
    {
      // No leg index given - search for nearest available route leg
      atools::geo::LineDistance result;
      int nearestlegIndex = route.getNearestRouteLegResult(pos, result, true /* ignoreNotEditable */, true /* ignoreMissed */);

      switch(result.status)
      {
        case atools::geo::INVALID:
          insertIndex = 0;
          break;
        case atools::geo::ALONG_TRACK:
          insertIndex = nearestlegIndex;
          break;
        case atools::geo::BEFORE_START:
          if(nearestlegIndex == 1)
            // Add before departure
            insertIndex = 0;
          else
            insertIndex = nearestlegIndex;
          break;
        case atools::geo::AFTER_END:
          if(nearestlegIndex == route.getSizeWithoutAlternates() - 1)
            insertIndex = nearestlegIndex + 1;
          else
            insertIndex = nearestlegIndex;
          break;
      }
    }
  }
  else
    // Adjust and use given leg index (insert after index point)
    insertIndex = legIndex + 1;

  qDebug() << "insertIndex" << insertIndex << "pos" << pos;

  return insertIndex;
}

/* Update airway attribute in flight plan entry and return minimum altitude for this airway segment */
void RouteController::updateFlightplanEntryAirway(int airwayId, FlightplanEntry& entry)
{
  map::MapAirway airway;
  NavApp::getAirwayTrackQueryGui()->getAirwayById(airway, airwayId);
  entry.setAirway(airway.name);
  entry.setFlag(atools::fs::pln::entry::TRACK, airway.isTrack());
}

/* Copy type and cruise altitude from widgets to flight plan */
void RouteController::updateFlightplanFromWidgets()
{
  // assignFlightplanPerfProperties(route.getFlightplan());
  updateFlightplanFromWidgets(route.getFlightplan());
}

void RouteController::updateFlightplanFromWidgets(Flightplan& flightplan)
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  flightplan.setFlightplanType(ui->comboBoxRouteType->currentIndex() == 0 ? atools::fs::pln::IFR : atools::fs::pln::VFR);
  flightplan.setCruiseAltitudeFt(Unit::rev(ui->spinBoxRouteAlt->value(), Unit::altFeetF));
}

QIcon RouteController::iconForLeg(const RouteLeg& leg, int size) const
{
  QIcon icon;
  if(leg.getMapType() == map::AIRPORT)
    icon = SymbolPainter::createAirportIcon(leg.getAirport(), size - 2);
  else if(leg.getVor().isValid())
    icon = SymbolPainter::createVorIcon(leg.getVor(), size, NavApp::isGuiStyleDark());
  else if(leg.getNdb().isValid())
    icon = SymbolPainter::createNdbIcon(size, NavApp::isGuiStyleDark());
  else if(leg.getWaypoint().isValid())
    icon = SymbolPainter::createWaypointIcon(size);
  else if(leg.getMapType() == map::USERPOINTROUTE)
    icon = SymbolPainter::createUserpointIcon(size);
  else if(leg.getMapType() == map::INVALID)
    icon = SymbolPainter::createWaypointIcon(size, mapcolors::routeInvalidPointColor);
  else if(leg.isAnyProcedure())
    icon = SymbolPainter::createProcedurePointIcon(size);

  return icon;
}

void RouteController::updatePlaceholderWidget()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  bool showPlaceholder = route.isEmpty();
  tableViewRoute->setVisible(!showPlaceholder);
  ui->textBrowserViewRoute->setVisible(showPlaceholder);
}

void RouteController::updateTableModelAndErrors()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  // Avoid callback selection changed which can result in crashes due to inconsistent route
  blockModel();
  model->removeRows(0, model->rowCount());

  float totalDistance = route.getTotalDistance();

  int row = 0;
  float cumulatedDistance = 0.f;

  // Create null pointer filled list for all items ==============
  QList<QStandardItem *> itemRow;
  for(int i = rcol::FIRST_COLUMN; i <= rcol::LAST_COLUMN; i++)
    itemRow.append(nullptr);

  for(int i = 0; i < route.size(); i++)
  {
    const RouteLeg& leg = route.value(i);
    const proc::MapProcedureLeg& procedureLeg = leg.getProcedureLeg();

    // Ident ===========================================
    QString identStr;
    if(leg.isAnyProcedure())
      // Get ident with IAF, FAF or other indication
      identStr = proc::procedureLegFixStr(procedureLeg);
    else
      identStr = leg.getDisplayIdent();

    QStandardItem *ident = new QStandardItem(iconForLeg(leg, tableViewRoute->verticalHeader()->defaultSectionSize() - 2), identStr);
    QFont f = ident->font();
    f.setBold(true);
    ident->setFont(f);
    ident->setTextAlignment(Qt::AlignRight);

    itemRow[rcol::IDENT] = ident;
    // highlightProcedureItems() does error checking for IDENT

    // Region, navaid name, procedure type ===========================================
    itemRow[rcol::REGION] = new QStandardItem(leg.getRegion());
    itemRow[rcol::NAME] = new QStandardItem(leg.getName());

    if(route.getSizeWithoutAlternates() > 1)
    {
      if(row == route.getDestinationAirportLegIndex() && !route.getDestinationAirportLeg().isAnyProcedure())
        itemRow[rcol::PROCEDURE] = new QStandardItem(tr("Destination"));
      else if(row == route.getDepartureAirportLegIndex() && !route.getDepartureAirportLeg().isAnyProcedure())
        itemRow[rcol::PROCEDURE] = new QStandardItem(tr("Departure"));
      else if(leg.isAlternate())
        itemRow[rcol::PROCEDURE] = new QStandardItem(tr("Alternate"));
      else if(leg.isAnyProcedure())
        itemRow[rcol::PROCEDURE] = new QStandardItem(route.getProcedureLegText(leg.getProcedureType(),
                                                                               false /* includeRunway */, false /* missedAsApproach */,
                                                                               true /* transitionAsProcedure */));
    }

    // Airway or leg type and restriction ===========================================
    if(leg.isRoute())
    {
      // Airway ========================
      const map::MapAirway& airway = leg.getAirway();

      QStringList awname(airway.isValid() && airway.isTrack() ? tr("Track %1").arg(leg.getAirwayName()) : leg.getAirwayName());

      if(airway.isValid())
      {
        awname.append(map::airwayTrackTypeToShortString(airway.type));

#ifdef DEBUG_INFORMATION
        awname.append("[" + map::airwayRouteTypeToStringShort(airway.routeType) + "]");
#endif

        awname.removeAll(QString());
        itemRow[rcol::RESTRICTION] = new QStandardItem(map::airwayAltTextShort(airway, false /* addUnit */, false /* narrow */));
      }

      itemRow[rcol::AIRWAY_OR_LEGTYPE] = new QStandardItem(awname.join(tr(" / ")));
      // highlightProcedureItems() does error checking
    }
    else
    {
      // Procedure ========================
      itemRow[rcol::AIRWAY_OR_LEGTYPE] =
        new QStandardItem(atools::strJoin({leg.getFlightplanEntry().getAirway(), proc::procedureLegTypeStr(procedureLeg.type)}, tr(", ")));
      itemRow[rcol::RESTRICTION] = new QStandardItem(proc::restrictionText(procedureLeg).join(tr(", ")));
    }

    // Get ILS for approach runway if it marks the end of an ILS or localizer approach procedure
    QStringList ilsTypeTexts, ilsFreqTexts;
    if(procedureLeg.isApproach() && leg.getRunwayEnd().isValid())
    {
      // Build string for ILS type - use recommended ILS which can also apply to non ILS approaches
      for(const map::MapIls& ils : route.getDestRunwayIlsFlightplanTable())
      {
        ilsTypeTexts.append(map::ilsType(ils, true /* gs */, true /* dme */, tr(", ")));

        if(!ils.isAnyGlsRnp())
          ilsFreqTexts.append(ils.freqMHzLocale());
      }
    }

    // VOR/NDB type ===========================
    if(leg.getVor().isValid())
      itemRow[rcol::TYPE] = new QStandardItem(map::vorFullShortText(leg.getVor()));
    else if(leg.getNdb().isValid())
      itemRow[rcol::TYPE] = new QStandardItem(map::ndbFullShortText(leg.getNdb()));
    else if(!ilsTypeTexts.isEmpty())
      itemRow[rcol::TYPE] = new QStandardItem(ilsTypeTexts.join(", "));

    // VOR/NDB frequency =====================
    if(leg.getVor().isValid())
    {
      if(leg.getVor().tacan)
        itemRow[rcol::FREQ] = new QStandardItem(leg.getVor().channel);
      else
        itemRow[rcol::FREQ] = new QStandardItem(QLocale().toString(leg.getFrequency() / 1000.f, 'f', 2));
    }
    else if(leg.getNdb().isValid())
      itemRow[rcol::FREQ] = new QStandardItem(QLocale().toString(leg.getFrequency() / 100.f, 'f', 1));
    else if(!ilsFreqTexts.isEmpty())
      itemRow[rcol::FREQ] = new QStandardItem(ilsFreqTexts.join(", "));

    // VOR/NDB range =====================
    if(leg.getRange() > 0 && (leg.getVor().isValid() || leg.getNdb().isValid()))
      itemRow[rcol::RANGE] = new QStandardItem(Unit::distNm(leg.getRange(), false));

    // Course =====================
    bool afterArrivalAirport = route.isAirportAfterArrival(i);
    bool parkingToDeparture = route.hasAnySidProcedure() && i == 0;

    if(row > 0 && !afterArrivalAirport && !parkingToDeparture)
    {
      float courseMag = map::INVALID_COURSE_VALUE, courseTrue = map::INVALID_COURSE_VALUE;
      leg.getMagTrueRealCourse(courseMag, courseTrue);

      if(courseMag < map::INVALID_COURSE_VALUE)
        itemRow[rcol::COURSE] = new QStandardItem(QLocale().toString(courseMag, 'f', 0));
      if(courseTrue < map::INVALID_COURSE_VALUE)
        itemRow[rcol::COURSETRUE] = new QStandardItem(QLocale().toString(courseTrue, 'f', 0));
    }

    // Distance =====================
    if(!afterArrivalAirport)
    {
      if(leg.getDistanceTo() < map::INVALID_DISTANCE_VALUE /*&& !leg.noDistanceDisplay()*/)
      {
        cumulatedDistance += leg.getDistanceTo();
        itemRow[rcol::DIST] = new QStandardItem(Unit::distNm(leg.getDistanceTo(), false));

        if(!procedureLeg.isMissed() && !leg.isAlternate())
        {
          float remaining = totalDistance - cumulatedDistance;
          if(remaining < 0.f)
            remaining = 0.f; // Catch the -0 case due to rounding errors
          itemRow[rcol::REMAINING_DISTANCE] = new QStandardItem(Unit::distNm(remaining, false));
        }
      }
    }

    itemRow[rcol::LATITUDE] = new QStandardItem(Unit::coordsLatY(leg.getPosition()));
    itemRow[rcol::LONGITUDE] = new QStandardItem(Unit::coordsLonX(leg.getPosition()));
    itemRow[rcol::MAGVAR] = new QStandardItem(map::magvarText(leg.getMagvarEndOrCalibrated(), true /* shortText */, false /* degSign */));

    itemRow[rcol::RECOMMENDED] = new QStandardItem(atools::elideTextShort(proc::procedureLegRecommended(procedureLeg).join(tr(", ")), 80));

    QString comment;
    if(leg.isAnyProcedure())
      // Show procedure remarks like turn and flyover
      comment = proc::procedureLegRemark(procedureLeg).join(tr(" / "));
    else
      // Show user comment on normal leg
      comment = leg.getFlightplanEntry().getComment();
    if(!comment.isEmpty())
    {
      itemRow[rcol::REMARKS] = new QStandardItem(atools::elideTextShort(comment, 80));
      itemRow[rcol::REMARKS]->setToolTip(atools::elideTextLinesShort(comment, 20, 80));
    }

    // Travel time, remaining fuel and ETA are updated in updateModelRouteTime

    // Create empty items for missing fields ===================
    for(int col = rcol::FIRST_COLUMN; col <= rcol::LAST_COLUMN; col++)
    {
      if(itemRow[col] == nullptr)
        itemRow[col] = new QStandardItem();

      // Do not allow editing and drag and drop
      itemRow[col]->setFlags(itemRow[col]->flags() & ~(Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled));
    }

    // Align cells to the right - rest is aligned in updateModelRouteTimeFuel ===============
    itemRow[rcol::REGION]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::REMAINING_DISTANCE]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::DIST]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::COURSE]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::COURSETRUE]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::RANGE]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::FREQ]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::RESTRICTION]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::LEG_TIME]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::ETA]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::FUEL_WEIGHT]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::FUEL_VOLUME]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::WIND]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::WIND_HEAD_TAIL]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::ALTITUDE]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::SAFE_ALTITUDE]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::LATITUDE]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::LONGITUDE]->setTextAlignment(Qt::AlignRight);
    itemRow[rcol::MAGVAR]->setTextAlignment(Qt::AlignRight);

    model->appendRow(itemRow);

    for(int col = rcol::FIRST_COLUMN; col <= rcol::LAST_COLUMN; col++)
      itemRow[col] = nullptr;
    row++;
  }

  // No need to update selection
  unBlockModel();

  updateModelTimeFuelWindAlt();

  Flightplan& flightplan = route.getFlightplan();

  if(!flightplan.isEmpty())
  {
    // Set spin box and block signals to avoid recursive call
    {
      QSignalBlocker blocker(ui->spinBoxRouteAlt);
      ui->spinBoxRouteAlt->setValue(atools::roundToInt(Unit::altFeetF(flightplan.getCruiseAltitudeFt())));
    }

    updateComboBoxFromFlightplanType();
  }

  // Update header tooltips
  for(int col = rcol::FIRST_COLUMN; col <= rcol::LAST_COLUMN; col++)
    model->horizontalHeaderItem(col)->setToolTip(routeColumnDescription.at(col));

  updateModelHighlightsAndErrors();
  highlightNextWaypoint(route.getActiveLegIndexCorrected());

  routeLabel->updateHeaderLabel();
  routeLabel->updateFooterSelectionLabel();

  updatePlaceholderWidget();

  // Value from ui file is ignored
  tableViewRoute->horizontalHeader()->setMinimumSectionSize(3);
}

void RouteController::updateComboBoxFromFlightplanType()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  Flightplan& flightplan = route.getFlightplan();

  // Set combo box and block signals to avoid recursive call
  QSignalBlocker blocker(ui->comboBoxRouteType);
  if(flightplan.getFlightplanType() == atools::fs::pln::IFR)
    ui->comboBoxRouteType->setCurrentIndex(0);
  else if(flightplan.getFlightplanType() == atools::fs::pln::VFR)
    ui->comboBoxRouteType->setCurrentIndex(1);
}

void RouteController::updateModelTimeFuelWindAlt()
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO;
#endif

  // Check if model is already initailized
  if(model->rowCount() == 0)
    return;

  using atools::fs::perf::AircraftPerf;
  const RouteAltitude& altitudeLegs = route.getAltitudeLegs();
  if(altitudeLegs.isEmpty())
    return;

  int row = 0;
  float cumulatedTravelTime = 0.f;
  bool setValues = !altitudeLegs.hasErrors();
  const AircraftPerf& perf = NavApp::getAircraftPerformance();
  float totalFuelLbsOrGal = altitudeLegs.getTripFuel() + altitudeLegs.getAlternateFuel();

  if(setValues)
  {
    totalFuelLbsOrGal *= perf.getContingencyFuelFactor();
    totalFuelLbsOrGal += perf.getExtraFuel() + perf.getReserveFuel();
  }

  // Remember colum widths
  QHeaderView *header = tableViewRoute->horizontalHeader();
  int widthLegTime = header->isSectionHidden(rcol::LEG_TIME) ? -1 : tableViewRoute->columnWidth(rcol::LEG_TIME);
  int widthEta = header->isSectionHidden(rcol::ETA) ? -1 : tableViewRoute->columnWidth(rcol::ETA);
  int widthFuelWeight = header->isSectionHidden(rcol::FUEL_WEIGHT) ? -1 : tableViewRoute->columnWidth(rcol::FUEL_WEIGHT);
  int widthFuelVol = header->isSectionHidden(rcol::FUEL_VOLUME) ? -1 : tableViewRoute->columnWidth(rcol::FUEL_VOLUME);
  int widthWind = header->isSectionHidden(rcol::WIND) ? -1 : tableViewRoute->columnWidth(rcol::WIND);
  int widthWindHt = header->isSectionHidden(rcol::WIND_HEAD_TAIL) ? -1 : tableViewRoute->columnWidth(rcol::WIND_HEAD_TAIL);
  int widthAlt = header->isSectionHidden(rcol::ALTITUDE) ? -1 : tableViewRoute->columnWidth(rcol::ALTITUDE);
  int widthSafeAlt = header->isSectionHidden(rcol::SAFE_ALTITUDE) ? -1 : tableViewRoute->columnWidth(rcol::SAFE_ALTITUDE);

  for(int i = 0; i < route.size(); i++)
  {
    if(i >= model->rowCount())
    {
      qWarning() << Q_FUNC_INFO << "Route size exceeds model size" << i << model->rowCount() - 1;
      break;
    }

    if(!setValues)
    {
      // Do not fill if collecting performance or route altitude is invalid
      model->item(row, rcol::LEG_TIME)->setText(QString());
      model->item(row, rcol::ETA)->setText(QString());
      model->item(row, rcol::FUEL_WEIGHT)->setText(QString());
      model->item(row, rcol::FUEL_VOLUME)->setText(QString());
      model->item(row, rcol::WIND)->setText(QString());
      model->item(row, rcol::WIND_HEAD_TAIL)->setText(QString());
      model->item(row, rcol::ALTITUDE)->setText(QString());
      model->item(row, rcol::SAFE_ALTITUDE)->setText(QString());
    }
    else
    {
      const RouteLeg& leg = route.value(i);
      const RouteAltitudeLeg& altLeg = altitudeLegs.value(i);

      // Leg time =====================================================================
      float travelTime = altLeg.getTime();
      if(row == 0 || !(travelTime < map::INVALID_TIME_VALUE) || leg.getProcedureLeg().isMissed())
        model->item(row, rcol::LEG_TIME)->setText((QString()));
      else
      {
        QString txt = formatter::formatMinutesHours(travelTime);
#ifdef DEBUG_INFORMATION_LEGTIME
        txt += " [" + QString::number(travelTime * 3600., 'f', 0) + "]";
#endif
        model->item(row, rcol::LEG_TIME)->setText(txt);
      }

      if(!leg.getProcedureLeg().isMissed())
      {
        // Arrival time =====================================================================
        QString txt = formatter::formatMinutesHours(cumulatedTravelTime + travelTime);

        // Do not sum up for alternates - calculate again from destination for each alternate
        if(!leg.isAlternate())
          cumulatedTravelTime += travelTime;

#ifdef DEBUG_INFORMATION_LEGTIME
        txt += " [" + QString::number(cumulatedTravelTime * 3600., 'f', 0) + "]";
#endif
        model->item(row, rcol::ETA)->setText(txt);

        // Fuel at leg =====================================================================
        if(!leg.isAlternate())
          totalFuelLbsOrGal -= altLeg.getFuel();
        float totalTempFuel = totalFuelLbsOrGal;

        if(leg.isAlternate())
          totalTempFuel -= altLeg.getFuel();

        float weight = 0.f, vol = 0.f;
        if(perf.useFuelAsVolume())
        {
          weight = atools::geo::fromGalToLbs(perf.isJetFuel(), totalTempFuel);
          vol = totalTempFuel;
        }
        else
        {
          weight = totalTempFuel;
          vol = atools::geo::fromLbsToGal(perf.isJetFuel(), totalTempFuel);
        }

        if(atools::almostEqual(vol, 0.f, 0.01f))
          // Avoid -0 case
          vol = 0.f;
        if(atools::almostEqual(weight, 0.f, 0.01f))
          // Avoid -0 case
          weight = 0.f;

        txt = perf.isFuelFlowValid() ? Unit::weightLbs(weight, false /* addUnit */) : QString();
        model->item(row, rcol::FUEL_WEIGHT)->setText(txt);

        txt = perf.isFuelFlowValid() ? Unit::volGallon(vol, false /* addUnit */) : QString();
        model->item(row, rcol::FUEL_VOLUME)->setText(txt);

        // Wind at waypoint ========================================================
        // Exclude departure and all after including destination airport
        if(row > route.getDepartureAirportLegIndex() && row < route.getDestinationAirportLegIndex())
        {
          txt.clear();
          float headWind = 0.f, crossWind = 0.f;
          if(altLeg.getWindSpeed() >= 1.f)
          {
            // Wind at waypoint at end of leg
            atools::geo::windForCourse(headWind, crossWind, altLeg.getWindSpeed(), altLeg.getWindDirection(), leg.getCourseEndTrue());

            txt = tr("%1 / %2").
                  arg(atools::geo::normalizeCourse(altLeg.getWindDirection() - leg.getMagvarEnd()), 0, 'f', 0).
                  arg(Unit::speedKts(altLeg.getWindSpeed(), false /* addUnit */));
          }

          model->item(row, rcol::WIND)->setText(txt);

          // Head or tailwind at waypoint ========================================================
          txt.clear();
          if(std::abs(headWind) >= 1.f)
          {
            QString ptr;
            if(headWind >= 1.f)
              ptr = TextPointer::getWindPointerSouth();
            else if(headWind <= -1.f)
              ptr = TextPointer::getWindPointerNorth();
            txt.append(tr("%1 %2").arg(ptr).arg(Unit::speedKts(std::abs(headWind), false /* addUnit */)));
          }
          model->item(row, rcol::WIND_HEAD_TAIL)->setText(txt);
        }

        // Altitude at waypoint ========================================================
        txt.clear();
        float alt = altLeg.getWaypointAltitude();
        if(alt < map::INVALID_ALTITUDE_VALUE)
          txt = Unit::altFeet(alt, false /* addUnit */);
        model->item(row, rcol::ALTITUDE)->setText(txt);

        // Leg safe altitude ========================================================
        txt.clear();
        // Get ground buffer when flying towards waypoint
        float safeAlt = NavApp::getGroundBufferForLegFt(i - 1);
        if(safeAlt < map::INVALID_ALTITUDE_VALUE)
          txt = Unit::altFeet(safeAlt, false /* addUnit */);
        model->item(row, rcol::SAFE_ALTITUDE)->setText(txt);
      } // if(!leg.getProcedureLeg().isMissed())
    } // else if(!route.isAirportAfterArrival(row))
    row++;
  } // for(int i = 0; i < route.size(); i++)

  // Set back column widths if visible - widget changes widths on setItem
  if(widthLegTime > 0)
    tableViewRoute->setColumnWidth(rcol::LEG_TIME, widthLegTime);
  else
    header->hideSection(rcol::LEG_TIME);

  if(widthEta > 0)
    tableViewRoute->setColumnWidth(rcol::ETA, widthEta);
  else
    header->hideSection(rcol::ETA);

  if(widthFuelWeight > 0)
    tableViewRoute->setColumnWidth(rcol::FUEL_WEIGHT, widthFuelWeight);
  else
    header->hideSection(rcol::FUEL_WEIGHT);

  if(widthFuelVol > 0)
    tableViewRoute->setColumnWidth(rcol::FUEL_VOLUME, widthFuelVol);
  else
    header->hideSection(rcol::FUEL_VOLUME);

  if(widthWind > 0)
    tableViewRoute->setColumnWidth(rcol::WIND, widthWind);
  else
    header->hideSection(rcol::WIND);

  if(widthWindHt > 0)
    tableViewRoute->setColumnWidth(rcol::WIND_HEAD_TAIL, widthWindHt);
  else
    header->hideSection(rcol::WIND_HEAD_TAIL);

  if(widthAlt > 0)
    tableViewRoute->setColumnWidth(rcol::ALTITUDE, widthAlt);
  else
    header->hideSection(rcol::ALTITUDE);

  if(widthSafeAlt > 0)
    tableViewRoute->setColumnWidth(rcol::SAFE_ALTITUDE, widthSafeAlt);
  else
    header->hideSection(rcol::SAFE_ALTITUDE);
}

void RouteController::disconnectedFromSimulator()
{
  qDebug() << Q_FUNC_INFO;

  route.resetActive();
  highlightNextWaypoint(-1);
  emit routeChanged(false /* geometryChanged */);
}

void RouteController::simDataChanged(const atools::fs::sc::SimConnectData& simulatorData)
{
  if(!loadingDatabaseState && atools::almostNotEqual(QDateTime::currentDateTime().toMSecsSinceEpoch(),
                                                     lastSimUpdate, static_cast<qint64>(MIN_SIM_UPDATE_TIME_MS)))
  {
    if(simulatorData.isUserAircraftValid())
    {
      aircraft = simulatorData.getUserAircraftConst();

      // Sequence only for airborne airplanes
      // Use more than one parameter since first X-Plane data packets are unreliable
      map::PosCourse position(aircraft.getPosition(), aircraft.getTrackDegTrue());
      if(aircraft.isFlying())
      {
        int previousRouteLeg = route.getActiveLegIndexCorrected();
        route.updateActiveLegAndPos(position);
        int activeLegIdx = route.getActiveLegIndexCorrected();

        if(!tableCleanupTimer.isActive())
        {
          // Timer is not active - set active if any option is set
          if((hasTableSelection() && OptionData::instance().getFlags2().testFlag(opts2::ROUTE_CLEAR_SELECTION)) ||
             OptionData::instance().getFlags2().testFlag(opts2::ROUTE_CENTER_ACTIVE_LEG))
            tableCleanupTimer.start();
        }

        if(activeLegIdx != previousRouteLeg)
        {
          // Use corrected indexes to highlight initial fix
          qDebug() << Q_FUNC_INFO << "new route leg" << previousRouteLeg << activeLegIdx;
          highlightNextWaypoint(activeLegIdx);
          NavApp::updateAllMaps();
        }
      }
      else
        route.updateActivePos(position);
    }
    lastSimUpdate = QDateTime::currentDateTime().toMSecsSinceEpoch();
  }
}

void RouteController::scrollToActive()
{
  if(NavApp::isConnectedAndAircraftFlying())
  {
    int routeLeg = route.getActiveLegIndexCorrected();

    // Check if model is already initailized
    if(model->rowCount() == 0)
      return;

    if(routeLeg != map::INVALID_INDEX_VALUE && routeLeg >= 0 &&
       OptionData::instance().getFlags2().testFlag(opts2::ROUTE_CENTER_ACTIVE_LEG))
      tableViewRoute->scrollTo(model->index(std::max(routeLeg - 1, 0), 0), QAbstractItemView::PositionAtTop);
  }
}

void RouteController::highlightNextWaypoint(int activeLegIdx)
{
  // Check if model is already initailized
  if(model->rowCount() == 0)
    return;

  activeLegIndex = activeLegIdx;
  for(int row = 0; row < model->rowCount(); ++row)
  {
    for(int col = 0; col < model->columnCount(); col++)
    {
      QStandardItem *item = model->item(row, col);
      if(item != nullptr)
      {
        item->setBackground(Qt::NoBrush);
        // Keep first column bold
        if(item->font().bold() && col != 0)
        {
          QFont font = item->font();
          font.setBold(tableViewRoute->font().bold());
          item->setFont(font);
        }
      }
    }
  }

  if(!route.isEmpty() && OptionData::instance().getFlags2().testFlag(opts2::ROUTE_HIGHLIGHT_ACTIVE_TABLE))
  {
    // Add magenta brush for all columns in active row ======================
    if(activeLegIndex >= 0 && activeLegIndex < route.size())
    {
      QColor color = NavApp::isGuiStyleDark() ? mapcolors::nextWaypointColorDark : mapcolors::nextWaypointColor;

      for(int col = 0; col < model->columnCount(); col++)
      {
        QStandardItem *item = model->item(activeLegIndex, col);
        if(item != nullptr)
        {
          item->setBackground(color);
          if(!item->font().bold())
          {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
          }
        }
      }
    }
  }
}

void RouteController::clearAirwayViolations()
{
  const RouteAltitude& altitudeLegs = route.getAltitudeLegs();
  int tocIndex = altitudeLegs.getTopOfClimbLegIndex();
  int todIndex = altitudeLegs.getTopOfDescentLegIndex();

  for(int i = 0; i < route.size(); i++)
  {
    if(i > tocIndex && i < todIndex)
      continue;

    const RouteLeg& leg = route.value(i);
    const RouteAltitudeLeg& altLeg = altitudeLegs.value(i);
    float maxAlt = altLeg.getMaxAltitude();
    float minAlt = altLeg.getMinAltitude();

    if(minAlt < map::INVALID_ALTITUDE_VALUE && maxAlt < map::INVALID_ALTITUDE_VALUE)
    {
      if(leg.isAirwaySetAndInvalid(minAlt, maxAlt, nullptr, nullptr))
      {
#ifdef DEBUG_INFORMATION
        qDebug() << Q_FUNC_INFO << "Violating restrictions" << i << leg;
#endif
        // Has airway but errors - remove airway in flight plan and leg
        route.eraseAirwayLeg(i);
      }
    }
  }
}

void RouteController::updateModelHighlightsAndErrors()
{
  flightplanErrors.clear();
  parkingErrors.clear();
  trackErrors = false;

  // Check if model is already initailized
  if(model->rowCount() == 0)
    return;

  const QString& departureParkingName = route.getFlightplan().getDepartureParkingName();

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "departureParkingName" << departureParkingName
           << "getDepartureParkingTypeStr()" << route.getFlightplan().getDepartureParkingTypeStr()
           << "getDepartureStart()" << route.getDepartureStart()
           << "getDepartureParking()" << route.getDepartureParking();
#endif

  if(!departureParkingName.isEmpty() && !route.getDepartureStart().isValid() && !route.getDepartureParking().isValid())
  {
    qWarning() << Q_FUNC_INFO << "Parking or start position" << departureParkingName << "not found at departure airport";
    parkingErrors.append(tr("Parking or start position \"%1\" not found at departure airport.").arg(departureParkingName));
  }

  bool dark = NavApp::isGuiStyleDark();
  const QColor defaultColor = QApplication::palette().color(QPalette::Normal, QPalette::Text);
  const QColor invalidColor = dark ? mapcolors::routeInvalidTableColorDark : mapcolors::routeInvalidTableColor;

  for(int row = 0; row < model->rowCount(); row++)
  {
    const RouteLeg& leg = route.value(row);
    const RouteAltitudeLeg& altLeg = route.getAltitudeLegAt(row);
    float maxAlt = altLeg.getMaxAltitude();
    float minAlt = altLeg.getMinAltitude();

    if(!(minAlt < map::INVALID_ALTITUDE_VALUE) || !(maxAlt < map::INVALID_ALTITUDE_VALUE))
      // Use cruise altitude if either is invalid
      minAlt = maxAlt = route.getCruiseAltitudeFt();

    if(!leg.isValid())
    {
      // Have to check here since sim updates can still happen while building the flight plan
      qWarning() << Q_FUNC_INFO << "Invalid index" << row;
      break;
    }

    for(int col = 0; col < model->columnCount(); col++)
    {
      QStandardItem *item = model->item(row, col);
      if(item != nullptr)
      {
        // Set default font color for all items ==============
        item->setForeground(defaultColor);

        if(leg.isAlternate())
          item->setForeground(dark ? mapcolors::routeAlternateTableColorDark : mapcolors::routeAlternateTableColor);
        else if(leg.isAnyProcedure())
        {
          if(leg.getProcedureLeg().isMissed())
            item->setForeground(dark ? mapcolors::routeProcedureMissedTableColorDark : mapcolors::routeProcedureMissedTableColor);
          else
            item->setForeground(dark ? mapcolors::routeProcedureTableColorDark : mapcolors::routeProcedureTableColor);
        }

        // Ident colum ==========================================
        if(col == rcol::IDENT)
        {
          if(leg.getMapType() == map::INVALID)
          {
            item->setForeground(invalidColor);
            QString err = tr("Waypoint \"%1\" not found.").arg(leg.getDisplayIdent());
            item->setToolTip(err);
            flightplanErrors.append(err);
          }
          else
            item->setToolTip(QString());

          if(leg.isAirport())
          {
            QFont font = item->font();
            if(leg.getAirport().addon())
            {
              font.setItalic(true);
              font.setUnderline(true);
            }
            if(leg.getAirport().closed())
              font.setStrikeOut(true);
            item->setFont(font);
          }
        }

        // Airway colum ==========================================
        if(col == rcol::AIRWAY_OR_LEGTYPE && leg.isRoute())
        {
          QStringList airwayErrors;
          bool trackError = false;
          if(leg.isAirwaySetAndInvalid(minAlt, maxAlt, &airwayErrors, &trackError))
          {
            // Has airway but errors
            item->setForeground(invalidColor);
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            if(!airwayErrors.isEmpty())
            {
              item->setToolTip(airwayErrors.join(tr("\n")));
              flightplanErrors.append(airwayErrors);
            }
          }
          else if(row != activeLegIndex)
          {
            // No airway or no errors - leave font bold if this is the active
            QFont font = item->font();
            font.setBold(tableViewRoute->font().bold());
            item->setFont(font);
            item->setToolTip(QString());
          }

          trackErrors |= trackError;
        }
      }
    }
  }
}

bool RouteController::hasErrors() const
{
  return !flightplanErrors.isEmpty() || !procedureErrors.isEmpty() || !alternateErrors.isEmpty() || !parkingErrors.isEmpty();
}

QStringList RouteController::getErrorStrings() const
{
  QStringList toolTip;
  if(hasErrors())
  {
    toolTip.append(flightplanErrors);
    toolTip.append(parkingErrors);

    if(!procedureErrors.isEmpty())
    {
      toolTip.append(tr("Cannot load %1: %2").
                     arg(procedureErrors.size() > 1 ? tr("procedures") : tr("procedure")).
                     arg(procedureErrors.join(tr(", "))));
      toolTip.append(tr("Save and reload flight plan or select new procedures to fix this."));
    }

    if(!alternateErrors.isEmpty())
      toolTip.append(tr("Cannot load %1: %2").
                     arg(alternateErrors.size() > 1 ? tr("alternates") : tr("alternate")).
                     arg(alternateErrors.join(tr(", "))));

    if(trackErrors)
      toolTip.append(tr("Download oceanic tracks in menu \"Flight Plan\"\n"
                        "or calculate the flight plan again if the flight plan uses tracks.",
                        "Keep in sync with menu names"));
  }

#ifdef DEBUG_INFORMATION
  if(!toolTip.isEmpty())
    qDebug() << Q_FUNC_INFO << toolTip;
#endif

  return toolTip;
}

void RouteController::flightplanLabelLinkActivated(const QString& link)
{
  qDebug() << Q_FUNC_INFO << link;
  // lnm://showdeparture

  QUrl url(link);
  if(url.scheme() == "lnm")
  {
    optsd::DisplayClickOptions opts = OptionData::instance().getDisplayClickOptions();

    if(url.host() == "showdeparture")
    {
      showAtIndex(route.getDepartureAirportLegIndex(), true /* info */, true /* map */, false /* doubleClick */);

      // Show departure procedures in search dialog if requested by user
      if(opts.testFlag(optsd::CLICK_AIRPORT_PROC))
      {
        bool departureFilter, arrivalFilter;
        route.getAirportProcedureFlags(route.getDepartureAirportLeg().getAirport(), -1, departureFilter, arrivalFilter);
        emit showProcedures(route.getDepartureAirportLeg().getAirport(), departureFilter, arrivalFilter);
      }
    }
    else if(url.host() == "showdepartureparking")
    {
      const RouteLeg& departureAirportLeg = route.getDepartureAirportLeg();
      if(departureAirportLeg.getDepartureParking().isValid())
        emit showPos(departureAirportLeg.getDepartureParking().getPosition(), 0.f, false /* doubleClick */);
      else if(departureAirportLeg.getDepartureStart().isValid())
        emit showPos(departureAirportLeg.getDepartureStart().getPosition(), 0.f, false /* doubleClick */);
      showAtIndex(route.getDepartureAirportLegIndex(), true /* info */, false /* map */, false /* doubleClick */);
    }
    else if(url.host() == "showdestination")
    {
      showAtIndex(route.getDestinationAirportLegIndex(), true /* info */, true /* map */, false /* doubleClick */);

      // Show arrival procedures in search dialog if requested by user
      if(opts.testFlag(optsd::CLICK_AIRPORT_PROC))
      {
        bool departureFilter, arrivalFilter;
        route.getAirportProcedureFlags(route.getDestinationAirportLeg().getAirport(), -1, departureFilter, arrivalFilter);
        emit showProcedures(route.getDestinationAirportLeg().getAirport(), departureFilter, arrivalFilter);
      }
    }
  }
}

void RouteController::clearRouteAndUndo()
{
  route.clearAll();
  routeFilename.clear();
  fileDepartureIdent.clear();
  fileDestinationIdent.clear();
  fileIfrVfr = pln::VFR;
  fileCruiseAltFt = 0.f;
  undoStack->clear();
  undoIndex = 0;
  undoIndexClean = 0;
  entryBuilder->setCurUserpointNumber(1);
  updateFlightplanFromWidgets();
}

/* Reset route and clear undo stack (new route) */
void RouteController::clearRoute()
{
  route.clearAll();
  entryBuilder->setCurUserpointNumber(1);
  updateFlightplanFromWidgets();
}

RouteCommand *RouteController::preChange(const QString& text)
{
  // Clean the flight plan from any procedure entries
  Flightplan flightplan = route.getFlightplanConst();
  flightplan.removeProcedureEntries();
  return new RouteCommand(this, flightplan, text);
}

void RouteController::postChange(RouteCommand *undoCommand)
{
  if(undoCommand != nullptr)
  {
    // Clean the flight plan from any procedure entries
    Flightplan flightplan = route.getFlightplanConst();
    flightplan.removeProcedureEntries();
    undoCommand->setFlightplanAfter(flightplan);

    if(undoIndex < undoIndexClean)
      undoIndexClean = -1;

    // Index and clean index workaround
    undoIndex++;
#ifdef DEBUG_INFORMATION
    qDebug() << "postChange undoIndex" << undoIndex << "undoIndexClean" << undoIndexClean;
#endif
    undoStack->push(undoCommand);
  }
}

proc::MapProcedureTypes RouteController::affectedProcedures(const QList<int>& indexes)
{
  proc::MapProcedureTypes types = proc::PROCEDURE_NONE;

  for(int index : indexes)
  {
    if(index == 0)
      // Delete SID if departure airport is affected
      types |= proc::PROCEDURE_DEPARTURE;

    if(index >= route.getDestinationAirportLegIndex())
    {
      int altIndex = route.getAlternateLegsOffset();
      // Check if trying to delete an alternate
      if(altIndex == map::INVALID_INDEX_VALUE || index < altIndex)
        // Delete all arrival procedures if destination airport is affected or an new leg is appended after
        types |= proc::PROCEDURE_ARRIVAL_ALL;
    }

    if(index >= 0 && index < route.getDestinationAirportLegIndex())
    {
      const proc::MapProcedureLeg& leg = route.value(index).getProcedureLeg();

      if(leg.isSidTransition())
        types |= proc::PROCEDURE_SID_TRANSITION;

      if(leg.isSid())
        // Delete SID and transition
        types |= proc::PROCEDURE_DEPARTURE;

      if(leg.isStarTransition())
        types |= proc::PROCEDURE_STAR_TRANSITION;

      if(leg.isStar())
        // Delete STAR and transition
        types |= proc::PROCEDURE_STAR_ALL;

      if(leg.isTransition())
        // Delete transition only
        types |= proc::PROCEDURE_TRANSITION;

      if(leg.isApproach() || leg.isMissed())
        // Delete transition and approach
        types |= proc::PROCEDURE_APPROACH_ALL_MISSED;
    }
  }

  if(types & proc::PROCEDURE_SID_TRANSITION && route.getSidLegs().procedureLegs.isEmpty() &&
     !route.getSidLegs().procedureFixIdent.isEmpty())
    // Remove the empty SID structure too
    types |= proc::PROCEDURE_SID;

  if(types & proc::PROCEDURE_STAR_TRANSITION && route.getStarLegs().procedureLegs.isEmpty())
    // Remove the empty STAR structure too
    types |= proc::PROCEDURE_STAR_ALL;

  qDebug() << Q_FUNC_INFO << "indexes" << indexes << "types" << types;

  return types;
}

void RouteController::remarksFlightplanToWidget()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  // Block signals to avoid recursion
  ui->plainTextEditRouteRemarks->blockSignals(true);
  ui->plainTextEditRouteRemarks->setPlainText(route.getFlightplanConst().getComment());
  ui->plainTextEditRouteRemarks->blockSignals(false);
}

void RouteController::remarksTextChanged()
{
  if(route.isEmpty())
    return;

  QString editText = NavApp::getMainUi()->plainTextEditRouteRemarks->toPlainText();
  pln::Flightplan& flightplan = route.getFlightplan();

  if(flightplan.getComment() != editText)
  {
    // Text is different add with undo
    RouteCommand *undoCommand = preChange(tr("Remarks changed"));
    flightplan.setComment(editText);
    postChange(undoCommand);

    // Update title and tab with "*" for changed
    NavApp::updateWindowTitle();
  }
}

void RouteController::updateRemarkHeader()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  if(!route.isEmpty())
  {
    // <SimData Cycle="2109">XP11<k/SimData>
    // <NavData Cycle="2109">XP11</NavData>
    // <AircraftPerformance>
    // <FilePath>JustFlight BAe 146 100.lnmperf</FilePath>
    // <Type>B461</Type>
    // <Name>JustFlight BAe 146 100</Name>
    // </AircraftPerformance>

    const QHash<QString, QString>& props = route.getFlightplanConst().getProperties();

    atools::util::HtmlBuilder html;

    // Aircraft peformance name and type =============================================================
    QString perf = atools::strJoin({props.value(atools::fs::pln::AIRCRAFT_PERF_NAME),
                                    props.value(atools::fs::pln::AIRCRAFT_PERF_TYPE)}, tr(", "), tr(", "));
    if(!perf.isEmpty())
      html.b(tr("Aircraft Performance: ")).text(perf);

    // Scenery data and cycle =============================================================
    using atools::fs::FsPaths;
    QStringList data;
    QString simdata = FsPaths::typeToShortDisplayName(FsPaths::stringToType(props.value(atools::fs::pln::SIMDATA)));
    QString navdata = FsPaths::typeToShortDisplayName(FsPaths::stringToType(props.value(atools::fs::pln::NAVDATA)));
    QString simcycle = props.value(atools::fs::pln::SIMDATA_CYCLE);
    QString navcycle = props.value(atools::fs::pln::NAVDATA_CYCLE);

    if(!simdata.isEmpty())
      data.append(tr("%1%2").arg(simdata).arg(simcycle.isEmpty() ? QString() : tr(" cycle %1").arg(simcycle)));

    if(!navdata.isEmpty() && (navdata != simdata || simcycle != navcycle))
      data.append(tr("%1%2").arg(navdata).arg(navcycle.isEmpty() ? QString() : tr(" cycle %1").arg(navcycle)));

    if(!data.isEmpty())
    {
      if(!perf.isEmpty())
        html.br();
      html.b(tr("Scenery data: ")).text(data.join(tr(", ")));
    }

    ui->labelRouteRemarksHeader->setVisible(!html.isEmpty());
    if(!html.isEmpty())
      ui->labelRouteRemarksHeader->setText(html.getHtml());
  }
  else
    // No plan - hide widget
    ui->labelRouteRemarksHeader->setVisible(false);
}

void RouteController::resetTabLayout()
{
  tabHandlerRoute->reset();
}

void RouteController::updateFooterErrorLabel()
{
  routeLabel->updateFooterErrorLabel();
}

void RouteController::updateRemarkWidget()
{
  QPlainTextEdit *edit = NavApp::getMainUi()->plainTextEditRouteRemarks;
  if(route.isEmpty())
  {
    edit->setReadOnly(true);
    edit->setPlaceholderText(tr("No flight plan.\n\nRemarks for the flight plan."));
  }
  else
  {
    edit->setReadOnly(false);
    edit->setPlaceholderText(tr("Remarks for the flight plan."));
  }
}

QStringList RouteController::getAllRouteColumns() const
{
  QStringList colums;
  for(int i = rcol::FIRST_COLUMN; i <= rcol::LAST_COLUMN; i++)
    colums.append(Unit::replacePlaceholders(routeColumns.value(i)).replace("-\n", "").replace("\n", " "));

  return colums;
}

void RouteController::clearAllErrors()
{
  procedureErrors.clear();
  alternateErrors.clear();
  flightplanErrors.clear();
  parkingErrors.clear();
  trackErrors = false;
}

void RouteController::blockModel()
{
  // model->blockSignals(true);
  modelUpdatesBlocked = true;
}

void RouteController::unBlockModel()
{
  // model->blockSignals(false);
  modelUpdatesBlocked = false;
}

#ifdef DEBUG_NETWORK_INFORMATION

void RouteController::debugNetworkClick(const atools::geo::Pos& pos)
{
  if(pos.isValid())
  {
    qDebug() << Q_FUNC_INFO << pos;

    atools::routing::RouteNetworkLoader loader(NavApp::getDatabaseNav(), NavApp::getDatabaseTrack());
    if(!routeNetworkAirway->isLoaded())
      loader.load(routeNetworkAirway);
    if(!routeNetworkRadio->isLoaded())
      loader.load(routeNetworkRadio);

    atools::routing::Node node = routeNetworkAirway->getNearestNode(pos);
    if(node.isValid())
    {
      qDebug() << "Airway node" << node;
      qDebug() << "Airway edges" << node.edges;
    }

    node = routeNetworkRadio->getNearestNode(pos);
    if(node.isValid())
    {
      qDebug() << "Radio node" << node;
      qDebug() << "Radio edges" << node.edges;
    }
  }
}

#endif
