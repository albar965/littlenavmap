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

#ifndef LITTLENAVMAP_MAPQUERY_H
#define LITTLENAVMAP_MAPQUERY_H

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
}
}

class CoordinateConverter;
class MapTypesFactory;
class MapLayer;

/*
 * Provides map related database queries. Fill objects of the maptypes namespace and maintains a cache.
 * Objects from methods returning a pointer to a list might be deleted from the cache and should be copied
 * if they have to be kept between event loop calls.
 * All ids are database ids.
 */
class MapQuery
  : public QObject
{
  Q_OBJECT

public:
  /*
   * @param sqlDb database for simulator scenery data
   * @param sqlDbNav for updated navaids
   */
  MapQuery(QObject *parent, atools::sql::SqlDatabase *sqlDb, atools::sql::SqlDatabase *sqlDbNav,
           atools::sql::SqlDatabase *sqlDbUser);
  ~MapQuery();

  /* Convert airport instances from/to simulator and third party nav databases */
  map::MapAirport  getAirportSim(const map::MapAirport& airport);
  map::MapAirport  getAirportNav(const map::MapAirport& airport);
  void getAirportSimReplace(map::MapAirport& airport);
  void getAirportNavReplace(map::MapAirport& airport);

  /* Get all airways that are attached to a waypoint */
  void getAirwaysForWaypoint(QList<map::MapAirway>& airways, int waypointId);

  /* Get all waypoints of an airway */
  void getWaypointsForAirway(QList<map::MapWaypoint>& waypoints, const QString& airwayName,
                             const QString& waypointIdent = QString());
  void getAirwayByNameAndWaypoint(map::MapAirway& airway, const QString& airwayName, const QString& waypoint1,
                                  const QString& waypoint2);

  /* Get all waypoints or an airway ordered by fragment an sequence number */
  void getWaypointListForAirwayName(QList<map::MapAirwayWaypoint>& waypoints, const QString& airwayName);

  void getAirwayById(map::MapAirway& airway, int airwayId);
  map::MapAirway getAirwayById(int airwayId);

  /* If waypoint is of type VOR get the related VOR object */
  void getVorForWaypoint(map::MapVor& vor, int waypointId);
  void getVorNearest(map::MapVor& vor, const atools::geo::Pos& pos);

  /* If waypoint is of type NDB get the related NDB object */
  void getNdbForWaypoint(map::MapNdb& ndb, int waypointId);
  void getNdbNearest(map::MapNdb& ndb, const atools::geo::Pos& pos);

  /* Get map objects by unique database id  */
  map::MapVor getVorById(int id);
  map::MapNdb getNdbById(int id);
  map::MapIls getIlsById(int id);

  QVector<map::MapIls> getIlsByAirportAndRunway(const QString& airportIdent, const QString& runway);
  map::MapWaypoint getWaypointById(int id);
  map::MapUserpoint getUserdataPointById(int id);
  void updateUserdataPoint(map::MapUserpoint& userpoint);

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
  void getMapObjectById(map::MapSearchResult& result, map::MapObjectTypes type, int id, bool airportFromNavDatabase);

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
  void getNearestObjects(const CoordinateConverter& conv, const MapLayer *mapLayer, bool airportDiagram,
                         map::MapObjectTypes types, int xs, int ys, int screenDistance,
                         map::MapSearchResult& result);

  /*
   * Fetch airports for a map coordinate rectangle
   * @param rect bounding rectangle for query
   * @param mapLayer used to find source table
   * @param lazy do not reload from database and return (probably incomplete) result from cache if true
   * @return pointer to airport cache. Create a copy if this is needed for a longer
   * time than for e.g. one drawing request.
   */
  const QList<map::MapAirport> *getAirports(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy);

  /* Similar to getAirports */
  const QList<map::MapWaypoint> *getWaypoints(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                              bool lazy);

  /* Similar to getAirports */
  const QList<map::MapVor> *getVors(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy);

  /* Similar to getAirports */
  const QList<map::MapNdb> *getNdbs(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy);

  /* Similar to getAirports */
  const QList<map::MapMarker> *getMarkers(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy);

  /* Similar to getAirports */
  const QList<map::MapIls> *getIls(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy);

  /* Similar to getAirports */
  const QList<map::MapAirway> *getAirways(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy);

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

private:
  void mapObjectByIdentInternal(map::MapSearchResult& result, map::MapObjectTypes type,
                                const QString& ident, const QString& region, const QString& airport,
                                const atools::geo::Pos& sortByDistancePos,
                                float maxDistance, bool airportFromNavDatabase);

  const QList<map::MapAirport> *fetchAirports(const Marble::GeoDataLatLonBox& rect,
                                              atools::sql::SqlQuery *query,
                                              bool lazy, bool overview);

  bool runwayCompare(const map::MapRunway& r1, const map::MapRunway& r2);

  MapTypesFactory *mapTypesFactory;
  atools::sql::SqlDatabase *db, *dbNav, *dbUser;

  /* Simple bounding rectangle caches */
  SimpleRectCache<map::MapAirport> airportCache;
  SimpleRectCache<map::MapWaypoint> waypointCache;
  SimpleRectCache<map::MapUserpoint> userpointCache;
  SimpleRectCache<map::MapVor> vorCache;
  SimpleRectCache<map::MapNdb> ndbCache;
  SimpleRectCache<map::MapMarker> markerCache;
  SimpleRectCache<map::MapIls> ilsCache;
  SimpleRectCache<map::MapAirway> airwayCache;

  /* ID/object caches */
  QCache<int, QList<map::MapRunway> > runwayOverwiewCache;

  static int queryMaxRows;

  /* Database queries */
  atools::sql::SqlQuery *runwayOverviewQuery = nullptr,
                        *airportByRectQuery = nullptr, *airportMediumByRectQuery = nullptr,
                        *airportLargeByRectQuery = nullptr;

  atools::sql::SqlQuery *waypointsByRectQuery = nullptr, *vorsByRectQuery = nullptr,
                        *ndbsByRectQuery = nullptr, *markersByRectQuery = nullptr, *ilsByRectQuery = nullptr,
                        *airwayByRectQuery = nullptr, *userdataPointByRectQuery = nullptr;

  atools::sql::SqlQuery *vorByIdentQuery = nullptr, *ndbByIdentQuery = nullptr, *waypointByIdentQuery = nullptr,
                        *ilsByIdentQuery = nullptr;

  atools::sql::SqlQuery *vorByIdQuery = nullptr, *ndbByIdQuery = nullptr,
                        *vorByWaypointIdQuery = nullptr, *ndbByWaypointIdQuery = nullptr, *waypointByIdQuery = nullptr,
                        *ilsByIdQuery = nullptr, *ilsQuerySimByName = nullptr, *vorNearestQuery = nullptr,
                        *ndbNearestQuery = nullptr, *userdataPointByIdQuery = nullptr;

  atools::sql::SqlQuery *airwayByWaypointIdQuery = nullptr, *airwayByNameAndWaypointQuery = nullptr,
                        *airwayByIdQuery = nullptr, *airwayWaypointByIdentQuery = nullptr,
                        *airwayWaypointsQuery = nullptr, *airwayByNameQuery = nullptr;
};

#endif // LITTLENAVMAP_MAPQUERY_H
