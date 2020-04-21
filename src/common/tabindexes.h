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

#ifndef LNM_COMMON_TABINDEXES_H
#define LNM_COMMON_TABINDEXES_H

#include <QVector>

/* Flight plan dock window tabs */
namespace rc {
enum TabRouteId
{
  ROUTE = 0,
  AIRCRAFT = 1,
  COLLECTION = 2
};

const QVector<int> TabRouteIds = {ROUTE, AIRCRAFT, COLLECTION};

}

namespace ic {

/* Info controller tabs - uses tab id instead of index for TabWidgetHandler */
enum TabInfoId
{
  INFO_AIRPORT = 0,
  INFO_RUNWAYS = 1,
  INFO_COM = 2,
  INFO_APPROACHES = 3,
  INFO_NEAREST = 4,
  INFO_WEATHER = 5,
  INFO_NAVAID = 6,
  INFO_AIRSPACE = 7,
  INFO_USERPOINT = 8,
  INFO_LOGBOOK = 9,
  INFO_ONLINE_CLIENT = 10,
  INFO_ONLINE_CENTER = 11
};

const QVector<int> TabInfoIds = {
  INFO_AIRPORT, INFO_RUNWAYS, INFO_COM, INFO_APPROACHES, INFO_NEAREST, INFO_WEATHER, INFO_NAVAID, INFO_AIRSPACE,
  INFO_USERPOINT, INFO_LOGBOOK, INFO_ONLINE_CLIENT, INFO_ONLINE_CENTER};

/* Info controller aircraft progress tabs */
enum TabAircraftId
{
  AIRCRAFT_USER = 0,
  AIRCRAFT_USER_PROGRESS = 1,
  AIRCRAFT_AI = 2
};

const QVector<int> TabAircraftIds = {AIRCRAFT_USER, AIRCRAFT_USER_PROGRESS, AIRCRAFT_AI};

}

/* Search tabs */
namespace si {

enum TabSearchId
{
  SEARCH_AIRPORT = 0,
  SEARCH_NAV = 1,
  SEARCH_PROC = 2,
  SEARCH_USER = 3,
  SEARCH_LOG = 4,
  SEARCH_ONLINE_CLIENT = 5,
  SEARCH_ONLINE_CENTER = 6,
  SEARCH_ONLINE_SERVER = 7
};

const QVector<int> TabSearchIds = {
  SEARCH_AIRPORT, SEARCH_NAV, SEARCH_PROC, SEARCH_USER, SEARCH_LOG, SEARCH_ONLINE_CLIENT, SEARCH_ONLINE_CENTER,
  SEARCH_ONLINE_SERVER};
}

#endif // LNM_COMMON_TABINDEXES_H
