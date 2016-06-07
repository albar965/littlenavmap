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
  ws.save({ui->tabWidgetInformation, ui->tabWidgetAircraft});

  maptypes::MapObjectRefList refs;
  for(const maptypes::MapAirport& airport  : currentSearchResult.airports)
    refs.append({airport.id, maptypes::AIRPORT});
  for(const maptypes::MapVor& vor : currentSearchResult.vors)
    refs.append({vor.id, maptypes::VOR});
  for(const maptypes::MapNdb& ndb : currentSearchResult.ndbs)
    refs.append({ndb.id, maptypes::NDB});
  for(const maptypes::MapWaypoint& waypoint : currentSearchResult.waypoints)
    refs.append({waypoint.id, maptypes::WAYPOINT});
  for(const maptypes::MapAirway& airway : currentSearchResult.airways)
    refs.append({airway.id, maptypes::AIRWAY});

  QStringList refList;
  for(const maptypes::MapObjectRef& ref : refs)
    refList.append(QString("%1;%2").arg(ref.id).arg(ref.type));
  atools::settings::Settings::instance()->setValue("InfoWindow/CurrentMapObjects", refList.join(";"));
}

void InfoController::restoreState()
{
  QString refsStr = atools::settings::Settings::instance()->value("InfoWindow/CurrentMapObjects").toString();
  QStringList refsStrList = refsStr.split(";", QString::SkipEmptyParts);

  maptypes::MapSearchResult res;
  for(int i = 0; i < refsStrList.size(); i += 2)
    mapQuery->getMapObjectById(res,
                               maptypes::MapObjectTypes(refsStrList.at(i + 1).toInt()),
                               refsStrList.at(i).toInt());

  showInformation(res);

  Ui::MainWindow *ui = mainWindow->getUi();
  atools::gui::WidgetState ws("InfoWindow/Widget");
  ws.restore({ui->tabWidgetInformation, ui->tabWidgetAircraft});
}

void InfoController::updateAirport()
{
  if(databaseLoadStatus)
    return;

  qDebug() << "InfoController::updateAirport";

  if(!currentSearchResult.airports.isEmpty())
  {
    HtmlBuilder html(true);
    maptypes::MapAirport ap;
    mapQuery->getAirportById(ap, currentSearchResult.airports.first().id);

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

    currentSearchResult.airports.clear();
    currentSearchResult.airportIds.clear();
    currentSearchResult.airports.append(airport);

    if(idx != AIRPORT && idx != RUNWAYS && idx != COM && idx != APPROACHES)
      ui->tabWidgetInformation->setCurrentIndex(AIRPORT);

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

  if(!result.vors.isEmpty() || !result.ndbs.isEmpty() || !result.waypoints.isEmpty() ||
     !result.airways.isEmpty())
  {
    currentSearchResult.vors.clear();
    currentSearchResult.vorIds.clear();
    currentSearchResult.ndbs.clear();
    currentSearchResult.ndbIds.clear();
    currentSearchResult.waypoints.clear();
    currentSearchResult.waypointIds.clear();
    currentSearchResult.airways.clear();
  }

  html.clear();
  for(const maptypes::MapVor& vor : result.vors)
  {
    currentSearchResult.vors.append(vor);

    if(result.airports.isEmpty())
      ui->tabWidgetInformation->setCurrentIndex(NAVAID);
    info->vorText(vor, html, iconBackColor);
    ui->textEditNavaidInfo->setText(html.getHtml());
  }

  for(const maptypes::MapNdb& ndb : result.ndbs)
  {
    currentSearchResult.ndbs.append(ndb);

    if(result.airports.isEmpty())
      ui->tabWidgetInformation->setCurrentIndex(NAVAID);
    info->ndbText(ndb, html, iconBackColor);
    ui->textEditNavaidInfo->setText(html.getHtml());
  }

  for(const maptypes::MapWaypoint& waypoint : result.waypoints)
  {
    currentSearchResult.waypoints.append(waypoint);

    if(result.airports.isEmpty())
      ui->tabWidgetInformation->setCurrentIndex(NAVAID);
    info->waypointText(waypoint, html, iconBackColor);
    ui->textEditNavaidInfo->setText(html.getHtml());
  }

  for(const maptypes::MapAirway& airway : result.airways)
  {
    currentSearchResult.airways.append(airway);

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

bool InfoController::canTextEditUpdate(const QTextEdit *textEdit)
{
  // Do not update if scrollbar is clicked
  return !textEdit->verticalScrollBar()->isSliderDown() &&
         !textEdit->horizontalScrollBar()->isSliderDown();
}

void InfoController::updateTextEdit(QTextEdit *textEdit, const QString& text)
{
  // Remember cursor position
  QTextCursor cursor = textEdit->textCursor();
  int pos = cursor.position();
  int anchor = cursor.anchor();

  // Remember scrollbar position
  int vScrollPos = textEdit->verticalScrollBar()->value();
  int hScrollPos = textEdit->horizontalScrollBar()->value();
  textEdit->setText(text);

  if(anchor != pos)
  {
    // Reset cursor
    int maxPos = textEdit->document()->characterCount() - 1;

    // Probably the document changed its size
    anchor = std::min(maxPos, anchor);
    pos = std::min(maxPos, pos);

    cursor.setPosition(anchor, QTextCursor::MoveAnchor);
    cursor.setPosition(pos, QTextCursor::KeepAnchor);
    textEdit->setTextCursor(cursor);
  }
  textEdit->verticalScrollBar()->setValue(vScrollPos);
  textEdit->horizontalScrollBar()->setValue(hScrollPos);
}

void InfoController::dataPacketReceived(atools::fs::sc::SimConnectData data)
{
  Ui::MainWindow *ui = mainWindow->getUi();

  if(!databaseLoadStatus)
  {
    if(!lastSimData.getPosition().isValid() ||
       // !lastSimData.getPosition().fuzzyEqual(data.getPosition(), atools::geo::Pos::POS_EPSILON_10M) ||
       atools::almostNotEqual(QDateTime::currentDateTime().toMSecsSinceEpoch(),
                              lastSimUpdate, static_cast<qint64>(500)))
    {
      HtmlBuilder html(true);

      if(ui->dockWidgetAircraft->isVisible())
      {
        if(canTextEditUpdate(ui->textEditAircraftInfo))
        {
          info->aircraftText(data, html);
          updateTextEdit(ui->textEditAircraftInfo, html.getHtml());
        }

        if(canTextEditUpdate(ui->textEditAircraftProgressInfo))
        {
          html.clear();
          info->aircraftProgressText(data, html, mainWindow->getRouteController()->getRouteMapObjects());
          updateTextEdit(ui->textEditAircraftProgressInfo, html.getHtml());
        }
      }
      lastSimData = data;
      lastSimUpdate = QDateTime::currentDateTime().toMSecsSinceEpoch();
    }
  }
}

void InfoController::connectedToSimulator()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  ui->textEditAircraftInfo->setText("Connected. Waiting for update.");
  ui->textEditAircraftProgressInfo->setText("Connected. Waiting for update.");
}

void InfoController::disconnectedFromSimulator()
{
  Ui::MainWindow *ui = mainWindow->getUi();
  ui->textEditAircraftInfo->setText("Disconnected.");
  ui->textEditAircraftProgressInfo->setText("Disconnected.");
}
