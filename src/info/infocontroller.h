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

#ifndef INFOCONTROLLER_H
#define INFOCONTROLLER_H

#include <QObject>

#include "fs/sc/simconnectdata.h"
#include <common/maptypes.h>

class MainWindow;
class MapQuery;
class InfoQuery;
class MapHtmlInfoBuilder;

class InfoController :
  public QObject
{
  Q_OBJECT

public:
  InfoController(MainWindow *parent, MapQuery *mapDbQuery, InfoQuery *infoDbQuery);
  virtual ~InfoController();

  void showInformation(maptypes::MapSearchResult result);

  void saveState();
  void restoreState();

  void updateAirport();

  void preDatabaseLoad();
  void postDatabaseLoad();

  void dataPacketReceived(atools::fs::sc::SimConnectData data);
  void connectedToSimulator();
  void disconnectedFromSimulator();

private:
  int curAirportId = -1;
  bool databaseLoadStatus = false;
  atools::fs::sc::SimConnectData lastSimData;

  MainWindow *mainWindow;
  MapQuery *mapQuery;
  InfoQuery *infoQuery;
  QColor iconBackColor;
  MapHtmlInfoBuilder *info;

};

#endif // INFOCONTROLLER_H
