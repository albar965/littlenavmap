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

#ifndef LITTLENAVMAP_INFOCONTROLLER_H
#define LITTLENAVMAP_INFOCONTROLLER_H

#include "fs/sc/simconnectdata.h"
#include "common/maptypes.h"

#include <QObject>

class MainWindow;
class MapQuery;
class InfoQuery;
class HtmlInfoBuilder;
class QTextEdit;
namespace ic {
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

}

/*
 * Takes care of the information and simulator aircraft dock windows and tabs
 */
class InfoController :
  public QObject
{
  Q_OBJECT

public:
  InfoController(MainWindow *parent, MapQuery *mapDbQuery, InfoQuery *infoDbQuery);
  virtual ~InfoController();

  /* Populates all tabs in the information dock with the given results. Only one airport is shown
   * but multiple navaids can be shown in the tab. */
  void showInformation(maptypes::MapSearchResult result);

  /* Save ids of the objects shown in the tabs to content can be restored on startup */
  void saveState();
  void restoreState();

  /* Update the currently shown airport information if weather data has changed */
  void updateAirport();

  /* Clear all panels and result set */
  void preDatabaseLoad();
  void postDatabaseLoad();

  /* Update aircraft and aircraft progress tab */
  void dataPacketReceived(atools::fs::sc::SimConnectData data);
  void connectedToSimulator();
  void disconnectedFromSimulator();

  /* Program options have changed */
  void optionsChanged();

signals:
  /* Emitted when the user clicks on the "Map" link in the text browsers */
  void showPos(const atools::geo::Pos& pos, int zoom);
  void showRect(const atools::geo::Rect& rect);

private:
  /* Do not update aircraft information more than every 0.5 seconds */
  static Q_DECL_CONSTEXPR int MIN_SIM_UPDATE_TIME_MS = 500;

  void updateTextEditFontSizes();
  bool canTextEditUpdate(const QTextEdit *textEdit);
  void updateTextEdit(QTextEdit *textEdit, const QString& text);
  void setTextEditFontSize(QTextEdit *textEdit, float origSize, int percent);
  void anchorClicked(const QUrl& url);
  void clearInfoTextBrowsers();
  bool showInformationInternal(maptypes::MapSearchResult result);

  bool databaseLoadStatus = false;
  atools::fs::sc::SimConnectData lastSimData;
  qint64 lastSimUpdate = 0;

  /* Airport and navaids that are currently shown in the tabs */
  maptypes::MapSearchResult currentSearchResult;

  MainWindow *mainWindow;
  MapQuery *mapQuery;
  InfoQuery *infoQuery;
  QColor iconBackColor;
  HtmlInfoBuilder *info;

  float simInfoFontPtSize = 10.f, infoFontPtSize = 10.f;
};

#endif // LITTLENAVMAP_INFOCONTROLLER_H
