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

#ifndef LITTLENAVMAP_FLIGHTPLANENTRYBUILDER_H
#define LITTLENAVMAP_FLIGHTPLANENTRYBUILDER_H

#include "common/maptypes.h"

namespace atools {
namespace geo {
class Pos;
}
namespace fs {
namespace pln {
class Flightplan;
class FlightplanEntry;
}
}
}

class MapQuery;

class FlightplanEntryBuilder
{
public:
  FlightplanEntryBuilder(MapQuery *mapQuery = nullptr);
  virtual ~FlightplanEntryBuilder();

  void buildFlightplanEntry(const maptypes::MapAirport& airport, atools::fs::pln::FlightplanEntry& entry);

  void buildFlightplanEntry(int id, const atools::geo::Pos& userPos, maptypes::MapObjectTypes type,
                            atools::fs::pln::FlightplanEntry& entry, bool resolveWaypoints,
                            int curUserpointNumber);

  void buildFlightplanEntry(const atools::geo::Pos& userPos, const maptypes::MapSearchResult& result,
                            atools::fs::pln::FlightplanEntry& entry, bool resolveWaypoints,
                            int curUserpointNumber, maptypes::MapObjectTypes type = maptypes::NONE);

  void buildFlightplanEntry(const maptypes::MapSearchResult& result,
                            atools::fs::pln::FlightplanEntry& entry, bool resolveWaypoints);

  void entryFromUserPos(const atools::geo::Pos& userPos, atools::fs::pln::FlightplanEntry& entry,
                        int curUserpointNumber);

  void entryFromNdb(const maptypes::MapNdb& ndb, atools::fs::pln::FlightplanEntry& entry);

  void entryFromVor(const maptypes::MapVor& vor, atools::fs::pln::FlightplanEntry& entry);

  void entryFromAirport(const maptypes::MapAirport& airport, atools::fs::pln::FlightplanEntry& entry);

  void entryFromWaypoint(const maptypes::MapWaypoint& waypoint, atools::fs::pln::FlightplanEntry& entry,
                         bool resolveWaypoints);

  MapQuery *getMapQuery() const
  {
    return query;
  }

private:
  MapQuery *query = nullptr;

};

#endif // LITTLENAVMAP_FLIGHTPLANENTRYBUILDER_H
