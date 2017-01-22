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

#ifndef LITTLENAVMAP_MAPQUERY_H
#define LITTLENAVMAP_MAPQUERY_H

#include "common/maptypes.h"
#include "mapgui/maplayer.h"

#include <QCache>
#include <QList>

#include <marble/GeoDataLatLonBox.h>

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
  MapQuery(QObject *parent, atools::sql::SqlDatabase *sqlDb);
  ~MapQuery();

  void getAirportAdminNamesById(int airportId, QString& city, QString& state, QString& country);

  void getAirportById(maptypes::MapAirport& airport, int airportId);

  void getAirportByIdent(maptypes::MapAirport& airport, const QString& ident);

  /* Get all airways that are attached to a waypoint */
  void getAirwaysForWaypoint(QList<maptypes::MapAirway>& airways, int waypointId);

  /* Get all airways that are attached to a waypoint */
  void getWaypointsForAirway(QList<maptypes::MapWaypoint>& waypoints, const QString& airwayName,
                             const QString& waypointIdent = QString());

  /* Get all waypoints or an airway ordered by fragment an sequence number */
  void getWaypointListForAirwayName(QList<maptypes::MapAirwayWaypoint>& waypoints, const QString& airwayName);

  void getAirwayById(maptypes::MapAirway& airway, int airwayId);

  /* If waypoint is of type VOR get the related VOR object */
  void getVorForWaypoint(maptypes::MapVor& vor, int waypointId);
  void getVorNearest(maptypes::MapVor& vor, const atools::geo::Pos& pos);

  /* If waypoint is of type NDB get the related NDB object */
  void getNdbForWaypoint(maptypes::MapNdb& ndb, int waypointId);
  void getNdbNearest(maptypes::MapNdb& ndb, const atools::geo::Pos& pos);

  /*
   * Get a map object by type, ident and region
   * @param result will receive objects based on type
   * @param type AIRPORT, VOR, NDB or WAYPOINT
   * @param ident ICAO ident
   * @param region two letter ICAO region code
   */
  void getMapObjectByIdent(maptypes::MapSearchResult& result, maptypes::MapObjectTypes type,
                           const QString& ident, const QString& region = QString());

  /*
   * Get a map object by type and id
   * @param result will receive objects based on type
   * @param type AIRPORT, VOR, NDB or WAYPOINT
   * @param id database id
   */
  void getMapObjectById(maptypes::MapSearchResult& result, maptypes::MapObjectTypes type, int id);

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
                         maptypes::MapObjectTypes types, int xs, int ys, int screenDistance,
                         maptypes::MapSearchResult& result);

  /*
   * Get a parking spot of an airport by name and number
   * @param parkings result
   * @param airportId airport where parking spot should be fetched
   * @param name name like PARKING, GATE_P, etc.
   * @param number parking number
   */
  void getParkingByNameAndNumber(QList<maptypes::MapParking>& parkings, int airportId, const QString& name,
                                 int number);

  /*
   * Get a start position of an airport (runway, helipad and water)
   * @param start result
   * @param airportId airport where start position should be fetched
   * @param runwayEndName name like 24, 12C, 13W
   * @param position needed to distinguish between helipads and runways which can have the same name
   */
  void getStartByNameAndPos(maptypes::MapStart& start, int airportId, const QString& runwayEndName,
                            const atools::geo::Pos& position);

  /* Get best start position for an airport. This is the longest preferrably hard surfaced primary runway end */
  void getBestStartPositionForAirport(maptypes::MapStart& start, int airportId);

  /*
   * Fetch airports for a map coordinate rectangle
   * @param rect bounding rectangle for query
   * @param mapLayer used to find source table
   * @param lazy do not reload from database and return (probably incomplete) result from cache if true
   * @return pointer to airport cache. Create a copy if this is needed for a longer
   * time than for e.g. one drawing request.
   */
  const QList<maptypes::MapAirport> *getAirports(const Marble::GeoDataLatLonBox& rect,
                                                 const MapLayer *mapLayer, bool lazy);

  /* Similar to getAirports */
  const QList<maptypes::MapWaypoint> *getWaypoints(const Marble::GeoDataLatLonBox& rect,
                                                   const MapLayer *mapLayer, bool lazy);

  /* Similar to getAirports */
  const QList<maptypes::MapVor> *getVors(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                         bool lazy);

  /* Similar to getAirports */
  const QList<maptypes::MapNdb> *getNdbs(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                         bool lazy);

  /* Similar to getAirports */
  const QList<maptypes::MapMarker> *getMarkers(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                               bool lazy);

  /* Similar to getAirports */
  const QList<maptypes::MapIls> *getIls(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                        bool lazy);

  /* Similar to getAirports */
  const QList<maptypes::MapAirway> *getAirways(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                               bool lazy);

  /* Get a partially filled runway list for the overview */
  const QList<maptypes::MapRunway> *getRunwaysForOverview(int airportId);

  /* Get a completely filled runway list for the airport */
  const QList<maptypes::MapRunway> *getRunways(int airportId);

  const QList<maptypes::MapApron> *getAprons(int airportId);

  const QList<maptypes::MapTaxiPath> *getTaxiPaths(int airportId);

  const QList<maptypes::MapParking> *getParkingsForAirport(int airportId);

  const QList<maptypes::MapStart> *getStartPositionsForAirport(int airportId);

  const QList<maptypes::MapHelipad> *getHelipads(int airportId);

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();

private:
  /* Simple spatial cache that deals with objects in a bounding rectangle but does not run any queries to load data */
  template<typename TYPE>
  struct SimpleRectCache
  {
    /*
     * @param rect bounding rectangle - all objects inside this rectangle are returned
     * @param mapLayer current map layer
     * @param lazy if true do not fetch new data but return the old potentially incomplete dataset
     * @return true after clearing the cache. The caller has to request new data
     */
    bool updateCache(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy);
    void clear();
    void validate();

    Marble::GeoDataLatLonBox curRect;
    const MapLayer *curMapLayer = nullptr;
    QList<TYPE> list;
  };

  const QList<maptypes::MapAirport> *fetchAirports(const Marble::GeoDataLatLonBox& rect,
                                                   atools::sql::SqlQuery *query, bool reverse,
                                                   bool lazy, bool overview);

  void bindCoordinatePointInRect(const Marble::GeoDataLatLonBox& rect, atools::sql::SqlQuery *query,
                                 const QString& prefix = QString());

  QList<Marble::GeoDataLatLonBox> splitAtAntiMeridian(const Marble::GeoDataLatLonBox& rect);

  static void inflateRect(Marble::GeoDataLatLonBox& rect, double width, double height);

  bool runwayCompare(const maptypes::MapRunway& r1, const maptypes::MapRunway& r2);

  MapTypesFactory *mapTypesFactory;
  atools::sql::SqlDatabase *db;

  /* Simple bounding rectangle caches */
  SimpleRectCache<maptypes::MapAirport> airportCache;
  SimpleRectCache<maptypes::MapWaypoint> waypointCache;
  SimpleRectCache<maptypes::MapVor> vorCache;
  SimpleRectCache<maptypes::MapNdb> ndbCache;
  SimpleRectCache<maptypes::MapMarker> markerCache;
  SimpleRectCache<maptypes::MapIls> ilsCache;
  SimpleRectCache<maptypes::MapAirway> airwayCache;

  /* ID/object caches */
  QCache<int, QList<maptypes::MapRunway> > runwayCache;
  QCache<int, QList<maptypes::MapRunway> > runwayOverwiewCache;
  QCache<int, QList<maptypes::MapApron> > apronCache;
  QCache<int, QList<maptypes::MapTaxiPath> > taxipathCache;
  QCache<int, QList<maptypes::MapParking> > parkingCache;
  QCache<int, QList<maptypes::MapStart> > startCache;
  QCache<int, QList<maptypes::MapHelipad> > helipadCache;

  /* Inflate bounding rectangle before passing it to query */
  static Q_DECL_CONSTEXPR double RECT_INFLATION_FACTOR_DEG = 0.3;
  static Q_DECL_CONSTEXPR double RECT_INFLATION_ADD_DEG = 0.1;
  static Q_DECL_CONSTEXPR int QUERY_ROW_LIMIT = 5000;

  /* Database queries */
  atools::sql::SqlQuery *airportByRectQuery = nullptr, *airportMediumByRectQuery = nullptr,
  *airportLargeByRectQuery = nullptr;

  atools::sql::SqlQuery *runwayOverviewQuery = nullptr, *apronQuery = nullptr,
  *parkingQuery = nullptr, *startQuery = nullptr, *helipadQuery = nullptr,
  *taxiparthQuery = nullptr, *runwaysQuery = nullptr,
  *parkingTypeAndNumberQuery = nullptr;

  atools::sql::SqlQuery *waypointsByRectQuery = nullptr, *vorsByRectQuery = nullptr,
  *ndbsByRectQuery = nullptr, *markersByRectQuery = nullptr, *ilsByRectQuery = nullptr,
  *airwayByRectQuery = nullptr;

  atools::sql::SqlQuery *airportByIdentQuery = nullptr, *vorByIdentQuery = nullptr,
  *ndbByIdentQuery = nullptr, *waypointByIdentQuery = nullptr;

  atools::sql::SqlQuery *vorByIdQuery = nullptr, *ndbByIdQuery = nullptr,
  *vorByWaypointIdQuery = nullptr, *ndbByWaypointIdQuery = nullptr, *waypointByIdQuery = nullptr,
  *ilsByIdQuery = nullptr, *runwayEndByIdQuery = nullptr,
  *vorNearestQuery = nullptr, *ndbNearestQuery = nullptr;

  atools::sql::SqlQuery *airportByIdQuery = nullptr, *airportAdminByIdQuery = nullptr;
  atools::sql::SqlQuery *airwayByWaypointIdQuery = nullptr;
  atools::sql::SqlQuery *airwayByIdQuery = nullptr;
  atools::sql::SqlQuery *airwayWaypointByIdentQuery = nullptr;
  atools::sql::SqlQuery *airwayWaypointsQuery = nullptr;
  atools::sql::SqlQuery *airwayByNameQuery = nullptr;
};

// ---------------------------------------------------------------------------------
template<typename TYPE>
bool MapQuery::SimpleRectCache<TYPE>::updateCache(const Marble::GeoDataLatLonBox& rect,
                                                  const MapLayer *mapLayer,
                                                  bool lazy)
{
  if(lazy)
    // Nothing changed11
    return false;

  // Store bounding rectangle and inflate it
  Marble::GeoDataLatLonBox cur(curRect);
  MapQuery::inflateRect(cur, cur.width(Marble::GeoDataCoordinates::Degree) *
                        RECT_INFLATION_FACTOR_DEG + RECT_INFLATION_ADD_DEG,
                        cur.height(Marble::GeoDataCoordinates::Degree) *
                        RECT_INFLATION_FACTOR_DEG + RECT_INFLATION_ADD_DEG);

  if(curRect.isEmpty() || !cur.contains(rect) ||
     !curMapLayer->hasSameQueryParameters(mapLayer))
  {
    // Rectangle not covered by loaded data or new layer selected
    list.clear();
    curRect = rect;
    curMapLayer = mapLayer;
    return true;
  }
  return false;
}

template<typename TYPE>
void MapQuery::SimpleRectCache<TYPE>::validate()
{
  if(list.size() >= QUERY_ROW_LIMIT)
  {
    curRect.clear();
    curMapLayer = nullptr;
  }
}

template<typename TYPE>
void MapQuery::SimpleRectCache<TYPE>::clear()
{
  list.clear();
  curRect.clear();
  curMapLayer = nullptr;
}

#endif // LITTLENAVMAP_MAPQUERY_H
