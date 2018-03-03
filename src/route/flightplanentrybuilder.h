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

#ifndef LITTLENAVMAP_FLIGHTPLANENTRYBUILDER_H
#define LITTLENAVMAP_FLIGHTPLANENTRYBUILDER_H

#include "common/mapflags.h"

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

namespace map {
struct MapAirport;

struct MapNdb;

struct MapVor;

struct MapWaypoint;

struct MapUserpoint;

struct MapSearchResult;

}

namespace proc {
struct MapProcedureLeg;

}

class MapQuery;

class FlightplanEntryBuilder
{
public:
  FlightplanEntryBuilder();
  virtual ~FlightplanEntryBuilder();

  void buildFlightplanEntry(const map::MapAirport& airport, atools::fs::pln::FlightplanEntry& entry) const;

  void buildFlightplanEntry(int id, const atools::geo::Pos& userPos, map::MapObjectTypes type,
                            atools::fs::pln::FlightplanEntry& entry, bool resolveWaypoints);

  void buildFlightplanEntry(const atools::geo::Pos& userPos, const map::MapSearchResult& result,
                            atools::fs::pln::FlightplanEntry& entry, bool resolveWaypoints,
                            map::MapObjectTypes type);

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

  void entryFromUserpoint(const map::MapUserpoint& userpoint, atools::fs::pln::FlightplanEntry& entry);

  int getCurUserpointNumber() const
  {
    return curUserpointNumber;
  }

  void setCurUserpointNumber(int value)
  {
    curUserpointNumber = value;
  }

private:
  MapQuery *mapQuery = nullptr;

  bool vorForWaypoint(const map::MapWaypoint& waypoint, map::MapVor& vor) const;
  bool ndbForWaypoint(const map::MapWaypoint& waypoint, map::MapNdb& ndb) const;

  /* Used to number user defined positions */
  int curUserpointNumber = 1;
};

#endif // LITTLENAVMAP_FLIGHTPLANENTRYBUILDER_H
