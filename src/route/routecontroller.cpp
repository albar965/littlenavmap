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
#include "fs/pln/flightplan.h"
#include "common/formatter.h"
#include "geo/calculations.h"
#include "gui/mainwindow.h"
#include "mapgui/navmapwidget.h"
#include <QStandardItemModel>
#include <QTableView>
#include <QHeaderView>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <gui/tablezoomhandler.h>
#include <gui/widgetstate.h>
#include <mapgui/mapquery.h>
#include <QSpinBox>
#include "common/mapcolors.h"
#include "parkingdialog.h"
#include "routeicondelegate.h"
#include "ui_mainwindow.h"
#include <settings/settings.h>
#include <gui/actiontextsaver.h>
#include <QListWidget>
#include <QVector2D>

// TODO tr
const QList<QString> ROUTE_COLUMNS({"Ident", "Region", "Name", "Airway", "Type", "Freq.",
                                    "Course\n°M", "Direct\n°M",
                                    "Distance\nnm", "Remaining\nnm"});
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
  LAST_COL = REMAINING
};

}

using namespace atools::fs::pln;
using namespace atools::geo;

RouteController::RouteController(MainWindow *parent, MapQuery *mapQuery, QTableView *tableView)
  : QObject(parent), parentWindow(parent), view(tableView), query(mapQuery)
{
  atools::gui::TableZoomHandler zoomHandler(view);
  Q_UNUSED(zoomHandler);

  Ui::MainWindow *ui = parentWindow->getUi();

  view->setContextMenuPolicy(Qt::CustomContextMenu);

  flightplan = new Flightplan();

  void (QSpinBox::*valueChangedPtr)(int) = &QSpinBox::valueChanged;
  connect(ui->spinBoxRouteSpeed, valueChangedPtr, this, &RouteController::updateLabel);
  connect(ui->spinBoxRouteAlt, valueChangedPtr, this, &RouteController::routeParamsChanged);

  void (QComboBox::*indexChangedPtr)(int) = &QComboBox::currentIndexChanged;
  connect(ui->comboBoxRouteType, indexChangedPtr, this, &RouteController::routeParamsChanged);

  connect(view, &QTableView::doubleClicked, this, &RouteController::doubleClick);
  connect(view, &QTableView::customContextMenuRequested, this, &RouteController::tableContextMenu);

  view->horizontalHeader()->setSectionsMovable(true);
  view->verticalHeader()->setSectionsMovable(false);
  view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

  dockWindowTitle = ui->dockWidgetRoute->windowTitle();
  model = new QStandardItemModel();
  QItemSelectionModel *m = view->selectionModel();
  view->setModel(model);
  delete m;

  iconDelegate = new RouteIconDelegate(routeMapObjects);
  view->setItemDelegateForColumn(0, iconDelegate);

  // Avoid stealing of keys from other default menus
  ui->actionRouteLegDown->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteLegUp->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  ui->actionRouteDeleteLeg->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  view->addActions({ui->actionRouteLegDown, ui->actionRouteLegUp, ui->actionRouteDeleteLeg});

  void (RouteController::*selChangedPtr)(const QItemSelection &selected, const QItemSelection &deselected) =
    &RouteController::tableSelectionChanged;
  connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, this, selChangedPtr);

  connect(ui->actionRouteLegDown, &QAction::triggered, this, &RouteController::moveLegsDown);
  connect(ui->actionRouteLegUp, &QAction::triggered, this, &RouteController::moveLegsUp);
  connect(ui->actionRouteDeleteLeg, &QAction::triggered, this, &RouteController::deleteLegs);
}

RouteController::~RouteController()
{
  delete model;
  delete iconDelegate;
  delete flightplan;
}

void RouteController::routeParamsChanged()
{
  changed = true;
  updateFlightplanData();
}

void RouteController::selectDepartureParking()
{
  ParkingDialog dialog(parentWindow, query, routeMapObjects.first().getAirport());

  int result = dialog.exec();
  dialog.hide();

  if(result == QDialog::Accepted)
  {
    maptypes::MapParking parking;
    if(dialog.getSelectedParking(parking))
      routeSetParking(parking);
  }
}

void RouteController::saveState()
{
  Ui::MainWindow *ui = parentWindow->getUi();

  atools::gui::WidgetState saver("Route/View");
  saver.save({view, ui->spinBoxRouteSpeed});

  atools::settings::Settings::instance()->setValue("Route/Filename", routeFilename);
}

void RouteController::restoreState()
{
  routeFilename = atools::settings::Settings::instance()->value("Route/Filename").toString();

  if(!routeFilename.isEmpty())
  {
    if(QFile::exists(routeFilename))
      loadFlightplan(routeFilename);
    else
    {
      routeFilename.clear();
      routeMapObjects.clear();
    }
  }

  Ui::MainWindow *ui = parentWindow->getUi();
  atools::gui::WidgetState saver("Route/View");
  model->setHorizontalHeaderLabels(ROUTE_COLUMNS);
  saver.restore({view, ui->spinBoxRouteSpeed});
}

void RouteController::getSelectedRouteMapObjects(QList<RouteMapObject>& selRouteMapObjects) const
{
  QItemSelection sm = view->selectionModel()->selection();
  for(const QItemSelectionRange& rng : sm)
    for(int row = rng.top(); row <= rng.bottom(); ++row)
      selRouteMapObjects.append(routeMapObjects.at(row));
}

void RouteController::newFlightplan()
{
  flightplan->clear();
  routeMapObjects.clear();
  routeFilename.clear();
  changed = false;
  totalDistance = 0.f;

  createRouteMapObjects();
  updateModel();
  updateWindowTitle();
  updateLabel();
  emit routeChanged();
}

void RouteController::loadFlightplan(const QString& filename)
{
  flightplan->clear();
  routeFilename = filename;
  changed = false;
  routeMapObjects.clear();
  totalDistance = 0.f;

  flightplan->load(filename);
  createRouteMapObjects();
  updateModel();
  updateWindowTitle();
  updateLabel();
  emit routeChanged();
}

void RouteController::saveFlighplanAs(const QString& filename)
{
  routeFilename = filename;
  flightplan->save(filename);
  changed = false;
  updateWindowTitle();
}

void RouteController::saveFlightplan()
{
  flightplan->save(routeFilename);
  changed = false;
  updateWindowTitle();
}

void RouteController::doubleClick(const QModelIndex& index)
{
  if(index.isValid())
  {
    qDebug() << "mouseDoubleClickEvent";

    const RouteMapObject& mo = routeMapObjects.at(index.row());

    if(mo.getMapObjectType() == maptypes::AIRPORT)
    {
      if(mo.getAirport().bounding.isPoint())
        emit showPos(mo.getPosition(), 2700);
      else
        emit showRect(mo.getAirport().bounding);
    }
    else
      emit showPos(mo.getPosition(), 2700);
  }
}

void RouteController::updateMoveAndDeleteActions()
{
  Ui::MainWindow *ui = parentWindow->getUi();
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

void RouteController::tableContextMenu(const QPoint& pos)
{
  qDebug() << "tableContextMenu";

  Ui::MainWindow *ui = parentWindow->getUi();

  atools::gui::ActionTextSaver saver({ui->actionMapNavaidRange});
  Q_UNUSED(saver);

  QModelIndex index = view->indexAt(pos);
  if(index.isValid())
  {
    const RouteMapObject& mo = routeMapObjects.at(index.row());

    QMenu menu;

    menu.addAction(ui->actionRouteLegUp);
    menu.addAction(ui->actionRouteLegDown);
    menu.addAction(ui->actionRouteDeleteLeg);

    menu.addSeparator();
    menu.addAction(ui->actionSearchSetMark);
    menu.addSeparator();

    menu.addAction(ui->actionSearchTableCopy);

    updateMoveAndDeleteActions();

    ui->actionSearchTableCopy->setEnabled(index.isValid());

    ui->actionMapRangeRings->setEnabled(true);
    ui->actionMapHideRangeRings->setEnabled(!parentWindow->getMapWidget()->getRangeRings().isEmpty());

    ui->actionMapNavaidRange->setText(tr("Show Navaid Range"));
    ui->actionMapNavaidRange->setEnabled(
      mo.getMapObjectType() == maptypes::VOR || mo.getMapObjectType() == maptypes::NDB);

    menu.addAction(ui->actionSearchTableSelectAll);

    menu.addSeparator();
    menu.addAction(ui->actionSearchResetView);

    menu.addSeparator();
    menu.addAction(ui->actionMapRangeRings);
    menu.addAction(ui->actionMapNavaidRange);
    menu.addAction(ui->actionMapHideRangeRings);

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
      }
      else if(action == ui->actionSearchTableSelectAll)
        view->selectAll();
      else if(action == ui->actionSearchSetMark)
        emit changeMark(mo.getPosition());
      else if(action == ui->actionMapRangeRings)
        parentWindow->getMapWidget()->addRangeRing(mo.getPosition());
      else if(action == ui->actionMapNavaidRange)
        parentWindow->getMapWidget()->addNavRangeRing(mo.getPosition(), mo.getMapObjectType(),
                                                      mo.getIdent(), mo.getFrequency(),
                                                      mo.getRange());
      else if(action == ui->actionMapHideRangeRings)
        parentWindow->getMapWidget()->clearRangeRings();
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

void RouteController::moveLegsDown()
{
  qDebug() << "Leg down";
  moveLegs(1);
}

void RouteController::moveLegsUp()
{
  qDebug() << "Leg up";
  moveLegs(-1);
}

void RouteController::moveLegs(int dir)
{
  QList<int> rows;
  selectedRows(rows, dir > 0);

  if(!rows.isEmpty())
  {
    QModelIndex curIdx = view->currentIndex();
    view->selectionModel()->clear();
    for(int row : rows)
    {
      flightplan->getEntries().move(row, row + dir);
      routeMapObjects.move(row, row + dir);
      model->insertRow(row + dir, model->takeRow(row));
    }
    changed = true;
    updateRouteMapObjects();
    updateFlightplanData();
    updateModel();
    updateLabel();
    updateWindowTitle();
    view->setCurrentIndex(model->index(curIdx.row() + dir, curIdx.column()));
    select(rows, dir);
    updateMoveAndDeleteActions();
    emit routeChanged();
  }
}

void RouteController::routeDelete(int routeIndex, maptypes::MapObjectTypes type)
{
  qDebug() << "route delete routeIndex" << routeIndex << "type" << type;

  flightplan->getEntries().removeAt(routeIndex);
  routeMapObjects.removeAt(routeIndex);

  changed = true;
  updateRouteMapObjects();
  updateFlightplanData();
  updateModel();
  updateWindowTitle();
  updateLabel();
  emit routeChanged();
}

void RouteController::deleteLegs()
{
  qDebug() << "Leg delete";
  QList<int> rows;
  selectedRows(rows, true);

  if(!rows.isEmpty())
  {
    int firstRow = rows.last();
    view->selectionModel()->clear();
    for(int row : rows)
    {
      flightplan->getEntries().removeAt(row);
      routeMapObjects.removeAt(row);
      model->removeRow(row);
    }
    changed = true;
    updateRouteMapObjects();
    updateFlightplanData();
    updateModel();
    updateWindowTitle();
    updateLabel();

    view->setCurrentIndex(model->index(firstRow, 0));
    updateMoveAndDeleteActions();
    emit routeChanged();
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

  if(routeMapObjects.isEmpty() || routeMapObjects.first().getMapObjectType() != maptypes::AIRPORT ||
     routeMapObjects.first().getId() != parking.airportId)
  {
    // No route, no start airport or different airport
    maptypes::MapAirport ap;
    query->getAirportById(ap, parking.airportId);
    routeStart(ap);
  }

  // Update the current airport which is new or the same as the one used by the parking
  flightplan->setDepartureParkingName(maptypes::parkingNameForFlightplan(parking));
  routeMapObjects.first().updateParking(parking);

  changed = true;
  updateRouteMapObjects();
  updateFlightplanData();
  updateModel();
  updateWindowTitle();
  updateLabel();
  emit routeChanged();
}

void RouteController::routeStart(const maptypes::MapAirport& airport)
{
  FlightplanEntry entry;
  buildFlightplanEntry(airport, entry);

  if(!flightplan->isEmpty())
  {
    const FlightplanEntry& first = flightplan->getEntries().first();
    if(first.getWaypointType() == entry::AIRPORT &&
       flightplan->getDepartureIdent() == first.getIcaoIdent() && flightplan->getEntries().size() > 1)
    {
      flightplan->getEntries().removeAt(0);
      routeMapObjects.removeAt(0);
    }
  }

  flightplan->getEntries().prepend(entry);

  RouteMapObject rmo(flightplan);
  rmo.loadFromAirport(&flightplan->getEntries().first(), airport, nullptr);
  routeMapObjects.prepend(rmo);
}

void RouteController::routeSetDest(maptypes::MapAirport airport)
{
  qDebug() << "route set dest id" << airport.id;

  FlightplanEntry entry;
  buildFlightplanEntry(airport, entry);

  if(!flightplan->isEmpty())
  {
    const FlightplanEntry& last = flightplan->getEntries().last();
    if(last.getWaypointType() == entry::AIRPORT &&
       flightplan->getDestinationIdent() == last.getIcaoIdent() && flightplan->getEntries().size() > 1)
    {
      flightplan->getEntries().removeLast();
      routeMapObjects.removeLast();
    }
  }

  flightplan->getEntries().append(entry);

  const RouteMapObject *rmoPred = nullptr;
  if(flightplan->getEntries().size() > 1)
    rmoPred = &routeMapObjects.at(routeMapObjects.size() - 1);

  RouteMapObject rmo(flightplan);
  rmo.loadFromAirport(&flightplan->getEntries().first(), airport, rmoPred);
  routeMapObjects.append(rmo);

  changed = true;
  updateRouteMapObjects();
  updateFlightplanData();
  updateModel();
  updateWindowTitle();
  updateLabel();
  emit routeChanged();
}

void RouteController::routeSetStart(maptypes::MapAirport airport)
{
  qDebug() << "route set start id" << airport.id;

  routeStart(airport);
  changed = true;
  updateRouteMapObjects();
  updateFlightplanData();
  updateModel();
  updateWindowTitle();
  updateLabel();
  emit routeChanged();
}

void RouteController::routeReplace(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type,
                                   int oldIndex)
{
  qDebug() << "route replace" << "user pos" << userPos << "id" << id
           << "type" << type << "old index" << oldIndex;

  FlightplanEntry entry;
  buildFlightplanEntry(id, userPos, type, entry);

  flightplan->getEntries().replace(oldIndex, entry);

  const RouteMapObject *rmoPred = nullptr;

  RouteMapObject rmo(flightplan);
  rmo.loadFromDatabaseByEntry(&flightplan->getEntries()[oldIndex], query, rmoPred);

  routeMapObjects.replace(oldIndex, rmo);

  changed = true;
  updateRouteMapObjects();
  updateFlightplanData();
  updateModel();
  updateWindowTitle();
  updateLabel();
  emit routeChanged();

}

void RouteController::routeAdd(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type)
{
  qDebug() << "route add id" << id << "type" << type;

  FlightplanEntry entry;
  buildFlightplanEntry(id, userPos, type, entry);

  int leg = nearestLeg(entry.getPosition());
  qDebug() << "nearestLeg" << leg;

  int insertIndex = leg;
  if(flightplan->isEmpty() || insertIndex == -1)
    insertIndex = 0;

  flightplan->getEntries().insert(insertIndex, entry);

  const RouteMapObject *rmoPred = nullptr;

  if(flightplan->isEmpty() && insertIndex > 0)
    rmoPred = &routeMapObjects.at(insertIndex - 1);
  RouteMapObject rmo(flightplan);
  rmo.loadFromDatabaseByEntry(&flightplan->getEntries()[insertIndex], query, rmoPred);

  routeMapObjects.insert(insertIndex, rmo);

  changed = true;
  updateRouteMapObjects();
  updateFlightplanData();
  updateModel();
  updateWindowTitle();
  updateLabel();
  emit routeChanged();
}

int RouteController::nearestLeg(const atools::geo::Pos& pos)
{
  int nearest = -1;
  float minDistance = std::numeric_limits<float>::max();
  for(int i = 1; i < routeMapObjects.size(); i++)
  {
    bool valid;
    float distance = std::abs(pos.distanceMeterToLine(routeMapObjects.at(i - 1).getPosition(),
                                                      routeMapObjects.at(i).getPosition(), valid));

    if(valid && distance < minDistance)
    {
      minDistance = distance;
      nearest = i;
    }
  }
  for(int i = 0; i < routeMapObjects.size(); i++)
  {
    float distance = routeMapObjects.at(i).getPosition().distanceMeterTo(pos);
    if(distance < minDistance)
    {
      minDistance = distance;
      nearest = i + 1;
    }
  }
  return nearest;
}

void RouteController::buildFlightplanEntry(const maptypes::MapAirport& airport, FlightplanEntry& entry)
{
  entry.setIcaoIdent(airport.ident);
  entry.setPosition(airport.position);
  entry.setWaypointType(entry::AIRPORT);
  entry.setWaypointId(entry.getIcaoIdent());
}

void RouteController::buildFlightplanEntry(int id, atools::geo::Pos userPos, maptypes::MapObjectTypes type,
                                           FlightplanEntry& entry)
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
  if(type == maptypes::PARKING)
  {
  }
  else if(type == maptypes::WAYPOINT)
  {
    const maptypes::MapWaypoint& wp = result.waypoints.first();
    entry.setIcaoIdent(wp.ident);
    entry.setPosition(wp.position);
    entry.setIcaoRegion(wp.region);
    entry.setWaypointType(entry::INTERSECTION);
    entry.setWaypointId(entry.getIcaoIdent());
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
  if(routeMapObjects.isEmpty())
    flightplan->clear();
  else
  {
    QString departureIcao, destinationIcao;

    const RouteMapObject& firstRmo = routeMapObjects.first();
    if(firstRmo.getMapObjectType() == maptypes::AIRPORT)
    {
      departureIcao = firstRmo.getAirport().ident;
      flightplan->setDepartureAiportName(firstRmo.getAirport().name);
      flightplan->setDepartureIdent(departureIcao);

      if(!firstRmo.getParking().name.isEmpty())
        flightplan->setDepartureParkingName(maptypes::parkingNameForFlightplan(firstRmo.getParking()));
      flightplan->setDeparturePos(firstRmo.getPosition());
    }
    else
    {
      flightplan->setDepartureAiportName(QString());
      flightplan->setDepartureIdent(QString());
      flightplan->setDepartureParkingName(QString());
      flightplan->setDeparturePos(Pos());
    }

    const RouteMapObject& lastRmo = routeMapObjects.last();
    if(lastRmo.getMapObjectType() == maptypes::AIRPORT)
    {
      destinationIcao = lastRmo.getAirport().ident;
      flightplan->setDestinationAiportName(lastRmo.getAirport().name);
      flightplan->setDestinationIdent(destinationIcao);
      flightplan->setDestinationPos(lastRmo.getPosition());
    }
    else
    {
      flightplan->setDestinationAiportName(QString());
      flightplan->setDestinationIdent(QString());
      flightplan->setDestinationPos(Pos());
    }

    Ui::MainWindow *ui = parentWindow->getUi();

    // <Descr>LFHO, EDRJ</Descr>
    flightplan->setDescription(departureIcao + ", " + destinationIcao);
    // <FPType>IFR</FPType>
    flightplan->setRouteType(atools::fs::pln::VOR);
    // <RouteType>LowAlt</RouteType>
    flightplan->setFlightplanType(
      ui->comboBoxRouteType->currentIndex() == 0 ? atools::fs::pln::IFR : atools::fs::pln::VFR);
    // <CruisingAlt>19000</CruisingAlt>
    flightplan->setCruisingAlt(ui->spinBoxRouteAlt->value());
    // <Title>LFHO to EDRJ</Title>
    flightplan->setTitle(departureIcao + " to " + destinationIcao);
  }
}

void RouteController::updateRouteMapObjects()
{
  totalDistance = 0.f;
  curUserpointNumber = 0;
  // Used to number user waypoints
  RouteMapObject *last = nullptr;
  for(int i = 0; i < routeMapObjects.size(); i++)
  {
    RouteMapObject& mapobj = routeMapObjects[i];
    mapobj.update(last);
    curUserpointNumber = std::max(curUserpointNumber, mapobj.getUserpointNumber());
    totalDistance += mapobj.getDistanceTo();

    if(i == 0)
      boundingRect = Rect(mapobj.getPosition());
    else
      boundingRect.extend(mapobj.getPosition());

    last = &mapobj;
  }
  curUserpointNumber++;
}

void RouteController::createRouteMapObjects()
{
  routeMapObjects.clear();

  RouteMapObject *last = nullptr;
  totalDistance = 0.f;
  curUserpointNumber = 0;
  // Create map objects first and calculate total distance
  for(int i = 0; i < flightplan->getEntries().size(); i++)
  {
    FlightplanEntry& entry = flightplan->getEntries()[i];

    RouteMapObject mapobj(flightplan);
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

    routeMapObjects.append(mapobj);
    last = &routeMapObjects.last();
  }
  curUserpointNumber++;
}

void RouteController::updateModel()
{
  model->removeRows(0, model->rowCount());

  int row = 0;
  float cumulatedDistance = 0.f;
  QList<QStandardItem *> items;
  for(const RouteMapObject& mapobj : routeMapObjects)
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
        items.append(new QStandardItem("DME (" + type + ")"));
      else if(mapobj.getVor().hasDme)
        items.append(new QStandardItem("VORDME (" + type + ")"));
      else
        items.append(new QStandardItem("VOR (" + type + ")"));
    }
    else if(mapobj.getMapObjectType() == maptypes::NDB)
    {
      QString type = mapobj.getNdb().type == "COMPASS_POINT" ? "CP" : mapobj.getNdb().type;
      items.append(new QStandardItem("NDB (" + type + ")"));
    }
    else
      items.append(nullptr);

    QStandardItem *item;
    if(mapobj.getFrequency() > 0)
    {
      if(mapobj.getMapObjectType() == maptypes::VOR)
        item = new QStandardItem(QString::number(mapobj.getFrequency() / 1000.f, 'f', 2) + " MHz");
      else if(mapobj.getMapObjectType() == maptypes::NDB)
        item = new QStandardItem(QString::number(mapobj.getFrequency() / 100.f, 'f', 1) + " kHz");
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
      item = new QStandardItem(QString::number(mapobj.getCourseTo(), 'f', 0));
      item->setTextAlignment(Qt::AlignRight);
      items.append(item);

      item = new QStandardItem(QString::number(mapobj.getCourseToRhumb(), 'f', 0));
      item->setTextAlignment(Qt::AlignRight);
      items.append(item);

      item = new QStandardItem(QString::number(mapobj.getDistanceTo(), 'f', 1));
      item->setTextAlignment(Qt::AlignRight);
      items.append(item);

    }

    cumulatedDistance += mapobj.getDistanceTo();
    // Remaining

    item = new QStandardItem(QString::number(totalDistance - cumulatedDistance, 'f', 1));
    item->setTextAlignment(Qt::AlignRight);
    items.append(item);

    model->appendRow(items);
    row++;
  }

  Ui::MainWindow *ui = parentWindow->getUi();

  ui->spinBoxRouteAlt->setValue(flightplan->getCruisingAlt());

  if(flightplan->getFlightplanType() == atools::fs::pln::IFR)
    ui->comboBoxRouteType->setCurrentIndex(0);
  else if(flightplan->getFlightplanType() == atools::fs::pln::VFR)
    ui->comboBoxRouteType->setCurrentIndex(1);
}

void RouteController::updateLabel()
{
  Ui::MainWindow *ui = parentWindow->getUi();
  QString startAirport("No airport"), destAirport("No airport");
  if(!flightplan->isEmpty())
  {
    if(flightplan->getEntries().first().getWaypointType() == entry::AIRPORT)
    {
      startAirport = flightplan->getDepartureAiportName() +
                     " (" + flightplan->getDepartureIdent() + ")";
      if(!flightplan->getDepartureParkingName().isEmpty())
      {
        QString park = flightplan->getDepartureParkingName().toLower();
        park[0] = park.at(0).toUpper();
        startAirport += " " + park;
      }
    }

    if(flightplan->getEntries().last().getWaypointType() == entry::AIRPORT)
      destAirport = flightplan->getDestinationAiportName() +
                    " (" + flightplan->getDestinationIdent() + ")";
    ui->labelRouteInfo->setText("<b>" + startAirport + "</b> to <b>" + destAirport + "</b>, " +
                                QString::number(totalDistance, 'f', 0) + " nm, " +
                                formatter::formatMinutesHoursLong(
                                  totalDistance / static_cast<float>(ui->spinBoxRouteSpeed->value())));
  }
  else
    ui->labelRouteInfo->setText(tr("No Flightplan loaded"));
}

void RouteController::updateWindowTitle()
{
  Ui::MainWindow *ui = parentWindow->getUi();
  ui->dockWidgetRoute->setWindowTitle(dockWindowTitle + " - " +
                                      QFileInfo(routeFilename).fileName() +
                                      (changed ? " *" : QString()));
}
