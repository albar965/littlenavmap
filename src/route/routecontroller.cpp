/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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
#include "route/routestring.h"
#include "route/routestringdialog.h"

#include "navapp.h"
#include "options/optiondata.h"
#include "common/procedurequery.h"
#include "common/constants.h"
#include "common/formatter.h"
#include "search/proceduresearch.h"
#include "common/unit.h"
#include "exception.h"
#include "export/csvexporter.h"
#include "gui/actiontextsaver.h"
#include "gui/errorhandler.h"
#include "gui/itemviewzoomhandler.h"
#include "gui/widgetstate.h"
#include "mapgui/mapquery.h"
#include "mapgui/mapwidget.h"
#include "parkingdialog.h"
#include "route/routefinder.h"
#include "route/routenetworkairway.h"
#include "route/routenetworkradio.h"
#include "settings/settings.h"
#include "ui_mainwindow.h"
#include "gui/dialog.h"
#include "atools.h"
#include "route/userwaypointdialog.h"
#include "route/flightplanentrybuilder.h"
#include "route/routestringdialog.h"
#include "util/htmlbuilder.h"
#include "common/symbolpainter.h"
#include "common/mapcolors.h"
#include "common/unit.h"

#include <QClipboard>
#include <QFile>
#include <QStandardItemModel>
#include <QInputDialog>

#include <marble/GeoDataLineString.h>

// Route table colum headings
const QList<QString> ROUTE_COLUMNS({QObject::tr("Ident"),
                                    QObject::tr("Region"),
                                    QObject::tr("Name"),
                                    QObject::tr("Procedure"),
                                    QObject::tr("Airway or\nProcedure"),
                                    QObject::tr("Restriction\n%alt%"),
                                    QObject::tr("Type"),
                                    QObject::tr("Freq.\nMHz/kHz/Cha."),
                                    QObject::tr("Range\n%dist%"),
                                    QObject::tr("Course\n°M"),
                                    QObject::tr("Direct\n°M"),
                                    QObject::tr("Distance\n%dist%"),
                                    QObject::tr("Remaining\n%dist%"),
                                    QObject::tr("Leg Time\nhh:mm"),
                                    QObject::tr("ETA\nhh:mm"),
                                    QObject::tr("Remarks")});

namespace rc {
// Route table column indexes
enum RouteColumns
{
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
  DIRECT,
  DIST,
  REMAINING_DISTANCE,
  LEG_TIME,
  ETA,
  REMARKS,
  LAST_COLUMN = REMARKS
};

}

using namespace atools::fs::pln;
using namespace atools::geo;
using Marble::GeoDataLatLonBox;
using Marble::GeoDataLineString;
using Marble::GeoDataCoordinates;

RouteController::RouteController(QMainWindow *parentWindow, QTableView *tableView)
  : QObject(parentWindow), mainWindow(parentWindow), view(tableView), query(NavApp::getMapQuery())
{
  // Set default table cell and font size to avoid Qt overly large cell sizes
  zoomHandler = new atools::gui::ItemViewZoomHandler(view);

  entryBuilder = new FlightplanEntryBuilder(query);

  symbolPainter = new SymbolPainter(Qt::transparent);

  // Use saved font size for table view
  zoomHandler->zoomPercent(OptionData::instance().getGuiRouteTableTextSize());
  updateIcons();

  view->setContextMenuPolicy(Qt::CustomContextMenu);

  // Create flight plan calculation caches
  routeNetworkRadio = new RouteNetworkRadio(NavApp::getDatabase());
  routeNetworkAirway = new RouteNetworkAirway(NavApp::getDatabase());

  // Set up undo/redo framework
  undoStack = new QUndoStack(mainWindow);
  undoStack->setUndoLimit(ROUTE_UNDO_LIMIT);

  QAction *undoAction = undoStack->createUndoAction(mainWindow, tr("&Undo Flight Plan"));
  undoAction->setIcon(QIcon(":/littlenavmap/resources/icons/undo.svg"));
  undoAction->setShortcut(QKeySequence("Ctrl+Z"));

  QAction *redoAction = undoStack->createRedoAction(mainWindow, tr("&Redo Flight Plan"));
  redoAction->setIcon(QIcon(":/littlenavmap/resources/icons/redo.svg"));
  redoAction->setShortcut(QKeySequence("Ctrl+Y"));

  connect(redoAction, &QAction::triggered, this, &RouteController::redoTriggered);
  connect(undoAction, &QAction::triggered, this, &RouteController::undoTriggered);

  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->toolBarRoute->insertAction(ui->actionRouteSelectParking, undoAction);
  ui->toolBarRoute->insertAction(ui->actionRouteSelectParking, redoAction);

  ui->menuRoute->insertActions(ui->actionRouteSelectParking, {undoAction, redoAction});
  ui->menuRoute->insertSeparator(ui->actionRouteSelectParking);

  connect(ui->spinBoxRouteSpeed, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &RouteController::routeSpeedChanged);

  connect(ui->spinBoxRouteAlt, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &RouteController::routeAltChanged);
  connect(ui->comboBoxRouteType, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
          this, &RouteController::routeTypeChanged);

  connect(view, &QTableView::doubleClicked, this, &RouteController::doubleClick);
  connect(view, &QTableView::customContextMenuRequested, this, &RouteController::tableContextMenu);

  connect(&routeAltDelayTimer, &QTimer::timeout, this, &RouteController::routeAltChangedDelayed);
  routeAltDelayTimer.setSingleShot(true);

  // set up table view
  view->horizontalHeader()->setSectionsMovable(true);
  view->verticalHeader()->setSectionsMovable(false);
  view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

  model = new QStandardItemModel();
  QItemSelectionModel *m = view->selectionModel();
  view->setModel(model);
  delete m;

  // Avoid stealing of keys from other default menus
  ui->actionRouteLegDown->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteLegUp->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteDeleteLeg->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteShowInformation->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteShowApproaches->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteShowOnMap->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  // Avoid stealing of Ctrl - C from other default menus
  ui->actionRouteTableCopy->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  // Need extra action connected to catch the default Ctrl-C in the table view
  connect(ui->actionRouteTableCopy, &QAction::triggered, this, &RouteController::tableCopyClipboard);

  // Add action/shortcuts to table view
  view->addActions({ui->actionRouteLegDown, ui->actionRouteLegUp, ui->actionRouteDeleteLeg,
                    ui->actionRouteTableCopy, ui->actionRouteShowInformation, ui->actionMapShowApproaches,
                    ui->actionRouteShowOnMap});

  void (RouteController::*selChangedPtr)(const QItemSelection &selected, const QItemSelection &deselected) =
    &RouteController::tableSelectionChanged;
  connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this, selChangedPtr);

  connect(ui->actionRouteLegDown, &QAction::triggered, this, &RouteController::moveSelectedLegsDown);
  connect(ui->actionRouteLegUp, &QAction::triggered, this, &RouteController::moveSelectedLegsUp);
  connect(ui->actionRouteDeleteLeg, &QAction::triggered, this, &RouteController::deleteSelectedLegs);

  connect(ui->actionRouteShowInformation, &QAction::triggered, this, &RouteController::showInformationMenu);
  connect(ui->actionRouteShowApproaches, &QAction::triggered, this, &RouteController::showApproachesMenu);
  connect(ui->actionRouteShowOnMap, &QAction::triggered, this, &RouteController::showOnMapMenu);

  connect(ui->dockWidgetRoute, &QDockWidget::visibilityChanged, this, &RouteController::dockVisibilityChanged);

  updateSpinboxSuffices();

}

RouteController::~RouteController()
{
  routeAltDelayTimer.stop();
  delete entryBuilder;
  delete model;
  delete undoStack;
  delete routeNetworkRadio;
  delete routeNetworkAirway;
  delete zoomHandler;
  delete symbolPainter;
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
void RouteController::tableCopyClipboard()
{
  qDebug() << "RouteController::tableCopyClipboard";

  QString csv;
  int exported = CsvExporter::selectionAsCsv(view, true, csv);

  if(!csv.isEmpty())
    QApplication::clipboard()->setText(csv);

  NavApp::setStatusMessage(QString(tr("Copied %1 entries to clipboard.")).arg(exported));
}

QString RouteController::tableAsHtml(int iconSizePixel) const
{
  using atools::util::HtmlBuilder;

  HtmlBuilder html(true);

  // Header lines
  html.h1(buildFlightplanLabel(false));
  html.h2(buildFlightplanLabel2());
  html.table();

  // Table header
  QHeaderView *header = view->horizontalHeader();
  html.tr(Qt::lightGray);
  html.th(QString()); // Icon
  for(int col = 0; col < model->columnCount(); col++)
    html.th(model->headerData(header->logicalIndex(col), Qt::Horizontal).
            toString().replace("-\n", "-").replace("\n", " "), atools::util::html::NOBR);
  html.trEnd();

  // Table content
  for(int row = 0; row < model->rowCount(); row++)
  {
    // First column is icon
    html.tr();
    const RouteLeg& routeLeg = route.at(row);
    QIcon icon;
    if(routeLeg.getMapObjectType() == map::AIRPORT)
      icon = SymbolPainter(Qt::white).createAirportIcon(routeLeg.getAirport(), iconSizePixel);
    else if(routeLeg.getMapObjectType() == map::WAYPOINT)
      icon = SymbolPainter(Qt::white).createWaypointIcon(iconSizePixel);
    else if(routeLeg.getMapObjectType() == map::VOR)
      icon = SymbolPainter(Qt::white).createVorIcon(routeLeg.getVor(), iconSizePixel);
    else if(routeLeg.getMapObjectType() == map::NDB)
      icon = SymbolPainter(Qt::white).createNdbIcon(iconSizePixel);
    else if(routeLeg.getMapObjectType() == map::USER)
      icon = SymbolPainter(Qt::white).createUserpointIcon(iconSizePixel);
    else if(routeLeg.getMapObjectType() == map::INVALID)
      icon = SymbolPainter(Qt::white).createWaypointIcon(iconSizePixel, mapcolors::routeInvalidPointColor);

    html.td();
    html.img(icon, QString(), QString(), QSize(iconSizePixel, iconSizePixel));
    html.tdEnd();

    // Rest of columns
    for(int col = 0; col < model->columnCount(); col++)
    {
      QStandardItem *item = model->item(row, header->logicalIndex(col));

      if(item != nullptr)
        html.td(item->text().toHtmlEscaped());
      else
        html.td(QString());
    }
    html.trEnd();
  }
  html.tableEnd();
  return html.getHtml();
}

void RouteController::routeStringToClipboard() const
{
  qDebug() << Q_FUNC_INFO;

  QString str = RouteString().createStringForRoute(route, getSpeedKts(), RouteStringDialog::getOptionsFromSettings());

  qDebug() << "route string" << str;
  if(!str.isEmpty())
    QApplication::clipboard()->setText(str);

  NavApp::setStatusMessage(QString(tr("Flight plan string to clipboard.")));
}

/* Spin box speed has changed value */
void RouteController::routeSpeedChanged()
{
  if(!route.isEmpty())
  {
    RouteCommand *undoCommand = preChange(tr("Change Speed"), rctype::SPEED);

    // Get type, speed and cruise altitude from widgets
    updateFlightplanFromWidgets();

    updateWindowLabel();
    updateModelRouteTime();
    postChange(undoCommand);

    NavApp::updateWindowTitle();
    emit routeChanged(false);
  }
}

/* Spin box altitude has changed value */
void RouteController::routeAltChanged()
{
  RouteCommand *undoCommand = nullptr;

  if(!route.isEmpty())
    undoCommand = preChange(tr("Change Altitude"), rctype::ALTITUDE);

  // Get type, speed and cruise altitude from widgets
  updateFlightplanFromWidgets();

  if(!route.isEmpty())
    postChange(undoCommand);

  NavApp::updateWindowTitle();

  routeAltDelayTimer.start(ROUTE_ALT_CHANGE_DELAY_MS);

  if(!route.isEmpty())
    emit routeChanged(false);
}

void RouteController::routeAltChangedDelayed()
{
  // Delay change to avoid hanging spin box when profile updates
  emit routeAltitudeChanged(route.getCruisingAltitudeFeet());
}

/* Combo box route type has value changed */
void RouteController::routeTypeChanged()
{
  RouteCommand *undoCommand = nullptr;

  if(!route.isEmpty())
    undoCommand = preChange(tr("Change Type"));

  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  if(!route.isEmpty())
    postChange(undoCommand);

  NavApp::updateWindowTitle();

  if(!route.isEmpty())
  {

    emit routeChanged(false);
    Ui::MainWindow *ui = NavApp::getMainUi();
    NavApp::setStatusMessage(tr("Flight plan type changed to %1.").arg(ui->comboBoxRouteType->currentText()));
  }
}

bool RouteController::selectDepartureParking()
{
  qDebug() << "selectDepartureParking";
  const map::MapAirport& airport = route.first().getAirport();
  ParkingDialog dialog(mainWindow, query, airport);

  int result = dialog.exec();
  dialog.hide();

  if(result == QDialog::Accepted)
  {
    // Set either start of parking
    map::MapParking parking;
    map::MapStart start;
    if(dialog.getSelectedParking(parking))
    {
      routeSetParking(parking);
      return true;
    }
    else if(dialog.getSelectedStartPosition(start))
    {
      routeSetStartPosition(start);
      return true;
    }
  }
  return false;
}

void RouteController::saveState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  atools::gui::WidgetState(lnm::ROUTE_VIEW).save({view, ui->spinBoxRouteSpeed, ui->comboBoxRouteType,
                                                  ui->spinBoxRouteAlt});

  atools::settings::Settings::instance().setValue(lnm::ROUTE_FILENAME, routeFilename);
}

void RouteController::updateTableHeaders()
{
  QList<QString> routeHeaders(ROUTE_COLUMNS);

  for(QString& str : routeHeaders)
    str = Unit::replacePlaceholders(str);

  model->setHorizontalHeaderLabels(routeHeaders);
}

void RouteController::restoreState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  updateTableHeaders();

  atools::gui::WidgetState state(lnm::ROUTE_VIEW, true, true);
  state.restore({view, ui->spinBoxRouteSpeed, ui->comboBoxRouteType, ui->spinBoxRouteAlt});

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_ROUTE)
  {
    QString newRouteFilename = atools::settings::Settings::instance().valueStr(lnm::ROUTE_FILENAME);

    if(!newRouteFilename.isEmpty())
    {
      if(QFile::exists(newRouteFilename))
      {
        if(!loadFlightplan(newRouteFilename))
        {
          // Cannot be loaded - clear current filename
          routeFilename.clear();
          fileDeparture.clear();
          fileDestination.clear();
          fileIfrVfr = VFR;
          route.clear();
        }
      }
      else
      {
        routeFilename.clear();
        fileDeparture.clear();
        fileDestination.clear();
        fileIfrVfr = VFR;
        route.clear();
      }
    }
  }
}

float RouteController::getSpeedKts() const
{
  return Unit::rev(static_cast<float>(NavApp::getMainUi()->spinBoxRouteSpeed->value()), Unit::speedKtsF);
}

void RouteController::getSelectedRouteLegs(QList<int>& selLegIndexes) const
{
  if(NavApp::getMainUi()->dockWidgetRoute->isVisible())
  {
    QItemSelection sm = view->selectionModel()->selection();
    for(const QItemSelectionRange& rng : sm)
    {
      for(int row = rng.top(); row <= rng.bottom(); ++row)
        selLegIndexes.append(row);
    }
  }
}

void RouteController::newFlightplan()
{
  qDebug() << "newFlightplan";
  clearRoute();

  // Copy current alt and type from widgets to flightplan
  updateFlightplanFromWidgets();

  createRouteLegsFromFlightplan();
  route.updateAll();

  updateTableModel();
  NavApp::updateWindowTitle();
  emit routeChanged(true);
}

void RouteController::loadFlightplan(const atools::fs::pln::Flightplan& flightplan, const QString& filename,
                                     bool quiet, bool changed, float speedKts)
{
  qDebug() << "loadFlightplan" << filename;

  clearRoute();

  if(changed)
    undoIndexClean = -1;

  routeFilename = filename;

  fileDeparture = flightplan.getDepartureIdent();
  fileDestination = flightplan.getDestinationIdent();
  fileIfrVfr = flightplan.getFlightplanType();

  route.setFlightplan(flightplan);

  route.getFlightplan().getProperties().insert("speed", QString::number(speedKts, 'f', 4));

  createRouteLegsFromFlightplan();

  loadProceduresFromFlightplan(false /* quiet */);
  route.updateAll();
  updateAirways();

  // Get number from user waypoint from user defined waypoint in fs flight plan
  entryBuilder->setCurUserpointNumber(route.getNextUserWaypointNumber());

  if(updateStartPositionBestRunway(false /* force */, !quiet /* undo */))
  {
    if(!quiet)
      atools::gui::Dialog(mainWindow).showInfoMsgBox(lnm::ACTIONS_SHOWROUTE_START_CHANGED,
                                                     tr("The flight plan had no valid start position.\n"
                                                        "The start position is now set to the longest "
                                                        "primary runway of the departure airport."),
                                                     tr("Do not &show this dialog again."));
  }

  if(speedKts > 0.f)
  {
    QSignalBlocker blocker(NavApp::getMainUi()->spinBoxRouteSpeed);
    Q_UNUSED(blocker);
    NavApp::getMainUi()->spinBoxRouteSpeed->setValue(atools::roundToInt(Unit::speedKtsF(speedKts)));
  }

  updateTableModel();
  NavApp::updateWindowTitle();

  qDebug() << route;

  emit routeChanged(true);
}

void RouteController::updateAirways()
{
  for(int i = 1; i < route.size(); i++)
  {
    RouteLeg& routeLeg = route[i];
    const RouteLeg& prevLeg = route.at(i - 1);

    if(!routeLeg.getAirwayName().isEmpty())
    {
      map::MapAirway airway;
      query->getAirwayByNameAndWaypoint(airway, routeLeg.getAirwayName(), prevLeg.getIdent(), routeLeg.getIdent());
      routeLeg.setAirway(airway);
    }
    else
      routeLeg.setAirway(map::MapAirway());
  }
}

void RouteController::loadProceduresFromFlightplan(bool quiet)
{
  if(route.isEmpty())
    return;

  proc::MapProcedureLegs arrival, departure, star;
  if(NavApp::getProcedureQuery()->getLegsForFlightplanProperties(route.getFlightplan().getProperties(),
                                                                 route.first().getAirport(),
                                                                 route.last().getAirport(),
                                                                 arrival, star, departure))
  {
    route.setDepartureProcedureLegs(departure);
    route.setStarProcedureLegs(star);
    route.setArrivalProcedureLegs(arrival);
    route.updateProcedureLegs(entryBuilder);
  }
  else
  {
    if(!quiet)
      atools::gui::Dialog(mainWindow).showInfoMsgBox(lnm::ACTIONS_SHOWROUTE_PROC_ERROR,
                                                     tr("Cannot load procedures into flight plan."),
                                                     tr("Do not &show this dialog again."));
    return;
  }

}

bool RouteController::loadFlightplan(const QString& filename)
{
  Flightplan newFlightplan;
  try
  {
    qDebug() << "loadFlightplan" << filename;
    // Will throw an exception if something goes wrong
    newFlightplan.load(filename);
    qDebug() << "Flight plan custom data" << newFlightplan.getProperties();

    // Convert altitude to local unit
    newFlightplan.setCruisingAltitude(
      atools::roundToInt(Unit::altFeetF(newFlightplan.getCruisingAltitude())));

    loadFlightplan(newFlightplan, filename, false /*quiet*/, false /*changed*/,
                   newFlightplan.getProperties().value("speed").toFloat());
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

bool RouteController::appendFlightplan(const QString& filename)
{
  Flightplan flightplan;
  try
  {
    qDebug() << "appendFlightplan" << filename;

    // Will throw an exception if something goes wrong
    flightplan.load(filename);
    // Convert altitude to local unit
    flightplan.setCruisingAltitude(
      atools::roundToInt(Unit::altFeetF(flightplan.getCruisingAltitude())));

    RouteCommand *undoCommand = preChange(tr("Append"));

    // Remove arrival from the save properites too
    route.removeProcedureLegs(proc::PROCEDURE_ARRIVAL_ALL);

    // Remove all procedure legs from route
    route.clearProcedureLegs(proc::PROCEDURE_ALL);

    for(const FlightplanEntry& entry : flightplan.getEntries())
      route.getFlightplan().getEntries().append(entry);

    route.getFlightplan().setDestinationAiportName(flightplan.getDestinationAiportName());
    route.getFlightplan().setDestinationIdent(flightplan.getDestinationIdent());
    route.getFlightplan().setDestinationPosition(flightplan.getDestinationPosition());

    createRouteLegsFromFlightplan();
    loadProceduresFromFlightplan(false /* quiet */);
    route.updateAll();
    updateAirways();

    updateTableModel();
    route.updateActiveLegAndPos(true /* force update */);

    postChange(undoCommand);
    NavApp::updateWindowTitle();
    emit routeChanged(true);
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

bool RouteController::saveFlighplanAs(const QString& filename, bool cleanExport)
{
  qDebug() << Q_FUNC_INFO << filename;
  QString savedFilename = routeFilename;
  routeFilename = filename;
  bool retval = saveFlightplan(cleanExport);

  if(cleanExport)
    // Revert back to original name
    routeFilename = savedFilename;
  return retval;
}

bool RouteController::saveFlighplanAsGfp(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;
  QString gfp = RouteString().createGfpStringForRoute(route);

  QFile file(filename);
  if(file.open(QFile::WriteOnly | QIODevice::Text))
  {
    QByteArray utf8 = gfp.toUtf8();
    file.write(utf8.data(), utf8.size());
    file.close();
    return true;
  }
  else
  {
    atools::gui::ErrorHandler(mainWindow).handleIOError(file, tr("While saving GFP file:"));
    return false;
  }
}

bool RouteController::saveFlighplanAsRte(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;

  try
  {
    route.getFlightplan().saveRte(filename);
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

bool RouteController::saveFlighplanAsFlp(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;

  try
  {
    route.getFlightplan().saveFlp(filename);
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

bool RouteController::saveFlighplanAsFms(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;

  try
  {
    route.getFlightplan().saveFms(filename);
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

bool RouteController::saveFlighplanAsGpx(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << filename;

  const AircraftTrack& aircraftTrack = NavApp::getAircraftTrack();
  atools::geo::LineString track;
  QVector<quint32> timestamps;

  for(const at::AircraftTrackPos& pos : aircraftTrack)
  {
    track.append(pos.pos);
    timestamps.append(pos.timestamp);
  }

  try
  {
    route.getFlightplan().saveGpx(filename, track, timestamps);
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

bool RouteController::saveFlightplan(bool cleanExport)
{
  qDebug() << Q_FUNC_INFO;

  try
  {
    if(!cleanExport)
    {
      fileDeparture = route.getFlightplan().getDepartureIdent();
      fileDestination = route.getFlightplan().getDestinationIdent();
      fileIfrVfr = route.getFlightplan().getFlightplanType();
    }

    // Will throw an exception if something goes wrong

    // Remember altitude in local units and set to feet before saving
    int oldCruise = route.getFlightplan().getCruisingAltitude();
    route.getFlightplan().setCruisingAltitude(
      atools::roundToInt(Unit::rev(static_cast<float>(route.getFlightplan().getCruisingAltitude()),
                                   Unit::altFeetF)));

    QHash<QString, QString>& properties = route.getFlightplan().getProperties();
    properties.insert("speed", QString::number(getSpeedKts(), 'f', 4));

    route.getFlightplan().save(routeFilename, cleanExport);
    route.getFlightplan().setCruisingAltitude(oldCruise);

    if(!cleanExport)
    {
      // Set clean undo state index since QUndoStack only returns weird values
      undoIndexClean = undoIndex;
      undoStack->setClean();
      NavApp::updateWindowTitle();
      qDebug() << "saveFlightplan undoIndex" << undoIndex << "undoIndexClean" << undoIndexClean;
    }
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
  qDebug() << "calculateDirect";

  // Stop any background tasks
  emit preRouteCalc();

  RouteCommand *undoCommand = preChange(tr("Direct Calculation"));

  route.getFlightplan().setRouteType(atools::fs::pln::DIRECT);
  route.removeRouteLegs();

  route.updateAll();
  updateAirways();

  updateTableModel();
  postChange(undoCommand);
  NavApp::updateWindowTitle();
  emit routeChanged(true);
  NavApp::setStatusMessage(tr("Calculated direct flight plan."));
}

void RouteController::calculateRadionav()
{
  qDebug() << "calculateRadionav";
  // Changing mode might need a clear
  routeNetworkRadio->setMode(nw::ROUTE_RADIONAV);

  RouteFinder routeFinder(routeNetworkRadio);

  if(calculateRouteInternal(&routeFinder, atools::fs::pln::VOR, tr("Radionnav Flight Plan Calculation"),
                            false /* fetch airways */, false /* Use altitude */))
    NavApp::setStatusMessage(tr("Calculated radio navaid flight plan."));
  else
    NavApp::setStatusMessage(tr("No route found."));
}

void RouteController::calculateHighAlt()
{
  qDebug() << "calculateHighAlt";
  routeNetworkAirway->setMode(nw::ROUTE_JET);

  RouteFinder routeFinder(routeNetworkAirway);

  if(calculateRouteInternal(&routeFinder, atools::fs::pln::HIGH_ALTITUDE,
                            tr("High altitude Flight Plan Calculation"),
                            true /* fetch airways */, false /* Use altitude */))
    NavApp::setStatusMessage(tr("Calculated high altitude (Jet airways) flight plan."));
  else
    NavApp::setStatusMessage(tr("No route found."));
}

void RouteController::calculateLowAlt()
{
  qDebug() << "calculateLowAlt";
  routeNetworkAirway->setMode(nw::ROUTE_VICTOR);

  RouteFinder routeFinder(routeNetworkAirway);

  if(calculateRouteInternal(&routeFinder, atools::fs::pln::LOW_ALTITUDE,
                            tr("Low altitude Flight Plan Calculation"),
                            /* fetch airways */ true, false /* Use altitude */))
    NavApp::setStatusMessage(tr("Calculated low altitude (Victor airways) flight plan."));
  else
    NavApp::setStatusMessage(tr("No route found."));
}

void RouteController::calculateSetAlt()
{
  qDebug() << "calculateSetAlt";
  routeNetworkAirway->setMode(nw::ROUTE_VICTOR | nw::ROUTE_JET);

  RouteFinder routeFinder(routeNetworkAirway);

  // Just decide by given altiude if this is a high or low plan
  atools::fs::pln::RouteType type;
  if(route.getFlightplan().getCruisingAltitude() > Unit::altFeetF(20000))
    type = atools::fs::pln::HIGH_ALTITUDE;
  else
    type = atools::fs::pln::LOW_ALTITUDE;

  if(calculateRouteInternal(&routeFinder, type, tr("Low altitude flight plan"),
                            true /* fetch airways */, true /* Use altitude */))
    NavApp::setStatusMessage(tr("Calculated high/low flight plan for given altitude."));
  else
    NavApp::setStatusMessage(tr("No route found."));
}

/* Calculate a flight plan to all types */
bool RouteController::calculateRouteInternal(RouteFinder *routeFinder, atools::fs::pln::RouteType type,
                                             const QString& commandName, bool fetchAirways,
                                             bool useSetAltitude)
{
  // Create wait cursor if calculation takes too long
  QGuiApplication::setOverrideCursor(Qt::WaitCursor);

  // Stop any background tasks
  emit preRouteCalc();

  Flightplan& flightplan = route.getFlightplan();

  int cruiseFt = atools::roundToInt(Unit::rev(flightplan.getCruisingAltitude(), Unit::altFeetF));
  int altitude = useSetAltitude ? cruiseFt : 0;

  routeFinder->setPreferVorToAirway(OptionData::instance().getFlags() & opts::ROUTE_PREFER_VOR);
  routeFinder->setPreferNdbToAirway(OptionData::instance().getFlags() & opts::ROUTE_PREFER_NDB);

  Pos departurePos = route.getStartAfterProcedure().getPosition();
  Pos destinationPos = route.getDestinationBeforeProcedure().getPosition();
  bool found = routeFinder->calculateRoute(departurePos, destinationPos, altitude);

  if(found)
  {
    // A route was found
    float distance = 0.f;
    QVector<rf::RouteEntry> calculatedRoute;

    // Fetch waypoints
    routeFinder->extractRoute(calculatedRoute, distance);

    // Compare to direct connection and check if route is too long
    float directDistance = departurePos.distanceMeterTo(destinationPos);
    float ratio = distance / directDistance;
    qDebug() << "route distance" << QString::number(distance, 'f', 0)
             << "direct distance" << QString::number(directDistance, 'f', 0) << "ratio" << ratio;

    if(ratio < MAX_DISTANCE_DIRECT_RATIO)
    {
      // Start undo
      RouteCommand *undoCommand = preChange(commandName);

      QList<FlightplanEntry>& entries = flightplan.getEntries();

      flightplan.setRouteType(type);
      // Erase all but start and destination
      entries.erase(flightplan.getEntries().begin() + 1, entries.end() - 1);

      // Create flight plan entries - will be copied later to the route map objects
      int minAltitude = 0;
      for(const rf::RouteEntry& routeEntry : calculatedRoute)
      {
        FlightplanEntry flightplanEntry;
        entryBuilder->buildFlightplanEntry(routeEntry.ref.id, atools::geo::EMPTY_POS, routeEntry.ref.type,
                                           flightplanEntry, fetchAirways);

        if(fetchAirways && routeEntry.airwayId != -1)
        {
          int alt = 0;
          updateFlightplanEntryAirway(routeEntry.airwayId, flightplanEntry, alt);
          minAltitude = std::max(minAltitude, alt);
        }

        entries.insert(entries.end() - 1, flightplanEntry);
      }

      minAltitude = atools::roundToInt(Unit::altFeetF(minAltitude));

      if(minAltitude != 0 && !useSetAltitude)
      {
        if(OptionData::instance().getFlags() & opts::ROUTE_EAST_WEST_RULE)
          // Apply simplified east/west rule
          minAltitude = adjustAltitude(departurePos, destinationPos, flightplan,
                                       minAltitude);

        flightplan.setCruisingAltitude(minAltitude);
      }

      createRouteLegsFromFlightplan();
      loadProceduresFromFlightplan(true /* quiet */);
      QGuiApplication::restoreOverrideCursor();

      route.removeDuplicateRouteLegs();
      route.updateAll();
      updateAirways();

      updateTableModel();
      route.updateActiveLegAndPos(true /* force update */);

      postChange(undoCommand);
      NavApp::updateWindowTitle();
      emit routeChanged(true);
    }
    else
      // Too long
      found = false;
  }

  QGuiApplication::restoreOverrideCursor();
  if(!found)
    atools::gui::Dialog(mainWindow).showInfoMsgBox(lnm::ACTIONS_SHOWROUTE_ERROR,
                                                   tr("Cannot find a route.\n"
                                                      "Try another routing type or create the flight plan manually."),
                                                   tr("Do not &show this dialog again."));

  return found;
}

void RouteController::adjustFlightplanAltitude()
{
  qDebug() << "Adjust altitude";

  if(route.isEmpty())
    return;

  Flightplan& fp = route.getFlightplan();
  int alt = adjustAltitude(fp.getDeparturePosition(), fp.getDestinationPosition(), fp, fp.getCruisingAltitude());

  if(alt != fp.getCruisingAltitude())
  {
    RouteCommand *undoCommand = preChange(tr("Adjust altitude"), rctype::ALTITUDE);
    fp.setCruisingAltitude(alt);

    updateTableModel();

    if(!route.isEmpty())
      postChange(undoCommand);

    NavApp::updateWindowTitle();

    if(!route.isEmpty())
      emit routeChanged(false);

    NavApp::setStatusMessage(tr("Adjusted flight plan altitude."));
  }
}

/* Apply simplified east/west rule */
int RouteController::adjustAltitude(const Pos& departurePos, const Pos& destinationPos,
                                    const Flightplan& flightplan, int minAltitude)
{
  float fpDir = departurePos.angleDegToRhumb(destinationPos);

  qDebug() << "minAltitude" << minAltitude << "fp dir" << fpDir;

  if(flightplan.getFlightplanType() == atools::fs::pln::IFR)
  {
    if(fpDir >= 0.f && fpDir <= 180.f)
      // General direction is east - round up to the next odd value
      minAltitude =
        static_cast<int>(std::ceil((minAltitude - 1000.f) / 2000.f) * 2000.f + 1000.f);
    else
      // General direction is west - round up to the next even value
      minAltitude = static_cast<int>(std::ceil((minAltitude) / 2000.f) * 2000.f);
  }
  else
  {
    if(fpDir >= 0.f && fpDir <= 180.f)
      // General direction is east - round up to the next odd value + 500
      minAltitude =
        static_cast<int>(std::ceil((minAltitude - 1500.f) / 2000.f) * 2000.f + 1500.f);
    else
      // General direction is west - round up to the next even value + 500
      minAltitude = static_cast<int>(std::ceil((minAltitude - 500.f) / 2000.f) * 2000.f + 500.f);
  }

  qDebug() << "corrected minAltitude" << minAltitude;
  return minAltitude;
}

void RouteController::reverseRoute()
{
  qDebug() << Q_FUNC_INFO;

  RouteCommand *undoCommand = preChange(tr("Reverse"), rctype::REVERSE);

  // Remove all procedures and properties
  route.removeProcedureLegs(proc::PROCEDURE_ALL);

  route.getFlightplan().reverse();

  QList<FlightplanEntry>& entries = route.getFlightplan().getEntries();
  if(entries.size() > 3)
  {
    // Move all airway names one entry down
    for(int i = entries.size() - 2; i >= 1; i--)
      entries[i].setAirway(entries.at(i - 1).getAirway());
  }

  createRouteLegsFromFlightplan();
  route.updateAll();
  updateAirways();
  updateStartPositionBestRunway(true /* force */, false /* undo */);

  updateTableModel();
  route.updateActiveLegAndPos(true /* force update */);

  postChange(undoCommand);
  NavApp::updateWindowTitle();
  emit routeChanged(true);
  NavApp::setStatusMessage(tr("Reversed flight plan."));
}

QString RouteController::buildDefaultFilename(const QString& extension, const QString& suffix) const
{
  QString filename;

  const Flightplan& flightplan = route.getFlightplan();

  if(flightplan.getFlightplanType() == atools::fs::pln::IFR)
    filename = "IFR ";
  else if(flightplan.getFlightplanType() == atools::fs::pln::VFR)
    filename = "VFR ";

  if(flightplan.getDepartureAiportName().isEmpty())
    filename += flightplan.getEntries().first().getIcaoIdent();
  else
    filename += flightplan.getDepartureAiportName() + " (" + flightplan.getDepartureIdent() + ")";

  filename += " to ";

  if(flightplan.getDestinationAiportName().isEmpty())
    filename += flightplan.getEntries().last().getIcaoIdent();
  else
    filename += flightplan.getDestinationAiportName() + " (" + flightplan.getDestinationIdent() + ")";

  filename += extension;
  filename += suffix;

  // Remove characters that are note allowed in most filesystems
  filename = atools::cleanFilename(filename);
  return filename;
}

QString RouteController::buildDefaultFilenameShort(const QString& sep, const QString& suffix) const
{
  QString filename;

  const Flightplan& flightplan = route.getFlightplan();

  filename += flightplan.getEntries().first().getIcaoIdent();
  filename += sep;

  filename += flightplan.getEntries().last().getIcaoIdent();
  filename += "." + suffix;

  // Remove characters that are note allowed in most filesystems
  filename = atools::cleanFilename(filename);
  return filename;
}

void RouteController::preDatabaseLoad()
{
  routeNetworkRadio->deInitQueries();
  routeNetworkAirway->deInitQueries();
  routeAltDelayTimer.stop();
}

void RouteController::postDatabaseLoad()
{
  routeNetworkRadio->initQueries();
  routeNetworkAirway->initQueries();

  // Remove the legs but keep the properties
  route.clearProcedureLegs(proc::PROCEDURE_ALL);

  createRouteLegsFromFlightplan();
  loadProceduresFromFlightplan(false /* quiet */);
  route.updateAll();
  updateAirways();

  // Update runway or parking if one of these has changed due to the database switch
  Flightplan& flightplan = route.getFlightplan();
  if(!flightplan.getEntries().isEmpty() &&
     flightplan.getEntries().first().getWaypointType() == atools::fs::pln::entry::AIRPORT &&
     flightplan.getDepartureParkingName().isEmpty())
    updateStartPositionBestRunway(false, true);

  updateTableModel();
  route.updateActiveLegAndPos(true /* force update */);

  NavApp::updateWindowTitle();
  routeAltDelayTimer.start(ROUTE_ALT_CHANGE_DELAY_MS);
}

/* Double click into table view */
void RouteController::doubleClick(const QModelIndex& index)
{
  qDebug() << Q_FUNC_INFO;
  if(index.isValid())
  {
    qDebug() << "mouseDoubleClickEvent";

    const RouteLeg& mo = route.at(index.row());

    if(mo.getMapObjectType() == map::AIRPORT)
      emit showRect(mo.getAirport().bounding, true);
    else
      emit showPos(mo.getPosition(), 0.f, true);

    map::MapSearchResult result;
    query->getMapObjectById(result, mo.getMapObjectType(), mo.getId());
    emit showInformation(result);
  }
}

void RouteController::updateMoveAndDeleteActions()
{
  QItemSelectionModel *sm = view->selectionModel();
  if(sm->hasSelection() && model->rowCount() > 0)
  {

    bool containsProc = false, moveDownTouchesProc = false, moveUpTouchesProc = false;
    QList<int> rows;
    selectedRows(rows, false);
    for(int row : rows)
    {
      if(route.at(row).isAnyProcedure())
      {
        containsProc = true;
        break;
      }
    }
    moveUpTouchesProc = rows.first() > 0 && route.at(rows.first() - 1).isAnyProcedure();
    moveDownTouchesProc = rows.first() < route.size() - 1 && route.at(rows.first() + 1).isAnyProcedure();

    Ui::MainWindow *ui = NavApp::getMainUi();
    ui->actionRouteLegUp->setEnabled(false);
    ui->actionRouteLegDown->setEnabled(false);
    ui->actionRouteDeleteLeg->setEnabled(false);

    if(model->rowCount() > 1)
    {
      ui->actionRouteDeleteLeg->setEnabled(true);
      ui->actionRouteLegUp->setEnabled(sm->hasSelection() && !sm->isRowSelected(0, QModelIndex()) &&
                                       !containsProc && !moveUpTouchesProc);

      ui->actionRouteLegDown->setEnabled(sm->hasSelection() &&
                                         !sm->isRowSelected(model->rowCount() - 1, QModelIndex()) &&
                                         !containsProc && !moveDownTouchesProc);
    }
    else if(model->rowCount() == 1)
      // Only one waypoint - nothing to move
      ui->actionRouteDeleteLeg->setEnabled(true);
  }
}

/* From context menu */
void RouteController::showInformationMenu()
{
  qDebug() << Q_FUNC_INFO;
  QModelIndex index = view->currentIndex();
  if(index.isValid())
  {
    const RouteLeg& routeLeg = route.at(index.row());
    map::MapSearchResult result;
    query->getMapObjectById(result, routeLeg.getMapObjectType(), routeLeg.getId());
    emit showInformation(result);
  }
}

/* From context menu */
void RouteController::showApproachesMenu()
{
  QModelIndex index = view->currentIndex();
  if(index.isValid())
  {
    const RouteLeg& routeLeg = route.at(index.row());
    emit showApproaches(routeLeg.getAirport());
  }
}

/* From context menu */
void RouteController::showOnMapMenu()
{
  QModelIndex index = view->currentIndex();
  if(index.isValid())
  {
    const RouteLeg& routeLeg = route.at(index.row());

    if(routeLeg.getMapObjectType() == map::AIRPORT)
      emit showRect(routeLeg.getAirport().bounding, false);
    else
      emit showPos(routeLeg.getPosition(), 0.f, false);

    if(routeLeg.getMapObjectType() == map::AIRPORT)
      NavApp::setStatusMessage(tr("Showing airport on map."));
    else
      NavApp::setStatusMessage(tr("Showing navaid on map."));
  }
}

void RouteController::tableContextMenu(const QPoint& pos)
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  QPoint menuPos = QCursor::pos();
  // Use widget center if position is not inside widget
  if(!ui->tableViewRoute->rect().contains(ui->tableViewRoute->mapFromGlobal(QCursor::pos())))
    menuPos = ui->tableViewRoute->mapToGlobal(ui->tableViewRoute->rect().center());

  qDebug() << "tableContextMenu";

  // Save text which will be changed below
  atools::gui::ActionTextSaver saver({ui->actionMapNavaidRange, ui->actionMapEditUserWaypoint, ui->actionRouteDeleteLeg});
  Q_UNUSED(saver);

  QModelIndex index = view->indexAt(pos);
  const RouteLeg *routeLeg = nullptr;
  if(index.isValid())
    routeLeg = &route.at(index.row());

  // Menu above a row

  updateMoveAndDeleteActions();

  ui->actionSearchTableCopy->setEnabled(index.isValid());

  ui->actionMapHideRangeRings->setEnabled(!NavApp::getMapWidget()->getDistanceMarkers().isEmpty() ||
                                          !NavApp::getMapWidget()->getRangeRings().isEmpty());

  if(routeLeg != nullptr)
  {
    ui->actionRouteShowInformation->setEnabled(routeLeg->isValid() &&
                                               routeLeg->getMapObjectType() != map::USER &&
                                               routeLeg->getMapObjectType() != map::INVALID);
    ui->actionRouteShowApproaches->setEnabled(routeLeg->isValid() &&
                                              routeLeg->getMapObjectType() == map::AIRPORT &&
                                              (routeLeg->getAirport().flags & map::AP_PROCEDURE));
    ui->actionRouteShowOnMap->setEnabled(true);
    ui->actionMapRangeRings->setEnabled(true);
    ui->actionSearchSetMark->setEnabled(true);

    // ui->actionRouteDeleteLeg->setText(route->isAnyProcedure() ?
    // tr("Delete Procedure") : tr("Delete selected Legs"));

#ifdef DEBUG_MOVING_AIRPLANE
    ui->actionRouteActivateLeg->setEnabled(routeLeg->isValid());
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
    ui->actionSearchSetMark->setEnabled(false);
  }

  ui->actionMapNavaidRange->setEnabled(false);

  ui->actionSearchTableSelectNothing->setEnabled(view->selectionModel()->hasSelection());

  ui->actionMapNavaidRange->setText(tr("Show Navaid Range"));

  ui->actionMapEditUserWaypoint->setEnabled(routeLeg != nullptr &&
                                            routeLeg->getMapObjectType() == map::USER);
  ui->actionMapEditUserWaypoint->setText(tr("Edit Name of User Waypoint"));

  QList<int> selectedRouteLegIndexes;
  getSelectedRouteLegs(selectedRouteLegIndexes);
  // If there are any radio navaids in the selected list enable range menu item
  for(int idx : selectedRouteLegIndexes)
  {
    const RouteLeg& leg = route.at(idx);
    if(leg.getVor().isValid() || leg.getNdb().isValid())
    {
      ui->actionMapNavaidRange->setEnabled(true);
      break;
    }
  }

  QMenu menu;
  menu.addAction(ui->actionRouteShowInformation);
  menu.addAction(ui->actionRouteShowApproaches);
  menu.addAction(ui->actionRouteShowOnMap);
  menu.addAction(ui->actionRouteActivateLeg);
  menu.addSeparator();

  menu.addAction(ui->actionRouteLegUp);
  menu.addAction(ui->actionRouteLegDown);
  menu.addAction(ui->actionRouteDeleteLeg);
  menu.addAction(ui->actionMapEditUserWaypoint);
  menu.addSeparator();

  menu.addAction(ui->actionMapRangeRings);
  menu.addAction(ui->actionMapNavaidRange);
  menu.addAction(ui->actionMapHideRangeRings);
  menu.addSeparator();

  menu.addAction(ui->actionSearchTableCopy);
  menu.addAction(ui->actionSearchTableSelectAll);
  menu.addAction(ui->actionSearchTableSelectNothing);
  menu.addSeparator();

  menu.addAction(ui->actionSearchResetView);
  menu.addSeparator();

  menu.addAction(ui->actionSearchSetMark);

  QAction *action = menu.exec(menuPos);
  if(action != nullptr)
    qDebug() << Q_FUNC_INFO << "selected" << action->text();
  else
    qDebug() << Q_FUNC_INFO << "no action selected";

  if(action != nullptr)
  {
    if(action == ui->actionSearchResetView)
    {
      // Reorder columns to match model order
      QHeaderView *header = view->horizontalHeader();
      for(int i = 0; i < header->count(); i++)
        header->moveSection(header->visualIndex(i), i);

      view->resizeColumnsToContents();
      NavApp::setStatusMessage(tr("Table view reset to defaults."));
    }
    else if(action == ui->actionSearchTableSelectAll)
      view->selectAll();
    else if(action == ui->actionSearchTableSelectNothing)
      view->clearSelection();
    else if(action == ui->actionSearchSetMark && routeLeg != nullptr)
      emit changeMark(routeLeg->getPosition());
    else if(action == ui->actionMapRangeRings && routeLeg != nullptr)
      NavApp::getMapWidget()->addRangeRing(routeLeg->getPosition());
    else if(action == ui->actionMapNavaidRange)
    {
      // Show range rings for all radio navaids
      for(int idx : selectedRouteLegIndexes)
      {
        const RouteLeg& routeLegSel = route.at(idx);
        if(routeLegSel.getMapObjectType() == map::VOR || routeLegSel.getMapObjectType() == map::NDB)
          NavApp::getMapWidget()->addNavRangeRing(routeLegSel.getPosition(), routeLegSel.getMapObjectType(),
                                                  routeLegSel.getIdent(), routeLegSel.getFrequencyOrChannel(),
                                                  routeLegSel.getRange());
      }
    }
    else if(action == ui->actionMapHideRangeRings)
      NavApp::getMapWidget()->clearRangeRingsAndDistanceMarkers();
    else if(action == ui->actionMapEditUserWaypoint)
      editUserWaypointName(index.row());
    else if(action == ui->actionRouteActivateLeg)
      activateLegManually(index.row());

    // Other actions emit signals directly
  }
}

/* Activate leg manually from menu */
void RouteController::activateLegManually(int index)
{
  route.setActiveLeg(index);
  highlightNextWaypoint(route.getActiveLegIndex());
  // Use geometry changed flag to force redraw
  emit routeChanged(true);
}

void RouteController::editUserWaypointName(int index)
{
  UserWaypointDialog dialog(mainWindow, route.at(index).getIdent());
  if(dialog.exec() == QDialog::Accepted && !dialog.getName().isEmpty())
  {
    RouteCommand *undoCommand = preChange(tr("Waypoint Name Change"));
    route[index].updateUserName(dialog.getName());
    model->item(index, rc::IDENT)->setText(dialog.getName());
    postChange(undoCommand);

    emit routeChanged(true);
  }
}

void RouteController::shownMapFeaturesChanged(map::MapObjectTypes types)
{
  qDebug() << Q_FUNC_INFO;
  route.setShownMapFeatures(types);
  route.setShownMapFeatures(types);
}

/* Hide or show map hightlights if dock visibility changes */
void RouteController::dockVisibilityChanged(bool visible)
{
  Q_UNUSED(visible);

  // Visible - send update to show map highlights
  // Not visible - send update to hide highlights
  tableSelectionChanged(QItemSelection(), QItemSelection());
}

void RouteController::tableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
  Q_UNUSED(selected);
  Q_UNUSED(deselected);

  updateMoveAndDeleteActions();
  QItemSelectionModel *sm = view->selectionModel();

  int selectedRows = 0;
  if(sm != nullptr && sm->hasSelection())
    selectedRows = sm->selectedRows().size();

  emit routeSelectionChanged(selectedRows, model->rowCount());
}

/* Called by undo command */
void RouteController::changeRouteUndo(const atools::fs::pln::Flightplan& newFlightplan)
{
  // Keep our own index as a workaround
  undoIndex--;

  qDebug() << "changeRouteUndo undoIndex" << undoIndex << "undoIndexClean" << undoIndexClean;
  changeRouteUndoRedo(newFlightplan);
}

/* Called by undo command */
void RouteController::changeRouteRedo(const atools::fs::pln::Flightplan& newFlightplan)
{
  // Keep our own index as a workaround
  undoIndex++;
  qDebug() << "changeRouteRedo undoIndex" << undoIndex << "undoIndexClean" << undoIndexClean;
  changeRouteUndoRedo(newFlightplan);
}

/* Called by undo command when commands are merged */
void RouteController::undoMerge()
{
  undoIndex--;
  qDebug() << "undoMerge undoIndex" << undoIndex << "undoIndexClean" << undoIndexClean;
}

/* Update window after undo or redo action */
void RouteController::changeRouteUndoRedo(const atools::fs::pln::Flightplan& newFlightplan)
{
  route.setFlightplan(newFlightplan);

  createRouteLegsFromFlightplan();
  loadProceduresFromFlightplan(true /* quiet */);
  route.updateAll();
  updateAirways();

  updateTableModel();
  NavApp::updateWindowTitle();
  updateMoveAndDeleteActions();
  emit routeChanged(true);
}

void RouteController::optionsChanged()
{
  zoomHandler->zoomPercent(OptionData::instance().getGuiRouteTableTextSize());
  updateIcons();
  updateTableHeaders();
  updateTableModel();

  updateSpinboxSuffices();
  view->update();
}

void RouteController::updateSpinboxSuffices()
{
  NavApp::getMainUi()->spinBoxRouteAlt->setSuffix(" " + Unit::getUnitAltStr());
  NavApp::getMainUi()->spinBoxRouteSpeed->setSuffix(" " + Unit::getUnitSpeedStr());
}

bool RouteController::hasChanged() const
{
  return undoIndexClean == -1 || undoIndexClean != undoIndex;
}

bool RouteController::doesFilenameMatchRoute()
{
  if(!routeFilename.isEmpty())
  {
    if(!(OptionData::instance().getFlags() & opts::GUI_AVOID_OVERWRITE_FLIGHTPLAN))
      return true;

    return fileIfrVfr == route.getFlightplan().getFlightplanType() &&
           fileDeparture == route.getFlightplan().getDepartureIdent() &&
           fileDestination == route.getFlightplan().getDestinationIdent();
  }
  return false;
}

/* Called by action */
void RouteController::moveSelectedLegsDown()
{
  qDebug() << "Leg down";
  moveSelectedLegsInternal(MOVE_DOWN);
}

/* Called by action */
void RouteController::moveSelectedLegsUp()
{
  qDebug() << "Leg up";
  moveSelectedLegsInternal(MOVE_UP);
}

void RouteController::moveSelectedLegsInternal(MoveDirection direction)
{
  // Get the selected rows. Depending on move direction order can be reversed to ease moving
  QList<int> rows;
  selectedRows(rows, direction == MOVE_DOWN /* reverse order */);

  if(!rows.isEmpty())
  {
    RouteCommand *undoCommand = preChange(tr("Move Waypoints"), rctype::MOVE);

    QModelIndex curIdx = view->currentIndex();
    // Remove selection
    view->selectionModel()->clear();
    for(int row : rows)
    {
      // Change flight plan
      route.getFlightplan().getEntries().move(row, row + direction);
      route.move(row, row + direction);

      // Move row
      model->insertRow(row + direction, model->takeRow(row));
    }

    int firstRow = rows.first();
    int lastRow = rows.last();

    bool forceDeparturePosition = false;
    if(direction == MOVE_DOWN)
    {
      qDebug() << "Move down" << firstRow << "to" << lastRow;
      // Departure moved down and was replaced by something else jumping up
      forceDeparturePosition = rows.contains(0);

      // Erase airway names at start of the moved block - last is smaller here
      eraseAirway(lastRow);
      eraseAirway(lastRow + 1);

      // Erase airway name at end of the moved block
      eraseAirway(firstRow + 2);
    }
    else if(direction == MOVE_UP)
    {
      qDebug() << "Move up" << firstRow << "to" << lastRow;
      // Something moved up and departure jumped up
      forceDeparturePosition = rows.contains(1);

      // Erase airway name at start of the moved block - last is larger here
      eraseAirway(firstRow - 1);

      // Erase airway names at end of the moved block
      eraseAirway(lastRow);
      eraseAirway(lastRow + 1);
    }

    route.updateAll();
    updateAirways();

    // Force update of start if departure airport was moved
    updateStartPositionBestRunway(forceDeparturePosition, false /* undo */);

    routeToFlightPlan();
    // Get type and cruise altitude from widgets
    updateFlightplanFromWidgets();

    updateTableModel();

    // Restore current position at new moved position
    view->setCurrentIndex(model->index(curIdx.row() + direction, curIdx.column()));
    // Restore previous selection at new moved position
    select(rows, direction);

    updateMoveAndDeleteActions();
    route.updateActiveLegAndPos(true /* force update */);

    postChange(undoCommand);
    NavApp::updateWindowTitle();
    emit routeChanged(true);
    NavApp::setStatusMessage(tr("Moved flight plan legs."));
  }
}

void RouteController::eraseAirway(int row)
{
  if(0 <= row && row < route.getFlightplan().getEntries().size())
    route.getFlightplan()[row].setAirway(QString());
}

/* Called by action */
void RouteController::deleteSelectedLegs()
{
  qDebug() << Q_FUNC_INFO << "Leg delete";

  // Get selected rows
  QList<int> rows;
  selectedRows(rows, true /* reverse */);

  if(!rows.isEmpty())
  {
    proc::MapProcedureTypes procs = affectedProcedures(rows);

    // Do not merge for procedure deletes
    RouteCommand *undoCommand = preChange(
      procs & proc::PROCEDURE_ALL ? tr("Delete Procedure") : tr("Delete Waypoints"),
      procs & proc::PROCEDURE_ALL ? rctype::EDIT : rctype::DELETE);

    int firstRow = rows.last();
    view->selectionModel()->clear();
    for(int row : rows)
    {
      route.getFlightplan().getEntries().removeAt(row);

      eraseAirway(row);

      route.removeAt(row);
      model->removeRow(row);
    }

    route.removeProcedureLegs(procs);

    route.updateAll();
    updateAirways();

    // Force update of start if departure airport was removed
    updateStartPositionBestRunway(rows.contains(0) /* force */, false /* undo */);

    routeToFlightPlan();
    // Get type and cruise altitude from widgets
    updateFlightplanFromWidgets();

    updateTableModel();

    // Update current position at the beginning of the former selection
    view->setCurrentIndex(model->index(firstRow, 0));
    updateMoveAndDeleteActions();
    route.updateActiveLegAndPos(true /* force update */);

    postChange(undoCommand);
    NavApp::updateWindowTitle();
    emit routeChanged(true);
    NavApp::setStatusMessage(tr("Removed flight plan legs."));
  }
}

/* Get selected row numbers from the table model */
void RouteController::selectedRows(QList<int>& rows, bool reverse)
{
  QItemSelection sm = view->selectionModel()->selection();
  for(const QItemSelectionRange& rng : sm)
  {
    for(int row = rng.top(); row <= rng.bottom(); row++)
      rows.append(row);
  }

  if(!rows.isEmpty())
  {
    // Remove from bottom to top - otherwise model creates a mess
    std::sort(rows.begin(), rows.end());
    if(reverse)
      std::reverse(rows.begin(), rows.end());
  }
}

/* Select all columns of the given rows adding offset to each row index */
void RouteController::select(QList<int>& rows, int offset)
{
  QItemSelection newSel;

  for(int row : rows)
    // Need to select all columns
    newSel.append(QItemSelectionRange(model->index(row + offset, rc::FIRST_COLUMN),
                                      model->index(row + offset, rc::LAST_COLUMN)));

  view->selectionModel()->select(newSel, QItemSelectionModel::ClearAndSelect);
}

void RouteController::routeSetParking(map::MapParking parking)
{
  qDebug() << "route set parking id" << parking.id;

  RouteCommand *undoCommand = preChange(tr("Set Parking"));

  if(route.isEmpty() || route.first().getMapObjectType() != map::AIRPORT ||
     route.first().getId() != parking.airportId)
  {
    // No route, no start airport or different airport
    map::MapAirport ap;
    query->getAirportById(ap, parking.airportId);
    routeSetDepartureInternal(ap);
    route.removeProcedureLegs(proc::PROCEDURE_DEPARTURE);
  }

  // Update the current airport which is new or the same as the one used by the parking spot
  route.getFlightplan().setDepartureParkingName(map::parkingNameForFlightplan(parking));
  route.getFlightplan().setDeparturePosition(parking.position);
  route.first().setDepartureParking(parking);

  route.updateAll();
  updateAirways();
  routeToFlightPlan();
  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  updateTableModel();

  postChange(undoCommand);
  NavApp::updateWindowTitle();

  emit routeChanged(true);

  NavApp::setStatusMessage(tr("Departure set to %1 parking %2.").arg(route.first().getIdent()).
                           arg(map::parkingNameNumberType(parking)));
}

/* Set start position (runway, helipad) for departure */
void RouteController::routeSetStartPosition(map::MapStart start)
{
  qDebug() << "route set start id" << start.id;

  RouteCommand *undoCommand = preChange(tr("Set Start Position"));

  // No need to update airport since this is called from dialog only

  // Update the current airport which is new or the same as the one used by the parking spot
  if(start.helipadNumber > 0)
    // Use helipad number
    route.getFlightplan().setDepartureParkingName(QString::number(start.helipadNumber));
  else
    // Use runway name
    route.getFlightplan().setDepartureParkingName(start.runwayName);

  route.getFlightplan().setDeparturePosition(start.position);
  route.first().setDepartureStart(start);

  route.updateAll();
  updateAirways();
  routeToFlightPlan();
  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  updateTableModel();

  postChange(undoCommand);
  NavApp::updateWindowTitle();
  emit routeChanged(true);

  NavApp::setStatusMessage(tr("Departure set to %1 start position %2.").arg(route.first().getIdent()).
                           arg(start.runwayName));
}

void RouteController::routeSetDeparture(map::MapAirport airport)
{
  qDebug() << "route set start id" << airport.id;

  RouteCommand *undoCommand = preChange(tr("Set Departure"));

  routeSetDepartureInternal(airport);

  route.removeProcedureLegs(proc::PROCEDURE_DEPARTURE);

  route.updateAll();
  updateAirways();
  routeToFlightPlan();
  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  updateTableModel();
  route.updateActiveLegAndPos(true /* force update */);

  postChange(undoCommand);
  NavApp::updateWindowTitle();
  emit routeChanged(true);
  NavApp::setStatusMessage(tr("Departure set to %1.").arg(route.first().getIdent()));
}

/* Add departure and add best runway start position */
void RouteController::routeSetDepartureInternal(const map::MapAirport& airport)
{
  FlightplanEntry entry;
  entryBuilder->buildFlightplanEntry(airport, entry);

  Flightplan& flightplan = route.getFlightplan();
  if(!flightplan.isEmpty())
  {
    const FlightplanEntry& first = flightplan.getEntries().first();
    if(first.getWaypointType() == entry::AIRPORT &&
       flightplan.getDepartureIdent() == first.getIcaoIdent() && flightplan.getEntries().size() > 1)
    {
      flightplan.getEntries().removeAt(0);
      route.removeAt(0);
    }
  }

  flightplan.getEntries().prepend(entry);

  RouteLeg routeLeg(&flightplan);
  routeLeg.createFromAirport(0, airport, nullptr);
  route.prepend(routeLeg);

  updateStartPositionBestRunway(true /* force */, false /* undo */);
}

void RouteController::routeSetDestination(map::MapAirport airport)
{
  qDebug() << "route set dest id" << airport.id;

  RouteCommand *undoCommand = preChange(tr("Set Destination"));

  routeSetDestinationInternal(airport);

  route.removeProcedureLegs(proc::PROCEDURE_ARRIVAL_ALL);

  route.updateAll();
  updateAirways();
  routeToFlightPlan();
  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  updateTableModel();
  route.updateActiveLegAndPos(true /* force update */);

  postChange(undoCommand);
  NavApp::updateWindowTitle();
  emit routeChanged(true);
  NavApp::setStatusMessage(tr("Destination set to %1.").arg(airport.ident));
}

void RouteController::routeSetDestinationInternal(const map::MapAirport& airport)
{
  FlightplanEntry entry;
  entryBuilder->buildFlightplanEntry(airport, entry);

  Flightplan& flightplan = route.getFlightplan();
  if(!flightplan.isEmpty())
  {
    // Remove current destination
    const FlightplanEntry& last = flightplan.getEntries().last();
    if(last.getWaypointType() == entry::AIRPORT &&
       flightplan.getDestinationIdent() == last.getIcaoIdent() && flightplan.getEntries().size() > 1)
    {
      // Remove the last airport if it is set as destination
      flightplan.getEntries().removeLast();
      route.removeLast();
    }
  }

  flightplan.getEntries().append(entry);

  const RouteLeg *lastLeg = nullptr;
  if(flightplan.getEntries().size() > 1)
    // Set predecessor if route has entries
    lastLeg = &route.at(route.size() - 1);

  RouteLeg routeLeg(&flightplan);
  routeLeg.createFromAirport(flightplan.getEntries().size() - 1, airport, lastLeg);
  route.append(routeLeg);

  updateStartPositionBestRunway(false /* force */, false /* undo */);
}

void RouteController::routeAttachProcedure(const proc::MapProcedureLegs& legs)
{
  qDebug() << Q_FUNC_INFO << legs;

  RouteCommand *undoCommand = preChange(tr("Add Procedure"));

  map::MapAirport airport;
  query->getAirportById(airport, legs.ref.airportId);
  if(legs.mapType & proc::PROCEDURE_STAR || legs.mapType & proc::PROCEDURE_ARRIVAL)
  {
    if(route.isEmpty() || route.last().getMapObjectType() != map::AIRPORT ||
       route.last().getId() != legs.ref.airportId)
    {
      // No route, no destination airport or different airport
      route.removeProcedureLegs(proc::PROCEDURE_ARRIVAL_ALL);
      routeSetDestinationInternal(airport);
    }
    // Will take care of the flight plan entries too
    if(legs.mapType & proc::PROCEDURE_STAR)
      route.setStarProcedureLegs(legs);
    if(legs.mapType & proc::PROCEDURE_ARRIVAL)
      route.setArrivalProcedureLegs(legs);

    route.updateProcedureLegs(entryBuilder);
  }
  else if(legs.mapType & proc::PROCEDURE_DEPARTURE)
  {
    if(route.isEmpty() || route.first().getMapObjectType() != map::AIRPORT ||
       route.first().getId() != legs.ref.airportId)
    {
      // No route, no departure airport or different airport
      route.removeProcedureLegs(proc::PROCEDURE_DEPARTURE);
      routeSetDepartureInternal(airport);
    }
    // Will take care of the flight plan entries too
    route.setDepartureProcedureLegs(legs);
    route.updateProcedureLegs(entryBuilder);
  }

  route.updateAll();
  updateAirways();
  routeToFlightPlan();

  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  updateTableModel();
  route.updateActiveLegAndPos(true /* force update */);

  postChange(undoCommand);
  NavApp::updateWindowTitle();

  emit routeChanged(true);

  NavApp::setStatusMessage(tr("Added procedure to flight plan."));
}

void RouteController::routeAdd(int id, atools::geo::Pos userPos, map::MapObjectTypes type, int legIndex)
{
  qDebug() << Q_FUNC_INFO << "route add" << "user pos" << userPos << "id" << id
           << "type" << type << "leg index" << legIndex;

  FlightplanEntry entry;
  entryBuilder->buildFlightplanEntry(id, userPos, type, entry, -1);

  int insertIndex = calculateInsertIndex(entry.getPosition(), legIndex);

  routeAddInternal(entry, insertIndex);
}

void RouteController::routeAddInternal(const FlightplanEntry& entry, int insertIndex)
{
  qDebug() << Q_FUNC_INFO << "insertIndex" << insertIndex;

  RouteCommand *undoCommand = preChange(tr("Add Waypoint"));

  Flightplan& flightplan = route.getFlightplan();
  flightplan.getEntries().insert(insertIndex, entry);
  eraseAirway(insertIndex);
  eraseAirway(insertIndex + 1);

  const RouteLeg *lastLeg = nullptr;

  if(flightplan.isEmpty() && insertIndex > 0)
    lastLeg = &route.at(insertIndex - 1);
  RouteLeg routeLeg(&flightplan);
  routeLeg.createFromDatabaseByEntry(insertIndex, query, lastLeg);

  route.insert(insertIndex, routeLeg);

  proc::MapProcedureTypes procs = affectedProcedures({insertIndex});
  route.removeProcedureLegs(procs);

  route.updateAll();
  updateAirways();
  // Force update of start if departure airport was added
  updateStartPositionBestRunway(false /* force */, false /* undo */);
  routeToFlightPlan();
  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  updateTableModel();
  route.updateActiveLegAndPos(true /* force update */);

  postChange(undoCommand);
  NavApp::updateWindowTitle();

  emit routeChanged(true);

  NavApp::setStatusMessage(tr("Added waypoint to flight plan."));
}

int RouteController::calculateInsertIndex(const atools::geo::Pos& pos, int legIndex)
{
  Flightplan& flightplan = route.getFlightplan();

  int insertIndex = -1;
  if(legIndex == map::INVALID_INDEX_VALUE)
    // Append
    insertIndex = route.size();
  else if(legIndex == -1)
  {
    if(flightplan.isEmpty())
      // First is  departure
      insertIndex = 0;
    else if(flightplan.getEntries().size() == 1)
      // Keep first as departure
      insertIndex = 1;
    else
    {
      // No leg index given - search for nearest available route leg
      atools::geo::LineDistance result;
      int nearestlegIndex = route.getNearestRouteLegResult(pos, result, true /* ignoreNotEditable */);

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
          if(nearestlegIndex == route.size() - 1)
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

void RouteController::routeReplace(int id, atools::geo::Pos userPos, map::MapObjectTypes type,
                                   int legIndex)
{
  qDebug() << "route replace" << "user pos" << userPos << "id" << id
           << "type" << type << "leg index" << legIndex;

  RouteCommand *undoCommand = preChange(tr("Change Waypoint"));

  const FlightplanEntry oldEntry = route.getFlightplan().getEntries().at(legIndex);

  FlightplanEntry entry;
  entryBuilder->buildFlightplanEntry(id, userPos, type, entry, -1);

  if(oldEntry.getWaypointType() == atools::fs::pln::entry::USER &&
     entry.getWaypointType() == atools::fs::pln::entry::USER)
    entry.setWaypointId(oldEntry.getWaypointId());

  Flightplan& flightplan = route.getFlightplan();

  flightplan.getEntries().replace(legIndex, entry);

  const RouteLeg *lastLeg = nullptr;
  if(legIndex > 0 && !route.isFlightplanEmpty())
    // Get predecessor of replaced entry
    lastLeg = &route.at(legIndex - 1);

  RouteLeg routeLeg(&flightplan);
  routeLeg.createFromDatabaseByEntry(legIndex, query, lastLeg);

  route.replace(legIndex, routeLeg);
  eraseAirway(legIndex);
  eraseAirway(legIndex + 1);

  if(legIndex == route.size() - 1)
    route.removeProcedureLegs(proc::PROCEDURE_ARRIVAL_ALL);

  if(legIndex == 0)
    route.removeProcedureLegs(proc::PROCEDURE_DEPARTURE);

  route.updateAll();
  updateAirways();

  // Force update of start if departure airport was changed
  updateStartPositionBestRunway(legIndex == 0 /* force */, false /* undo */);

  routeToFlightPlan();
  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  updateTableModel();
  route.updateActiveLegAndPos(true /* force update */);

  postChange(undoCommand);
  NavApp::updateWindowTitle();
  emit routeChanged(true);
  NavApp::setStatusMessage(tr("Replaced waypoint in flight plan."));
}

void RouteController::routeDelete(int index)
{
  qDebug() << Q_FUNC_INFO << index;

  RouteCommand *undoCommand = preChange(tr("Delete"));

  route.getFlightplan().getEntries().removeAt(index);

  route.removeAt(index);
  eraseAirway(index);

  if(index == route.size())
    route.removeProcedureLegs(proc::PROCEDURE_ARRIVAL_ALL);

  if(index == 0)
    route.removeProcedureLegs(proc::PROCEDURE_DEPARTURE);

  route.updateAll();
  updateAirways();

  // Force update of start if departure airport was removed
  updateStartPositionBestRunway(index == 0 /* force */, false /* undo */);
  routeToFlightPlan();
  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();

  updateTableModel();

  postChange(undoCommand);
  NavApp::updateWindowTitle();
  emit routeChanged(true);

  NavApp::setStatusMessage(tr("Removed waypoint from flight plan."));
}

/* Update airway attribute in flight plan entry and return minimum altitude for this airway segment */
void RouteController::updateFlightplanEntryAirway(int airwayId, FlightplanEntry& entry, int& minAltitude)
{
  map::MapAirway airway;
  query->getAirwayById(airway, airwayId);
  entry.setAirway(airway.name);
  minAltitude = airway.minAltitude;
}

/* Copy all data from route map objects and widgets to the flight plan */
void RouteController::routeToFlightPlan()
{
  Flightplan& flightplan = route.getFlightplan();

  if(route.isEmpty())
    flightplan.clear();
  else
  {
    QString departureIcao, destinationIcao;

    const RouteLeg& firstLeg = route.first();
    if(firstLeg.getMapObjectType() == map::AIRPORT)
    {
      departureIcao = firstLeg.getAirport().ident;
      flightplan.setDepartureAiportName(firstLeg.getAirport().name);
      flightplan.setDepartureIdent(departureIcao);

      if(route.hasDepartureParking())
      {
        flightplan.setDepartureParkingName(map::parkingNameForFlightplan(firstLeg.getDepartureParking()));
        flightplan.setDeparturePosition(firstLeg.getDepartureParking().position);
      }
      else if(route.hasDepartureStart())
      {
        if(route.hasDepartureHelipad())
          // Use helipad number
          flightplan.setDepartureParkingName(QString::number(firstLeg.getDepartureStart().helipadNumber));
        else
          // Use runway name
          flightplan.setDepartureParkingName(firstLeg.getDepartureStart().runwayName);
        flightplan.setDeparturePosition(firstLeg.getDepartureStart().position);
      }
      else
        // No start position and no parking - use airport/navaid position
        flightplan.setDeparturePosition(firstLeg.getPosition());
    }
    else
    {
      // Invalid departure
      flightplan.setDepartureAiportName(QString());
      flightplan.setDepartureIdent(QString());
      flightplan.setDepartureParkingName(QString());
      flightplan.setDeparturePosition(Pos());
    }

    const RouteLeg& lastLeg = route.last();
    if(lastLeg.getMapObjectType() == map::AIRPORT)
    {
      destinationIcao = lastLeg.getAirport().ident;
      flightplan.setDestinationAiportName(lastLeg.getAirport().name);
      flightplan.setDestinationIdent(destinationIcao);
      flightplan.setDestinationPosition(lastLeg.getPosition());
    }
    else
    {
      // Invalid destination
      flightplan.setDestinationAiportName(QString());
      flightplan.setDestinationIdent(QString());
      flightplan.setDestinationPosition(Pos());
    }

    // <Descr>LFHO, EDRJ</Descr>
    flightplan.setDescription(departureIcao + ", " + destinationIcao);

    // <Title>LFHO to EDRJ</Title>
    flightplan.setTitle(departureIcao + " to " + destinationIcao);
  }
}

/* Copy type and cruise altitude from widgets to flight plan */
void RouteController::updateFlightplanFromWidgets()
{
  Flightplan& flightplan = route.getFlightplan();
  Ui::MainWindow *ui = NavApp::getMainUi();
  flightplan.setFlightplanType(
    ui->comboBoxRouteType->currentIndex() == 0 ? atools::fs::pln::IFR : atools::fs::pln::VFR);
  flightplan.setCruisingAltitude(ui->spinBoxRouteAlt->value());

  route.getFlightplan().getProperties().insert("speed", QString::number(getSpeedKts(), 'f', 4));
}

/* Loads navaids from database and create all route map objects from flight plan.  */
void RouteController::createRouteLegsFromFlightplan()
{
  route.clear();

  Flightplan& flightplan = route.getFlightplan();

  const RouteLeg *last = nullptr;

  // Create map objects first and calculate total distance
  for(int i = 0; i < flightplan.getEntries().size(); i++)
  {
    RouteLeg mapobj(&flightplan);
    mapobj.createFromDatabaseByEntry(i, query, last);

    if(mapobj.getMapObjectType() == map::INVALID)
      // Not found in database
      qWarning() << "Entry for ident" << flightplan.at(i).getIcaoIdent()
                 << "region" << flightplan.at(i).getIcaoRegion() << "is not valid";

    route.append(mapobj);
    last = &route.last();
  }
}

/* Update table view model completely */
void RouteController::updateTableModel()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  model->removeRows(0, model->rowCount());
  float totalDistance = route.getTotalDistance();

  int row = 0;
  float cumulatedDistance = 0.f;

  QList<QStandardItem *> itemRow;
  for(int i = rc::FIRST_COLUMN; i <= rc::LAST_COLUMN; i++)
    itemRow.append(nullptr);

  for(int i = 0; i < route.size(); i++)
  {
    const RouteLeg& leg = route.at(i);
    bool afterArrivalAirport = route.isAirportAfterArrival(i);

    QIcon icon;
    if(leg.getMapObjectType() == map::AIRPORT)
      icon = symbolPainter->createAirportIcon(leg.getAirport(), iconSize);
    else if(leg.getVor().isValid())
      icon = symbolPainter->createVorIcon(leg.getVor(), iconSize);
    else if(leg.getNdb().isValid())
      icon = ndbIcon;
    else if(leg.getWaypoint().isValid())
      icon = waypointIcon;
    else if(leg.getMapObjectType() == map::USER)
      icon = userpointIcon;
    else if(leg.getMapObjectType() == map::INVALID)
      icon = invalidIcon;
    else if(leg.isAnyProcedure())
      icon = procedureIcon;

    QStandardItem *ident = new QStandardItem(icon, leg.getIdent());
    QFont f = ident->font();
    f.setBold(true);
    ident->setFont(f);
    ident->setTextAlignment(Qt::AlignRight);

    if(leg.getMapObjectType() == map::INVALID)
      ident->setForeground(Qt::red);

    itemRow[rc::IDENT] = ident;
    itemRow[rc::REGION] = new QStandardItem(leg.getRegion());
    itemRow[rc::NAME] = new QStandardItem(leg.getName());
    itemRow[rc::PROCEDURE] = new QStandardItem(proc::procedureTypeText(leg.getProcedureLeg()));

    if(leg.isRoute())
    {
      itemRow[rc::AIRWAY_OR_LEGTYPE] = new QStandardItem(leg.getAirwayName());
      if(leg.getAirway().isValid() && leg.getAirway().minAltitude > 0)
        itemRow[rc::RESTRICTION] = new QStandardItem(Unit::altFeet(leg.getAirway().minAltitude, false));
    }
    else
    {
      itemRow[rc::AIRWAY_OR_LEGTYPE] = new QStandardItem(proc::procedureLegTypeStr(leg.getProcedureLegType()));
      itemRow[rc::RESTRICTION] =
        new QStandardItem(proc::altRestrictionTextShort(leg.getProcedureLeg().altRestriction));
    }

    // VOR/NDB type ===========================
    if(leg.getVor().isValid())
      itemRow[rc::TYPE] = new QStandardItem(map::vorFullShortText(leg.getVor()));
    else if(leg.getNdb().isValid())
      itemRow[rc::TYPE] = new QStandardItem(map::ndbFullShortText(leg.getNdb()));

    // VOR/NDB frequency =====================
    if(leg.getVor().isValid())
    {
      if(leg.getVor().tacan)
        itemRow[rc::FREQ] = new QStandardItem(leg.getVor().channel);
      else
        itemRow[rc::FREQ] = new QStandardItem(QLocale().toString(leg.getFrequency() / 1000.f, 'f', 2));
    }
    else if(leg.getNdb().isValid())
      itemRow[rc::FREQ] = new QStandardItem(QLocale().toString(leg.getFrequency() / 100.f, 'f', 1));

    // VOR/NDB range =====================
    if(leg.getRange() > 0 && (leg.getVor().isValid() || leg.getNdb().isValid()))
      itemRow[rc::RANGE] = new QStandardItem(Unit::distNm(leg.getRange(), false));

    // Course =====================
    if(row > 0 && !afterArrivalAirport)
    {
      QString trueCourse = route.isTrueCourse() ? tr("(°T)") : QString();

      if(leg.getCourseToMag() < map::INVALID_COURSE_VALUE)
        itemRow[rc::COURSE] = new QStandardItem(QLocale().toString(leg.getCourseToMag(), 'f', 0) + trueCourse);
      if(leg.getCourseToRhumbMag() < map::INVALID_COURSE_VALUE)
        itemRow[rc::DIRECT] = new QStandardItem(QLocale().toString(leg.getCourseToRhumbMag(), 'f', 0) + trueCourse);
    }

    if(!afterArrivalAirport)
    {
      if(leg.getDistanceTo() < map::INVALID_DISTANCE_VALUE) // Distance =====================
      {
        cumulatedDistance += leg.getDistanceTo();
        itemRow[rc::DIST] = new QStandardItem(Unit::distNm(leg.getDistanceTo(), false));

        if(!leg.getProcedureLeg().isMissed())
        {
          float remaining = totalDistance - cumulatedDistance;
          if(remaining < 0.f)
            remaining = 0.f;  // Catch the -0 case due to rounding errors
          itemRow[rc::REMAINING_DISTANCE] = new QStandardItem(Unit::distNm(remaining, false));
        }
      }
    }

    if(leg.isAnyProcedure())
      itemRow[rc::REMARKS] = new QStandardItem(proc::procedureLegRemark(leg.getProcedureLeg()));

    // Travel time and ETA are updated in updateModelRouteTime

    // Create empty items for missing fields
    for(int col = rc::FIRST_COLUMN; col <= rc::LAST_COLUMN; col++)
    {
      if(itemRow[col] == nullptr)
        itemRow[col] = new QStandardItem();
      itemRow[col]->setFlags(itemRow[col]->flags() &
                             ~(Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled));
    }

    itemRow[rc::REMAINING_DISTANCE]->setTextAlignment(Qt::AlignRight);
    itemRow[rc::DIST]->setTextAlignment(Qt::AlignRight);
    itemRow[rc::COURSE]->setTextAlignment(Qt::AlignRight);
    itemRow[rc::DIRECT]->setTextAlignment(Qt::AlignRight);
    itemRow[rc::RANGE]->setTextAlignment(Qt::AlignRight);
    itemRow[rc::FREQ]->setTextAlignment(Qt::AlignRight);
    itemRow[rc::RESTRICTION]->setTextAlignment(Qt::AlignRight);

    model->appendRow(itemRow);

    for(int col = rc::FIRST_COLUMN; col <= rc::LAST_COLUMN; col++)
      itemRow[col] = nullptr;
    row++;
  }

  updateModelRouteTime();

  Flightplan& flightplan = route.getFlightplan();

  if(!flightplan.isEmpty())
  {
    // Set spin box and block signals to avoid recursive call
    {
      QSignalBlocker blocker(ui->spinBoxRouteAlt);
      Q_UNUSED(blocker);
      ui->spinBoxRouteAlt->setValue(flightplan.getCruisingAltitude());
    }

    {
      QSignalBlocker blocker(ui->spinBoxRouteSpeed);
      Q_UNUSED(blocker);
      ui->spinBoxRouteSpeed->setValue(atools::roundToInt(flightplan.getProperties().value("speed").toFloat()));
    }

    { // Set combo box and block signals to avoid recursive call
      QSignalBlocker blocker(ui->comboBoxRouteType);
      Q_UNUSED(blocker);
      if(flightplan.getFlightplanType() == atools::fs::pln::IFR)
        ui->comboBoxRouteType->setCurrentIndex(0);
      else if(flightplan.getFlightplanType() == atools::fs::pln::VFR)
        ui->comboBoxRouteType->setCurrentIndex(1);
    }
  }

  highlightProcedureItems();
  highlightNextWaypoint(route.getActiveLegIndexCorrected());
  updateWindowLabel();
}

/* Update travel times in table view model after speed change */
void RouteController::updateModelRouteTime()
{
  int row = 0;
  float cumulatedDistance = 0.f;
  for(const RouteLeg& leg : route)
  {
    if(!route.isAirportAfterArrival(row))
    {
      if(row == 0)
        model->setItem(row, rc::LEG_TIME, new QStandardItem());
      else
      {
        float travelTime = calcTravelTime(leg.getDistanceTo());
        model->setItem(row, rc::LEG_TIME, new QStandardItem(formatter::formatMinutesHours(travelTime)));
      }

      if(!leg.getProcedureLeg().isMissed())
      {
        cumulatedDistance += leg.getDistanceTo();
        float eta = calcTravelTime(cumulatedDistance);
        model->setItem(row, rc::ETA, new QStandardItem(formatter::formatMinutesHours(eta)));
      }
    }
    row++;
  }
}

void RouteController::disconnectedFromSimulator()
{
  qDebug() << Q_FUNC_INFO;

  route.resetActive();
  highlightNextWaypoint(-1);
  emit routeChanged(false);
}

void RouteController::simDataChanged(const atools::fs::sc::SimConnectData& simulatorData)
{
  if(atools::almostNotEqual(QDateTime::currentDateTime().toMSecsSinceEpoch(),
                            lastSimUpdate, static_cast<qint64>(MIN_SIM_UPDATE_TIME_MS)))
  {
    const atools::fs::sc::SimConnectUserAircraft& aircraft = simulatorData.getUserAircraft();

    // Sequence only for airborne airplanes
    if(!aircraft.isOnGround())
    {
      map::PosCourse position(aircraft.getPosition(), aircraft.getTrackDegTrue());
      int previousRouteLeg = route.getActiveLegIndexCorrected();
      route.updateActiveLegAndPos(position);
      int routeLeg = route.getActiveLegIndexCorrected();

      if(routeLeg != previousRouteLeg)
      {
        // Use corrected indexes to highlight initial fix
        qDebug() << "new route leg" << previousRouteLeg << routeLeg;
        highlightNextWaypoint(routeLeg);
      }
    }

    lastSimUpdate = QDateTime::currentDateTime().toMSecsSinceEpoch();
  }
}

/* */
void RouteController::highlightNextWaypoint(int nearestLegIndex)
{
  for(int row = 0; row < model->rowCount(); ++row)
  {
    for(int col = rc::FIRST_COLUMN; col <= rc::LAST_COLUMN; ++col)
    {
      QStandardItem *item = model->item(row, col);
      if(item != nullptr)
      {
        item->setBackground(Qt::NoBrush);
        // Keep first column bold
        if(item->font().bold() && col != 0)
        {
          QFont font = item->font();
          font.setBold(false);
          item->setFont(font);
        }
      }
    }
  }

  if(!route.isEmpty())
  {
    if(nearestLegIndex >= 0 && nearestLegIndex < route.size())
    {
      QColor color = OptionData::instance().isGuiStyleDark() ?
                     mapcolors::nextWaypointColorDark : mapcolors::nextWaypointColor;

      for(int col = rc::FIRST_COLUMN; col <= rc::LAST_COLUMN; ++col)
      {
        QStandardItem *item = model->item(nearestLegIndex, col);
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
  highlightProcedureItems();
}

void RouteController::highlightProcedureItems()
{
  for(int row = 0; row < model->rowCount(); ++row)
  {
    for(int col = 0; col < model->columnCount(); ++col)
    {
      QStandardItem *item = model->item(row, col);
      if(item != nullptr)
      {
        const RouteLeg& leg = route.at(row);
        if(leg.isAnyProcedure())
        {
          if(leg.getProcedureLeg().isMissed())
            item->setForeground(OptionData::instance().isGuiStyleDark() ? Qt::darkYellow : Qt::darkRed);
          else
            item->setForeground(OptionData::instance().isGuiStyleDark() ? Qt::cyan : Qt::darkBlue);
        }
        else if((col == rc::IDENT && leg.getMapObjectType() == map::INVALID) ||
                (col == rc::AIRWAY_OR_LEGTYPE && leg.isRoute() && !leg.getAirwayName().isEmpty() &&
                 !leg.getAirway().isValid()))
          item->setForeground(Qt::red);
        else
          item->setForeground(QApplication::palette().color(QPalette::Normal, QPalette::Text));
      }
    }
  }
}

/* Update the dock window top level label */
void RouteController::updateWindowLabel()
{
  NavApp::getMainUi()->labelRouteInfo->setText(buildFlightplanLabel(true) + "<br/>" + buildFlightplanLabel2());
}

QString RouteController::buildFlightplanLabel(bool html) const
{
  const Flightplan& flightplan = route.getFlightplan();

  QString departure(tr("Invalid")), destination(tr("Invalid")), approach;

  if(!flightplan.isEmpty())
  {
    // Add departure to text ==============================================================
    if(route.hasValidDeparture())
    {
      departure = flightplan.getDepartureAiportName() +
                  " (" + flightplan.getDepartureIdent() + ")";

      if(route.first().getDepartureParking().isValid())
        departure += " " + map::parkingNameNumberType(route.first().getDepartureParking());
      else if(route.first().getDepartureStart().isValid())
      {
        const map::MapStart& start = route.first().getDepartureStart();
        if(route.hasDepartureHelipad())
          departure += tr(" Helipad %1").arg(start.helipadNumber);
        else if(!start.runwayName.isEmpty())
          departure += tr(" Runway %1").arg(start.runwayName);
        else
          departure += tr(" Unknown Start");
      }
    }
    else
      departure = QString("%1 (%2)").
                  arg(flightplan.getEntries().first().getIcaoIdent()).
                  arg(flightplan.getEntries().first().getWaypointTypeAsString());

    // Add destination to text ==============================================================
    if(route.hasValidDestination())
    {
      destination = flightplan.getDestinationAiportName() + " (" + flightplan.getDestinationIdent() + ")";
      // airportId = route.last().getAirport().id;
    }
    else
      destination = QString("%1 (%2)").
                    arg(flightplan.getEntries().last().getIcaoIdent()).
                    arg(flightplan.getEntries().last().getWaypointTypeAsString());

    // Add procedures to text ==============================================================
    const proc::MapProcedureLegs& arrivalLegs = route.getArrivalLegs();
    const proc::MapProcedureLegs& starLegs = route.getStarLegs();
    if(route.hasAnyProcedure())
    {
      QStringList procedureText;
      QVector<bool> boldTextFlag;

      const proc::MapProcedureLegs& departureLegs = route.getDepartureLegs();
      if(!departureLegs.isEmpty())
      {
        // Add departure procedure to text
        if(!departureLegs.runwayEnd.isValid())
        {
          boldTextFlag << false;
          procedureText.append(tr("Depart via SID"));
        }
        else
        {
          boldTextFlag << false << true << false;
          procedureText.append(tr("Depart runway "));
          procedureText.append(departureLegs.runwayEnd.name);
          procedureText.append(tr("via SID"));
        }

        QString sid(departureLegs.approachFixIdent);
        if(!departureLegs.transitionFixIdent.isEmpty())
          sid += "." + departureLegs.transitionFixIdent;
        boldTextFlag << true;
        procedureText.append(sid);

        if(arrivalLegs.mapType & proc::PROCEDURE_ARRIVAL_ALL || starLegs.mapType &
           proc::PROCEDURE_ARRIVAL_ALL)
        {
          boldTextFlag << false;
          procedureText.append(tr(". "));
        }
      }

      // Add arrival procedures procedure to text
      if(!starLegs.isEmpty())
      {
        boldTextFlag << false << true;
        procedureText.append(tr("From STAR"));

        QString star(starLegs.approachFixIdent);
        if(!starLegs.transitionFixIdent.isEmpty())
          star += "." + starLegs.transitionFixIdent;
        procedureText.append(star);
      }

      if(arrivalLegs.mapType & proc::PROCEDURE_TRANSITION)
      {
        boldTextFlag << false << true;
        procedureText.append(!starLegs.isEmpty() ? tr("via") : tr("Via"));
        procedureText.append(arrivalLegs.transitionFixIdent);
      }

      if(arrivalLegs.mapType & proc::PROCEDURE_APPROACH)
      {
        boldTextFlag << false;
        procedureText.append((arrivalLegs.mapType & proc::PROCEDURE_TRANSITION ||
                              !starLegs.isEmpty()) ? tr("and") : tr("Via"));

        boldTextFlag << true;
        procedureText.append(arrivalLegs.approachType);
        boldTextFlag << true;
        procedureText.append(arrivalLegs.approachFixIdent);

        // Add runway for approach
        boldTextFlag << false << true;
        procedureText.append(procedureText.isEmpty() ? tr("To runway") : tr("to runway"));
        procedureText.append(arrivalLegs.runwayEnd.name);
      }
      else
      {
        boldTextFlag << false;
        procedureText.append(tr("to airport"));
      }

      if(html)
      {
        for(int i = 0; i < procedureText.size(); i++)
        {
          if(boldTextFlag.at(i))
            procedureText[i] = "<b>" + procedureText.at(i) + "</b>";
        }
      }
      approach = procedureText.join(" ");
    }
  }

  QString fp(tr("No Flightplan loaded"));
  if(!flightplan.isEmpty())
    fp = tr("<b>%1</b> to <b>%2</b>").arg(departure).arg(destination);

  if(html)
    return fp + (approach.isEmpty() ? QString() : "<br/>" + approach);
  else
    return tr("%1 to %2 %3").arg(departure).arg(destination).arg(approach);
}

QString RouteController::buildFlightplanLabel2() const
{
  const Flightplan& flightplan = route.getFlightplan();
  if(!flightplan.isEmpty())
  {
    QString routeType;
    switch(flightplan.getRouteType())
    {
      case atools::fs::pln::LOW_ALTITUDE:
        routeType = tr("Low Altitude");
        break;
      case atools::fs::pln::HIGH_ALTITUDE:
        routeType = tr("High Altitude");
        break;
      case atools::fs::pln::VOR:
        routeType = tr("Radionav");
        break;
      case atools::fs::pln::DIRECT:
        routeType = tr("Direct");
        break;
    }

    float totalDistance = route.getTotalDistance();

    return tr("%1, %2, %3").
           arg(Unit::distNm(totalDistance)).
           arg(formatter::formatMinutesHoursLong(calcTravelTime(route.getTotalDistance()))).
           arg(routeType);
  }
  else
    return QString();
}

float RouteController::calcTravelTime(float distance) const
{
  float travelTime = 0.f;
  float speedKts = Unit::rev(static_cast<float>(NavApp::getMainUi()->spinBoxRouteSpeed->value()), Unit::speedKtsF);
  if(speedKts > 0.f)
    travelTime = distance / speedKts;
  return travelTime;
}

/* Reset route and clear undo stack (new route) */
void RouteController::clearRoute()
{
  route.removeProcedureLegs();
  route.getFlightplan().clear();
  route.getFlightplan().getProperties().clear();
  route.resetActive();
  route.clear();
  route.setTotalDistance(0.f);

  routeFilename.clear();
  fileDeparture.clear();
  fileDestination.clear();
  fileIfrVfr = VFR;
  undoStack->clear();
  undoIndex = 0;
  undoIndexClean = 0;
  entryBuilder->setCurUserpointNumber(1);
}

/* Call this before doing any change to the flight plan that should be undoable */
RouteCommand *RouteController::preChange(const QString& text, rctype::RouteCmdType rcType)
{
  // Clean the flight plan from any procedure entries
  Flightplan flightplan = route.getFlightplan();
  flightplan.removeNoSaveEntries();
  return new RouteCommand(this, flightplan, text, rcType);
}

/* Call this after doing a change to the flight plan that should be undoable */
void RouteController::postChange(RouteCommand *undoCommand)
{
  if(undoCommand == nullptr)
    return;

  // Clean the flight plan from any procedure entries
  Flightplan flightplan = route.getFlightplan();
  flightplan.removeNoSaveEntries();
  undoCommand->setFlightplanAfter(flightplan);

  if(undoIndex < undoIndexClean)
    undoIndexClean = -1;

  // Index and clean index workaround
  undoIndex++;
  qDebug() << "postChange undoIndex" << undoIndex << "undoIndexClean" << undoIndexClean;
  undoStack->push(undoCommand);
}

/*
 * Select the best runway start position for the departure airport.
 * @param force Update even if a start position is already set
 * @param undo Embed in undo operation
 * @return true if parking was changed
 */
bool RouteController::updateStartPositionBestRunway(bool force, bool undo)
{
  if(route.hasValidDeparture())
  {
    RouteLeg& routeLeg = route.first();

    if(force || (!route.hasDepartureParking() && !route.hasDepartureStart()))
    {
      // Reset departure position to best runway
      map::MapStart start;
      query->getBestStartPositionForAirport(start, routeLeg.getAirport().id);

      // Check if the airport has a start position - sone add-on airports don't
      if(start.isValid())
      {
        RouteCommand *undoCommand = nullptr;

        if(undo)
          undoCommand = preChange(tr("Set Start Position"));

        routeLeg.setDepartureStart(start);
        routeToFlightPlan();

        if(undo)
          postChange(undoCommand);
        return true;
      }
    }
  }
  return false;
}

proc::MapProcedureTypes RouteController::affectedProcedures(const QList<int>& indexes)
{
  proc::MapProcedureTypes types = proc::PROCEDURE_NONE;

  for(int index : indexes)
  {
    if(index == 0)
      // Delete SID if departure airport is affected
      types |= proc::PROCEDURE_DEPARTURE;

    if(index >= route.size() - 1)
      // Delete all arrival procedures if destination airport is affected or an new leg is appended after
      types |= proc::PROCEDURE_ARRIVAL_ALL;

    if(index >= 0 && index < route.size())
    {
      const proc::MapProcedureLeg& leg = route.at(index).getProcedureLeg();

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
        types |= proc::PROCEDURE_ARRIVAL;
    }
  }

  return types;
}

void RouteController::updateIcons()
{
  ndbIcon = symbolPainter->createNdbIcon(iconSize);
  waypointIcon = symbolPainter->createWaypointIcon(iconSize);
  userpointIcon = symbolPainter->createUserpointIcon(iconSize);
  invalidIcon = symbolPainter->createWaypointIcon(iconSize, mapcolors::routeInvalidPointColor);
  procedureIcon = symbolPainter->createProcedurePointIcon(iconSize);
}
