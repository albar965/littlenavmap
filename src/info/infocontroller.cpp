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

#include "infocontroller.h"

#include <gui/mainwindow.h>
#include "ui_mainwindow.h"

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

InfoController::InfoController(MainWindow *parent) :
  QObject(parent), mainWindow(parent)
{

}

InfoController::~InfoController()
{

}

void InfoController::showInformation(maptypes::MapSearchResult result)
{
  qDebug() << "InfoController::showInformation";

  Ui::MainWindow *ui = mainWindow->getUi();
  int idx = ui->tabWidgetInformation->currentIndex();

  if(!result.airports.isEmpty())
  {
    if(idx != AIRPORT && idx != RUNWAYS && idx != COM && idx != APPROACHES)
      ui->tabWidgetInformation->setCurrentIndex(AIRPORT);
    ui->textEditAirportInfo->setText(result.airports.first().ident);
  }
  else if(!result.vors.isEmpty())
  {
    ui->tabWidgetInformation->setCurrentIndex(NAVAID);
    ui->textEditNavaidInfo->setText(result.vors.first().ident);
  }
  else if(!result.ndbs.isEmpty())
  {
    ui->tabWidgetInformation->setCurrentIndex(NAVAID);
    ui->textEditNavaidInfo->setText(result.ndbs.first().ident);
  }
  else if(!result.waypoints.isEmpty())
  {
    ui->tabWidgetInformation->setCurrentIndex(NAVAID);
    ui->textEditNavaidInfo->setText(result.waypoints.first().ident);
  }
}
