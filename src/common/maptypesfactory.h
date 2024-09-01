/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#ifndef LITTLENAVMAP_MAPTYPESFACTORY_H
#define LITTLENAVMAP_MAPTYPESFACTORY_H

#include "common/mapflags.h"

namespace atools {
namespace sql {

class SqlRecord;
}
}

namespace map {
struct MapAirport;
struct MapRunway;
struct MapRunwayEnd;
struct MapVor;
struct MapNdb;
struct MapHolding;
struct MapWaypoint;
struct MapAirway;
struct MapIls;
struct MapParking;
struct MapAirspace;
struct MapStart;
struct MapHelipad;
struct MapMarker;
struct MapUserpoint;
struct MapLogbookEntry;
struct MapAirportMsa;
}

/*
 * Create all map objects (namespace maptypes) from sql records. The sql records can be
 * a result from sql queries or manually built.
 */
class MapTypesFactory
{
public:
  /*
   * Populate airport object.
   * @param complete if false only id and position are present in the record. Used for creating the object
   * based on incomplete records in the search.
   * @param nav filled from third party nav database
   */
  void fillAirport(const atools::sql::SqlRecord& record, map::MapAirport& airport, bool complete, bool nav,
                   bool xplane);

  /*
   * @param overview if true fill only fields needed for airport overview symbol (white filled runways)
   */
  void fillRunway(const atools::sql::SqlRecord& record, map::MapRunway& runway, bool overview);
  void fillRunwayEnd(const atools::sql::SqlRecord& record, map::MapRunwayEnd& end, bool nav);

  void fillVor(const atools::sql::SqlRecord& record, map::MapVor& vor);
  void fillVorFromNav(const atools::sql::SqlRecord& record, map::MapVor& vor); /* Uses nav_search table flags */

  void fillNdb(const atools::sql::SqlRecord& record, map::MapNdb& ndb);

  void fillWaypoint(const atools::sql::SqlRecord& record, map::MapWaypoint& waypoint, bool track);
  void fillWaypointFromNav(const atools::sql::SqlRecord& record, map::MapWaypoint& waypoint);

  void fillAirwayOrTrack(const atools::sql::SqlRecord& record, map::MapAirway& airway, bool track);
  void fillMarker(const atools::sql::SqlRecord& record, map::MapMarker& marker);
  void fillIls(const atools::sql::SqlRecord& record, map::MapIls& ils, float runwayHeadingTrue = map::INVALID_HEADING_VALUE);
  void fillHolding(const atools::sql::SqlRecord& record, map::MapHolding& holding);
  void fillAirportMsa(const atools::sql::SqlRecord& record, map::MapAirportMsa& airportMsa);

  void fillParking(const atools::sql::SqlRecord& record, map::MapParking& parking);
  void fillStart(const atools::sql::SqlRecord& record, map::MapStart& start);

  void fillAirspace(const atools::sql::SqlRecord& record, map::MapAirspace& airspace, map::MapAirspaceSource src);

  void fillHelipad(const atools::sql::SqlRecord& record, map::MapHelipad& helipad);

  void fillUserdataPoint(const atools::sql::SqlRecord& rec, map::MapUserpoint& obj);

  void fillLogbookEntry(const atools::sql::SqlRecord& rec, map::MapLogbookEntry& obj);

private:
  void fillVorBase(const atools::sql::SqlRecord& record, map::MapVor& vor);
  void fillAirportBase(const atools::sql::SqlRecord& record, map::MapAirport& ap, bool complete);
  map::MapAirportFlags airportFlag(const atools::sql::SqlRecord& record, const QString& field, map::MapAirportFlags airportFlag);
  map::MapAirportFlags fillAirportFlags(const atools::sql::SqlRecord& record, bool overview);
  map::MapType strToType(const QString& navType);

};

#endif // LITTLENAVMAP_MAPTYPESFACTORY_H
