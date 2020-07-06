/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

#ifndef LNM_MAPCONTEXTMENU_H
#define LNM_MAPCONTEXTMENU_H

#include <QObject>

#include "geo/pos.h"

class MapWidget;
class QAction;
class QMenu;

namespace atools {
namespace gui {
class ActionTextSaver;
class ActionStateSaver;
}
namespace fs {
namespace sc {
class SimConnectAircraft;
class SimConnectAircraft;
class SimConnectUserAircraft;
}
}
}

namespace map {
struct MapAirport;
struct MapVor;
struct MapNdb;
struct MapWaypoint;
struct MapIls;
struct MapUserpointRoute;
struct MapAirway;
struct MapParking;
struct MapHelipad;
struct MapAirspace;
struct MapUserpoint;
struct MapLogbookEntry;
struct MapSearchResult;
}

class MapContextMenu :
  public QObject
{
  Q_OBJECT

public:
  explicit MapContextMenu(MapWidget *mapWidgetParam);
  virtual ~MapContextMenu() override;

  /* Do not allow copying */
  MapContextMenu(MapContextMenu const&) = delete;
  MapContextMenu& operator=(MapContextMenu const&) = delete;

  bool exec(QPoint menuPos, QPoint point);

  QAction *getSelectedAction() const;
  const map::MapSearchResult& getSelectedResult() const;

  bool isVisibleOnMap() const;
  bool isAircraft() const;
  bool isAirportDestination() const;
  bool isAirportDeparture() const;

  const atools::geo::Pos& getPos() const;
  const map::MapAirport *getAirport() const;
  const map::MapUserpoint *getUserpoint() const;
  const map::MapLogbookEntry *getLogEntry() const;
  const map::MapVor *getVor() const;
  const map::MapNdb *getNdb() const;
  const map::MapWaypoint *getWaypoint() const;
  const atools::fs::sc::SimConnectAircraft *getAiAircraft() const;
  const atools::fs::sc::SimConnectAircraft *getOnlineAircraft() const;
  const atools::fs::sc::SimConnectUserAircraft *getUserAircraft() const;
  const map::MapParking *getParking() const;
  const map::MapHelipad *getHelipad() const;
  const map::MapAirspace *getAirspace() const;
  const map::MapIls *getIls() const;
  const map::MapUserpointRoute *getUserpointRoute() const;
  const map::MapAirspace *getOnlineCenter() const;
  const map::MapAirway *getAirway() const;

  int getDistMarkerIndex() const;
  int getTrafficPatternIndex() const;
  int getHoldIndex() const;
  int getRangeMarkerIndex() const;
  int getRouteIndex() const;

private:
  void buildMenu(QMenu& menu);

  MapWidget *mapWidget;
  QAction *selectedAction = nullptr;
  bool visibleOnMap = false;
  bool aircraft = false;

  int distMarkerIndex = -1, trafficPatternIndex = -1, holdIndex = -1, rangeMarkerIndex = -1, routeIndex = -1;

  atools::geo::Pos pos;
  map::MapSearchResult *result;
  map::MapAirport *airport = nullptr;
  atools::fs::sc::SimConnectAircraft *aiAircraft = nullptr;
  atools::fs::sc::SimConnectAircraft *onlineAircraft = nullptr;
  atools::fs::sc::SimConnectUserAircraft *userAircraft = nullptr;
  map::MapVor *vor = nullptr;
  map::MapNdb *ndb = nullptr;
  map::MapWaypoint *waypoint = nullptr;
  map::MapIls *ils = nullptr;
  map::MapUserpointRoute *userpointRoute = nullptr;
  map::MapAirway *airway = nullptr;
  map::MapParking *parking = nullptr;
  map::MapHelipad *helipad = nullptr;
  map::MapAirspace *airspace = nullptr, *onlineCenter = nullptr;
  map::MapUserpoint *userpoint = nullptr;
  map::MapLogbookEntry *logEntry = nullptr;
  bool airportDestination = false, airportDeparture = false;

  atools::gui::ActionTextSaver *textSaver;
  atools::gui::ActionStateSaver *stateSaver;
};

#endif // LNM_MAPCONTEXTMENU_H
