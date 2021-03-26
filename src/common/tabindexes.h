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
  REMARKS = 1,
  AIRCRAFT = 2,
  COLLECTION = 3
};

const QVector<int> TabRouteIds = {ROUTE, REMARKS, AIRCRAFT, COLLECTION};

}

namespace ic {

/* Info controller tabs - uses tab id instead of index for TabWidgetHandler */
enum TabInfoId
{
  INFO_AIRPORT = 0,
  INFO_NAVAID = 1,
  INFO_AIRSPACE = 2,
  INFO_USERPOINT = 3,
  INFO_LOGBOOK = 4,
  INFO_ONLINE_CLIENT = 5,
  INFO_ONLINE_CENTER = 6
};

const QVector<int> TabInfoIds = {
  INFO_AIRPORT, INFO_NAVAID, INFO_AIRSPACE, INFO_USERPOINT, INFO_LOGBOOK, INFO_ONLINE_CLIENT, INFO_ONLINE_CENTER};

/* Info controller tabs - subwindow for airport */
enum TabAirportInfoId
{
  INFO_AIRPORT_OVERVIEW = 0,
  INFO_AIRPORT_RUNWAYS = 1,
  INFO_AIRPORT_COM = 2,
  INFO_AIRPORT_APPROACHES = 3,
  INFO_AIRPORT_NEAREST = 4,
  INFO_AIRPORT_WEATHER = 5
};

const QVector<int> TabAirportInfoIds = {
  INFO_AIRPORT_OVERVIEW, INFO_AIRPORT_RUNWAYS, INFO_AIRPORT_COM, INFO_AIRPORT_APPROACHES, INFO_AIRPORT_NEAREST,
  INFO_AIRPORT_WEATHER
};

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
