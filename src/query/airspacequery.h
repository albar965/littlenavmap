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

#ifndef LITTLENAVMAP_AIRSPACEQUERY_H
#define LITTLENAVMAP_AIRSPACEQUERY_H

#include "query/querytypes.h"
#include "common/maptypes.h"

#include <QCache>

namespace atools {
namespace geo {
class Rect;
}
namespace sql {
class SqlDatabase;
class SqlQuery;
class SqlRecord;
}
}

class MapTypesFactory;
class MapLayer;

/*
 * Provides map related database queries around airspaces. Fill objects of the maptypes namespace and maintains a cache.
 * Objects from methods returning a pointer to a list might be deleted from the cache and should be copied
 * if they have to be kept between event loop calls.
 * All ids are database ids.
 *
 * Two instance are used in LNM. One for normal airspaces and one for online ATC.
 */
class AirspaceQuery
  : public QObject
{
  Q_OBJECT

public:
  /*
   * @param sqlDb database for simulator scenery data
   * @param sqlDbNav for updated navaids
   */
  AirspaceQuery(QObject *parent, atools::sql::SqlDatabase *sqlDb, bool onlineSchema);
  ~AirspaceQuery();

  void getAirspaceById(map::MapAirspace& airspace, int airspaceId);

  map::MapAirspace getAirspaceById(int airspaceId);

  /* Get record with all rows from atc table */
  atools::sql::SqlRecord getAirspaceRecordById(int airspaceId);

  const QList<map::MapAirspace> *getAirspaces(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                              map::MapAirspaceFilter filter, float flightPlanAltitude, bool lazy);
  const atools::geo::LineString *getAirspaceGeometry(int boundaryId);

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();
  void clearCache();

private:
  MapTypesFactory *mapTypesFactory;
  atools::sql::SqlDatabase *db;

  /* Simple bounding rectangle caches */
  SimpleRectCache<map::MapAirspace> airspaceCache;
  map::MapAirspaceFilter lastAirspaceFilter = {map::AIRSPACE_NONE, map::AIRSPACE_FLAG_NONE};
  float lastFlightplanAltitude = 0.f;

  /* ID/object caches */
  QCache<int, atools::geo::LineString> airspaceLineCache;

  static int queryMaxRows;

  /* Database queries */
  atools::sql::SqlQuery *airspaceByRectQuery = nullptr, *airspaceByRectBelowAltQuery = nullptr,
                        *airspaceByRectAboveAltQuery = nullptr, *airspaceByRectAtAltQuery = nullptr,
                        *airspaceLinesByIdQuery = nullptr, *airspaceByIdQuery = nullptr;

  bool online;
};

#endif // LITTLENAVMAP_AIRSPACEQUERY_H
