/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel alex@littlenavmap.org
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

/* Flight plan dock window tabs */
namespace rc {
enum TabIndex
{
  ROUTE = 0,
  AIRCRAFT = 1,
  COLLECTION = 2
};

}

namespace ic {

/* Info controller tabs */
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

/* Info controller aircraft progress tabs */
enum TabIndexAircraft
{
  AIRCRAFT_USER = 0,
  AIRCRAFT_USER_PROGRESS = 1,
  AIRCRAFT_AI = 2
};

}

/* Search tabs */
namespace si {

enum SearchTabIndex
{
  SEARCH_AIRPORT = 0,
  SEARCH_NAV = 1,
  SEARCH_PROC = 2,
  SEARCH_USER = 3,
  SEARCH_ONLINE_CLIENT = 4,
  SEARCH_ONLINE_CENTER = 5,
  SEARCH_ONLINE_SERVER = 6
};

}

#endif // LNM_COMMON_TABINDEXES_H
