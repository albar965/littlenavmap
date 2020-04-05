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

#ifndef LITTLENAVMAP_MAPQUERY_H
#define LITTLENAVMAP_MAPQUERY_H

#include "query/querytypes.h"

#include <QCache>

namespace atools {
namespace geo {
class Rect;
}
namespace sql {
class SqlDatabase;
class SqlQuery;
}
}

class CoordinateConverter;
class MapTypesFactory;
class MapLayer;

/*
 * Provides map related database queries.
 *
 * All ids are database ids.
 */
class MapQuery
{
public:
  /*
   * @param sqlDb database for simulator scenery data
   * @param sqlDbNav for updated navaids
   */
  MapQuery(atools::sql::SqlDatabase *sqlDb, atools::sql::SqlDatabase *sqlDbNav,
           atools::sql::SqlDatabase *sqlDbUser);
  ~MapQuery();

  /* Convert airport instances from/to simulator and third party nav databases */
  map::MapAirport  getAirportSim(const map::MapAirport& airport);
  map::MapAirport  getAirportNav(const map::MapAirport& airport);
  void getAirportSimReplace(map::MapAirport& airport);
  void getAirportNavReplace(map::MapAirport& airport);

  /* If waypoint is of type VOR get the related VOR object */
  void getVorForWaypoint(map::MapVor& vor, int waypointId);
  void getVorNearest(map::MapVor& vor, const atools::geo::Pos& pos);

  /* If waypoint is of type NDB get the related NDB object */
  void getNdbForWaypoint(map::MapNdb& ndb, int waypointId);
  void getNdbNearest(map::MapNdb& ndb, const atools::geo::Pos& pos);

  /* Get map objects by unique database id  */
  /* From nav db, depending on mode */
  map::MapVor getVorById(int id);

  /* From nav db, depending on mode */
  map::MapNdb getNdbById(int id);

  /* Always from sim db */
  map::MapIls getIlsById(int id);

  QVector<map::MapIls> getIlsByAirportAndRunway(const QString& airportIdent, const QString& runway);

  /* Get runway end and try lower and higher numbers if nothing was found - adds a dummy entry with airport
   * position if no runway ends were found */
  void getRunwayEndByNameFuzzy(QList<map::MapRunwayEnd>& runwayEnds, const QString& name,
                               const map::MapAirport& airport, bool navData);

  /*
   * Get a map object by type, ident and region
   * @param result will receive objects based on type
   * @param type AIRPORT, VOR, NDB or WAYPOINT
   * @param ident ICAO ident
   * @param region two letter ICAO region code
   */
  void getMapObjectByIdent(map::MapSearchResult& result, map::MapObjectTypes type,
                           const QString& ident, const QString& region = QString(), const QString& airport = QString(),
                           const atools::geo::Pos& sortByDistancePos = atools::geo::EMPTY_POS,
                           float maxDistance = map::INVALID_DISTANCE_VALUE, bool airportFromNavDatabase = false);

  void getMapObjectByIdent(map::MapSearchResult& result, map::MapObjectTypes type,
                           const QString& ident, const QString& region,
                           const QString& airport, bool airportFromNavDatabase);

  /*
   * Get a map object by type and id
   * @param result will receive objects based on type
   * @param type AIRPORT, VOR, NDB or WAYPOINT
   * @param id database id
   */
  void getMapObjectById(map::MapSearchResult& result, map::MapObjectTypes type, map::MapAirspaceSources src, int id,
                        bool airportFromNavDatabase);

  /*
   * Get objects near a screen coordinate from the cache which will cover all visible objects.
   * No objects are loaded from the database.
   *
   * @param conv Converter to calcualte screen coordinates
   * @param mapLayer current map layer
   * @param airportDiagram get nearest parking and helipads too
   * @param types map objects to fetch AIRPORT, VOR, NDB, WAYPOINT, AIRWAY, etc.
   * @param xs/ys Screen coordinates
   * @param screenDistance maximum distance to coordinates
   * @param result will receive objects based on type
   */
  void getNearestScreenObjects(const CoordinateConverter& conv, const MapLayer *mapLayer, bool airportDiagram,
                               map::MapObjectTypes types, int xs, int ys, int screenDistance,
                               map::MapSearchResult& result);

  /* Only VOR, NDB, ILS and waypoints
   * All sorted by distance to pos with a maximum distance distanceNm
   * Uses distance * 4 and searches again if nothing was found.*/
  map::MapSearchResultIndex *getNearestNavaids(const atools::geo::Pos& pos, float distanceNm,
                                               map::MapObjectTypes type, int maxIls, float maxIlsDist);

  /*
   * Fetch airports for a map coordinate rectangle.
   * Fill objects of the maptypes namespace and maintains a cache.
   * Objects from methods returning a pointer to a list might be deleted from the cache and should be copied
   * if they have to be kept between event loop calls.
   * @param rect bounding rectangle for query
   * @param mapLayer used to find source table
   * @param lazy do not reload from database and return (probably incomplete) result from cache if true
   * @return pointer to airport cache. Create a copy if this is needed for a longer
   * time than for e.g. one drawing request.
   */
  const QList<map::MapAirport> *getAirports(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy);

  /* Similar to getAirports */
  const QList<map::MapVor> *getVors(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy);

  /* Similar to getAirports */
  const QList<map::MapNdb> *getNdbs(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy);

  /* Similar to getAirports */
  const QList<map::MapMarker> *getMarkers(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy);

  /* Similar to getAirports */
  const QList<map::MapIls> *getIls(Marble::GeoDataLatLonBox rect, const MapLayer *mapLayer, bool lazy);

  /* Get a partially filled runway list for the overview */
  const QList<map::MapRunway> *getRunwaysForOverview(int airportId);

  /* Similar to getAirports but no caching since user points can change */
  const QList<map::MapUserpoint> getUserdataPoints(const Marble::GeoDataLatLonBox& rect, const QStringList& types,
                                                   const QStringList& typesAll,
                                                   bool unknownType, float distance);

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();

  bool hasAnyArrivalProcedures(const map::MapAirport& airport);
  bool hasDepartureProcedures(const map::MapAirport& airport);

private:
  map::MapSearchResultIndex *nearestNavaidsInternal(const atools::geo::Pos& pos, float distanceNm,
                                                    map::MapObjectTypes type, int maxIls, float maxIlsDist);

  void mapObjectByIdentInternal(map::MapSearchResult& result, map::MapObjectTypes type,
                                const QString& ident, const QString& region, const QString& airport,
                                const atools::geo::Pos& sortByDistancePos,
                                float maxDistance, bool airportFromNavDatabase);

  const QList<map::MapAirport> *fetchAirports(const Marble::GeoDataLatLonBox& rect,
                                              atools::sql::SqlQuery *query,
                                              bool lazy, bool overview);
  QVector<map::MapIls> ilsByAirportAndRunway(const QString& airportIdent, const QString& runway);

  void runwayEndByNameFuzzy(QList<map::MapRunwayEnd>& runwayEnds, const QString& name, const map::MapAirport& airport,
                            bool navData);

  MapTypesFactory *mapTypesFactory;
  atools::sql::SqlDatabase *dbSim, *dbNav, *dbUser;

  /* Simple bounding rectangle caches */
  query::SimpleRectCache<map::MapAirport> airportCache;
  query::SimpleRectCache<map::MapUserpoint> userpointCache;
  query::SimpleRectCache<map::MapVor> vorCache;
  query::SimpleRectCache<map::MapNdb> ndbCache;
  query::SimpleRectCache<map::MapMarker> markerCache;
  query::SimpleRectCache<map::MapIls> ilsCache;

  /* ID/object caches */
  QCache<int, QList<map::MapRunway> > runwayOverwiewCache;
  QCache<query::NearestCacheKeyNavaid, map::MapSearchResultIndex> nearestNavaidCache;

  static int queryMaxRows;

  /* Database queries */
  atools::sql::SqlQuery *runwayOverviewQuery = nullptr,
                        *airportByRectQuery = nullptr, *airportMediumByRectQuery = nullptr,
                        *airportLargeByRectQuery = nullptr;

  atools::sql::SqlQuery *vorsByRectQuery = nullptr, *ndbsByRectQuery = nullptr, *markersByRectQuery = nullptr,
                        *ilsByRectQuery = nullptr, *userdataPointByRectQuery = nullptr;

  atools::sql::SqlQuery *vorByIdentQuery = nullptr, *ndbByIdentQuery = nullptr, *ilsByIdentQuery = nullptr;

  atools::sql::SqlQuery *vorByIdQuery = nullptr, *ndbByIdQuery = nullptr, *vorByWaypointIdQuery = nullptr,
                        *ndbByWaypointIdQuery = nullptr, *ilsByIdQuery = nullptr, *ilsQuerySimByName = nullptr,
                        *vorNearestQuery = nullptr,
                        *ndbNearestQuery = nullptr, *userdataPointByIdQuery = nullptr;
};

#endif // LITTLENAVMAP_MAPQUERY_H
