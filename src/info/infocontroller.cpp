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

#include "logging/loggingdefs.h"
#include "route/routecontroller.h"
#include "common/weatherreporter.h"
#include "infocontroller.h"
#include <QImageReader>
#include <QSettings>
#include <gui/mainwindow.h>
#include <gui/widgetstate.h>
#include "infoquery.h"
#include "ui_mainwindow.h"
#include <common/htmlbuilder.h>
#include <common/maphtmlinfobuilder.h>
#include <common/symbolpainter.h>
#include <mapgui/mapquery.h>
#include <settings/settings.h>
#include <QScrollBar>
#include "atools.h"

const int SYMBOL_SIZE = 20;

enum TabIndex
{
  AIRPORT = 0,
  RUNWAYS = 1,
  COM = 2,
  APPROACHES = 3,
  NAVAID = 4,
  NAVMAP_LEGEND = 5,
  MAP_LEGEND = 6
};

InfoController::InfoController(MainWindow *parent, MapQuery *mapDbQuery, InfoQuery *infoDbQuery) :
  QObject(parent), mainWindow(parent), mapQuery(mapDbQuery), infoQuery(infoDbQuery)
{
  iconBackColor = QApplication::palette().color(QPalette::Active, QPalette::Base);

  info = new MapHtmlInfoBuilder(mapQuery, infoQuery, true);
}

InfoController::~InfoController()
{
  delete info;
}

void InfoController::saveState()
{
  Ui::MainWindow *ui = mainWindow->getUi();

  atools::gui::WidgetState ws("InfoWindow/Widget");
  ws.save({ui->tabWidgetInformation, ui->textEditAirportInfo, ui->textEditRunwayInfo, ui->textEditComInfo,
           ui->textEditApproachInfo, ui->textEditNavaidInfo});

  atools::settings::Settings::instance()->setValue("InfoWindow/CurrentAirportId", curAirportId);
}

void InfoController::restoreState()
{
  Ui::MainWindow *ui = mainWindow->getUi();

  atools::gui::WidgetState ws("InfoWindow/Widget");
  ws.restore({ui->tabWidgetInformation, ui->textEditAirportInfo, ui->textEditRunwayInfo, ui->textEditComInfo,
              ui->textEditApproachInfo, ui->textEditNavaidInfo});

  curAirportId = atools::settings::Settings::instance()->value("InfoWindow/CurrentAirportId", -1).toInt();

  updateAirport();
}

void InfoController::updateAirport()
{
  if(databaseLoadStatus)
    return;

  qDebug() << "InfoController::updateAirport";

  if(curAirportId != -1)
  {
    HtmlBuilder html(true);
    maptypes::MapAirport ap;
    mapQuery->getAirportById(ap, curAirportId);

    info->airportText(ap, html,
                      &mainWindow->getRouteController()->getRouteMapObjects(),
                      mainWindow->getWeatherReporter(), iconBackColor);
    mainWindow->getUi()->textEditAirportInfo->setText(html.getHtml());
  }
}

void InfoController::showInformation(maptypes::MapSearchResult result)
{
  qDebug() << "InfoController::showInformation";

  HtmlBuilder html(true);

  Ui::MainWindow *ui = mainWindow->getUi();
  int idx = ui->tabWidgetInformation->currentIndex();

  if(!result.airports.isEmpty())
  {
    const maptypes::MapAirport& airport = result.airports.first();

    if(idx != AIRPORT && idx != RUNWAYS && idx != COM && idx != APPROACHES)
      ui->tabWidgetInformation->setCurrentIndex(AIRPORT);

    curAirportId = airport.id;
    updateAirport();

    html.clear();
    info->runwayText(airport, html, iconBackColor);
    ui->textEditRunwayInfo->setText(html.getHtml());

    html.clear();
    info->comText(airport, html, iconBackColor);
    ui->textEditComInfo->setText(html.getHtml());

    html.clear();
    info->approachText(airport, html, iconBackColor);
    ui->textEditApproachInfo->setText(html.getHtml());
  }

  html.clear();
  for(const maptypes::MapVor& vor : result.vors)
  {
    if(result.airports.isEmpty())
      ui->tabWidgetInformation->setCurrentIndex(NAVAID);
    info->vorText(vor, html, iconBackColor);
    ui->textEditNavaidInfo->setText(html.getHtml());
  }

  for(const maptypes::MapNdb& ndb : result.ndbs)
  {
    if(result.airports.isEmpty())
      ui->tabWidgetInformation->setCurrentIndex(NAVAID);
    info->ndbText(ndb, html, iconBackColor);
    ui->textEditNavaidInfo->setText(html.getHtml());
  }

  for(const maptypes::MapWaypoint& waypoint : result.waypoints)
  {
    if(result.airports.isEmpty())
      ui->tabWidgetInformation->setCurrentIndex(NAVAID);
    info->waypointText(waypoint, html, iconBackColor);
    ui->textEditNavaidInfo->setText(html.getHtml());
  }

  for(const maptypes::MapAirway& airway : result.airways)
  {
    if(result.airports.isEmpty())
      ui->tabWidgetInformation->setCurrentIndex(NAVAID);
    info->airwayText(airway, html);
    ui->textEditNavaidInfo->setText(html.getHtml());
  }
}

void InfoController::preDatabaseLoad()
{
  databaseLoadStatus = true;
}

void InfoController::postDatabaseLoad()
{
  databaseLoadStatus = false;
}

void InfoController::dataPacketReceived(atools::fs::sc::SimConnectData data)
{

  Ui::MainWindow *ui = mainWindow->getUi();
  if(ui->dockWidgetAircraft->isVisible() && !databaseLoadStatus &&
     !ui->textEditAircraftInfo->verticalScrollBar()->isSliderDown())
  {
    if(!lastSimData.getPosition().isValid() ||
       // !lastSimData.getPosition().fuzzyEqual(data.getPosition(), atools::geo::Pos::POS_EPSILON_10M) ||
       atools::almostNotEqual(QDateTime::currentDateTime().toMSecsSinceEpoch(),
                              lastSimUpdate, static_cast<qint64>(500)))
    {
      HtmlBuilder html(true);

      const RouteMapObjectList& rmoList = mainWindow->getRouteController()->getRouteMapObjects();
      info->aircraftText(data, html, rmoList);

      int val = ui->textEditAircraftInfo->verticalScrollBar()->value();
      ui->textEditAircraftInfo->setText(html.getHtml());
      ui->textEditAircraftInfo->verticalScrollBar()->setValue(val);
      lastSimData = data;
      lastSimUpdate = QDateTime::currentDateTime().toMSecsSinceEpoch();
    }
  }
}

void InfoController::connectedToSimulator()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  ui->textEditAircraftInfo->setText("Connected.");
}

void InfoController::disconnectedFromSimulator()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  ui->textEditAircraftInfo->setText("Disconnected.");
}
