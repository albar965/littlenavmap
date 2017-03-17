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

class MapQuery;

class FlightplanEntryBuilder
{
public:
  FlightplanEntryBuilder(MapQuery *mapQuery = nullptr);
  virtual ~FlightplanEntryBuilder();

  void buildFlightplanEntry(const maptypes::MapAirport& airport, atools::fs::pln::FlightplanEntry& entry) const;

  void buildFlightplanEntry(int id, const atools::geo::Pos& userPos, maptypes::MapObjectTypes type,
                            atools::fs::pln::FlightplanEntry& entry, bool resolveWaypoints);

  void buildFlightplanEntry(const atools::geo::Pos& userPos, const maptypes::MapSearchResult& result,
                            atools::fs::pln::FlightplanEntry& entry, bool resolveWaypoints,
                            maptypes::MapObjectTypes type = maptypes::NONE);

  void buildFlightplanEntry(const maptypes::MapSearchResult& result,
                            atools::fs::pln::FlightplanEntry& entry, bool resolveWaypoints);

  void buildFlightplanEntry(const maptypes::MapApproachLeg& leg,
                            atools::fs::pln::FlightplanEntry& entry, bool resolveWaypoints);

  void entryFromUserPos(const atools::geo::Pos& userPos, atools::fs::pln::FlightplanEntry& entry);

  void entryFromNdb(const maptypes::MapNdb& ndb, atools::fs::pln::FlightplanEntry& entry) const;

  void entryFromVor(const maptypes::MapVor& vor, atools::fs::pln::FlightplanEntry& entry) const;

  void entryFromAirport(const maptypes::MapAirport& airport, atools::fs::pln::FlightplanEntry& entry) const;

  void entryFromWaypoint(const maptypes::MapWaypoint& waypoint, atools::fs::pln::FlightplanEntry& entry,
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

  bool vorForWaypoint(const maptypes::MapWaypoint& waypoint, maptypes::MapVor& vor) const;
  bool ndbForWaypoint(const maptypes::MapWaypoint& waypoint, maptypes::MapNdb& ndb) const;

  /* Used to number user defined positions */
  int curUserpointNumber = 1;
};

#endif // LITTLENAVMAP_FLIGHTPLANENTRYBUILDER_H
