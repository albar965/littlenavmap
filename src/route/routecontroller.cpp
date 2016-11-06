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

#include "routecontroller.h"

#include "options/optiondata.h"
#include "common/constants.h"
#include "common/formatter.h"
#include "exception.h"
#include "export/csvexporter.h"
#include "gui/actiontextsaver.h"
#include "gui/errorhandler.h"
#include "gui/mainwindow.h"
#include "gui/tablezoomhandler.h"
#include "gui/widgetstate.h"
#include "mapgui/mapquery.h"
#include "mapgui/mapwidget.h"
#include "parkingdialog.h"
#include "route/routefinder.h"
#include "route/routeicondelegate.h"
#include "route/routenetworkairway.h"
#include "route/routenetworkradio.h"
#include "settings/settings.h"
#include "ui_mainwindow.h"
#include "gui/dialog.h"
#include "atools.h"
#include "util/htmlbuilder.h"
#include "common/symbolpainter.h"
#include "common/mapcolors.h"

#include <QClipboard>
#include <QFile>
#include <QStandardItemModel>

#include <marble/GeoDataLineString.h>

// Route table colum headings
const QList<QString> ROUTE_COLUMNS({QObject::tr("Ident"),
                                    QObject::tr("Region"),
                                    QObject::tr("Name"),
                                    QObject::tr("Airway"),
                                    QObject::tr("Type"),
                                    QObject::tr("Freq.\nMHz/kHz"),
                                    QObject::tr("Range\nnm"),
                                    QObject::tr("Course\n°M"),
                                    QObject::tr("Direct\n°M"),
                                    QObject::tr("Distance\nnm"),
                                    QObject::tr("Remaining\nnm"),
                                    QObject::tr("Leg Time\nhh:mm"),
                                    QObject::tr("ETA\nhh:mm UTC")});

namespace rc {
// Route table column indexes
enum RouteColumns
{
  FIRST_COLUMN,
  IDENT = FIRST_COLUMN,
  REGION,
  NAME,
  AIRWAY,
  TYPE,
  FREQ,
  RANGE,
  COURSE,
  DIRECT,
  DIST,
  REMAINING_DISTANCE,
  LEG_TIME,
  ETA,
  LAST_COLUMN = ETA
};

}

const int PRINT_ICON_SIZE = 15;

using namespace atools::fs::pln;
using namespace atools::geo;
using Marble::GeoDataLatLonBox;
using Marble::GeoDataLineString;
using Marble::GeoDataCoordinates;

RouteController::RouteController(MainWindow *parentWindow, MapQuery *mapQuery, QTableView *tableView)
  : QObject(parentWindow), mainWindow(parentWindow), view(tableView), query(mapQuery)
{
  // Set default table cell and font size to avoid Qt overly large cell sizes
  zoomHandler = new atools::gui::TableZoomHandler(view);

  view->setContextMenuPolicy(Qt::CustomContextMenu);

  // Create flight plan calculation caches
  routeNetworkRadio = new RouteNetworkRadio(mainWindow->getDatabase());
  routeNetworkAirway = new RouteNetworkAirway(mainWindow->getDatabase());

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

  Ui::MainWindow *ui = mainWindow->getUi();
  ui->routeToolBar->insertAction(ui->actionRouteSelectParking, undoAction);
  ui->routeToolBar->insertAction(ui->actionRouteSelectParking, redoAction);

  ui->menuRoute->insertActions(ui->actionRouteSelectParking, {undoAction, redoAction});
  ui->menuRoute->insertSeparator(ui->actionRouteSelectParking);

  connect(ui->spinBoxRouteSpeed, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &RouteController::updateWindowLabel);
  connect(ui->spinBoxRouteSpeed, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &RouteController::updateModelRouteTime);
  connect(ui->spinBoxRouteAlt, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &RouteController::routeAltChanged);
  connect(ui->comboBoxRouteType, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
          this, &RouteController::routeTypeChanged);

  connect(view, &QTableView::doubleClicked, this, &RouteController::doubleClick);
  connect(view, &QTableView::customContextMenuRequested, this, &RouteController::tableContextMenu);

  // set up table view
  view->horizontalHeader()->setSectionsMovable(true);
  view->verticalHeader()->setSectionsMovable(false);
  view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

  model = new QStandardItemModel();
  QItemSelectionModel *m = view->selectionModel();
  view->setModel(model);
  delete m;

  // Add airport and navaid icons to first column
  iconDelegate = new RouteIconDelegate(route);
  view->setItemDelegateForColumn(0, iconDelegate);

  // Avoid stealing of keys from other default menus
  ui->actionRouteLegDown->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteLegUp->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteDeleteLeg->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteShowInformation->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteShowOnMap->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  // Avoid stealing of Ctrl - C from other default menus
  ui->actionRouteTableCopy->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  // Need extra action connected to catch the default Ctrl-C in the table view
  connect(ui->actionRouteTableCopy, &QAction::triggered, this, &RouteController::tableCopyClipboard);

  // Add action/shortcuts to table view
  view->addActions({ui->actionRouteLegDown, ui->actionRouteLegUp, ui->actionRouteDeleteLeg,
                    ui->actionRouteTableCopy, ui->actionRouteShowInformation, ui->actionRouteShowOnMap});

  void (RouteController::*selChangedPtr)(const QItemSelection &selected, const QItemSelection &deselected) =
    &RouteController::tableSelectionChanged;
  connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this, selChangedPtr);

  connect(ui->actionRouteLegDown, &QAction::triggered, this, &RouteController::moveSelectedLegsDown);
  connect(ui->actionRouteLegUp, &QAction::triggered, this, &RouteController::moveSelectedLegsUp);
  connect(ui->actionRouteDeleteLeg, &QAction::triggered, this, &RouteController::deleteSelectedLegs);

  connect(ui->actionRouteShowInformation, &QAction::triggered, this, &RouteController::showInformationMenu);
  connect(ui->actionRouteShowOnMap, &QAction::triggered, this, &RouteController::showOnMapMenu);

  connect(ui->dockWidgetRoute, &QDockWidget::visibilityChanged, this, &RouteController::dockVisibilityChanged);

  // Use saved font size for table view
  zoomHandler->zoomPercent(OptionData::instance().getGuiRouteTableTextSize());
}

RouteController::~RouteController()
{
  delete model;
  delete iconDelegate;
  delete undoStack;
  delete routeNetworkRadio;
  delete routeNetworkAirway;
  delete zoomHandler;
}

void RouteController::undoTriggered()
{
  mainWindow->setStatusMessage(QString(tr("Undo flight plan change.")));
}

void RouteController::redoTriggered()
{
  mainWindow->setStatusMessage(QString(tr("Redo flight plan change.")));
}

/* Ctrl-C - copy selected table contents in CSV format to clipboard */
void RouteController::tableCopyClipboard()
{
  qDebug() << "RouteController::tableCopyClipboard";

  QString csv;
  int exported = CsvExporter::selectionAsCsv(view, true, csv);

  if(!csv.isEmpty())
    QApplication::clipboard()->setText(csv);

  mainWindow->setStatusMessage(QString(tr("Copied %1 entries to clipboard.")).arg(exported));
}

QString RouteController::tableAsHtml()
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
    const RouteMapObject& rmo = route.at(row);
    QIcon icon;
    if(rmo.getMapObjectType() == maptypes::AIRPORT)
      icon = SymbolPainter(Qt::white).createAirportIcon(rmo.getAirport(), PRINT_ICON_SIZE);
    else if(rmo.getMapObjectType() == maptypes::WAYPOINT)
      icon = SymbolPainter(Qt::white).createWaypointIcon(PRINT_ICON_SIZE);
    else if(rmo.getMapObjectType() == maptypes::VOR)
      icon = SymbolPainter(Qt::white).createVorIcon(rmo.getVor(), PRINT_ICON_SIZE);
    else if(rmo.getMapObjectType() == maptypes::NDB)
      icon = SymbolPainter(Qt::white).createNdbIcon(PRINT_ICON_SIZE);
    else if(rmo.getMapObjectType() == maptypes::USER)
      icon = SymbolPainter(Qt::white).createUserpointIcon(PRINT_ICON_SIZE);
    else if(rmo.getMapObjectType() == maptypes::INVALID)
      icon = SymbolPainter(Qt::white).createWaypointIcon(PRINT_ICON_SIZE, mapcolors::routeInvalidPointColor);

    html.td();
    html.img(icon, QString(), QString(), QSize(PRINT_ICON_SIZE, PRINT_ICON_SIZE));
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

/* Spin box altitude has changed value */
void RouteController::routeAltChanged()
{
  RouteCommand *undoCommand = nullptr;

  if(!route.isEmpty())
    undoCommand = preChange(tr("Change Altitude"), rctype::ALTITUDE);

  routeToFlightPlan();

  if(!route.isEmpty())
    postChange(undoCommand);

  mainWindow->updateWindowTitle();

  if(!route.isEmpty())
    emit routeChanged(false);
}

/* Combo box route type has value changed */
void RouteController::routeTypeChanged()
{
  RouteCommand *undoCommand = nullptr;

  if(!route.isEmpty())
    undoCommand = preChange(tr("Change Type"));

  routeToFlightPlan();

  if(!route.isEmpty())
    postChange(undoCommand);

  mainWindow->updateWindowTitle();

  if(!route.isEmpty())
  {
    emit routeChanged(false);
    Ui::MainWindow *ui = mainWindow->getUi();
    mainWindow->setStatusMessage(tr("Flight plan type changed to %1.").arg(ui->comboBoxRouteType->currentText()));
  }
}

bool RouteController::selectDepartureParking()
{
  qDebug() << "selectDepartureParking";
  const maptypes::MapAirport& airport = route.first().getAirport();
  ParkingDialog dialog(mainWindow, query, airport);

  int result = dialog.exec();
  dialog.hide();

  if(result == QDialog::Accepted)
  {
    // Set either start of parking
    maptypes::MapParking parking;
    maptypes::MapStart start;
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
  Ui::MainWindow *ui = mainWindow->getUi();

  atools::gui::WidgetState(lnm::ROUTE_VIEW).save({view, ui->spinBoxRouteSpeed,
                                                  ui->comboBoxRouteType,
                                                  ui->spinBoxRouteAlt});

  atools::settings::Settings::instance().setValue(lnm::ROUTE_FILENAME, routeFilename);
}

void RouteController::restoreState()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  model->setHorizontalHeaderLabels(ROUTE_COLUMNS);
  atools::gui::WidgetState(lnm::ROUTE_VIEW).restore({view, ui->spinBoxRouteSpeed,
                                                     ui->comboBoxRouteType,
                                                     ui->spinBoxRouteAlt});

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
          route.clear();
        }
      }
      else
      {
        routeFilename.clear();
        route.clear();
      }
    }
  }
}

void RouteController::getSelectedRouteMapObjects(QList<int>& selRouteMapObjectIndexes) const
{
  if(mainWindow->getUi()->dockWidgetRoute->isVisible())
  {
    QItemSelection sm = view->selectionModel()->selection();
    for(const QItemSelectionRange& rng : sm)
    {
      for(int row = rng.top(); row <= rng.bottom(); ++row)
        selRouteMapObjectIndexes.append(row);
    }
  }
}

void RouteController::newFlightplan()
{
  qDebug() << "newFlightplan";
  clearRoute();

  // Copy current alt and type from widgets to flightplan
  updateFlightplanFromWidgets();

  createRouteMapObjects();
  updateTableModel();
  mainWindow->updateWindowTitle();
  updateWindowLabel();
  emit routeChanged(true);
}

bool RouteController::loadFlightplan(const QString& filename)
{
  Flightplan newFlightplan;
  try
  {
    qDebug() << "loadFlightplan" << filename;
    // Will throw an exception if something goes wrong
    newFlightplan.load(filename);

    clearRoute();
    routeFilename = filename;
    route.setFlightplan(newFlightplan);
    createRouteMapObjects();
    if(updateStartPositionBestRunway(false /* force */, true /* undo */))
    {
      atools::gui::Dialog(mainWindow).showInfoMsgBox(lnm::ACTIONS_SHOWROUTE_START_CHANGED,
                                                     tr("The flight plan had no valid start position.\n"
                                                        "The start position is now set to the longest "
                                                        "primary runway of the departure airport."),
                                                     tr("Do not &show this dialog again."));
    }
    updateTableModel();
    mainWindow->updateWindowTitle();
    updateWindowLabel();

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

bool RouteController::appendFlightplan(const QString& filename)
{
  Flightplan flightplan;
  try
  {
    qDebug() << "appendFlightplan" << filename;

    // Will throw an exception if something goes wrong
    flightplan.load(filename);

    RouteCommand *undoCommand = preChange(tr("Append"));

    for(const FlightplanEntry& entry : flightplan.getEntries())
      route.getFlightplan().getEntries().append(entry);

    route.getFlightplan().setDestinationAiportName(flightplan.getDestinationAiportName());
    route.getFlightplan().setDestinationIdent(flightplan.getDestinationIdent());
    route.getFlightplan().setDestinationPosition(flightplan.getDestinationPosition());

    createRouteMapObjects();

    updateTableModel();
    postChange(undoCommand);
    mainWindow->updateWindowTitle();
    updateWindowLabel();

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

bool RouteController::saveFlighplanAs(const QString& filename)
{
  qDebug() << "saveFlighplanAs" << filename;
  routeFilename = filename;
  return saveFlightplan();
}

bool RouteController::saveFlightplan()
{
  try
  {
    qDebug() << "saveFlighplan" << routeFilename;
    // Will throw an exception if something goes wrong
    route.getFlightplan().save(routeFilename);

    // Set clean undo state index since QUndoStack only returns weird values
    undoIndexClean = undoIndex;
    undoStack->setClean();
    mainWindow->updateWindowTitle();
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
  qDebug() << "calculateDirect";

  // Stop any background tasks
  emit preRouteCalc();

  RouteCommand *undoCommand = preChange(tr("Direct Calculation"));

  Flightplan& flightplan = route.getFlightplan();

  flightplan.setRouteType(atools::fs::pln::DIRECT);

  if(flightplan.getEntries().size() >= 2)
    // Remove all waypoints
    flightplan.getEntries().erase(flightplan.getEntries().begin() + 1, flightplan.getEntries().end() - 1);

  createRouteMapObjects();
  updateTableModel();
  updateWindowLabel();
  postChange(undoCommand);
  mainWindow->updateWindowTitle();
  emit routeChanged(true);
  mainWindow->setStatusMessage(tr("Calculated direct flight plan."));
}

void RouteController::calculateRadionav()
{
  qDebug() << "calculateRadionav";
  // Changing mode might need a clear
  routeNetworkRadio->setMode(nw::ROUTE_RADIONAV);

  RouteFinder routeFinder(routeNetworkRadio);

  if(calculateRouteInternal(&routeFinder, atools::fs::pln::VOR, tr("Radionnav Flight Plan Calculation"),
                            false /* fetch airways */, false /* Use altitude */))
    mainWindow->setStatusMessage(tr("Calculated radio navaid flight plan."));
  else
    mainWindow->setStatusMessage(tr("No route found."));
}

void RouteController::calculateHighAlt()
{
  qDebug() << "calculateHighAlt";
  routeNetworkAirway->setMode(nw::ROUTE_JET);

  RouteFinder routeFinder(routeNetworkAirway);

  if(calculateRouteInternal(&routeFinder, atools::fs::pln::HIGH_ALTITUDE,
                            tr("High altitude Flight Plan Calculation"),
                            true /* fetch airways */, false /* Use altitude */))
    mainWindow->setStatusMessage(tr("Calculated high altitude (Jet airways) flight plan."));
  else
    mainWindow->setStatusMessage(tr("No route found."));
}

void RouteController::calculateLowAlt()
{
  qDebug() << "calculateLowAlt";
  routeNetworkAirway->setMode(nw::ROUTE_VICTOR);

  RouteFinder routeFinder(routeNetworkAirway);

  if(calculateRouteInternal(&routeFinder, atools::fs::pln::LOW_ALTITUDE,
                            tr("Low altitude Flight Plan Calculation"),
                            /* fetch airways */ true, false /* Use altitude */))
    mainWindow->setStatusMessage(tr("Calculated low altitude (Victor airways) flight plan."));
  else
    mainWindow->setStatusMessage(tr("No route found."));
}

void RouteController::calculateSetAlt()
{
  qDebug() << "calculateSetAlt";
  routeNetworkAirway->setMode(nw::ROUTE_VICTOR | nw::ROUTE_JET);

  RouteFinder routeFinder(routeNetworkAirway);

  // Just decide by given altiude if this is a high or low plan
  atools::fs::pln::RouteType type;
  if(route.getFlightplan().getCruisingAltitude() > 20000)
    type = atools::fs::pln::HIGH_ALTITUDE;
  else
    type = atools::fs::pln::LOW_ALTITUDE;

  if(calculateRouteInternal(&routeFinder, type, tr("Low altitude flight plan"),
                            true /* fetch airways */, true /* Use altitude */))
    mainWindow->setStatusMessage(tr("Calculated high/low flight plan for given altitude."));
  else
    mainWindow->setStatusMessage(tr("No route found."));
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

  int altitude = useSetAltitude ? flightplan.getCruisingAltitude() : 0;

  routeFinder->setPreferVorToAirway(OptionData::instance().getFlags() & opts::ROUTE_PREFER_VOR);
  routeFinder->setPreferNdbToAirway(OptionData::instance().getFlags() & opts::ROUTE_PREFER_NDB);

  Pos departurePos = flightplan.getEntries().first().getPosition();
  Pos destinationPos = flightplan.getEntries().last().getPosition();
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
        FlightplanEntry flightplanEentry;
        buildFlightplanEntry(routeEntry.ref.id, atools::geo::EMPTY_POS, routeEntry.ref.type, flightplanEentry,
                             fetchAirways);

        if(fetchAirways && routeEntry.airwayId != -1)
        {
          int alt = 0;
          updateFlightplanEntryAirway(routeEntry.airwayId, flightplanEentry, alt);
          minAltitude = std::max(minAltitude, alt);
        }

        entries.insert(entries.end() - 1, flightplanEentry);
      }

      if(minAltitude != 0 && !useSetAltitude)
      {
        if(OptionData::instance().getFlags() & opts::ROUTE_EAST_WEST_RULE)
        {
          // Apply simplified east/west rule
          float fpDir = departurePos.angleDegToRhumb(destinationPos);

          qDebug() << "minAltitude" << minAltitude << "fp dir" << fpDir;

          if(fpDir >= 0.f && fpDir <= 180.f)
            // General direction is east - round up to the next odd value
            minAltitude = static_cast<int>(std::ceil((minAltitude - 1000.f) / 2000.f) * 2000.f + 1000.f);
          else
            // General direction is west - round up to the next even value
            minAltitude = static_cast<int>(std::ceil(minAltitude / 2000.f) * 2000.f);

          if(flightplan.getFlightplanType() == atools::fs::pln::VFR)
            minAltitude += 500;

          qDebug() << "corrected minAltitude" << minAltitude;
        }

        flightplan.setCruisingAltitude(minAltitude);
      }

      QGuiApplication::restoreOverrideCursor();
      createRouteMapObjects();
      updateTableModel();
      updateWindowLabel();
      postChange(undoCommand);
      mainWindow->updateWindowTitle();
      emit routeChanged(true);
    }
    else
      // Too long
      found = false;
  }

  QGuiApplication::restoreOverrideCursor();
  if(!found)
    atools::gui::Dialog(mainWindow).showInfoMsgBox(lnm::ACTIONS_SHOWROUTEERROR,
                                                   tr("Cannot find a route.\n"
                                                      "Try another routing type or create the flight plan manually."),
                                                   tr("Do not &show this dialog again."));

  return found;
}

void RouteController::reverse()
{
  qDebug() << "reverse";

  RouteCommand *undoCommand = preChange(tr("Reverse"), rctype::REVERSE);

  route.getFlightplan().reverse();

  createRouteMapObjects();
  updateStartPositionBestRunway(true /* force */, false /* undo */);
  updateTableModel();
  updateWindowLabel();
  postChange(undoCommand);
  mainWindow->updateWindowTitle();
  emit routeChanged(true);
  mainWindow->setStatusMessage(tr("Reversed flight plan."));
}

QString RouteController::buildDefaultFilename() const
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
  filename += ".pln";

  // Remove characters that are note allowed in most filesystems
  filename.replace('\\', ' ').replace('/', ' ').replace(':', ' ').replace('\'', ' ').
  replace('<', ' ').replace('>', ' ').replace('?', ' ').replace('$', ' ').replace("  ", " ");
  return filename;
}

bool RouteController::isFlightplanEmpty() const
{
  return route.isFlightplanEmpty();
}

bool RouteController::hasValidDeparture() const
{
  return route.hasValidDeparture();
}

bool RouteController::hasValidDestination() const
{
  return route.hasValidDestination();
}

bool RouteController::hasEntries() const
{
  return route.hasEntries();
}

bool RouteController::canCalcRoute() const
{
  return route.canCalcRoute();
}

void RouteController::preDatabaseLoad()
{
  routeNetworkRadio->deInitQueries();
  routeNetworkAirway->deInitQueries();
}

void RouteController::postDatabaseLoad()
{
  routeNetworkRadio->initQueries();
  routeNetworkAirway->initQueries();
  createRouteMapObjects();
  updateTableModel();
  mainWindow->updateWindowTitle();
  updateWindowLabel();
}

/* Double click into table view */
void RouteController::doubleClick(const QModelIndex& index)
{
  if(index.isValid())
  {
    qDebug() << "mouseDoubleClickEvent";

    const RouteMapObject& mo = route.at(index.row());

    if(mo.getMapObjectType() == maptypes::AIRPORT)
      emit showRect(mo.getAirport().bounding);
    else
      emit showPos(mo.getPosition(), -1);

    maptypes::MapSearchResult result;
    query->getMapObjectById(result, mo.getMapObjectType(), mo.getId());
    emit showInformation(result);
  }
}

/* Update action states after move and delete */
void RouteController::updateMoveAndDeleteActions()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  QItemSelectionModel *sm = view->selectionModel();

  ui->actionRouteLegUp->setEnabled(false);
  ui->actionRouteLegDown->setEnabled(false);
  ui->actionRouteDeleteLeg->setEnabled(false);

  if(sm->hasSelection() && model->rowCount() > 0)
  {
    if(model->rowCount() > 1)
    {
      ui->actionRouteDeleteLeg->setEnabled(true);
      ui->actionRouteLegUp->setEnabled(sm->hasSelection() && !sm->isRowSelected(0, QModelIndex()));

      ui->actionRouteLegDown->setEnabled(sm->hasSelection() &&
                                         !sm->isRowSelected(model->rowCount() - 1, QModelIndex()));
    }
    else if(model->rowCount() == 1)
      // Only one waypoint - nothing to move
      ui->actionRouteDeleteLeg->setEnabled(true);
  }
}

/* From context menu */
void RouteController::showInformationMenu()
{
  QModelIndex index = view->currentIndex();
  if(index.isValid())
  {
    const RouteMapObject& routeMapObject = route.at(index.row());
    maptypes::MapSearchResult result;
    query->getMapObjectById(result, routeMapObject.getMapObjectType(), routeMapObject.getId());
    emit showInformation(result);
  }
}

/* From context menu */
void RouteController::showOnMapMenu()
{
  QModelIndex index = view->currentIndex();
  if(index.isValid())
  {
    const RouteMapObject& routeMapObject = route.at(index.row());

    if(routeMapObject.getMapObjectType() == maptypes::AIRPORT)
      emit showRect(routeMapObject.getAirport().bounding);
    else
      emit showPos(routeMapObject.getPosition(), -1);

    if(routeMapObject.getMapObjectType() == maptypes::AIRPORT)
      mainWindow->setStatusMessage(tr("Showing airport on map."));
    else
      mainWindow->setStatusMessage(tr("Showing navaid on map."));
  }
}

void RouteController::tableContextMenu(const QPoint& pos)
{
  Ui::MainWindow *ui = mainWindow->getUi();

  QPoint menuPos = QCursor::pos();
  // Use widget center if position is not inside widget
  if(!ui->tableViewRoute->rect().contains(ui->tableViewRoute->mapFromGlobal(QCursor::pos())))
    menuPos = ui->tableViewRoute->mapToGlobal(ui->tableViewRoute->rect().center());

  qDebug() << "tableContextMenu";

  // Save text which will be changed below
  atools::gui::ActionTextSaver saver({ui->actionMapNavaidRange});
  Q_UNUSED(saver);

  QModelIndex index = view->indexAt(pos);
  if(index.isValid())
  {
    // Menu above a row
    const RouteMapObject& routeMapObject = route.at(index.row());

    updateMoveAndDeleteActions();

    ui->actionSearchTableCopy->setEnabled(index.isValid());

    ui->actionMapRangeRings->setEnabled(true);
    ui->actionMapHideRangeRings->setEnabled(!mainWindow->getMapWidget()->getDistanceMarkers().isEmpty() ||
                                            !mainWindow->getMapWidget()->getRangeRings().isEmpty());

    ui->actionRouteShowInformation->setEnabled(routeMapObject.isValid());

    ui->actionRouteShowOnMap->setEnabled(true);

    ui->actionMapNavaidRange->setEnabled(false);
    ui->actionMapNavaidRange->setText(tr("Show Navaid Range"));

    QList<int> selectedRouteMapObjectIndexes;
    getSelectedRouteMapObjects(selectedRouteMapObjectIndexes);
    // If there are any radio navaids in the selected list enable range menu item
    for(int idx : selectedRouteMapObjectIndexes)
    {
      const RouteMapObject& rmo = route.at(idx);
      if(rmo.getMapObjectType() == maptypes::VOR || rmo.getMapObjectType() == maptypes::NDB)
      {
        ui->actionMapNavaidRange->setEnabled(true);
        break;
      }
    }

    QMenu menu;
    menu.addAction(ui->actionRouteShowInformation);
    menu.addAction(ui->actionRouteShowOnMap);
    menu.addSeparator();

    menu.addAction(ui->actionRouteLegUp);
    menu.addAction(ui->actionRouteLegDown);
    menu.addAction(ui->actionRouteDeleteLeg);
    menu.addSeparator();

    menu.addAction(ui->actionMapRangeRings);
    menu.addAction(ui->actionMapNavaidRange);
    menu.addAction(ui->actionMapHideRangeRings);
    menu.addSeparator();

    menu.addAction(ui->actionSearchTableCopy);
    menu.addAction(ui->actionSearchTableSelectAll);
    menu.addSeparator();

    menu.addAction(ui->actionSearchResetView);
    menu.addSeparator();

    menu.addAction(ui->actionSearchSetMark);

    QAction *action = menu.exec(menuPos);
    if(action != nullptr)
    {
      if(action == ui->actionSearchResetView)
      {
        // Reorder columns to match model order
        QHeaderView *header = view->horizontalHeader();
        for(int i = 0; i < header->count(); ++i)
          header->moveSection(header->visualIndex(i), i);

        view->resizeColumnsToContents();
        mainWindow->setStatusMessage(tr("Table view reset to defaults."));
      }
      else if(action == ui->actionSearchTableSelectAll)
        view->selectAll();
      else if(action == ui->actionSearchSetMark)
        emit changeMark(routeMapObject.getPosition());
      else if(action == ui->actionMapRangeRings)
        mainWindow->getMapWidget()->addRangeRing(routeMapObject.getPosition());
      else if(action == ui->actionMapNavaidRange)
      {
        // Show range rings for all radio navaids
        for(int idx : selectedRouteMapObjectIndexes)
        {
          const RouteMapObject& rmo = route.at(idx);
          if(rmo.getMapObjectType() == maptypes::VOR || rmo.getMapObjectType() == maptypes::NDB)
            mainWindow->getMapWidget()->addNavRangeRing(rmo.getPosition(), rmo.getMapObjectType(),
                                                        rmo.getIdent(), rmo.getFrequency(),
                                                        rmo.getRange());
        }
      }
      else if(action == ui->actionMapHideRangeRings)
        mainWindow->getMapWidget()->clearRangeRingsAndDistanceMarkers();

      // Other actions emit signals directly
    }
  }
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

  createRouteMapObjects();
  updateTableModel();
  mainWindow->updateWindowTitle();
  updateWindowLabel();
  updateMoveAndDeleteActions();
  emit routeChanged(true);
}

void RouteController::optionsChanged()
{
  zoomHandler->zoomPercent(OptionData::instance().getGuiRouteTableTextSize());
  updateTableModel();
  view->update();
}

bool RouteController::hasChanged() const
{
  return undoIndexClean == -1 || undoIndexClean != undoIndex;
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

    updateRouteMapObjects();

    // Force update of start if departure airport was moved
    updateStartPositionBestRunway(forceDeparturePosition, false /* undo */);

    routeToFlightPlan();
    updateTableModel();
    updateWindowLabel();

    // Restore current position at new moved position
    view->setCurrentIndex(model->index(curIdx.row() + direction, curIdx.column()));
    // Restore previous selection at new moved position
    select(rows, direction);

    updateMoveAndDeleteActions();

    postChange(undoCommand);
    mainWindow->updateWindowTitle();

    emit routeChanged(true);
    mainWindow->setStatusMessage(tr("Moved flight plan legs."));
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
  qDebug() << "Leg delete";

  // Get selected rows
  QList<int> rows;
  selectedRows(rows, true /* reverse */);

  if(!rows.isEmpty())
  {
    RouteCommand *undoCommand = preChange(tr("Delete Waypoints"), rctype::DELETE);

    int firstRow = rows.last();
    view->selectionModel()->clear();
    for(int row : rows)
    {
      route.getFlightplan().getEntries().removeAt(row);

      eraseAirway(row);

      route.removeAt(row);
      model->removeRow(row);
    }
    updateRouteMapObjects();

    // Force update of start if departure airport was removed
    updateStartPositionBestRunway(rows.contains(0) /* force */, false /* undo */);

    routeToFlightPlan();
    updateTableModel();
    updateWindowLabel();

    // Update current position at the beginning of the former selection
    view->setCurrentIndex(model->index(firstRow, 0));
    updateMoveAndDeleteActions();

    postChange(undoCommand);
    mainWindow->updateWindowTitle();

    emit routeChanged(true);
    mainWindow->setStatusMessage(tr("Removed flight plan legs."));
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

void RouteController::routeSetParking(maptypes::MapParking parking)
{
  qDebug() << "route set parking id" << parking.id;

  RouteCommand *undoCommand = preChange(tr("Set Parking"));

  if(route.isEmpty() || route.first().getMapObjectType() != maptypes::AIRPORT ||
     route.first().getId() != parking.airportId)
  {
    // No route, no start airport or different airport
    maptypes::MapAirport ap;
    query->getAirportById(ap, parking.airportId);
    routeSetDepartureInternal(ap);
  }

  // Update the current airport which is new or the same as the one used by the parking spot
  route.getFlightplan().setDepartureParkingName(maptypes::parkingNameForFlightplan(parking));
  route.getFlightplan().setDeparturePosition(parking.position);
  route.first().setDepartureParking(parking);

  updateRouteMapObjects();
  routeToFlightPlan();
  updateTableModel();
  updateWindowLabel();

  postChange(undoCommand);
  mainWindow->updateWindowTitle();

  emit routeChanged(true);

  mainWindow->setStatusMessage(tr("Departure set to %1 parking %2.").arg(route.first().getIdent()).
                               arg(maptypes::parkingNameNumberType(parking)));
}

/* Set start position (runway, helipad) for departure */
void RouteController::routeSetStartPosition(maptypes::MapStart start)
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

  updateRouteMapObjects();
  routeToFlightPlan();
  updateTableModel();
  updateWindowLabel();

  postChange(undoCommand);
  mainWindow->updateWindowTitle();

  emit routeChanged(true);

  mainWindow->setStatusMessage(tr("Departure set to %1 start position %2.").arg(route.first().getIdent()).
                               arg(start.runwayName));
}

/* Add departure and add best runway start position */
void RouteController::routeSetDepartureInternal(const maptypes::MapAirport& airport)
{
  FlightplanEntry entry;
  buildFlightplanEntry(airport, entry);

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

  RouteMapObject rmo(&flightplan);
  rmo.createFromAirport(0, airport, nullptr);
  route.prepend(rmo);

  updateStartPositionBestRunway(true /* force */, false /* undo */);
}

void RouteController::routeSetDeparture(maptypes::MapAirport airport)
{
  qDebug() << "route set start id" << airport.id;

  RouteCommand *undoCommand = preChange(tr("Set Departure"));

  routeSetDepartureInternal(airport);

  updateRouteMapObjects();
  routeToFlightPlan();
  updateTableModel();
  updateWindowLabel();

  postChange(undoCommand);
  mainWindow->updateWindowTitle();

  emit routeChanged(true);
  mainWindow->setStatusMessage(tr("Departure set to %1.").arg(route.first().getIdent()));
}

void RouteController::routeSetDestination(maptypes::MapAirport airport)
{
  qDebug() << "route set dest id" << airport.id;

  RouteCommand *undoCommand = preChange(tr("Set Destination"));

  FlightplanEntry entry;
  buildFlightplanEntry(airport, entry);
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

  const RouteMapObject *rmoPred = nullptr;
  if(flightplan.getEntries().size() > 1)
    // Set predecessor if route has entries
    rmoPred = &route.at(route.size() - 1);

  RouteMapObject rmo(&flightplan);
  rmo.createFromAirport(flightplan.getEntries().size() - 1, airport, rmoPred);
  route.append(rmo);

  updateRouteMapObjects();
  updateStartPositionBestRunway(false /* force */, false /* undo */);
  routeToFlightPlan();
  updateTableModel();
  updateWindowLabel();

  postChange(undoCommand);
  mainWindow->updateWindowTitle();

  emit routeChanged(true);

  mainWindow->setStatusMessage(tr("Destination set to %1.").arg(airport.ident));
}

void RouteController::routeAdd(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type, int legIndex)
{
  qDebug() << "route add" << "user pos" << userPos << "id" << id
           << "type" << type << "leg index" << legIndex;

  RouteCommand *undoCommand = preChange(tr("Add Waypoint"));

  FlightplanEntry entry;
  buildFlightplanEntry(id, userPos, type, entry);
  Flightplan& flightplan = route.getFlightplan();

  int insertIndex = -1;
  if(legIndex != -1)
    // Use given leg index
    insertIndex = legIndex + 1;
  else
  {
    if(flightplan.isEmpty())
      // First is  departure
      insertIndex = 0;
    else if(flightplan.getEntries().size() == 1)
      // Keep first as departure
      insertIndex = 1;
    else
    {
      // No leg index given - search for nearest
      int legOrPt = route.getNearestLegOrPointIndex(entry.getPosition());
      qDebug() << "nearestLeg" << legOrPt;

      // Positive values are legs - negative are points
      insertIndex = atools::absInt(legOrPt);
      if(legOrPt == -1 /* First point - add before departure */)
        // Add at the beginning
        insertIndex = 0;
    }
  }
  flightplan.getEntries().insert(insertIndex, entry);
  eraseAirway(insertIndex);
  eraseAirway(insertIndex + 1);

  const RouteMapObject *rmoPred = nullptr;

  if(flightplan.isEmpty() && insertIndex > 0)
    rmoPred = &route.at(insertIndex - 1);
  RouteMapObject rmo(&flightplan);
  rmo.createFromDatabaseByEntry(insertIndex, query, rmoPred);

  route.insert(insertIndex, rmo);

  updateRouteMapObjects();
  // Force update of start if departure airport was added
  updateStartPositionBestRunway(false /* force */, false /* undo */);
  routeToFlightPlan();
  updateTableModel();
  updateWindowLabel();

  postChange(undoCommand);
  mainWindow->updateWindowTitle();

  emit routeChanged(true);

  mainWindow->setStatusMessage(tr("Added waypoint to flight plan."));
}

void RouteController::routeReplace(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type,
                                   int legIndex)
{
  qDebug() << "route replace" << "user pos" << userPos << "id" << id
           << "type" << type << "leg index" << legIndex;

  RouteCommand *undoCommand = preChange(tr("Change Waypoint"));

  FlightplanEntry entry;
  buildFlightplanEntry(id, userPos, type, entry);

  Flightplan& flightplan = route.getFlightplan();

  flightplan.getEntries().replace(legIndex, entry);

  const RouteMapObject *rmoPred = nullptr;
  if(legIndex > 0 && !route.isFlightplanEmpty())
    // Get predecessor of replaced entry
    rmoPred = &route.at(legIndex - 1);

  RouteMapObject rmo(&flightplan);
  rmo.createFromDatabaseByEntry(legIndex, query, rmoPred);

  route.replace(legIndex, rmo);
  eraseAirway(legIndex);
  eraseAirway(legIndex + 1);

  updateRouteMapObjects();

  // Force update of start if departure airport was changed
  updateStartPositionBestRunway(legIndex == 0 /* force */, false /* undo */);

  routeToFlightPlan();
  updateTableModel();
  updateWindowLabel();

  postChange(undoCommand);
  mainWindow->updateWindowTitle();

  emit routeChanged(true);
  mainWindow->setStatusMessage(tr("Replaced waypoint in flight plan."));
}

void RouteController::routeDelete(int index)
{
  qDebug() << "route delete routeIndex" << index;

  RouteCommand *undoCommand = preChange(tr("Delete"));

  route.getFlightplan().getEntries().removeAt(index);

  route.removeAt(index);
  eraseAirway(index);

  updateRouteMapObjects();
  // Force update of start if departure airport was removed
  updateStartPositionBestRunway(index == 0 /* force */, false /* undo */);
  routeToFlightPlan();
  updateTableModel();
  updateWindowLabel();

  postChange(undoCommand);
  mainWindow->updateWindowTitle();

  emit routeChanged(true);

  mainWindow->setStatusMessage(tr("Removed waypoint from flight plan."));
}

/* Update airway attribute in flight plan entry and return minimum altitude for this airway segment */
void RouteController::updateFlightplanEntryAirway(int airwayId, FlightplanEntry& entry, int& minAltitude)
{
  maptypes::MapAirway airway;
  query->getAirwayById(airway, airwayId);
  entry.setAirway(airway.name);
  minAltitude = airway.minAltitude;
}

/* Copy airport attributes to flight plan entry */
void RouteController::buildFlightplanEntry(const maptypes::MapAirport& airport, FlightplanEntry& entry)
{
  entry.setIcaoIdent(airport.ident);
  entry.setPosition(airport.position);
  entry.setWaypointType(entry::AIRPORT);
  entry.setWaypointId(entry.getIcaoIdent());
}

/* create a flight plan entry from object id/type or user position */
void RouteController::buildFlightplanEntry(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type,
                                           FlightplanEntry& entry, bool resolveWaypoints)
{
  maptypes::MapSearchResult result;
  query->getMapObjectById(result, type, id);

  if(type == maptypes::AIRPORT)
  {
    const maptypes::MapAirport& airport = result.airports.first();
    entry.setIcaoIdent(airport.ident);
    entry.setPosition(airport.position);
    entry.setWaypointType(entry::AIRPORT);
    entry.setWaypointId(entry.getIcaoIdent());
  }
  else if(type == maptypes::WAYPOINT)
  {
    const maptypes::MapWaypoint& waypoint = result.waypoints.first();
    bool useWaypoint = true;

    if(resolveWaypoints && waypoint.type == "VOR")
    {
      // Convert waypoint to underlying VOR for airway routes
      maptypes::MapVor vor;
      query->getVorForWaypoint(vor, waypoint.id);

      // Check for invalid references that are caused by the navdata update
      if(!vor.ident.isEmpty())
      {
        useWaypoint = false;
        entry.setIcaoIdent(vor.ident);
        entry.setPosition(vor.position);
        entry.setIcaoRegion(vor.region);
        entry.setWaypointType(entry::VOR);
        entry.setWaypointId(entry.getIcaoIdent());
      }
    }
    else if(resolveWaypoints && waypoint.type == "NDB")
    {
      // Convert waypoint to underlying NDB for airway routes
      maptypes::MapNdb ndb;
      query->getNdbForWaypoint(ndb, waypoint.id);

      // Check for invalid references that are caused by the navdata update
      if(!ndb.ident.isEmpty())
      {
        useWaypoint = false;
        entry.setIcaoIdent(ndb.ident);
        entry.setPosition(ndb.position);
        entry.setIcaoRegion(ndb.region);
        entry.setWaypointType(entry::NDB);
        entry.setWaypointId(entry.getIcaoIdent());
      }
    }

    if(useWaypoint)
    {
      entry.setIcaoIdent(waypoint.ident);
      entry.setPosition(waypoint.position);
      entry.setIcaoRegion(waypoint.region);
      entry.setWaypointType(entry::INTERSECTION);
      entry.setWaypointId(entry.getIcaoIdent());
    }
  }
  else if(type == maptypes::VOR)
  {
    const maptypes::MapVor& vor = result.vors.first();
    entry.setIcaoIdent(vor.ident);
    entry.setPosition(vor.position);
    entry.setIcaoRegion(vor.region);
    entry.setWaypointType(entry::VOR);
    entry.setWaypointId(entry.getIcaoIdent());
  }
  else if(type == maptypes::NDB)
  {
    const maptypes::MapNdb& ndb = result.ndbs.first();
    entry.setIcaoIdent(ndb.ident);
    entry.setPosition(ndb.position);
    entry.setIcaoRegion(ndb.region);
    entry.setWaypointType(entry::NDB);
    entry.setWaypointId(entry.getIcaoIdent());
  }
  else if(type == maptypes::USER)
  {
    entry.setPosition(userPos);
    entry.setWaypointType(entry::USER);
    entry.setIcaoIdent(QString());
    entry.setWaypointId("WP" + QString::number(curUserpointNumber));
  }
  else
    qWarning() << "Unknown Map object type" << type;
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

    const RouteMapObject& firstRmo = route.first();
    if(firstRmo.getMapObjectType() == maptypes::AIRPORT)
    {
      departureIcao = firstRmo.getAirport().ident;
      flightplan.setDepartureAiportName(firstRmo.getAirport().name);
      flightplan.setDepartureIdent(departureIcao);

      if(route.hasDepartureParking())
      {
        flightplan.setDepartureParkingName(maptypes::parkingNameForFlightplan(firstRmo.getDepartureParking()));
        flightplan.setDeparturePosition(firstRmo.getDepartureParking().position);
      }
      else if(route.hasDepartureStart())
      {
        if(route.hasDepartureHelipad())
          // Use helipad number
          flightplan.setDepartureParkingName(QString::number(firstRmo.getDepartureStart().helipadNumber));
        else
          // Use runway name
          flightplan.setDepartureParkingName(firstRmo.getDepartureStart().runwayName);
        flightplan.setDeparturePosition(firstRmo.getDepartureStart().position);
      }
      else
        // No start position and no parking - use airport/navaid position
        flightplan.setDeparturePosition(firstRmo.getPosition());
    }
    else
    {
      // Invalid departure
      flightplan.setDepartureAiportName(QString());
      flightplan.setDepartureIdent(QString());
      flightplan.setDepartureParkingName(QString());
      flightplan.setDeparturePosition(Pos());
    }

    const RouteMapObject& lastRmo = route.last();
    if(lastRmo.getMapObjectType() == maptypes::AIRPORT)
    {
      destinationIcao = lastRmo.getAirport().ident;
      flightplan.setDestinationAiportName(lastRmo.getAirport().name);
      flightplan.setDestinationIdent(destinationIcao);
      flightplan.setDestinationPosition(lastRmo.getPosition());
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

  // Get type and cruise altitude from widgets
  updateFlightplanFromWidgets();
}

/* Copy type and cruise altitude from widgets to flight plan */
void RouteController::updateFlightplanFromWidgets()
{
  Flightplan& flightplan = route.getFlightplan();
  Ui::MainWindow *ui = mainWindow->getUi();
  flightplan.setFlightplanType(
    ui->comboBoxRouteType->currentIndex() == 0 ? atools::fs::pln::IFR : atools::fs::pln::VFR);
  flightplan.setCruisingAltitude(ui->spinBoxRouteAlt->value());
}

/* Update distance, course, bounding rect and total distance for route map objects.
 *  Also calculates maximum number of user points. */
void RouteController::updateRouteMapObjects()
{
  float totalDistance = 0.f;

  // Used to number user waypoints
  curUserpointNumber = 0;

  RouteMapObject *last = nullptr;
  for(int i = 0; i < route.size(); i++)
  {
    RouteMapObject& mapobj = route[i];
    mapobj.updateDistanceAndCourse(i, last);
    curUserpointNumber = std::max(curUserpointNumber, mapobj.getUserpointNumber());
    totalDistance += mapobj.getDistanceTo();
    last = &mapobj;
  }
  route.setTotalDistance(totalDistance);

  // Next number for user point name
  curUserpointNumber++;

  updateBoundingRect();
}

/* Loads navaids from database and create all route map objects from flight plan.  */
void RouteController::createRouteMapObjects()
{
  route.clear();

  Flightplan& flightplan = route.getFlightplan();

  RouteMapObject *last = nullptr;
  float totalDistance = 0.f;

  // Used to number user waypoints
  curUserpointNumber = 0;

  // Create map objects first and calculate total distance
  for(int i = 0; i < flightplan.getEntries().size(); i++)
  {
    RouteMapObject mapobj(&flightplan);
    mapobj.createFromDatabaseByEntry(i, query, last);
    curUserpointNumber = std::max(curUserpointNumber, mapobj.getUserpointNumber());

    if(mapobj.getMapObjectType() == maptypes::INVALID)
      // Not found in database
      qWarning() << "Entry for ident" << flightplan.at(i).getIcaoIdent()
                 << "region" << flightplan.at(i).getIcaoRegion() << "is not valid";

    totalDistance += mapobj.getDistanceTo();

    route.append(mapobj);
    last = &route.last();
  }

  route.setTotalDistance(totalDistance);

  // Next number for user point name
  curUserpointNumber++;

  updateBoundingRect();
}

/* Update the bounding rect using marble functions to catch anti meridian overlap */
void RouteController::updateBoundingRect()
{
  GeoDataLineString line;

  for(const RouteMapObject& rmo : route)
    line.append(GeoDataCoordinates(rmo.getPosition().getLonX(),
                                   rmo.getPosition().getLatY(), 0., GeoDataCoordinates::Degree));

  GeoDataLatLonBox box = GeoDataLatLonBox::fromLineString(line);
  boundingRect = Rect(box.west(), box.north(), box.east(), box.south());
  boundingRect.toDeg();
}

/* Update travel times in table view model after speed change */
void RouteController::updateModelRouteTime()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  int row = 0;
  float cumulatedDistance = 0.f;
  for(const RouteMapObject& mapobj : route)
  {
    cumulatedDistance += mapobj.getDistanceTo();
    if(row == 0)
      model->setItem(row, rc::LEG_TIME, nullptr);
    else
    {
      float travelTime = mapobj.getDistanceTo() / static_cast<float>(ui->spinBoxRouteSpeed->value());
      model->setItem(row, rc::LEG_TIME, new QStandardItem(formatter::formatMinutesHours(travelTime)));
    }

    float eta = cumulatedDistance / static_cast<float>(ui->spinBoxRouteSpeed->value());
    model->setItem(row, rc::ETA, new QStandardItem(formatter::formatMinutesHours(eta)));
    row++;
  }
}

/* Update table view model completely */
void RouteController::updateTableModel()
{
  Ui::MainWindow *ui = mainWindow->getUi();

  model->removeRows(0, model->rowCount());
  float totalDistance = route.getTotalDistance();

  int row = 0;
  float cumulatedDistance = 0.f;

  QList<QStandardItem *> itemRow;
  for(const RouteMapObject& mapobj : route)
  {
    itemRow.clear();
    itemRow.append(new QStandardItem(mapobj.getIdent()));
    itemRow.append(new QStandardItem(mapobj.getRegion()));
    itemRow.append(new QStandardItem(mapobj.getName()));
    itemRow.append(new QStandardItem(mapobj.getAirway()));

    // VOR/NDB type
    if(mapobj.getMapObjectType() == maptypes::VOR)
    {
      QString type = mapobj.getVor().type.at(0);

      if(mapobj.getVor().dmeOnly)
        itemRow.append(new QStandardItem(tr("DME (%1)").arg(type)));
      else if(mapobj.getVor().hasDme)
        itemRow.append(new QStandardItem(tr("VORDME (%1)").arg(type)));
      else
        itemRow.append(new QStandardItem(tr("VOR (%1)").arg(type)));
    }
    else if(mapobj.getMapObjectType() == maptypes::NDB)
    {
      QString type = mapobj.getNdb().type == "COMPASS_POINT" ? tr("CP") : mapobj.getNdb().type;
      itemRow.append(new QStandardItem(tr("NDB (%1)").arg(type)));
    }
    else
      itemRow.append(nullptr);

    // VOR/NDB frequency
    QStandardItem *item;
    if(mapobj.getFrequency() > 0)
    {
      if(mapobj.getMapObjectType() == maptypes::VOR)
        item = new QStandardItem(QLocale().toString(mapobj.getFrequency() / 1000.f, 'f', 2));
      else if(mapobj.getMapObjectType() == maptypes::NDB)
        item = new QStandardItem(QLocale().toString(mapobj.getFrequency() / 100.f, 'f', 1));
      else
        item = new QStandardItem();
      item->setTextAlignment(Qt::AlignRight);
      itemRow.append(item);
    }
    else
      itemRow.append(nullptr);

    if(mapobj.getRange() > 0)
    {
      if(mapobj.getMapObjectType() == maptypes::VOR || mapobj.getMapObjectType() == maptypes::NDB)
        item = new QStandardItem(QLocale().toString(mapobj.getRange()));
      else
        item = new QStandardItem();
      item->setTextAlignment(Qt::AlignRight);
      itemRow.append(item);
    }
    else
      itemRow.append(nullptr);

    if(row == 0)
    {
      // No course and distance for departure airport
      itemRow.append(nullptr);
      itemRow.append(nullptr);
      itemRow.append(nullptr);
    }
    else
    {
      item = new QStandardItem(QLocale().toString(mapobj.getCourseTo(), 'f', 0));
      item->setTextAlignment(Qt::AlignRight);
      itemRow.append(item);

      item = new QStandardItem(QLocale().toString(mapobj.getCourseToRhumb(), 'f', 0));
      item->setTextAlignment(Qt::AlignRight);
      itemRow.append(item);

      item = new QStandardItem(QLocale().toString(mapobj.getDistanceTo(), 'f', 1));
      item->setTextAlignment(Qt::AlignRight);
      itemRow.append(item);
    }

    cumulatedDistance += mapobj.getDistanceTo();

    float remaining = totalDistance - cumulatedDistance;
    if(remaining < 0.f)
      remaining = 0.f;  // Catch the -0 case due to rounding errors
    item = new QStandardItem(QLocale().toString(remaining, 'f', 1));
    item->setTextAlignment(Qt::AlignRight);
    itemRow.append(item);

    // Travel time and ETA are updated in updateModelRouteTime
    itemRow.append(nullptr);
    itemRow.append(nullptr);

    model->appendRow(itemRow);
    row++;
  }

  updateModelRouteTime();

  Flightplan& flightplan = route.getFlightplan();

  if(!flightplan.isEmpty())
  {
    // Set spin box and block signals to avoid recursive call
    ui->spinBoxRouteAlt->blockSignals(true);
    ui->spinBoxRouteAlt->setValue(flightplan.getCruisingAltitude());
    ui->spinBoxRouteAlt->blockSignals(false);

    // Set combo box and block signals to avoid recursive call
    ui->comboBoxRouteType->blockSignals(true);
    if(flightplan.getFlightplanType() == atools::fs::pln::IFR)
      ui->comboBoxRouteType->setCurrentIndex(0);
    else if(flightplan.getFlightplanType() == atools::fs::pln::VFR)
      ui->comboBoxRouteType->setCurrentIndex(1);
    ui->comboBoxRouteType->blockSignals(false);
  }
}

/* Update the dock window top level label */
void RouteController::updateWindowLabel()
{
  mainWindow->getUi()->labelRouteInfo->setText(buildFlightplanLabel(true) + buildFlightplanLabel2());
}

QString RouteController::buildFlightplanLabel(bool html)
{
  const Flightplan& flightplan = route.getFlightplan();

  QString departure(tr("Invalid")), destination(tr("Invalid"));
  if(!flightplan.isEmpty())
  {
    if(hasValidDeparture())
    {
      departure = flightplan.getDepartureAiportName() +
                  " (" + flightplan.getDepartureIdent() + ")";

      if(route.first().getDepartureParking().position.isValid())
        departure += " " + maptypes::parkingNameNumberType(route.first().getDepartureParking());
      else if(route.first().getDepartureStart().position.isValid())
      {
        const maptypes::MapStart& start = route.first().getDepartureStart();
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

    if(flightplan.getEntries().last().getWaypointType() == entry::AIRPORT)
      destination = flightplan.getDestinationAiportName() +
                    " (" + flightplan.getDestinationIdent() + ")";
    else
      destination = QString("%1 (%2)").
                    arg(flightplan.getEntries().last().getIcaoIdent()).
                    arg(flightplan.getEntries().last().getWaypointTypeAsString());

    if(html)
      return tr("<b>%1</b> to <b>%2</b><br/>").
             arg(departure).
             arg(destination);
    else
      return tr("%1 to %2").
             arg(departure).
             arg(destination);
  }
  else
    return tr("No Flightplan loaded");
}

QString RouteController::buildFlightplanLabel2()
{
  const Flightplan& flightplan = route.getFlightplan();
  Ui::MainWindow *ui = mainWindow->getUi();
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

    float travelTime = totalDistance / static_cast<float>(ui->spinBoxRouteSpeed->value());

    return tr("%1 nm, %2, %3").
           arg(QLocale().toString(totalDistance, 'f', 0)).
           arg(formatter::formatMinutesHoursLong(travelTime)).
           arg(routeType);
  }
  else
    return QString();
}

/* Reset route and clear undo stack (new route) */
void RouteController::clearRoute()
{
  route.getFlightplan().clear();
  route.clear();
  route.setTotalDistance(0.f);
  routeFilename.clear();
  undoStack->clear();
  undoIndex = 0;
  undoIndexClean = 0;
}

/* Call this before doing any change to the flight plan that should be undoable */
RouteCommand *RouteController::preChange(const QString& text, rctype::RouteCmdType rcType)
{
  return new RouteCommand(this, route.getFlightplan(), text, rcType);
}

/* Call this after doing a change to the flight plan that should be undoable */
void RouteController::postChange(RouteCommand *undoCommand)
{
  undoCommand->setFlightplanAfter(route.getFlightplan());

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
    RouteMapObject& rmo = route.first();

    if(force || (!route.hasDepartureParking() && !route.hasDepartureStart()))
    {
      // Reset departure position to best runway
      maptypes::MapStart start;
      query->getBestStartPositionForAirport(start, rmo.getAirport().id);

      // Check if the airport has a start position - sone add-on airports don't
      if(start.position.isValid())
      {
        RouteCommand *undoCommand = nullptr;

        if(undo)
          undoCommand = preChange(tr("Set Start Position"));

        rmo.setDepartureStart(start);
        routeToFlightPlan();

        if(undo)
          postChange(undoCommand);
        return true;
      }
    }
  }
  return false;
}
