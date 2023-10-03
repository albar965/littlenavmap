/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

namespace map {
struct MapResult;
struct MapResultIndex;
}

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
  MapQuery(atools::sql::SqlDatabase *sqlDbSim, atools::sql::SqlDatabase *sqlDbNav, atools::sql::SqlDatabase *sqlDbUser);
  ~MapQuery();

  MapQuery(const MapQuery& other) = delete;
  MapQuery& operator=(const MapQuery& other) = delete;

  /* Convert airport instances from/to simulator and third party nav databases */
  map::MapAirport  getAirportSim(const map::MapAirport& airport) const;
  map::MapAirport  getAirportNav(const map::MapAirport& airport) const;
  void getAirportSimReplace(map::MapAirport& airport) const;
  void getAirportNavReplace(map::MapAirport& airport) const;

  /* Get from nav or sim database trying nav first. Values are zero if not available */
  void getAirportTransitionAltiudeAndLevel(const map::MapAirport& airport, float& transitionAltitude, float& transitionLevel) const;

  /* If waypoint is of type VOR get the related VOR object */
  void getVorForWaypoint(map::MapVor& vor, int waypointId) const;
  void getVorNearest(map::MapVor& vor, const atools::geo::Pos& pos) const;

  /* If waypoint is of type NDB get the related NDB object */
  void getNdbForWaypoint(map::MapNdb& ndb, int waypointId) const;
  void getNdbNearest(map::MapNdb& ndb, const atools::geo::Pos& pos) const;

  /* Fetch VOR and NDB features if a waypoint in allWaypoints is one of these types.
   * Additionally filter waypoints for airway/track types. */
  void resolveWaypointNavaids(const QList<map::MapWaypoint>& allWaypoints, QHash<int, map::MapWaypoint>& waypoints,
                              QHash<int, map::MapVor>& vors, QHash<int, map::MapNdb>& ndbs, bool flightplan,
                              bool normalWaypoints, bool victorWaypoints, bool jetWaypoints, bool trackWaypoints) const;

  /* Get map objects by unique database id  */
  /* From nav db, depending on mode */
  map::MapVor getVorById(int id) const;

  /* From nav db, depending on mode */
  map::MapNdb getNdbById(int id) const;

  /* Always from nav db */
  map::MapIls getIlsById(int id) const;

  /* Either nav or sim db in this order where available */
  map::MapAirportMsa getAirportMsaById(int id) const;

  /* True if table ils contains GLS/RNP approaches - GLS ground stations or GBAS threshold points */
  bool hasGls() const
  {
    return gls;
  }

  /* Either nav or sim db in this order where available */
  map::MapHolding getHoldingById(int id) const;

  /* True if table is present in schema and has one row */
  bool hasHoldings() const
  {
    return holdingByIdQuery != nullptr;
  }

  /* True if table is present in schema and has one row */
  bool hasAirportMsa() const
  {
    return airportMsaByIdQuery != nullptr;
  }

  /* Get ILS from sim database based on airport ident and runway name.
   * Runway name can be zero prefixed or prefixed with "RW". */
  const QVector<map::MapIls> getIlsByAirportAndRunway(const QString& airportIdent, const QString& runway) const;

  /* Get ILS from sim database based on airport ident and ILS ident. Uses exact match. */
  QVector<map::MapIls> getIlsByAirportAndIdent(const QString& airportIdent, const QString& ilsIdent) const;

  /* Get runway end and try lower and higher numbers if nothing was found - adds a dummy entry with airport
   * position if no runway ends were found */
  void getRunwayEndByNameFuzzy(QList<map::MapRunwayEnd>& runwayEnds, const QString& name,
                               const map::MapAirport& airport, bool navData) const;

  /*
   * Get a map object by type, ident and region. Results are appended.
   * @param result will receive objects based on type
   * @param type AIRPORT, VOR, NDB or WAYPOINT
   * @param ident ICAO ident
   * @param region two letter ICAO region code
   */
  void getMapObjectByIdent(map::MapResult& result, map::MapTypes type, const QString& ident, const QString& region = QString(),
                           const QString& airport = QString(), const atools::geo::Pos& sortByDistancePos = atools::geo::EMPTY_POS,
                           float maxDistanceMeter = map::INVALID_DISTANCE_VALUE, bool airportFromNavDatabase = false,
                           map::AirportQueryFlags flags = map::AP_QUERY_ALL) const;

  void getMapObjectByIdent(map::MapResult& result, map::MapTypes type, const QString& ident, const QString& region,
                           const QString& airport, bool airportFromNavDatabase, map::AirportQueryFlags flags = map::AP_QUERY_ALL) const;

  /*
   * Get a map object by type and id
   * @param result will receive objects based on type
   * @param type AIRPORT, VOR, NDB or WAYPOINT
   * @param id database id
   */
  void getMapObjectById(map::MapResult& result, map::MapTypes type, map::MapAirspaceSources src, int id,
                        bool airportFromNavDatabase) const;

  /*
   * Get objects near a screen coordinate from the cache which will cover all visible objects.
   * No objects are loaded from the database.
   *
   * Note that the used cache is related to the used MapPaintWidget.
   *
   * @param conv Converter to calcualte screen coordinates
   * @param mapLayer current map layer
   * @param airportDiagram get nearest parking and helipads too
   * @param types map objects to fetch AIRPORT, VOR, NDB, WAYPOINT, AIRWAY, etc.
   * @param xs/ys Screen coordinates
   * @param screenDistance maximum distance to coordinates
   * @param result will receive objects based on type
   */
  void getNearestScreenObjects(const CoordinateConverter& conv, const MapLayer *mapLayer, const QSet<int>& shownDetailAirportIds,
                               bool airportDiagram,
                               map::MapTypes types, map::MapDisplayTypes displayTypes, int xs, int ys, int screenDistance,
                               map::MapResult& result) const;

  /* Only VOR, NDB, ILS and waypoints
   * All sorted by distance to pos with a maximum distance distanceNm
   * Uses distance * 4 and searches again if nothing was found.*/
  map::MapResultIndex *getNearestNavaids(const atools::geo::Pos& pos, float distanceNm,
                                         map::MapTypes type, int maxIls, float maxIlsDistNm);

  /*
   * Fetch airports for a map coordinate rectangle.
   * Fill objects of the maptypes namespace and maintains a cache.
   * Objects from methods returning a pointer to a list might be deleted from the cache and should be copied
   * if they have to be kept between event loop calls.
   *
   * Note that the cache is related to the used MapPaintWidget.
   *
   * @param rect bounding rectangle for query
   * @param mapLayer used to find source table
   * @param addon Force addon display
   * @param overflow Returns true in case of query overflow
   * @param lazy do not reload from database and return (probably incomplete) result from cache if true
   * @return pointer to airport cache. Create a copy if this is needed for a longer
   * time than for e.g. one drawing request.
   */
  const QList<map::MapAirport> *getAirports(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy,
                                            map::MapTypes types, bool& overflow);

  const QList<map::MapAirport> *getAirportsByRect(const atools::geo::Rect& rect, const MapLayer *mapLayer, bool lazy, map::MapTypes types,
                                                  bool& overflow);

  /* Similar to getAirports */
  const QList<map::MapVor> *getVors(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy, bool& overflow);

  const QList<map::MapVor> *getVorsByRect(const atools::geo::Rect& rect, const MapLayer *mapLayer, bool lazy,
                                          bool& overflow);

  /* Similar to getAirports */
  const QList<map::MapNdb> *getNdbs(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy, bool& overflow);

  const QList<map::MapNdb> *getNdbsByRect(const atools::geo::Rect& rect, const MapLayer *mapLayer, bool lazy,
                                          bool& overflow);

  /* Similar to getAirports */
  const QList<map::MapMarker> *getMarkers(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy, bool& overflow);

  const QList<map::MapMarker> *getMarkersByRect(const atools::geo::Rect& rect, const MapLayer *mapLayer, bool lazy,
                                                bool& overflow);

  /* Similar to getAirports */
  const QList<map::MapHolding> *getHoldings(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy, bool& overflow);

  /* As above */
  const QList<map::MapAirportMsa> *getAirportMsa(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy, bool& overflow);

  /* Similar to getAirports */
  const QList<map::MapIls> *getIls(Marble::GeoDataLatLonBox rect, const MapLayer *mapLayer, bool lazy, bool& overflow);

  /* Get a partially filled runway list for the overview */
  const QList<map::MapRunway> *getRunwaysForOverview(int airportId);

  /* Similar to getAirports but no caching since user points can change */
  const QList<map::MapUserpoint> getUserdataPoints(const Marble::GeoDataLatLonBox& rect, const QStringList& types,
                                                   const QStringList& typesAll, bool unknownType, float distanceNm);

  /* Get related airport for navaids from current nav database.
   * found is true if navaid search was successful and max distance to pos is not exceeded. */
  QString getAirportIdentFromWaypoint(const QString& ident, const QString& region, const atools::geo::Pos& pos, bool found) const;
  QString getAirportIdentFromVor(const QString& ident, const QString& region, const atools::geo::Pos& pos, bool found) const;
  QString getAirportIdentFromNdb(const QString& ident, const QString& region, const atools::geo::Pos& pos, bool found) const;

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();

  /* Check in navdatabase (Navigraph or other) if airport has procedures.
   * Slow but does a fuzzy search to find airport in navdata */
  bool hasProcedures(const map::MapAirport& airport) const;
  bool hasArrivalProcedures(const map::MapAirport& airport) const;
  bool hasDepartureProcedures(const map::MapAirport& airport) const;

private:
  map::MapResultIndex *nearestNavaidsInternal(const atools::geo::Pos& pos, float distanceNm,
                                              map::MapTypes type, int maxIls, float maxIlsDist);

  void mapObjectByIdentInternal(map::MapResult& result, map::MapTypes type,
                                const QString& ident, const QString& region, const QString& airport,
                                const atools::geo::Pos& sortByDistancePos,
                                float maxDistanceMeter, bool airportFromNavDatabase, map::AirportQueryFlags flags) const;

  const QList<map::MapAirport> *fetchAirports(const Marble::GeoDataLatLonBox& rect, atools::sql::SqlQuery *query,
                                              bool lazy, bool overview, bool addon, bool normal, bool& overflow);

  QVector<map::MapIls> ilsByAirportAndRunway(const QString& airportIdent, const QString& runway) const;

  void runwayEndByNameFuzzy(QList<map::MapRunwayEnd>& runwayEnds, const QString& name, const map::MapAirport& airport,
                            bool navData) const;
  QString airportIdentFromQuery(const QString& queryStr, const QString& ident, const QString& region,
                                const atools::geo::Pos& pos, bool& found) const;

  MapTypesFactory *mapTypesFactory;
  atools::sql::SqlDatabase *dbSim, *dbNav, *dbUser;

  /* Simple bounding rectangle caches */
  bool airportCacheAddonFlag = false; // Keep addon status flag for comparing
  bool airportCacheNormalFlag = false; // Keep normal (non add-on) status flag for comparing
  query::SimpleRectCache<map::MapAirport> airportCache;
  query::SimpleRectCache<map::MapUserpoint> userpointCache;
  query::SimpleRectCache<map::MapVor> vorCache;
  query::SimpleRectCache<map::MapNdb> ndbCache;
  query::SimpleRectCache<map::MapMarker> markerCache;
  query::SimpleRectCache<map::MapHolding> holdingCache;
  query::SimpleRectCache<map::MapIls> ilsCache;
  query::SimpleRectCache<map::MapAirportMsa> airportMsaCache;

  bool gls = false;

  /* ID/object caches */
  QCache<int, QList<map::MapRunway> > runwayOverwiewCache;
  QCache<query::NearestCacheKeyNavaid, map::MapResultIndex> nearestNavaidCache;

  static int queryMaxRows;

  /* Database queries */
  atools::sql::SqlQuery *runwayOverviewQuery = nullptr,
                        *airportByRectQuery = nullptr, *airportAddonByRectQuery = nullptr,
                        *airportMsaByRectQuery = nullptr, *airportMsaByIdentQuery = nullptr, *airportMsaByIdQuery = nullptr;

  atools::sql::SqlQuery *vorsByRectQuery = nullptr, *ndbsByRectQuery = nullptr, *markersByRectQuery = nullptr,
                        *ilsByRectQuery = nullptr, *holdingByRectQuery = nullptr, *userdataPointByRectQuery = nullptr;

  atools::sql::SqlQuery *vorByIdentQuery = nullptr, *ndbByIdentQuery = nullptr, *ilsByIdentQuery = nullptr;

  atools::sql::SqlQuery *vorByIdQuery = nullptr, *ndbByIdQuery = nullptr, *vorByWaypointIdQuery = nullptr,
                        *ndbByWaypointIdQuery = nullptr, *ilsByIdQuery = nullptr, *holdingByIdQuery = nullptr,
                        *ilsQuerySimByAirportAndRw = nullptr, *ilsQuerySimByAirportAndIdent = nullptr,
                        *vorNearestQuery = nullptr, *ndbNearestQuery = nullptr;
};

#endif // LITTLENAVMAP_MAPQUERY_H
