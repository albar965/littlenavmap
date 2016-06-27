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

#include <QClipboard>
#include <QFile>
#include <QStandardItemModel>
#include <QMessageBox>

const QList<QString> ROUTE_COLUMNS({QObject::tr("Ident"),
                                    QObject::tr("Region"),
                                    QObject::tr("Name"),
                                    QObject::tr("Airway"),
                                    QObject::tr("Type"),
                                    QObject::tr("Freq.\nMHz/kHz"),
                                    QObject::tr("Course\n°M"),
                                    QObject::tr("Direct\n°M"),
                                    QObject::tr("Distance\nnm"),
                                    QObject::tr("Remaining\nnm"),
                                    QObject::tr("Leg Time\nhh:mm"),
                                    QObject::tr("ETA\nhh:mm UTC")});

namespace rc {
enum RouteColumns
{
  FIRST_COL,
  IDENT = FIRST_COL,
  REGION,
  NAME,
  AIRWAY,
  TYPE,
  FREQ,
  COURSE,
  DIRECT,
  DIST,
  REMAINING,
  TIME,
  ETA,
  // GROUND_ALT,
  LAST_COL = ETA
};

}

using namespace atools::fs::pln;
using namespace atools::geo;

RouteController::RouteController(MainWindow *parentWindow, MapQuery *mapQuery, QTableView *tableView)
  : QObject(parentWindow), mainWindow(parentWindow), view(tableView), query(mapQuery)
{
  atools::gui::TableZoomHandler zoomHandler(view);
  Q_UNUSED(zoomHandler);

  view->setContextMenuPolicy(Qt::CustomContextMenu);

  routeNetworkRadio = new RouteNetworkRadio(query->getDatabase());
  routeNetworkAirway = new RouteNetworkAirway(query->getDatabase());

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
          this, &RouteController::updateLabel);
  connect(ui->spinBoxRouteSpeed, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &RouteController::updateModelRouteTime);
  connect(ui->spinBoxRouteAlt, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &RouteController::routeAltChanged);
  connect(ui->comboBoxRouteType, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
          this, &RouteController::routeTypeChanged);

  connect(view, &QTableView::doubleClicked, this, &RouteController::doubleClick);
  connect(view, &QTableView::customContextMenuRequested, this, &RouteController::tableContextMenu);

  view->horizontalHeader()->setSectionsMovable(true);
  view->verticalHeader()->setSectionsMovable(false);
  view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

  model = new QStandardItemModel();
  QItemSelectionModel *m = view->selectionModel();
  view->setModel(model);
  delete m;

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

  view->addActions({ui->actionRouteLegDown, ui->actionRouteLegUp, ui->actionRouteDeleteLeg,
                    ui->actionRouteTableCopy, ui->actionRouteShowInformation, ui->actionRouteShowOnMap});

  void (RouteController::*selChangedPtr)(const QItemSelection &selected, const QItemSelection &deselected) =
    &RouteController::tableSelectionChanged;
  connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this, selChangedPtr);

  connect(ui->actionRouteLegDown, &QAction::triggered, this, &RouteController::moveLegsDown);
  connect(ui->actionRouteLegUp, &QAction::triggered, this, &RouteController::moveLegsUp);
  connect(ui->actionRouteDeleteLeg, &QAction::triggered, this, &RouteController::deleteLegs);

  connect(ui->actionRouteShowInformation, &QAction::triggered, this, &RouteController::showInformationMenu);
  connect(ui->actionRouteShowOnMap, &QAction::triggered, this, &RouteController::showOnMapMenu);
}

RouteController::~RouteController()
{
  delete model;
  delete iconDelegate;
  delete undoStack;
  delete routeNetworkRadio;
  delete routeNetworkAirway;
}

void RouteController::undoTriggered()
{
  mainWindow->setStatusMessage(QString(tr("Undo flight plan change.")));
}

void RouteController::redoTriggered()
{
  mainWindow->setStatusMessage(QString(tr("Redo flight plan change.")));
}

void RouteController::tableCopyClipboard()
{
  qDebug() << "RouteController::tableCopyClipboard";

  QString csv;
  int exported = CsvExporter::selectionAsCsv(view, true, csv);

  if(!csv.isEmpty())
    QApplication::clipboard()->setText(csv);

  mainWindow->setStatusMessage(QString(tr("Copied %1 entries to clipboard.")).arg(exported));
}

void RouteController::routeAltChanged()
{
  RouteCommand *undoCommand = nullptr;

  if(!route.isEmpty())
    undoCommand = preChange(tr("Change Altitude"), rctype::ALTITUDE);

  updateFlightplanData();

  if(!route.isEmpty())
    postChange(undoCommand);

  updateWindowTitle();

  if(!route.isEmpty())
    emit routeChanged(false);
}

void RouteController::routeTypeChanged()
{
  RouteCommand *undoCommand = nullptr;

  if(!route.isEmpty())
    undoCommand = preChange(tr("Change Type"));

  updateFlightplanData();

  if(!route.isEmpty())
    postChange(undoCommand);

  updateWindowTitle();

  if(!route.isEmpty())
  {
    emit routeChanged(false);
    Ui::MainWindow *ui = mainWindow->getUi();
    mainWindow->setStatusMessage(tr("Flight plan type changed to %1.").arg(ui->comboBoxRouteType->currentText()));
  }
}

bool RouteController::selectDepartureParking()
{
  const maptypes::MapAirport& airport = route.first().getAirport();
  ParkingDialog dialog(mainWindow, query, airport);

  int result = dialog.exec();
  dialog.hide();

  if(result == QDialog::Accepted)
  {
    maptypes::MapParking parking;
    if(dialog.getSelectedParking(parking))
    {
      routeSetParking(parking);
      return true;
    }
  }
  return false;
}

void RouteController::saveState()
{
  Ui::MainWindow *ui = mainWindow->getUi();

  atools::gui::WidgetState saver(lnm::ROUTE_VIEW);
  saver.save({view, ui->spinBoxRouteSpeed, ui->comboBoxRouteType, ui->spinBoxRouteAlt});

  atools::settings::Settings::instance().setValue(lnm::ROUTE_FILENAME, routeFilename);
}

void RouteController::restoreState()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  atools::gui::WidgetState saver(lnm::ROUTE_VIEW);
  model->setHorizontalHeaderLabels(ROUTE_COLUMNS);
  saver.restore({view, ui->spinBoxRouteSpeed, ui->comboBoxRouteType, ui->spinBoxRouteAlt});

  if(OptionData::instance().getFlags() & opts::STARTUP_LOAD_ROUTE)
  {
    QString newRouteFilename = atools::settings::Settings::instance().valueStr(lnm::ROUTE_FILENAME);

    if(!newRouteFilename.isEmpty())
    {
      if(QFile::exists(newRouteFilename))
      {
        if(!loadFlightplan(newRouteFilename))
        {
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

void RouteController::getSelectedRouteMapObjects(QList<RouteMapObject>& selRouteMapObjects) const
{
  QItemSelection sm = view->selectionModel()->selection();
  for(const QItemSelectionRange& rng : sm)
    for(int row = rng.top(); row <= rng.bottom(); ++row)
      selRouteMapObjects.append(route.at(row));
}

void RouteController::newFlightplan()
{
  clearRoute();

  // Copy current alt and type from widgets to flightplan
  updateFlightplanFromWidgets();

  createRouteMapObjects();
  updateModel();
  updateWindowTitle();
  updateLabel();
  emit routeChanged(true);
}

bool RouteController::loadFlightplan(const QString& filename)
{
  Flightplan newFlightplan;
  try
  {
    newFlightplan.load(filename);

    clearRoute();
    routeFilename = filename;
    route.setFlightplan(newFlightplan);
    createRouteMapObjects();
    updateModel();
    updateWindowTitle();
    updateLabel();
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
  routeFilename = filename;
  return saveFlightplan();
}

bool RouteController::saveFlightplan()
{
  try
  {
    route.getFlightplan().save(routeFilename);
    undoIndexClean = undoIndex;
    undoStack->setClean();
    updateWindowTitle();
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
  RouteCommand *undoCommand = preChange(tr("Direct Flight Plan Calculation"));

  Flightplan& flightplan = route.getFlightplan();
  flightplan.setRouteType(atools::fs::pln::DIRECT);
  flightplan.getEntries().erase(flightplan.getEntries().begin() + 1, flightplan.getEntries().end() - 1);

  createRouteMapObjects();
  updateModel();
  updateLabel();
  postChange(undoCommand);
  updateWindowTitle();
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
                            false, false))
    mainWindow->setStatusMessage(tr("Calculated radio navaid flight plan."));
  else
    mainWindow->setStatusMessage(tr("No route found."));
}

void RouteController::calculateHighAlt()
{
  qDebug() << "calculateHighAlt";
  routeNetworkAirway->setMode(nw::ROUTE_JET);

  RouteFinder routeFinder(routeNetworkAirway);

  if(calculateRouteInternal(&routeFinder, atools::fs::pln::HIGH_ALT,
                            tr("High altitude Flight Plan Calculation"), true, false))
    mainWindow->setStatusMessage(tr("Calculated high altitude (Jet airways) flight plan."));
  else
    mainWindow->setStatusMessage(tr("No route found."));
}

void RouteController::calculateLowAlt()
{
  qDebug() << "calculateLowAlt";
  routeNetworkAirway->setMode(nw::ROUTE_VICTOR);

  RouteFinder routeFinder(routeNetworkAirway);

  if(calculateRouteInternal(&routeFinder, atools::fs::pln::LOW_ALT, tr("Low altitude Flight Plan Calculation"),
                            true, false))
    mainWindow->setStatusMessage(tr("Calculated low altitude (Victor airways) flight plan."));
  else
    mainWindow->setStatusMessage(tr("No route found."));
}

void RouteController::calculateSetAlt()
{
  qDebug() << "calculateSetAlt";
  routeNetworkAirway->setMode(nw::ROUTE_VICTOR | nw::ROUTE_JET);

  RouteFinder routeFinder(routeNetworkAirway);

  atools::fs::pln::RouteType type;
  if(route.getFlightplan().getCruisingAlt() > 20000)
    type = atools::fs::pln::HIGH_ALT;
  else
    type = atools::fs::pln::LOW_ALT;

  if(calculateRouteInternal(&routeFinder, type, tr("Low altitude flight plan"), true, true))
    mainWindow->setStatusMessage(tr("Calculated high/low flight plan for given altitude."));
  else
    mainWindow->setStatusMessage(tr("No route found."));
}

bool RouteController::calculateRouteInternal(RouteFinder *routeFinder, atools::fs::pln::RouteType type,
                                             const QString& commandName, bool fetchAirways,
                                             bool useSetAltitude)
{
  QGuiApplication::setOverrideCursor(Qt::WaitCursor);

  Flightplan& flightplan = route.getFlightplan();

  int altitude = useSetAltitude ? flightplan.getCruisingAlt() : 0;

  routeFinder->setPreferVorToAirway(OptionData::instance().getFlags() & opts::ROUTE_PREFER_VOR);
  routeFinder->setPreferNdbToAirway(OptionData::instance().getFlags() & opts::ROUTE_PREFER_NDB);

  bool found = routeFinder->calculateRoute(flightplan.getDeparturePos(),
                                           flightplan.getDestinationPos(), altitude);

  if(found)
  {
    float distance = 0.f;
    QVector<rf::RouteEntry> calculatedRoute;
    routeFinder->extractRoute(calculatedRoute, distance);

    float directDistance = flightplan.getDeparturePos().distanceMeterTo(flightplan.getDestinationPos());
    float ratio = distance / directDistance;
    qDebug() << "route distance" << QString::number(distance, 'f', 0)
             << "direct distance" << QString::number(directDistance, 'f', 0) << "ratio" << ratio;

    if(ratio < MAX_DISTANCE_DIRECT_RATIO)
    {
      RouteCommand *undoCommand = preChange(commandName);

      QList<FlightplanEntry>& entries = flightplan.getEntries();

      flightplan.setRouteType(type);
      // Erase all but start and destination
      entries.erase(flightplan.getEntries().begin() + 1, entries.end() - 1);

      int minAltitude = 0;
      for(const rf::RouteEntry& routeEntry : calculatedRoute)
      {
        // qDebug() << "Route id" << routeEntry.ref.id << "type" << routeEntry.ref.type;
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
          float fpDir = flightplan.getDeparturePos().angleDegToRhumb(flightplan.getDestinationPos());

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

        flightplan.setCruisingAlt(minAltitude);
      }

      QGuiApplication::restoreOverrideCursor();
      createRouteMapObjects();
      updateModel();
      updateLabel();
      postChange(undoCommand);
      updateWindowTitle();
      emit routeChanged(true);
    }
    else
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

  RouteCommand *undoCommand = preChange(tr("Reverse Flight Plan"), rctype::REVERSE);

  route.getFlightplan().reverse();

  createRouteMapObjects();
  updateModel();
  updateLabel();
  postChange(undoCommand);
  updateWindowTitle();
  emit routeChanged(true);
  mainWindow->setStatusMessage(tr("Reversed flight plan."));
}

QString RouteController::getDefaultFilename() const
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
  return route.getFlightplan().isEmpty();
}

bool RouteController::hasValidStart() const
{
  return !route.getFlightplan().isEmpty() &&
         route.getFlightplan().getEntries().first().getWaypointType() ==
         atools::fs::pln::entry::AIRPORT;
}

bool RouteController::hasValidDestination() const
{
  return !route.getFlightplan().isEmpty() &&
         route.getFlightplan().getEntries().last().getWaypointType() ==
         atools::fs::pln::entry::AIRPORT;
}

bool RouteController::hasValidParking() const
{
  if(hasValidStart())
  {
    const QList<maptypes::MapParking> *parkingCache = query->getParkingsForAirport(route.first().getId());

    int numParking = 0;
    for(const maptypes::MapParking& p : *parkingCache)
      if(p.type != "FUEL")
        numParking++;

    if(numParking > 0)
      return !route.getFlightplan().getDepartureParkingName().isEmpty();
    else
      // No parking available - so no parking selection is ok
      return true;
  }
  else
    return false;
}

bool RouteController::hasEntries() const
{
  return route.getFlightplan().getEntries().size() > 2;
}

bool RouteController::hasRoute() const
{
  return !route.getFlightplan().isEmpty();
}

void RouteController::preDatabaseLoad()
{
  routeNetworkRadio->clear();
  routeNetworkRadio->deInitQueries();
  routeNetworkAirway->clear();
  routeNetworkAirway->deInitQueries();
}

void RouteController::postDatabaseLoad()
{
  routeNetworkRadio->initQueries();
  routeNetworkAirway->initQueries();
  createRouteMapObjects();
  updateModel();
  updateWindowTitle();
  updateLabel();
}

void RouteController::doubleClick(const QModelIndex& index)
{
  if(index.isValid())
  {
    qDebug() << "mouseDoubleClickEvent";

    const RouteMapObject& mo = route.at(index.row());

    if(mo.getMapObjectType() == maptypes::AIRPORT && mo.getAirport().bounding.isPoint())
      emit showRect(mo.getAirport().bounding);
    else
      emit showPos(mo.getPosition(), 2700);

    maptypes::MapSearchResult result;
    query->getMapObjectById(result, mo.getMapObjectType(), mo.getId());
    emit showInformation(result);
  }
}

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
      ui->actionRouteLegUp->setEnabled(sm->hasSelection() &&
                                       !sm->isRowSelected(0, QModelIndex()));

      ui->actionRouteLegDown->setEnabled(sm->hasSelection() &&
                                         !sm->isRowSelected(model->rowCount() - 1, QModelIndex()));
    }
    else if(model->rowCount() == 1)
      // Only one waypoint - nothing to move
      ui->actionRouteDeleteLeg->setEnabled(true);
  }
}

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

void RouteController::showOnMapMenu()
{
  QModelIndex index = view->currentIndex();
  if(index.isValid())
  {
    const RouteMapObject& routeMapObject = route.at(index.row());
    if(routeMapObject.getMapObjectType() == maptypes::AIRPORT &&
       !routeMapObject.getAirport().bounding.isPoint())
      emit showRect(routeMapObject.getAirport().bounding);
    else
      emit showPos(routeMapObject.getPosition(), 2700);

    if(routeMapObject.getMapObjectType() == maptypes::AIRPORT)
      mainWindow->setStatusMessage(tr("Showing airport on map."));
    else
      mainWindow->setStatusMessage(tr("Showing navaid on map."));
  }
}

void RouteController::tableContextMenu(const QPoint& pos)
{
  qDebug() << "tableContextMenu";

  Ui::MainWindow *ui = mainWindow->getUi();

  atools::gui::ActionTextSaver saver({ui->actionMapNavaidRange});
  Q_UNUSED(saver);

  QModelIndex index = view->indexAt(pos);
  if(index.isValid())
  {
    const RouteMapObject& routeMapObject = route.at(index.row());

    updateMoveAndDeleteActions();

    ui->actionSearchTableCopy->setEnabled(index.isValid());

    ui->actionMapRangeRings->setEnabled(true);
    ui->actionMapHideRangeRings->setEnabled(!mainWindow->getMapWidget()->getDistanceMarkers().isEmpty() ||
                                            !mainWindow->getMapWidget()->getRangeRings().isEmpty());

    ui->actionRouteShowInformation->setEnabled(true);

    ui->actionRouteShowOnMap->setEnabled(true);

    ui->actionMapNavaidRange->setEnabled(false);
    ui->actionMapNavaidRange->setText(tr("Show Navaid Range"));

    QList<RouteMapObject> selectedRouteMapObjects;
    getSelectedRouteMapObjects(selectedRouteMapObjects);
    for(const RouteMapObject& rmo : selectedRouteMapObjects)
      if(rmo.getMapObjectType() == maptypes::VOR || rmo.getMapObjectType() == maptypes::NDB)
      {
        ui->actionMapNavaidRange->setEnabled(true);
        break;
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

    QAction *action = menu.exec(QCursor::pos());
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

        for(const RouteMapObject& rmo : selectedRouteMapObjects)
        {
          if(rmo.getMapObjectType() == maptypes::VOR || rmo.getMapObjectType() == maptypes::NDB)
            mainWindow->getMapWidget()->addNavRangeRing(rmo.getPosition(), rmo.getMapObjectType(),
                                                        rmo.getIdent(), rmo.getFrequency(),
                                                        rmo.getRange());
        }

      }
      else if(action == ui->actionMapHideRangeRings)
        mainWindow->getMapWidget()->clearRangeRingsAndDistanceMarkers();
    }
  }
}

void RouteController::tableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
  Q_UNUSED(selected);
  Q_UNUSED(deselected);

  updateMoveAndDeleteActions();
  tableSelectionChanged();
}

void RouteController::tableSelectionChanged()
{
  QItemSelectionModel *sm = view->selectionModel();

  int selectedRows = 0;
  if(sm != nullptr && sm->hasSelection())
    selectedRows = sm->selectedRows().size();

  emit routeSelectionChanged(selectedRows, model->rowCount());
}

void RouteController::changeRouteUndo(const atools::fs::pln::Flightplan& newFlightplan)
{
  qDebug() << "changeRouteUndo";
  undoIndex--;

  changeRouteUndoRedo(newFlightplan);
}

void RouteController::undoMerge()
{
  qDebug() << "undoMerge";
  undoIndex--;
}

void RouteController::changeRouteRedo(const atools::fs::pln::Flightplan& newFlightplan)
{
  qDebug() << "changeRouteRedo";
  undoIndex++;
  changeRouteUndoRedo(newFlightplan);
}

void RouteController::changeRouteUndoRedo(const atools::fs::pln::Flightplan& newFlightplan)
{
  route.setFlightplan(newFlightplan);

  createRouteMapObjects();
  updateModel();
  updateWindowTitle();
  updateLabel();
  updateMoveAndDeleteActions();
  emit routeChanged(true);
}

bool RouteController::hasChanged() const
{
  return undoIndexClean != undoIndex;
  // return changed;
}

void RouteController::moveLegsDown()
{
  qDebug() << "Leg down";
  moveLegsInternal(1);
}

void RouteController::moveLegsUp()
{
  qDebug() << "Leg up";
  moveLegsInternal(-1);
}

void RouteController::moveLegsInternal(int dir)
{
  QList<int> rows;
  selectedRows(rows, dir > 0);

  if(!rows.isEmpty())
  {
    RouteCommand *undoCommand = preChange(tr("Move Waypoints"), rctype::MOVE);

    QModelIndex curIdx = view->currentIndex();
    view->selectionModel()->clear();
    for(int row : rows)
    {
      route.getFlightplan().getEntries().move(row, row + dir);
      route.move(row, row + dir);
      model->insertRow(row + dir, model->takeRow(row));
    }
    updateRouteMapObjects();
    updateFlightplanData();
    updateModel();
    updateLabel();
    view->setCurrentIndex(model->index(curIdx.row() + dir, curIdx.column()));
    select(rows, dir);
    updateMoveAndDeleteActions();

    postChange(undoCommand);
    updateWindowTitle();

    emit routeChanged(true);
    mainWindow->setStatusMessage(tr("Moved flight plan legs."));
  }
}

void RouteController::routeDelete(int routeIndex, maptypes::MapObjectTypes type)
{
  qDebug() << "route delete routeIndex" << routeIndex << "type" << type;

  RouteCommand *undoCommand = preChange(tr("Delete"));

  route.getFlightplan().getEntries().removeAt(routeIndex);

  route.removeAt(routeIndex);

  updateRouteMapObjects();
  updateFlightplanData();
  updateModel();
  updateLabel();

  postChange(undoCommand);
  updateWindowTitle();

  emit routeChanged(true);

  mainWindow->setStatusMessage(tr("Removed waypoint from flight plan."));
}

void RouteController::deleteLegs()
{
  qDebug() << "Leg delete";
  QList<int> rows;
  selectedRows(rows, true);

  if(!rows.isEmpty())
  {
    RouteCommand *undoCommand = preChange(tr("Delete Waypoints"), rctype::DELETE);

    int firstRow = rows.last();
    view->selectionModel()->clear();
    for(int row : rows)
    {
      route.getFlightplan().getEntries().removeAt(row);
      route.removeAt(row);
      model->removeRow(row);
    }
    updateRouteMapObjects();
    updateFlightplanData();
    updateModel();
    updateLabel();

    view->setCurrentIndex(model->index(firstRow, 0));
    updateMoveAndDeleteActions();

    postChange(undoCommand);
    updateWindowTitle();

    emit routeChanged(true);
    mainWindow->setStatusMessage(tr("Removed flight plan legs."));
  }
}

void RouteController::selectedRows(QList<int>& rows, bool reverse)
{
  QItemSelection sm = view->selectionModel()->selection();
  for(const QItemSelectionRange& rng : sm)
    for(int row = rng.top(); row <= rng.bottom(); row++)
      rows.append(row);

  if(!rows.isEmpty())
  {
    // Remove from bottom to top - otherwise model creates a mess
    std::sort(rows.begin(), rows.end());
    if(reverse)
      std::reverse(rows.begin(), rows.end());
  }
}

void RouteController::select(QList<int>& rows, int offset)
{
  QItemSelection newSel;

  for(int row : rows)
    newSel.append(QItemSelectionRange(model->index(row + offset, rc::FIRST_COL),
                                      model->index(row + offset, rc::LAST_COL)));

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
    routeSetStartInternal(ap);
  }

  // Update the current airport which is new or the same as the one used by the parking spot
  route.getFlightplan().setDepartureParkingName(maptypes::parkingNameForFlightplan(parking));
  route.first().updateParking(parking);

  updateRouteMapObjects();
  updateFlightplanData();
  updateModel();
  updateLabel();

  postChange(undoCommand);
  updateWindowTitle();

  emit routeChanged(true);

  mainWindow->setStatusMessage(tr("Departure set to %1 parking %2.").arg(route.first().getIdent()).
                               arg(maptypes::parkingNameNumberType(parking)));
}

void RouteController::routeSetStartInternal(const maptypes::MapAirport& airport)
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

  RouteMapObject rmo(&flightplan, mainWindow->getElevationModel());
  rmo.loadFromAirport(&flightplan.getEntries().first(), airport, nullptr);
  route.prepend(rmo);
}

void RouteController::routeSetDest(maptypes::MapAirport airport)
{
  qDebug() << "route set dest id" << airport.id;

  RouteCommand *undoCommand = preChange(tr("Set Destination"));

  FlightplanEntry entry;
  buildFlightplanEntry(airport, entry);
  Flightplan& flightplan = route.getFlightplan();

  if(!flightplan.isEmpty())
  {
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
    rmoPred = &route.at(route.size() - 1);

  RouteMapObject rmo(&flightplan, mainWindow->getElevationModel());
  rmo.loadFromAirport(&flightplan.getEntries().last(), airport, rmoPred);
  route.append(rmo);

  updateRouteMapObjects();
  updateFlightplanData();
  updateModel();
  updateLabel();

  postChange(undoCommand);
  updateWindowTitle();

  emit routeChanged(true);

  mainWindow->setStatusMessage(tr("Destination set to %1.").arg(airport.ident));
}

void RouteController::routeSetStart(maptypes::MapAirport airport)
{
  qDebug() << "route set start id" << airport.id;

  RouteCommand *undoCommand = preChange(tr("Set Departure"));

  routeSetStartInternal(airport);

  // Reset parking
  route.getFlightplan().setDepartureParkingName(QString());
  route.first().updateParking(maptypes::MapParking());

  updateRouteMapObjects();
  updateFlightplanData();
  updateModel();
  updateLabel();

  postChange(undoCommand);
  updateWindowTitle();

  emit routeChanged(true);
  mainWindow->setStatusMessage(tr("Departure set to %1.").arg(route.first().getIdent()));
}

void RouteController::routeReplace(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type,
                                   int legIndex)
{
  qDebug() << "route replace" << "user pos" << userPos << "id" << id
           << "type" << type << "old index" << legIndex;

  RouteCommand *undoCommand = preChange(tr("Change Waypoint"));

  FlightplanEntry entry;
  buildFlightplanEntry(id, userPos, type, entry);

  Flightplan& flightplan = route.getFlightplan();

  flightplan.getEntries().replace(legIndex, entry);

  const RouteMapObject *rmoPred = nullptr;

  RouteMapObject rmo(&flightplan, mainWindow->getElevationModel());
  rmo.loadFromDatabaseByEntry(&flightplan.getEntries()[legIndex], query, rmoPred);

  route.replace(legIndex, rmo);

  updateRouteMapObjects();
  updateFlightplanData();
  updateModel();
  updateLabel();

  postChange(undoCommand);
  updateWindowTitle();

  emit routeChanged(true);
  mainWindow->setStatusMessage(tr("Replaced waypoint in flight plan."));
}

void RouteController::routeAdd(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type, int legIndex)
{
  qDebug() << "route add id" << id << "type" << type;

  RouteCommand *undoCommand = preChange(tr("Add Waypoint"));

  FlightplanEntry entry;
  buildFlightplanEntry(id, userPos, type, entry);
  Flightplan& flightplan = route.getFlightplan();

  int insertIndex = -1;
  if(legIndex != -1)
    insertIndex = legIndex + 1;
  else
  {
    int leg = route.getNearestLegOrPointIndex(entry.getPosition());
    qDebug() << "nearestLeg" << leg;

    insertIndex = leg;
    if(flightplan.isEmpty() || insertIndex == -1)
      insertIndex = 0;
  }
  flightplan.getEntries().insert(insertIndex, entry);

  const RouteMapObject *rmoPred = nullptr;

  if(flightplan.isEmpty() && insertIndex > 0)
    rmoPred = &route.at(insertIndex - 1);
  RouteMapObject rmo(&flightplan, mainWindow->getElevationModel());
  rmo.loadFromDatabaseByEntry(&flightplan.getEntries()[insertIndex], query, rmoPred);

  route.insert(insertIndex, rmo);

  updateRouteMapObjects();
  updateFlightplanData();
  updateModel();
  updateLabel();

  postChange(undoCommand);
  updateWindowTitle();

  emit routeChanged(true);

  mainWindow->setStatusMessage(tr("Added waypoint to flight plan."));
}

void RouteController::buildFlightplanEntry(const maptypes::MapAirport& airport, FlightplanEntry& entry)
{
  entry.setIcaoIdent(airport.ident);
  entry.setPosition(airport.position);
  entry.setWaypointType(entry::AIRPORT);
  entry.setWaypointId(entry.getIcaoIdent());
}

void RouteController::updateFlightplanEntryAirway(int airwayId, FlightplanEntry& entry, int& minAltitude)
{
  maptypes::MapAirway airway;
  query->getAirwayById(airway, airwayId);
  entry.setAirway(airway.name);
  minAltitude = airway.minalt;

  // qDebug() << "airway id" << airwayId << "name" << airway.name << "alt" << airway.minalt
  // << "type" << airway.type;
}

void RouteController::buildFlightplanEntry(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type,
                                           FlightplanEntry& entry, bool resolveWaypoints)
{
  maptypes::MapSearchResult result;
  query->getMapObjectById(result, type, id);

  if(type == maptypes::AIRPORT)
  {
    const maptypes::MapAirport& ap = result.airports.first();
    entry.setIcaoIdent(ap.ident);
    entry.setPosition(ap.position);
    entry.setWaypointType(entry::AIRPORT);
    entry.setWaypointId(entry.getIcaoIdent());
  }
  else if(type == maptypes::PARKING)
  {
    // TODO is this ever called with parking
  }
  else if(type == maptypes::WAYPOINT)
  {
    const maptypes::MapWaypoint& wp = result.waypoints.first();

    if(resolveWaypoints && wp.type == "VOR")
    {
      // Convert waypoint to underlying VOR for airway routes
      maptypes::MapVor vor;
      query->getVorForWaypoint(vor, wp.id);

      entry.setIcaoIdent(vor.ident);
      entry.setPosition(vor.position);
      entry.setIcaoRegion(vor.region);
      entry.setWaypointType(entry::VOR);
      entry.setWaypointId(entry.getIcaoIdent());
    }
    else if(resolveWaypoints && wp.type == "NDB")
    {
      // Convert waypoint to underlying NDB for airway routes
      maptypes::MapNdb ndb;
      query->getNdbForWaypoint(ndb, wp.id);

      entry.setIcaoIdent(ndb.ident);
      entry.setPosition(ndb.position);
      entry.setIcaoRegion(ndb.region);
      entry.setWaypointType(entry::NDB);
      entry.setWaypointId(entry.getIcaoIdent());
    }
    else
    {
      entry.setIcaoIdent(wp.ident);
      entry.setPosition(wp.position);
      entry.setIcaoRegion(wp.region);
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

void RouteController::updateFlightplanData()
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

      if(!firstRmo.getParking().name.isEmpty())
        flightplan.setDepartureParkingName(maptypes::parkingNameForFlightplan(firstRmo.getParking()));
      flightplan.setDeparturePos(firstRmo.getPosition());
    }
    else
    {
      flightplan.setDepartureAiportName(QString());
      flightplan.setDepartureIdent(QString());
      flightplan.setDepartureParkingName(QString());
      flightplan.setDeparturePos(Pos());
    }

    const RouteMapObject& lastRmo = route.last();
    if(lastRmo.getMapObjectType() == maptypes::AIRPORT)
    {
      destinationIcao = lastRmo.getAirport().ident;
      flightplan.setDestinationAiportName(lastRmo.getAirport().name);
      flightplan.setDestinationIdent(destinationIcao);
      flightplan.setDestinationPos(lastRmo.getPosition());
    }
    else
    {
      flightplan.setDestinationAiportName(QString());
      flightplan.setDestinationIdent(QString());
      flightplan.setDestinationPos(Pos());
    }

    // <Descr>LFHO, EDRJ</Descr>
    flightplan.setDescription(departureIcao + ", " + destinationIcao);

    // <Title>LFHO to EDRJ</Title>
    flightplan.setTitle(departureIcao + " to " + destinationIcao);
  }
  updateFlightplanFromWidgets();
}

void RouteController::updateFlightplanFromWidgets()
{
  Flightplan& flightplan = route.getFlightplan();
  Ui::MainWindow *ui = mainWindow->getUi();
  // <FPType>IFR</FPType>
  // flightplan.setRouteType(atools::fs::pln::VOR);
  // <RouteType>LowAlt</RouteType>
  flightplan.setFlightplanType(
    ui->comboBoxRouteType->currentIndex() == 0 ? atools::fs::pln::IFR : atools::fs::pln::VFR);
  // <CruisingAlt>19000</CruisingAlt>
  flightplan.setCruisingAlt(ui->spinBoxRouteAlt->value());
}

void RouteController::updateRouteMapObjects()
{
  float totalDistance = 0.f;
  curUserpointNumber = 0;
  // Used to number user waypoints
  RouteMapObject *last = nullptr;
  for(int i = 0; i < route.size(); i++)
  {
    RouteMapObject& mapobj = route[i];
    mapobj.update(last);
    curUserpointNumber = std::max(curUserpointNumber, mapobj.getUserpointNumber());
    totalDistance += mapobj.getDistanceTo();

    if(i == 0)
      boundingRect = Rect(mapobj.getPosition());
    else
      boundingRect.extend(mapobj.getPosition());

    last = &mapobj;
  }
  route.setTotalDistance(totalDistance);

  curUserpointNumber++;
}

void RouteController::createRouteMapObjects()
{
  route.clear();

  Flightplan& flightplan = route.getFlightplan();

  RouteMapObject *last = nullptr;
  float totalDistance = 0.f;
  curUserpointNumber = 0;

  // Create map objects first and calculate total distance
  for(int i = 0; i < flightplan.getEntries().size(); i++)
  {
    FlightplanEntry& entry = flightplan.getEntries()[i];

    RouteMapObject mapobj(&flightplan, mainWindow->getElevationModel());
    mapobj.loadFromDatabaseByEntry(&entry, query, last);
    curUserpointNumber = std::max(curUserpointNumber, mapobj.getUserpointNumber());

    if(mapobj.getMapObjectType() == maptypes::INVALID)
      qWarning() << "Entry for ident" << entry.getIcaoIdent() <<
      "region" << entry.getIcaoRegion() << "is not valid";

    totalDistance += mapobj.getDistanceTo();
    if(i == 0)
      boundingRect = Rect(mapobj.getPosition());
    else
      boundingRect.extend(mapobj.getPosition());

    route.append(mapobj);
    last = &route.last();
  }

  route.setTotalDistance(totalDistance);
  curUserpointNumber++;
}

void RouteController::updateModelRouteTime()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  int row = 0;
  float cumulatedDistance = 0.f;
  for(const RouteMapObject& mapobj : route)
  {
    cumulatedDistance += mapobj.getDistanceTo();
    if(row == 0)
      model->setItem(row, rc::TIME, nullptr);
    else
    {
      float travelTime = mapobj.getDistanceTo() / static_cast<float>(ui->spinBoxRouteSpeed->value());
      model->setItem(row, rc::TIME, new QStandardItem(formatter::formatMinutesHours(travelTime)));
    }

    float eta = cumulatedDistance / static_cast<float>(ui->spinBoxRouteSpeed->value());
    model->setItem(row, rc::ETA, new QStandardItem(formatter::formatMinutesHours(eta)));
    row++;
  }
}

void RouteController::updateModel()
{
  Ui::MainWindow *ui = mainWindow->getUi();

  model->removeRows(0, model->rowCount());
  float totalDistance = route.getTotalDistance();

  int row = 0;
  float cumulatedDistance = 0.f;
  QList<QStandardItem *> items;
  for(const RouteMapObject& mapobj : route)
  {
    items.clear();
    items.append(new QStandardItem(mapobj.getIdent()));
    items.append(new QStandardItem(mapobj.getRegion()));
    items.append(new QStandardItem(mapobj.getName()));
    items.append(new QStandardItem(mapobj.getFlightplanEntry()->getAirway()));

    if(mapobj.getMapObjectType() == maptypes::VOR)
    {
      QString type = mapobj.getVor().type.at(0);

      if(mapobj.getVor().dmeOnly)
        items.append(new QStandardItem(tr("DME (%1)").arg(type)));
      else if(mapobj.getVor().hasDme)
        items.append(new QStandardItem(tr("VORDME (%1)").arg(type)));
      else
        items.append(new QStandardItem(tr("VOR (%1)").arg(type)));
    }
    else if(mapobj.getMapObjectType() == maptypes::NDB)
    {
      QString type = mapobj.getNdb().type == tr("COMPASS_POINT") ? tr("CP") : mapobj.getNdb().type;
      items.append(new QStandardItem(tr("NDB (%1)").arg(type)));
    }
    else
      items.append(nullptr);

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
      items.append(item);
    }
    else
      items.append(nullptr);

    if(row == 0)
    {
      items.append(nullptr);
      items.append(nullptr);
      items.append(nullptr);
    }
    else
    {
      item = new QStandardItem(QLocale().toString(mapobj.getCourseTo(), 'f', 0));
      item->setTextAlignment(Qt::AlignRight);
      items.append(item);

      item = new QStandardItem(QLocale().toString(mapobj.getCourseToRhumb(), 'f', 0));
      item->setTextAlignment(Qt::AlignRight);
      items.append(item);

      item = new QStandardItem(QLocale().toString(mapobj.getDistanceTo(), 'f', 1));
      item->setTextAlignment(Qt::AlignRight);
      items.append(item);
    }

    cumulatedDistance += mapobj.getDistanceTo();

    float remaining = totalDistance - cumulatedDistance;
    if(remaining < 0.f)
      remaining = 0.f;  // Catch the -0 case due to rounding errors
    item = new QStandardItem(QLocale().toString(remaining, 'f', 1));
    item->setTextAlignment(Qt::AlignRight);
    items.append(item);

    // Travel time and ETA - updated in updateModelRouteTime
    items.append(nullptr);
    items.append(nullptr);

    model->appendRow(items);
    row++;
  }

  updateModelRouteTime();

  Flightplan& flightplan = route.getFlightplan();

  ui->spinBoxRouteAlt->blockSignals(true);
  ui->spinBoxRouteAlt->setValue(flightplan.getCruisingAlt());
  ui->spinBoxRouteAlt->blockSignals(false);

  ui->comboBoxRouteType->blockSignals(true);
  if(flightplan.getFlightplanType() == atools::fs::pln::IFR)
    ui->comboBoxRouteType->setCurrentIndex(0);
  else if(flightplan.getFlightplanType() == atools::fs::pln::VFR)
    ui->comboBoxRouteType->setCurrentIndex(1);
  ui->comboBoxRouteType->blockSignals(false);
}

void RouteController::updateLabel()
{
  const Flightplan& flightplan = route.getFlightplan();

  Ui::MainWindow *ui = mainWindow->getUi();
  QString startAirport(tr("No airport")), destAirport(tr("No airport"));
  if(!flightplan.isEmpty())
  {
    if(flightplan.getEntries().first().getWaypointType() == entry::AIRPORT)
    {
      startAirport = flightplan.getDepartureAiportName() +
                     " (" + flightplan.getDepartureIdent() + ")";
      if(!flightplan.getDepartureParkingName().isEmpty())
      {
        QString park = flightplan.getDepartureParkingName().toLower();
        park[0] = park.at(0).toUpper();
        startAirport += " " + park;
      }
    }

    if(flightplan.getEntries().last().getWaypointType() == entry::AIRPORT)
      destAirport = flightplan.getDestinationAiportName() +
                    " (" + flightplan.getDestinationIdent() + ")";

    QString routeType;
    switch(flightplan.getRouteType())
    {
      case atools::fs::pln::UNKNOWN_ROUTE:
        break;
      case atools::fs::pln::LOW_ALT:
        routeType = tr(", Low Altitude");
        break;
      case atools::fs::pln::HIGH_ALT:
        routeType = tr(", High Altitude");
        break;
      case atools::fs::pln::VOR:
        routeType = tr(", Radionav");
        break;
      case atools::fs::pln::DIRECT:
        routeType = tr(", Direct");
        break;

    }
    float totalDistance = route.getTotalDistance();

    float travelTime = totalDistance / static_cast<float>(ui->spinBoxRouteSpeed->value());
    ui->labelRouteInfo->setText("<b>" + startAirport + "</b> to <b>" + destAirport + "</b>, " +
                                QLocale().toString(totalDistance, 'f', 0) + " nm, " +
                                formatter::formatMinutesHoursLong(travelTime) +
                                routeType);
  }
  else
    ui->labelRouteInfo->setText(tr("No Flightplan loaded"));
}

void RouteController::updateWindowTitle()
{
  mainWindow->updateWindowTitle();
}

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

RouteCommand *RouteController::preChange(const QString& text, rctype::RouteCmdType rcType)
{
  return new RouteCommand(this, route.getFlightplan(), text, rcType);
}

void RouteController::postChange(RouteCommand *undoCommand)
{
  undoCommand->setFlightplanAfter(route.getFlightplan());

  undoIndex++;
  undoStack->push(undoCommand);
}
