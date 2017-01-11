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

#ifndef LITTLENAVMAP_MAPTYPESFACTORY_H
#define LITTLENAVMAP_MAPTYPESFACTORY_H

#include "common/maptypes.h"

namespace atools {
namespace sql {

class SqlRecord;
}
}

/*
 * Create all map objects (namespace maptypes) from sql records. The sql records can be
 * a result from sql queries or manually built.
 */
class MapTypesFactory
{
public:
  MapTypesFactory();
  ~MapTypesFactory();

  /*
   * Populate airport object.
   * @param complete if false only id and position are present in the record. Used for creating the object
   * based on incomplete records in the search.
   */
  void fillAirport(const atools::sql::SqlRecord& record, maptypes::MapAirport& airport, bool complete);

  /* Populate airport from queries based on the overview tables airport_medium and airport_large. */
  void fillAirportForOverview(const atools::sql::SqlRecord& record, maptypes::MapAirport& airport);

  /*
   * @param overview if true fill only fields needed for airport overview symbol (white filled runways)
   */
  void fillRunway(const atools::sql::SqlRecord& record, maptypes::MapRunway& runway, bool overview);

  void fillVor(const atools::sql::SqlRecord& record, maptypes::MapVor& vor);
  void fillVorFromNav(const atools::sql::SqlRecord& record, maptypes::MapVor& vor);

  void fillNdb(const atools::sql::SqlRecord& record, maptypes::MapNdb& ndb);

  void fillWaypoint(const atools::sql::SqlRecord& record, maptypes::MapWaypoint& waypoint);
  void fillWaypointFromNav(const atools::sql::SqlRecord& record, maptypes::MapWaypoint& waypoint);

  void fillAirway(const atools::sql::SqlRecord& record, maptypes::MapAirway& airway);
  void fillMarker(const atools::sql::SqlRecord& record, maptypes::MapMarker& marker);
  void fillIls(const atools::sql::SqlRecord& record, maptypes::MapIls& ils);

  void fillParking(const atools::sql::SqlRecord& record, maptypes::MapParking& parking);
  void fillStart(const atools::sql::SqlRecord& record, maptypes::MapStart& start);

private:
  void fillVorBase(const atools::sql::SqlRecord& record, maptypes::MapVor& vor);

  void fillAirportBase(const atools::sql::SqlRecord& record, maptypes::MapAirport& ap, bool complete);

  maptypes::MapAirportFlags airportFlag(const atools::sql::SqlRecord& record, const QString& field,
                                        maptypes::MapAirportFlags airportFlag);
  maptypes::MapAirportFlags fillAirportFlags(const atools::sql::SqlRecord& record, bool overview);

};

#endif // LITTLENAVMAP_MAPTYPESFACTORY_H
