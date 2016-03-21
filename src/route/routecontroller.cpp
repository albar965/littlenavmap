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

#include "gui/mainwindow.h"

#include <QStandardItemModel>
#include <QTableView>
#include <QHeaderView>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <gui/tablezoomhandler.h>
#include <gui/widgetstate.h>
#include <mapgui/mapquery.h>

#include "ui_mainwindow.h"
#include <settings/settings.h>

using namespace atools::fs::pln;

RouteController::RouteController(MainWindow *parent, MapQuery *mapQuery, QTableView *view)
  : QObject(parent), parentWindow(parent), tableView(view), query(mapQuery)
{
  atools::gui::TableZoomHandler zoomHandler(view);
  Q_UNUSED(zoomHandler);

  view->horizontalHeader()->setSectionsMovable(false);
  view->verticalHeader()->setSectionsMovable(false);
  view->verticalHeader()->setHighlightSections(true);
  view->verticalHeader()->setSectionsClickable(false);
  view->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

  view->setDragDropOverwriteMode(false);
  Ui::MainWindow *ui = parentWindow->getUi();
  dockWindowTitle = ui->dockWidgetRoute->windowTitle();
  model = new QStandardItemModel();
  view->setModel(model);
}

RouteController::~RouteController()
{
  delete model;
  delete flightplan;
}

void RouteController::saveState()
{
  atools::gui::WidgetState saver("Route/View");
  saver.save(tableView);

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

  atools::gui::WidgetState saver("Route/View");
  saver.restore(tableView);
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
  model->setHorizontalHeaderLabels({"Ident", "Region", "Airway", "Type", "Frequency", "Course"});
  if(flightplan != nullptr)
  {
    Ui::MainWindow *ui = parentWindow->getUi();

    ui->spinBoxRouteAlt->setValue(flightplan->getCruisingAlt());

    if(flightplan->getFlightplanType() == atools::fs::pln::IFR)
      ui->comboBoxRouteType->setCurrentIndex(0);
    else if(flightplan->getFlightplanType() == atools::fs::pln::VFR)
      ui->comboBoxRouteType->setCurrentIndex(1);

    ui->labelRouteInfo->setText(flightplan->getDepartureAiportName() +
                                " (" + flightplan->getDepartureIdent() + ") to " +
                                flightplan->getDestinationAiportName() +
                                " (" + flightplan->getDestinationIdent() + ")");

    RouteMapObject last;
    QList<QStandardItem *> items;
    bool first = true;
    for(const FlightplanEntry& entry : flightplan->getEntries())
    {
      RouteMapObject mapObj(entry, query);

      if(!mapObj.isValid())
        qWarning() << "Entry for ident" << entry.getIcaoIdent() <<
        "region" << entry.getIcaoRegion() << "is not valid";

      routeMapObjects.append(mapObj);

      items.clear();
      items.append(new QStandardItem(mapObj.getFlightplanEntry().getIcaoIdent()));
      items.append(new QStandardItem(mapObj.getFlightplanEntry().getIcaoRegion()));
      items.append(new QStandardItem(mapObj.getFlightplanEntry().getAirway()));
      items.append(new QStandardItem(mapObj.getFlightplanEntry().getWaypointTypeAsString()));

      QStandardItem *item;
      if(mapObj.getFrequency() > 0)
      {
        if(mapObj.getMapObjectType() == maptypes::VOR)
          item = new QStandardItem(QString::number(mapObj.getFrequency() / 1000.f, 'f', 2) + " MHz");
        else if(mapObj.getMapObjectType() == maptypes::NDB)
          item = new QStandardItem(QString::number(mapObj.getFrequency() / 100.f, 'f', 1) + " kHz");
        else
          item = new QStandardItem();
        item->setTextAlignment(Qt::AlignRight);
        items.append(item);
      }
      else
        items.append(new QStandardItem());

      if(!first)
      {
        float course = last.getPosition().angleDegTo(mapObj.getPosition());
        item = new QStandardItem(QString::number(course, 'f', 1));
        item->setTextAlignment(Qt::AlignRight);
        items.append(item);
      }
      else
      {
        first = false;
        items.append(new QStandardItem());
      }

      model->appendRow(items);

      last = mapObj;
    }

  }

}

void RouteController::updateWindowTitle()
{
  Ui::MainWindow *ui = parentWindow->getUi();
  ui->dockWidgetRoute->setWindowTitle(dockWindowTitle + " - " +
                                      QFileInfo(routeFilename).fileName() +
                                      (changed ? " *" : QString()));
}
