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

#ifndef LITTLENAVMAP_AIRSPACEQUERY_H
#define LITTLENAVMAP_AIRSPACEQUERY_H

#include "query/querytypes.h"

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
 * Several instances for different databases might be used depending on MapAirspaceSources src value.
 */
class AirspaceQuery
{
public:
  AirspaceQuery(atools::sql::SqlDatabase *sqlDb, map::MapAirspaceSources src);
  ~AirspaceQuery();

  void getAirspaceById(map::MapAirspace& airspace, int airspaceId);

  map::MapAirspace getAirspaceById(int airspaceId);
  bool hasAirspaceById(int airspaceId);

  /* Get record with all rows from atc table - only online airspaces */
  atools::sql::SqlRecord getOnlineAirspaceRecordById(int airspaceId);

  /* Only for not online databases - includes values from metadata tables */
  atools::sql::SqlRecord getAirspaceInfoRecordById(int airspaceId);

  /* Get airspaces for map display */
  const QList<map::MapAirspace> *getAirspaces(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                              map::MapAirspaceFilter filter, float flightPlanAltitude, bool lazy);
  const atools::geo::LineString *getAirspaceGeometryByName(int airspaceId);

  /* Query raw geometry blob by online callsign (name) and facility type */
  atools::geo::LineString *getAirspaceGeometryByName(const QString& callsign, const QString& facilityType);

  /* Tries to fetch online airspace geometry by  file name. */
  atools::geo::LineString *getAirspaceGeometryByFile(QString callsign);

  /* True if tables atc or boundary have content. Updated in clearCache and initQueries */
  bool hasAirspacesDatabase()
  {
    return hasAirspaces;
  }

  /* Create and prepare all queries */
  void initQueries();

  /* Close all query objects thus disconnecting from the database */
  void deInitQueries();

  /* Clear all internal caches after reloading online centers */
  void clearCache();

private:
  void updateAirspaceStatus();

  MapTypesFactory *mapTypesFactory;
  atools::sql::SqlDatabase *db;

  /* Simple bounding rectangle caches */
  query::SimpleRectCache<map::MapAirspace> airspaceCache;
  map::MapAirspaceFilter lastAirspaceFilter = {map::AIRSPACE_NONE, map::AIRSPACE_FLAG_NONE};
  float lastFlightplanAltitude = 0.f;

  /* ID/object caches */
  QCache<int, atools::geo::LineString> airspaceLineCache;
  QCache<QString, atools::geo::LineString> onlineCenterGeoCache, onlineCenterGeoFileCache;

  static int queryMaxRows;

  /* True if tables atc or boundary have content. Updated in clearCache and initQueries */
  bool hasAirspaces = false,
  /* true if database contains new FIR/UIR types */
       hasFirUir = false;

  /* Database queries */
  atools::sql::SqlQuery *airspaceByRectQuery = nullptr, *airspaceByRectBelowAltQuery = nullptr,
                        *airspaceByRectAboveAltQuery = nullptr, *airspaceByRectAtAltQuery = nullptr,
                        *airspaceLinesByIdQuery = nullptr, *airspaceGeoByNameQuery = nullptr,
                        *airspaceGeoByFileQuery = nullptr, *airspaceByIdQuery = nullptr, *airspaceInfoQuery = nullptr;

  /* Source database definition */
  map::MapAirspaceSources source;
};

#endif // LITTLENAVMAP_AIRSPACEQUERY_H
