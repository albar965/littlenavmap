/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

namespace proc {
struct MapProcedureLeg;

}

class MapQuery;

class FlightplanEntryBuilder
{
public:
  FlightplanEntryBuilder(MapQuery *mapQuery = nullptr);
  virtual ~FlightplanEntryBuilder();

  void buildFlightplanEntry(const map::MapAirport& airport, atools::fs::pln::FlightplanEntry& entry) const;

  void buildFlightplanEntry(int id, const atools::geo::Pos& userPos, map::MapObjectTypes type,
                            atools::fs::pln::FlightplanEntry& entry, bool resolveWaypoints);

  void buildFlightplanEntry(const atools::geo::Pos& userPos, const map::MapSearchResult& result,
                            atools::fs::pln::FlightplanEntry& entry, bool resolveWaypoints,
                            map::MapObjectTypes type = map::NONE);

  void buildFlightplanEntry(const map::MapSearchResult& result,
                            atools::fs::pln::FlightplanEntry& entry, bool resolveWaypoints);

  void buildFlightplanEntry(const proc::MapProcedureLeg& leg,
                            atools::fs::pln::FlightplanEntry& entry, bool resolveWaypoints);

  void entryFromUserPos(const atools::geo::Pos& userPos, atools::fs::pln::FlightplanEntry& entry);

  void entryFromNdb(const map::MapNdb& ndb, atools::fs::pln::FlightplanEntry& entry) const;

  void entryFromVor(const map::MapVor& vor, atools::fs::pln::FlightplanEntry& entry) const;

  void entryFromAirport(const map::MapAirport& airport, atools::fs::pln::FlightplanEntry& entry) const;

  void entryFromWaypoint(const map::MapWaypoint& waypoint, atools::fs::pln::FlightplanEntry& entry,
                         bool resolveWaypoints) const;

  MapQuery *getMapQuery() const
  {
    return query;
  }

  int getCurUserpointNumber() const
  {
    return curUserpointNumber;
  }

  void setCurUserpointNumber(int value)
  {
    curUserpointNumber = value;
  }

private:
  MapQuery *query = nullptr;

  bool vorForWaypoint(const map::MapWaypoint& waypoint, map::MapVor& vor) const;
  bool ndbForWaypoint(const map::MapWaypoint& waypoint, map::MapNdb& ndb) const;

  /* Used to number user defined positions */
  int curUserpointNumber = 1;
};

#endif // LITTLENAVMAP_FLIGHTPLANENTRYBUILDER_H
