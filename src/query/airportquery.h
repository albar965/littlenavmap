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

#ifndef LITTLENAVMAP_AIRPORTQUERY_H
#define LITTLENAVMAP_AIRPORTQUERY_H

#include "common/maptypes.h"
#include "mapgui/maplayer.h"

#include <QCache>
#include <QList>

#include <functional>

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
class AirportQuery
{
public:
  /*
   * @param sqlDb database for simulator scenery data
   * @param sqlDbNav for updated navaids
   */
  AirportQuery(atools::sql::SqlDatabase *sqlDb, bool nav);
  ~AirportQuery();

  void getAirportAdminNamesById(int airportId, QString& city, QString& state, QString& country);

  void getAirportById(map::MapAirport& airport, int airportId);
  map::MapAirport getAirportById(int airportId);

  /* get airport by ident which also might be the X-Plane internal id */
  void getAirportByIdent(map::MapAirport& airport, const QString& ident);
  map::MapAirport getAirportByIdent(const QString& ident);

  /* get airport by ICAO id if table airport has column icao */
  void getAirportByIcao(map::MapAirport& airport, const QString& icao);
  map::MapAirport getAirportByIcao(const QString& icao);

  /* Try to get airport by ident, icao or position as a fallback if pos is valid */
  void getAirportFuzzy(map::MapAirport& airport, const QString& ident, const QString& icao,
                       const atools::geo::Pos& pos);

  atools::geo::Pos getAirportPosByIdent(const QString& ident);

  bool hasProcedures(const QString& ident) const;

  /* True if there are STAR or approaches */
  bool hasAnyArrivalProcedures(const QString& ident) const;

  /* True if airport has SID */
  bool hasDepartureProcedures(const QString& ident) const;

  /* Get the region for airports where it is missing. This uses an expensive query to get the
   * region from the nearest waypoints. Region is set in MapAirport.
   * This is a workaround for missing information and not precise. */
  void getAirportRegion(map::MapAirport& airport);

  /*
   * Get a parking spot of an airport by name and number
   * @param parkings result
   * @param airportId airport where parking spot should be fetched
   * @param name name like PARKING, GATE_P, etc.
   * @param number parking number
   */
  void getParkingByNameAndNumber(QList<map::MapParking>& parkings, int airportId, const QString& name, int number);
  void getParkingByName(QList<map::MapParking>& parkings, int airportId, const QString& name,
                        const atools::geo::Pos& sortByDistancePos);

  /*
   * Get a start position of an airport (runway, helipad and water)
   * @param start result
   * @param airportId airport where start position should be fetched
   * @param runwayEndName name like 24, 12C, 13W
   * @param position needed to distinguish between helipads and runways which can have the same name
   */
  void getStartByNameAndPos(map::MapStart& start, int airportId, const QString& runwayEndName,
                            const atools::geo::Pos& position);
  void getStartById(map::MapStart& start, int startId);

  /* Get best start position for an airport. This is the longest preferrably hard surfaced primary runway end */
  void getBestStartPositionForAirport(map::MapStart& start, int airportId, const QString& runwayName);

  /* Get a completely filled runway list for the airport.
   * runways are sorted to get betters ones (hard surface, longer) at the end of a list */
  const QList<map::MapRunway> *getRunways(int airportId);

  QStringList getRunwayNames(int airportId);
  void getRunwayEndByNames(map::MapSearchResult& result, const QString& runwayName, const QString& airportIdent);
  map::MapRunwayEnd getRunwayEndByName(int airportId, const QString& runway);
  bool getBestRunwayEndForPosAndCourse(map::MapRunwayEnd& runwayEnd, map::MapAirport& airport,
                                       const atools::geo::Pos& pos, float trackTrue);

  const QList<map::MapApron> *getAprons(int airportId);

  const QList<map::MapTaxiPath> *getTaxiPaths(int airportId);

  const QList<map::MapParking> *getParkingsForAirport(int airportId);

  const QList<map::MapStart> *getStartPositionsForAirport(int airportId);

  const QList<map::MapHelipad> *getHelipads(int airportId);

  /* Get nearest airports that have a procedure sorted by distance to pos with a maximum distance distanceNm.
   * Uses distance * 4 and searches again if nothing was found.*/
  map::MapSearchResultIndex *getNearestAirportsProc(const map::MapAirport& airport, float distanceNm);

  /* Get a list of runways of all airports inside rectangle sorted by distance to pos */
  void getRunways(QVector<map::MapRunway>& runways, const atools::geo::Rect& rect, const atools::geo::Pos& pos);

  /* Get the best fitting runway end from the given list of runways according to heading.
   *  Only the rearest airport is returned if no runway was found */
  void getBestRunwayEndAndAirport(map::MapRunwayEnd& runwayEnd, map::MapAirport& airport,
                                  const QVector<map::MapRunway>& runways, const atools::geo::Pos& pos, float heading,
                                  float maxRwDistance, float maxHeadingDeviation);

  map::MapRunwayEnd getRunwayEndById(int id);

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();

  QHash<int, QList<map::MapParking> > getParkingCache() const;

  QHash<int, QList<map::MapHelipad> > getHelipadCache() const;

  static QStringList airportColumns(const atools::sql::SqlDatabase *db);
  static QStringList airportOverviewColumns(const atools::sql::SqlDatabase *db);

private:
  /* Key for nearestCache combining all query parameters */
  struct NearestCacheKeyAirport
  {
    atools::geo::Pos pos;
    float distanceNm;

    bool operator==(const NearestCacheKeyAirport& other) const
    {
      return pos == other.pos && std::abs(distanceNm - other.distanceNm) < 0.01;
    }

    bool operator!=(const NearestCacheKeyAirport& other) const
    {
      return !operator==(other);
    }

  };

  friend inline uint qHash(const AirportQuery::NearestCacheKeyAirport& key);

  map::MapSearchResultIndex *nearestAirportsProcInternal(const map::MapAirport& airport, float distanceNm);

  const QList<map::MapAirport> *fetchAirports(const Marble::GeoDataLatLonBox& rect,
                                              atools::sql::SqlQuery *query, bool reverse,
                                              bool lazy, bool overview);

  bool runwayCompare(const map::MapRunway& r1, const map::MapRunway& r2);
  bool hasQueryByAirportIdent(atools::sql::SqlQuery& query, const QString& ident) const;
  void startByNameAndPos(map::MapStart& start, int airportId, const QString& runwayEndName,
                         const atools::geo::Pos& position);
  void runwayEndByNames(map::MapSearchResult& result, const QString& runwayName, const QString& airportIdent);
  map::MapRunwayEnd runwayEndByName(int airportId, const QString& runway);

  const int queryRowLimit = 5000;

  /* true if third party navdata */
  bool navdata;

  MapTypesFactory *mapTypesFactory;
  atools::sql::SqlDatabase *db;

  /* ID/object caches */
  QCache<int, QList<map::MapRunway> > runwayCache;
  QCache<int, QList<map::MapApron> > apronCache;
  QCache<int, QList<map::MapTaxiPath> > taxipathCache;
  QCache<int, QList<map::MapParking> > parkingCache;
  QCache<int, QList<map::MapStart> > startCache;
  QCache<int, QList<map::MapHelipad> > helipadCache;

  QCache<QString, map::MapAirport> airportIdentCache, airportIcaoCache;
  QCache<int, map::MapAirport> airportIdCache;
  QCache<NearestCacheKeyAirport, map::MapSearchResultIndex> nearestAirportCache;

  /* Database queries */
  atools::sql::SqlQuery *runwayOverviewQuery = nullptr, *apronQuery = nullptr,
                        *parkingQuery = nullptr, *startQuery = nullptr, *startByIdQuery = nullptr,
                        *helipadQuery = nullptr,
                        *taxiparthQuery = nullptr, *runwaysQuery = nullptr,
                        *parkingTypeAndNumberQuery = nullptr,
                        *parkingNameQuery = nullptr;

  atools::sql::SqlQuery *airportByIdentQuery = nullptr, *airportByIcaoQuery = nullptr, *airportByPosQuery = nullptr,
                        *airportCoordsByIdentQuery = nullptr, *airportByRectAndProcQuery = nullptr,
                        *runwayEndByIdQuery = nullptr, *runwayEndByNameQuery = nullptr, *airportByIdQuery = nullptr,
                        *airportAdminByIdQuery = nullptr, *airportProcByIdentQuery = nullptr,
                        *procArrivalByAirportIdentQuery = nullptr, *procDepartureByAirportIdentQuery = nullptr;
};

#endif // LITTLENAVMAP_AIRPORTQUERY_H
