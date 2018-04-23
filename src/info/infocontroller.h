/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#ifndef LITTLENAVMAP_INFOCONTROLLER_H
#define LITTLENAVMAP_INFOCONTROLLER_H

#include "fs/sc/simconnectdata.h"
#include "common/maptypes.h"

#include <QObject>

class MainWindow;
class MapQuery;
class AirportQuery;
class AirspaceQuery;
class InfoQuery;
class HtmlInfoBuilder;
class QTextEdit;

namespace ic {
enum TabIndex
{
  INFO_AIRPORT = 0,
  INFO_RUNWAYS = 1,
  INFO_COM = 2,
  INFO_APPROACHES = 3,
  INFO_WEATHER = 4,
  INFO_NAVAID = 5,
  INFO_AIRSPACE = 6,
  INFO_ONLINE_CLIENT = 7,
  INFO_ONLINE_CENTER = 8
};

enum TabIndexAircraft
{
  AIRCRAFT_USER = 0,
  AIRCRAFT_USER_PROGRESS = 1,
  AIRCRAFT_AI = 2
};

}

/*
 * Takes care of the information and simulator aircraft dock windows and tabs
 */
class InfoController :
  public QObject
{
  Q_OBJECT

public:
  InfoController(MainWindow *parent);
  virtual ~InfoController();

  /* Populates all tabs in the information dock with the given results. Only one airport is shown
   * but multiple navaids can be shown in the tab. */
  void showInformation(map::MapSearchResult result);

  /* Save ids of the objects shown in the tabs to content can be restored on startup */
  void saveState();
  void restoreState();

  /* Update the currently shown airport information if weather data has changed */
  void updateAirport();
  void updateProgress();

  /* Clear all panels and result set */
  void preDatabaseLoad();
  void postDatabaseLoad();

  /* Update aircraft and aircraft progress tab */
  void simulatorDataReceived(atools::fs::sc::SimConnectData data);
  void connectedToSimulator();
  void disconnectedFromSimulator();

  /* Program options have changed */
  void optionsChanged();

  const HtmlInfoBuilder *getHtmlInfoBuilder() const
  {
    return infoBuilder;
  }

  void updateAllInformation();

  void onlineClientAndAtcUpdated();

  void onlineNetworkChanged();

signals:
  /* Emitted when the user clicks on the "Map" link in the text browsers */
  void showPos(const atools::geo::Pos& pos, float zoom, bool doubleClick);
  void showRect(const atools::geo::Rect& rect, bool doubleClick);

private:
  /* Do not update aircraft information more than every 0.5 seconds */
  static Q_DECL_CONSTEXPR int MIN_SIM_UPDATE_TIME_MS = 500;

  void updateTextEditFontSizes();
  void setTextEditFontSize(QTextEdit *textEdit, float origSize, int percent);
  void anchorClicked(const QUrl& url);
  void clearInfoTextBrowsers();
  void showInformationInternal(map::MapSearchResult result, bool showWindows);
  void updateAiAirports(const atools::fs::sc::SimConnectData& data);
  void updateAirportInternal(bool newAirport);
  void currentTabChanged(int index);
  void updateUserAircraftText();
  void updateAircraftProgressText();
  void updateAiAircraftText();
  void updateAircraftInfo();

  bool databaseLoadStatus = false;
  atools::fs::sc::SimConnectData lastSimData;
  qint64 lastSimUpdate = 0;

  /* Airport and navaids that are currently shown in the tabs */
  map::MapSearchResult currentSearchResult;

  MainWindow *mainWindow = nullptr;
  MapQuery *mapQuery = nullptr;
  AirspaceQuery *airspaceQuery = nullptr;
  AirspaceQuery *airspaceQueryOnline = nullptr;
  AirportQuery *airportQuery = nullptr;
  HtmlInfoBuilder *infoBuilder = nullptr;

  float simInfoFontPtSize = 10.f, infoFontPtSize = 10.f;

};

#endif // LITTLENAVMAP_INFOCONTROLLER_H
