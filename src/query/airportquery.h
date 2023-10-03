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

#ifndef LITTLENAVMAP_AIRPORTQUERY_H
#define LITTLENAVMAP_AIRPORTQUERY_H

#include "common/mapflags.h"

#include <QCache>
#include <QSet>

namespace Marble {
class GeoDataLatLonBox;
}
namespace map {
struct MapResult;
struct MapResultIndex;
struct MapAirport;
struct MapRunway;
struct MapRunwayEnd;
struct MapParking;
struct MapHelipad;
struct MapStart;
struct MapApron;
struct MapTaxiPath;
}

namespace atools {
namespace geo {
class Rect;
class Pos;
}
namespace sql {
class SqlDatabase;
class SqlQuery;
}
}

class CoordinateConverter;
class MapTypesFactory;
class MapLayer;

/* Key for nearestCache combining all query parameters */
class NearestCacheKeyAirport;

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

  AirportQuery(const AirportQuery& other) = delete;
  AirportQuery& operator=(const AirportQuery& other) = delete;

  void getAirportAdminNamesById(int airportId, QString& city, QString& state, QString& country);

  void getAirportById(map::MapAirport& airport, int airportId);
  map::MapAirport getAirportById(int airportId);

  /* Get airport by ident which also might be the X-Plane internal id.
   * Does not do fuzzy search or by several idents and uses cache. */
  void getAirportByIdent(map::MapAirport& airport, const QString& ident);
  map::MapAirport getAirportByIdent(const QString& ident);

  /* Get airports by ident which also might be the X-Plane internal id.
   * This considers cut off ids as required in X-Plane plans.
   * Does not use cache. */
  void getAirportsByTruncatedIdent(QList<map::MapAirport>& airports, QString ident);
  QList<map::MapAirport> getAirportsByTruncatedIdent(const QString& ident);

  /* Get airport by ICAO, IATA, FAA or local id if table airport has columns.
   * Does *not* look for ident.
   * Returns all airports where either id matches given ident.
   * Does not use cache.
   * Airports will be sorted by distance to pos if given and excluded by distance if given too.*/
  void getAirportsByOfficialIdent(QList<map::MapAirport>& airports, const QString& ident, const atools::geo::Pos *pos = nullptr,
                                  float maxDistanceMeter = map::INVALID_DISTANCE_VALUE, map::AirportQueryFlags flags = map::AP_QUERY_ALL);

  QList<map::MapAirport> getAirportsByOfficialIdent(const QString& ident, const atools::geo::Pos *pos = nullptr,
                                                    float maxDistanceMeter = map::INVALID_DISTANCE_VALUE,
                                                    map::AirportQueryFlags flags = map::AP_QUERY_ALL);

  void getAirportByOfficialIdent(map::MapAirport& airport, const QString& ident, const atools::geo::Pos *pos = nullptr,
                                 float maxDistanceMeter = map::INVALID_DISTANCE_VALUE, map::AirportQueryFlags flags = map::AP_QUERY_ALL);

  map::MapAirport getAirportByOfficialIdent(const QString& ident, const atools::geo::Pos *pos = nullptr,
                                            float maxDistanceMeter = map::INVALID_DISTANCE_VALUE,
                                            map::AirportQueryFlags flags = map::AP_QUERY_ALL);

  /* Try to get airport by ident, icao or position as a fallback if pos is valid,
   * Need to get ident, icao and pos as copies to avoid overwriting.
   * Does not use cache. */
  void getAirportFuzzy(map::MapAirport& airport, const map::MapAirport airportCopy);

  /* Get display ident (ICAO, IATA, FAA or local) based on internal ident */
  QString getDisplayIdent(const QString& ident);

  /* Used as callback for loading METAR data to fetch airport coordinates for nearest */
  atools::geo::Pos getAirportPosByIdent(const QString& ident);

  /* Used as callback for loading METAR data to fetch airport coordinates for nearest. Looks for either ident or icao. */
  atools::geo::Pos getAirportPosByIdentOrIcao(const QString& identOrIcao);

  /* Checks ident or IATA from hash if the airport has procedures and applies the flag.
   * Fast but not 100 percent accurate for airports with not matching idents since no fuzzy search is done. */
  void correctAirportProcedureFlag(map::MapAirport& airport);

  void correctAirportProcedureFlag(map::MapAirport *airport)
  {
    correctAirportProcedureFlag(*airport);
  }

  /* true if airport has procedures. Airport must be available in this database (sim or nav). */
  bool hasProcedures(const map::MapAirport& airport) const;

  /* True if there are STAR or approaches. Airport must be available in this database (sim or nav). */
  bool hasArrivalProcedures(const map::MapAirport& airport) const;

  /* True if airport has SID. Airport must be available in this database (sim or nav). */
  bool hasDepartureProcedures(const map::MapAirport& airport) const;

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
  void getParkingByNameNumberSuffix(QList<map::MapParking>& parkings, int airportId, const QString& name, int number,
                                    const QString& suffix);
  void getParkingByName(QList<map::MapParking>& parkings, int airportId, const QString& name, const atools::geo::Pos& sortByDistancePos);

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

  /* Get a completely filled runway list for the airport.
   * runways are sorted to get betters ones (hard surface, longer) at the end of a list */
  const QList<map::MapRunway> *getRunways(int airportId);

  QStringList getRunwayNames(int airportId);
  void getRunwayEndByNames(map::MapResult& result, const QString& runwayName, const QString& airportIdent);
  map::MapRunwayEnd getRunwayEndByName(int airportId, const QString& runway);
  bool getBestRunwayEndForPosAndCourse(map::MapRunwayEnd& runwayEnd, map::MapAirport& airport,
                                       const atools::geo::Pos& pos, float trackTrue, bool helicopter);

  const QList<map::MapApron> *getAprons(int airportId);

  const QList<map::MapTaxiPath> *getTaxiPaths(int airportId);

  const QList<map::MapParking> *getParkingsForAirport(int airportId);

  const QList<map::MapStart> *getStartPositionsForAirport(int airportId);

  const QList<map::MapHelipad> *getHelipads(int airportId);

  /* Get nearest airports that have a procedure and are sorted by distance to pos with a maximum distance distanceNm.
   * Uses distance * 4 and searches again if nothing was found.*/
  const map::MapResultIndex *getNearestProcAirports(const atools::geo::Pos& pos, const QString& ident, float distanceNm);

  /* Get the best fitting runway end from the given list of runways according to heading.
   * Only the rearest airport is returned if no runway was found */
  void bestRunwayEndAndAirport(map::MapRunwayEnd& runwayEnd, map::MapAirport& airport, const map::MapResultIndex& runwayAirports,
                               const atools::geo::Pos& pos, float heading, float maxDistanceMeter, float maxHeadingDeviationDeg);

  map::MapRunwayEnd getRunwayEndById(int id);

  /* Get other runway end for given one */
  map::MapRunwayEnd getOpposedRunwayEnd(int airportId, const map::MapRunwayEnd& runwayEnd);
  map::MapRunway getRunwayByEndId(int airportId, int runwayEndId);

  /* Close all query objects thus disconnecting from the database */
  void initQueries();

  /* Create and prepare all queries */
  void deInitQueries();

  /* Get copies of cached objects */
  QHash<int, QList<map::MapParking> > getParkingCache() const;
  QHash<int, QList<map::MapHelipad> > getHelipadCache() const;

  static QStringList airportColumns(const atools::sql::SqlDatabase *db);
  static QStringList airportOverviewColumns(const atools::sql::SqlDatabase *db);

private:
  friend inline uint qHash(const NearestCacheKeyAirport& key);

  const map::MapResultIndex *nearestProcAirportsInternal(const atools::geo::Pos& pos, const QString& ident, float distanceNm);

  const QList<map::MapAirport> *fetchAirports(const Marble::GeoDataLatLonBox& rect, atools::sql::SqlQuery *query, bool reverse,
                                              bool lazy, bool overview);

  bool runwayCompare(const map::MapRunway& r1, const map::MapRunway& r2);
  bool hasQueryByAirportId(atools::sql::SqlQuery& query, int id) const;
  void startByNameAndPos(map::MapStart& start, int airportId, const QString& runwayEndName, const atools::geo::Pos& position);
  void runwayEndByNames(map::MapResult& result, const QString& runwayName, const QString& airportIdent);
  map::MapRunwayEnd runwayEndByName(int airportId, const QString& runway);

  /* Get a list of runways of all airports inside rectangle sorted by distance to pos.
   * Airport objects are not complete. */
  void getRunwaysAndAirports(map::MapResultIndex& runwayAirports, const atools::geo::Rect& rect, const atools::geo::Pos& pos,
                             bool noRunway);

  /* Load all airport idents of airports with procedures if this is a navdata query object */
  void loadAirportProcedureCache();

  /* Checks ident or IATA from hash if the airport has procedures.
   * Fast but not 100 percent accurate for airports with not matching idents since no fuzzy search is done. */
  bool hasAirportProcedures(const QString& ident, const QString& iata);

  /* true if third party navdata */
  bool navdata;

  MapTypesFactory *mapTypesFactory;
  atools::sql::SqlDatabase *db;

  /* airport ID / object caches */
  QCache<int, QList<map::MapRunway> > runwayCache;
  QCache<int, QList<map::MapApron> > apronCache;
  QCache<int, QList<map::MapTaxiPath> > taxipathCache;
  QCache<int, QList<map::MapParking> > parkingCache;
  QCache<int, QList<map::MapStart> > startCache;
  QCache<int, QList<map::MapHelipad> > helipadCache;

  QCache<QString, map::MapAirport> airportIdentCache;
  QCache<int, map::MapAirport> airportIdCache, airportFuzzyIdCache;
  QCache<NearestCacheKeyAirport, map::MapResultIndex> nearestAirportCache;
  QSet<QString> airportsWithProceduresIdent, airportsWithProceduresIata;

  /* Available ident columns in airport table. Set to true if column exists and has not null values. */
  bool icaoCol = false, faaCol = false, iataCol = false, localCol = false;

  /* Database queries */
  atools::sql::SqlQuery *runwayOverviewQuery = nullptr, *apronQuery = nullptr,
                        *parkingQuery = nullptr, *startQuery = nullptr, *startByIdQuery = nullptr,
                        *helipadQuery = nullptr, *taxiparthQuery = nullptr, *runwaysQuery = nullptr,
                        *parkingTypeNumberQuery = nullptr, *parkingTypeNumberSuffixQuery = nullptr,
                        *parkingNameQuery = nullptr;

  atools::sql::SqlQuery *airportByIdentQuery = nullptr, *airportsByTruncatedIdentQuery = nullptr,
                        *airportByOfficialQuery = nullptr, *airportByPosQuery = nullptr,
                        *airportCoordsByIdentQuery = nullptr, *airportCoordsByIdentOrIcaoQuery = nullptr,
                        *airportByRectAndProcQuery = nullptr, *runwayEndByIdQuery = nullptr, *runwayEndByNameQuery = nullptr,
                        *airportByIdQuery = nullptr, *airportAdminByIdQuery = nullptr, *airportProcByIdQuery = nullptr,
                        *procArrivalByAirportIdQuery = nullptr, *procDepartureByAirportIdQuery = nullptr;
};

#endif // LITTLENAVMAP_AIRPORTQUERY_H
