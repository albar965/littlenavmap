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
#include "table/formatter.h"
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

#include "routeicondelegate.h"
#include "ui_mainwindow.h"
#include <settings/settings.h>
#include <gui/actiontextsaver.h>

using namespace atools::fs::pln;

RouteController::RouteController(MainWindow *parent, MapQuery *mapQuery, QTableView *view)
  : QObject(parent), parentWindow(parent), tableView(view), query(mapQuery)
{
  atools::gui::TableZoomHandler zoomHandler(view);
  Q_UNUSED(zoomHandler);

  Ui::MainWindow *ui = parentWindow->getUi();

  view->setContextMenuPolicy(Qt::CustomContextMenu);

  void (QSpinBox::*valueChangedPtr)(int) = &QSpinBox::valueChanged;
  connect(ui->spinBoxRouteSpeed, valueChangedPtr, this, &RouteController::updateLabel);
  connect(view, &QTableView::doubleClicked, this, &RouteController::doubleClick);
  connect(view, &QTableView::customContextMenuRequested, this, &RouteController::tableContextMenu);

  view->horizontalHeader()->setSectionsMovable(false);
  view->verticalHeader()->setSectionsMovable(false);
  view->verticalHeader()->setHighlightSections(true);
  view->verticalHeader()->setSectionsClickable(false);
  view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

  view->setDragDropOverwriteMode(false);
  dockWindowTitle = ui->dockWidgetRoute->windowTitle();
  model = new QStandardItemModel();
  QItemSelectionModel *m = view->selectionModel();
  view->setModel(model);
  delete m;

  // TODO delete old and new delegate
  view->setItemDelegateForColumn(0, new RouteIconDelegate(routeMapObjects));

  void (RouteController::*selChangedPtr)(const QItemSelection &selected, const QItemSelection &deselected) =
    &RouteController::tableSelectionChanged;
  connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, selChangedPtr);
}

RouteController::~RouteController()
{
  delete model;
  delete flightplan;
}

void RouteController::saveState()
{
  Ui::MainWindow *ui = parentWindow->getUi();

  atools::gui::WidgetState saver("Route/View");
  saver.save({tableView, ui->spinBoxRouteSpeed});

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
  saver.restore({tableView, ui->spinBoxRouteSpeed});
}

void RouteController::getSelectedRouteMapObjects(QList<RouteMapObject>& selRouteMapObjects) const
{
  QItemSelection sm = tableView->selectionModel()->selection();
  for(const QItemSelectionRange& rng : sm)
    for(int row = rng.top(); row <= rng.bottom(); ++row)
      selRouteMapObjects.append(routeMapObjects.at(row));
}

void RouteController::newFlightplan()
{
  delete flightplan;
  flightplan = nullptr;
  routeMapObjects.clear();
  routeFilename.clear();
  changed = false;
  flightplanToView();
  updateWindowTitle();
  updateLabel();
  emit routeChanged();
}

void RouteController::loadFlightplan(const QString& filename)
{
  delete flightplan;
  flightplan = nullptr;
  routeFilename = filename;
  changed = false;
  routeMapObjects.clear();

  flightplan = new Flightplan();
  flightplan->load(filename);
  flightplanToView();
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
  if(changed && flightplan != nullptr)
  {
    flightplan->save(routeFilename);
    changed = false;
  }
}

void RouteController::flightplanToView()
{
  model->removeRows(0, model->rowCount());
  model->setHorizontalHeaderLabels({"Ident", "Region", "Name", "Airway", "Type", "Freq.",
                                    "Course\n°M", "Direct\n°M",
                                    "Distance\nnm", "Remaining\nnm"});

  if(flightplan != nullptr)
  {
    RouteMapObject last;
    totalDistance = 0.f;
    // Used to number user waypoints
    int userIdentIndex = 1;
    int row = 0;

    // Create map objects first and calculate total distance
    for(const FlightplanEntry& entry : flightplan->getEntries())
    {
      RouteMapObject mapobj(entry, query, row == 0 ? nullptr : &last, userIdentIndex);

      if(mapobj.getMapObjectType() == maptypes::INVALID)
        qWarning() << "Entry for ident" << entry.getIcaoIdent() <<
        "region" << entry.getIcaoRegion() << "is not valid";

      totalDistance += mapobj.getDistanceTo();
      routeMapObjects.append(mapobj);
      last = mapobj;
      row++;
    }

    row = 0;
    float cumulatedDistance = 0.f;
    QList<QStandardItem *> items;
    for(const RouteMapObject& mapobj : routeMapObjects)
    {
      items.clear();
      items.append(new QStandardItem(mapobj.getIdent()));
      items.append(new QStandardItem(mapobj.getRegion()));
      items.append(new QStandardItem(mapobj.getName()));
      items.append(new QStandardItem(mapobj.getFlightplanEntry().getAirway()));

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

      float distance = mapobj.getDistanceTo();
      cumulatedDistance += distance;

      if(row == 0)
      {
        boundingRect = atools::geo::Rect(mapobj.getPosition());
        items.append(nullptr);
        items.append(nullptr);
        items.append(nullptr);
      }
      else
      {
        item = new QStandardItem(QString::number(mapobj.getCourse(), 'f', 0));
        item->setTextAlignment(Qt::AlignRight);
        items.append(item);

        item = new QStandardItem(QString::number(mapobj.getCourseRhumb(), 'f', 0));
        item->setTextAlignment(Qt::AlignRight);
        items.append(item);

        item = new QStandardItem(QString::number(distance, 'f', 1));
        item->setTextAlignment(Qt::AlignRight);
        items.append(item);

        boundingRect.extend(mapobj.getPosition());
      }

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
}

void RouteController::updateLabel()
{
  Ui::MainWindow *ui = parentWindow->getUi();
  if(flightplan != nullptr)
    ui->labelRouteInfo->setText(flightplan->getDepartureAiportName() +
                                " (" + flightplan->getDepartureIdent() + ") to " +
                                flightplan->getDestinationAiportName() +
                                " (" + flightplan->getDestinationIdent() + "), " +
                                QString::number(totalDistance, 'f', 0) + " nm, " +
                                formatter::formatMinutesHoursLong(
                                  totalDistance / static_cast<float>(ui->spinBoxRouteSpeed->value())));
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

void RouteController::tableContextMenu(const QPoint& pos)
{
  qDebug() << "tableContextMenu";

  Ui::MainWindow *ui = parentWindow->getUi();

  atools::gui::ActionTextSaver saver({ui->actionMapNavaidRange});

  atools::geo::Pos position;
  QModelIndex index = tableView->indexAt(pos);
  if(index.isValid())
  {
    const RouteMapObject& mo = routeMapObjects.at(index.row());

    QMenu menu;

    menu.addAction(ui->actionSearchSetMark);
    menu.addSeparator();

    menu.addAction(ui->actionSearchTableCopy);
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
        QHeaderView *header = tableView->horizontalHeader();
        for(int i = 0; i < header->count(); ++i)
          header->moveSection(header->visualIndex(i), i);

        tableView->resizeColumnsToContents();
      }
      else if(action == ui->actionSearchTableSelectAll)
        tableView->selectAll();
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

  tableSelectionChanged();
}

void RouteController::tableSelectionChanged()
{
  QItemSelectionModel *sm = tableView->selectionModel();

  int selectedRows = 0;
  if(sm != nullptr && sm->hasSelection())
    selectedRows = sm->selectedRows().size();

  emit routeSelectionChanged(selectedRows, model->rowCount());
}
