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

#include "query/airportquery.h"

#include "common/constants.h"
#include "common/maptypesfactory.h"
#include "query/querytypes.h"
#include "common/mapresult.h"
#include "fs/common/binarygeometry.h"
#include "sql/sqldatabase.h"
#include "common/maptools.h"
#include "settings/settings.h"
#include "app/navapp.h"
#include "sql/sqlutil.h"
#include "fs/util/fsutil.h"

#include <QStringBuilder>

using namespace Marble;
using namespace atools::sql;
using map::MapAirport;
using map::MapParking;
using map::MapHelipad;

namespace ageo = atools::geo;

/* Key for nearestCache combining all query parameters */
class NearestCacheKeyAirport
{
public:
  NearestCacheKeyAirport(const atools::geo::Pos& posParam, float distanceNmParam)
    : pos(posParam), distanceNm(distanceNmParam)
  {
  }

  atools::geo::Pos pos;
  float distanceNm;

  bool operator==(const NearestCacheKeyAirport& other) const
  {
    return pos.almostEqual(other.pos) && atools::almostEqual(distanceNm, other.distanceNm, 0.01f);
  }

  bool operator!=(const NearestCacheKeyAirport& other) const
  {
    return !operator==(other);
  }

};

inline uint qHash(const NearestCacheKeyAirport& key)
{
  return qHash(key.pos) ^ qHash(key.distanceNm);
}

/* maximum difference in angle for aircraft to recognize the right runway */
const static float MAX_HEADING_RUNWAY_DEVIATION_DEG = 20.f;
const static float MAX_HEADING_RUNWAY_DEVIATION_DEG_2 = 45.f;
const static float MAX_HEADING_RUNWAY_DEVIATION_DEG_3 = map::INVALID_HEADING_VALUE; // Ignore heading and get nearest runway end
const static float MAX_RUNWAY_DISTANCE_METER = 150.f; // First iteration
const static float MAX_RUNWAY_DISTANCE_METER_2 = 2000.f; // Second iteration if first failed
const static float MAX_RUNWAY_DISTANCE_METER_3 = 10000.f; // Third iteration if second failed

const static float MAX_FUZZY_AIRPORT_DISTANCE_METER = 500.f;

AirportQuery::AirportQuery(atools::sql::SqlDatabase *sqlDb, bool nav)
  : navdata(nav), db(sqlDb)
{
  mapTypesFactory = new MapTypesFactory();
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  runwayCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "RunwayCache", 2000).toInt());
  apronCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "ApronCache", 1000).toInt());
  taxipathCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "TaxipathCache", 1000).toInt());
  parkingCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "ParkingCache", 1000).toInt());
  startCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "StartCache", 1000).toInt());
  helipadCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "HelipadCache", 1000).toInt());
  airportIdCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "AirportIdCache", 1000).toInt());
  airportFuzzyIdCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "AirportFuzzyIdCache", 1000).toInt());
  airportIdentCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "AirportIdentCache", 1000).toInt());
}

AirportQuery::~AirportQuery()
{
  deInitQueries();
  delete mapTypesFactory;
}

void AirportQuery::loadAirportProcedureCache()
{
  // Load all airport idents having procedures from navdatabase
  airportsWithProceduresIdent.clear();
  airportsWithProceduresIata.clear();

  if(navdata && NavApp::isNavdataMixed())
  {
    SqlQuery query(db);
    query.exec("select ident, iata from airport where num_approach > 0");
    while(query.next())
    {
      airportsWithProceduresIdent.insert(query.valueStr(0));
      airportsWithProceduresIata.insert(query.valueStr(1));
    }
  }
}

void AirportQuery::correctAirportProcedureFlag(MapAirport& airport)
{
  if(!airportsWithProceduresIdent.isEmpty())
    airport.flags.setFlag(map::AP_PROCEDURE, hasAirportProcedures(airport.ident, airport.iata));
}

bool AirportQuery::hasAirportProcedures(const QString& ident, const QString& iata)
{
  if(!ident.isEmpty() && airportsWithProceduresIdent.contains(ident))
    return true;

  if(!iata.isEmpty() && airportsWithProceduresIata.contains(iata))
    return true;

  return false;
}

void AirportQuery::getAirportAdminNamesById(int airportId, QString& city, QString& state, QString& country)
{
  if(!query::valid(Q_FUNC_INFO, airportAdminByIdQuery))
    return;

  airportAdminByIdQuery->bindValue(":id", airportId);
  airportAdminByIdQuery->exec();
  if(airportAdminByIdQuery->next())
  {
    city = airportAdminByIdQuery->value("city").toString();
    state = airportAdminByIdQuery->value("state").toString();
    country = airportAdminByIdQuery->value("country").toString();
  }
  airportAdminByIdQuery->finish();
}

map::MapAirport AirportQuery::getAirportById(int airportId)
{
  map::MapAirport airport;
  getAirportById(airport, airportId);
  return airport;
}

void AirportQuery::getAirportById(map::MapAirport& airport, int airportId)
{
  if(!query::valid(Q_FUNC_INFO, airportByIdQuery))
    return;

  int routeIndex = airport.routeIndex; // Remember index if set
  map::MapAirport *ap = airportIdCache.object(airportId);

  if(ap != nullptr)
    airport = *ap;
  else
  {
    ap = new map::MapAirport;

    airportByIdQuery->bindValue(":id", airportId);
    airportByIdQuery->exec();
    if(airportByIdQuery->next())
      mapTypesFactory->fillAirport(airportByIdQuery->record(), *ap, true /* complete */, navdata, NavApp::isAirportDatabaseXPlane(navdata));
    airportByIdQuery->finish();

    if(!navdata)
      NavApp::getAirportQueryNav()->correctAirportProcedureFlag(ap);

    airport = *ap;
    airportIdCache.insert(airportId, ap);
  }

  airport.routeIndex = routeIndex;
}

map::MapAirport AirportQuery::getAirportByIdent(const QString& ident)
{
  map::MapAirport airport;
  getAirportByIdent(airport, ident);
  return airport;
}

void AirportQuery::getAirportByIdent(map::MapAirport& airport, const QString& ident)
{
  if(!query::valid(Q_FUNC_INFO, airportByIdentQuery))
    return;

  map::MapAirport *ap = airportIdentCache.object(ident);

  if(ap != nullptr)
    airport = *ap;
  else
  {
    ap = new map::MapAirport;

    airportByIdentQuery->bindValue(":ident", ident);
    airportByIdentQuery->exec();
    if(airportByIdentQuery->next())
      mapTypesFactory->fillAirport(airportByIdentQuery->record(), *ap, true /* complete */, navdata,
                                   NavApp::isAirportDatabaseXPlane(navdata));
    airportByIdentQuery->finish();
    if(!navdata)
      NavApp::getAirportQueryNav()->correctAirportProcedureFlag(ap);

    airport = *ap;
    airportIdentCache.insert(ident, ap);
  }
}

QList<MapAirport> AirportQuery::getAirportsByTruncatedIdent(const QString& ident)
{
  QList<map::MapAirport> airport;
  getAirportsByTruncatedIdent(airport, ident);
  return airport;
}

void AirportQuery::getAirportsByTruncatedIdent(QList<map::MapAirport>& airports, QString ident)
{
  if(!query::valid(Q_FUNC_INFO, airportsByTruncatedIdentQuery))
    return;

  if(!ident.endsWith(QChar('%')))
    ident.append(QChar('%'));

  airportsByTruncatedIdentQuery->bindValue(":ident", ident);
  airportsByTruncatedIdentQuery->exec();
  while(airportsByTruncatedIdentQuery->next())
  {
    map::MapAirport airport;
    mapTypesFactory->fillAirport(airportsByTruncatedIdentQuery->record(), airport, true /* complete */, navdata,
                                 NavApp::isAirportDatabaseXPlane(navdata));
    if(!navdata)
      NavApp::getAirportQueryNav()->correctAirportProcedureFlag(airport);
    airports.append(airport);
  }
}

QList<map::MapAirport> AirportQuery::getAirportsByOfficialIdent(const QString& ident, const atools::geo::Pos *pos,
                                                                float maxDistanceMeter, map::AirportQueryFlags flags)
{
  QList<map::MapAirport> airports;
  getAirportsByOfficialIdent(airports, ident, pos, maxDistanceMeter, flags);
  return airports;
}

void AirportQuery::getAirportByOfficialIdent(map::MapAirport& airport, const QString& ident, const atools::geo::Pos *pos,
                                             float maxDistanceMeter, map::AirportQueryFlags flags)
{
  QList<map::MapAirport> airports;
  getAirportsByOfficialIdent(airports, ident, pos, maxDistanceMeter, flags);
  airport = airports.isEmpty() ? map::MapAirport() : airports.constFirst();
}

MapAirport AirportQuery::getAirportByOfficialIdent(const QString& ident, const atools::geo::Pos *pos, float maxDistanceMeter,
                                                   map::AirportQueryFlags flags)
{
  QList<map::MapAirport> airports;
  getAirportsByOfficialIdent(airports, ident, pos, maxDistanceMeter, flags);
  return airports.isEmpty() ? map::MapAirport() : airports.constFirst();
}

void AirportQuery::getAirportsByOfficialIdent(QList<map::MapAirport>& airports, const QString& ident,
                                              const atools::geo::Pos *pos, float maxDistanceMeter, map::AirportQueryFlags flags)
{
  if(!query::valid(Q_FUNC_INFO, airportByOfficialQuery))
    return;

  // Use impossible value if not searching by ident since query connects with "or"
  const static QLatin1String INVALID_ID("====");

  if(!ident.isEmpty())
  {
    airportByOfficialQuery->bindValue(":ident", flags.testFlag(map::AP_QUERY_IDENT) ? ident : INVALID_ID);

    if(icaoCol)
      airportByOfficialQuery->bindValue(":icao", flags.testFlag(map::AP_QUERY_ICAO) ? ident : INVALID_ID);

    if(faaCol)
      airportByOfficialQuery->bindValue(":faa", flags.testFlag(map::AP_QUERY_FAA) ? ident : INVALID_ID);

    if(iataCol)
      airportByOfficialQuery->bindValue(":iata", flags.testFlag(map::AP_QUERY_IATA) ? ident : INVALID_ID);

    if(localCol)
      airportByOfficialQuery->bindValue(":local", flags.testFlag(map::AP_QUERY_LOCAL) ? ident : INVALID_ID);

    airportByOfficialQuery->exec();
    while(airportByOfficialQuery->next())
    {
      map::MapAirport airport;
      mapTypesFactory->fillAirport(airportByOfficialQuery->record(), airport, true /* complete */, navdata,
                                   NavApp::isAirportDatabaseXPlane(navdata));
      if(!navdata)
        NavApp::getAirportQueryNav()->correctAirportProcedureFlag(airport);
      airports.append(airport);
    }
  }

  if(pos != nullptr)
  {
    maptools::sortByDistance(airports, *pos);

    if(maxDistanceMeter < map::INVALID_DISTANCE_VALUE)
      maptools::removeByDistance(airports, *pos, maxDistanceMeter);
  }
}

QString AirportQuery::getDisplayIdent(const QString& ident)
{
  QString dispIdent = getAirportByIdent(ident).displayIdent();
  return dispIdent.isEmpty() ? ident : dispIdent;
}

void AirportQuery::getAirportFuzzy(map::MapAirport& airport, const map::MapAirport airportCopy)
{
  if(!airportCopy.isValid())
    return;

  map::MapAirport *apCached = airportFuzzyIdCache.object(airportCopy.id);

  if(apCached != nullptr)
    // Found in cache - copy from pointer
    airport = *apCached;
  else
  {
    QList<map::MapAirport> airports;

    // Create new airport for caching
    apCached = new map::MapAirport;

    // airportFrom has to be copied to avoid overwriting
    // Try exact ident match first
    map::MapAirport airportByIdent;
    getAirportByIdent(airportByIdent, airportCopy.ident);

    // Try other codes if given as second attempt
    if(airportByIdent.isValid())
      airports.append(airportByIdent);

    // Sort by distance and remove too far away in case an ident was assigned to a new airport
    maptools::sortByDistance(airports, airportCopy.position);
    maptools::removeByDistance(airports, airportCopy.position, MAX_FUZZY_AIRPORT_DISTANCE_METER);

    if(airports.isEmpty())
    {
      // Try ICAO first on all fields (ICAO, not IATA, FAA and local)
      if(airportCopy.icao != airportCopy.ident)
        getAirportsByOfficialIdent(airports, airportCopy.icao, nullptr, map::INVALID_DISTANCE_VALUE,
                                   map::AP_QUERY_ICAO | map::AP_QUERY_FAA | map::AP_QUERY_LOCAL);

      // Try IATA next on all fields
      getAirportsByOfficialIdent(airports, airportCopy.iata, nullptr, map::INVALID_DISTANCE_VALUE,
                                 map::AP_QUERY_ICAO | map::AP_QUERY_IATA | map::AP_QUERY_FAA | map::AP_QUERY_LOCAL);

      // Try FAA next on all fields except IATA
      getAirportsByOfficialIdent(airports, airportCopy.faa, nullptr, map::INVALID_DISTANCE_VALUE,
                                 map::AP_QUERY_ICAO | map::AP_QUERY_FAA | map::AP_QUERY_LOCAL);
    }

    // Fall back to coordinate based search and look for centers within certain distance
    if(airports.isEmpty() && airportCopy.position.isValid())
    {
      ageo::Rect rect(airportCopy.position, ageo::nmToMeter(10.f), true /* fast */);

      query::fetchObjectsForRect(rect, airportByPosQuery, [ =, &airports](atools::sql::SqlQuery *query) -> void {
        map::MapAirport airportByCoord;
        mapTypesFactory->fillAirport(query->record(), airportByCoord, true /* complete */,
                                     navdata, NavApp::isAirportDatabaseXPlane(navdata));
        if(!navdata)
          NavApp::getAirportQueryNav()->correctAirportProcedureFlag(airportByCoord);
        airports.append(airportByCoord);
      });
    }

    if(!airports.isEmpty())
    {
      // Sort by distance and remove too far away
      maptools::sortByDistance(airports, airportCopy.position);
      maptools::removeByDistance(airports, airportCopy.position, MAX_FUZZY_AIRPORT_DISTANCE_METER);

      if(!airports.isEmpty())
        // Assign to cache object
        *apCached = airports.constFirst();
    } // else assign empty airport to indicate that is it not available

    // Can be valid or empty
    airport = *apCached;

    // Also insert negative entries for not found
    airportFuzzyIdCache.insert(airportCopy.id, apCached);

#ifdef DEBUG_INFORMATION
    qDebug() << Q_FUNC_INFO << "Found" << *apCached << "as replacement for" << airportCopy;
#endif
  }
}

atools::geo::Pos AirportQuery::getAirportPosByIdent(const QString& ident)
{
  if(!query::valid(Q_FUNC_INFO, airportCoordsByIdentQuery))
    return atools::geo::EMPTY_POS;

  ageo::Pos pos;
  airportCoordsByIdentQuery->bindValue(":ident", ident);
  airportCoordsByIdentQuery->exec();
  if(airportCoordsByIdentQuery->next())
    pos = ageo::Pos(airportCoordsByIdentQuery->value("lonx").toFloat(), airportCoordsByIdentQuery->value("laty").toFloat());
  airportCoordsByIdentQuery->finish();
  return pos;
}

atools::geo::Pos AirportQuery::getAirportPosByIdentOrIcao(const QString& identOrIcao)
{
  if(!query::valid(Q_FUNC_INFO, airportCoordsByIdentOrIcaoQuery))
    return atools::geo::EMPTY_POS;

  ageo::Pos pos;
  airportCoordsByIdentOrIcaoQuery->bindValue(":identicao", identOrIcao);
  airportCoordsByIdentOrIcaoQuery->exec();
  if(airportCoordsByIdentOrIcaoQuery->next())
    pos = ageo::Pos(airportCoordsByIdentOrIcaoQuery->value("lonx").toFloat(), airportCoordsByIdentOrIcaoQuery->value("laty").toFloat());
  airportCoordsByIdentOrIcaoQuery->finish();
  return pos;
}

bool AirportQuery::hasProcedures(const map::MapAirport& airport) const
{
  return hasQueryByAirportId(*airportProcByIdQuery, airport.id);
}

bool AirportQuery::hasArrivalProcedures(const map::MapAirport& airport) const
{
  return hasQueryByAirportId(*procArrivalByAirportIdQuery, airport.id);
}

bool AirportQuery::hasDepartureProcedures(const map::MapAirport& airport) const
{
  return hasQueryByAirportId(*procDepartureByAirportIdQuery, airport.id);
}

bool AirportQuery::hasQueryByAirportId(atools::sql::SqlQuery& query, int id) const
{
  bool retval = false;
  query.bindValue(":id", id);
  query.exec();
  if(query.next())
    retval = true;

  query.finish();
  return retval;
}

void AirportQuery::getAirportRegion(map::MapAirport& airport)
{
  if(!airport.region.isEmpty())
    return;

  // Use smallest manhattan distance to airport
  SqlQuery query(db);
  query.prepare("select w.region from waypoint w "
                "where w.region is not null "
                "order by (abs(:lonx - w.lonx) + abs(:laty - w.laty)) limit 1");

  query.bindValue(":lonx", airport.position.getLonX());
  query.bindValue(":laty", airport.position.getLatY());

  query.exec();
  if(query.next())
    airport.region = query.valueStr(0);
  query.finish();
}

map::MapRunwayEnd AirportQuery::getRunwayEndById(int id)
{
  map::MapRunwayEnd end;
  if(!query::valid(Q_FUNC_INFO, runwayEndByIdQuery))
    return end;

  runwayEndByIdQuery->bindValue(":id", id);
  runwayEndByIdQuery->exec();
  if(runwayEndByIdQuery->next())
    mapTypesFactory->fillRunwayEnd(runwayEndByIdQuery->record(), end, navdata);
  runwayEndByIdQuery->finish();
  return end;
}

map::MapRunwayEnd AirportQuery::getOpposedRunwayEnd(int airportId, const map::MapRunwayEnd& runwayEnd)
{
  const QList<map::MapRunway> *runways = getRunways(airportId);
  if(runways != nullptr)
  {
    for(const map::MapRunway& runway : *runways)
    {
      if(runway.primaryEndId == runwayEnd.id)
        return getRunwayEndById(runway.secondaryEndId);

      if(runway.secondaryEndId == runwayEnd.id)
        return getRunwayEndById(runway.primaryEndId);
    }
  }
  return map::MapRunwayEnd();
}

map::MapRunway AirportQuery::getRunwayByEndId(int airportId, int runwayEndId)
{
  const QList<map::MapRunway> *runways = getRunways(airportId);

  if(runways != nullptr)
  {
    for(const map::MapRunway& runway : *runways)
    {
      if(runway.primaryEndId == runwayEndId || runway.secondaryEndId == runwayEndId)
        return runway;
    }
  }
  return map::MapRunway();
}

void AirportQuery::getRunwayEndByNames(map::MapResult& result, const QString& runwayName, const QString& airportIdent)
{
  for(const QString& rname : atools::fs::util::runwayNameZeroPrefixVariants(runwayName))
  {
    runwayEndByNames(result, rname, airportIdent);
    if(!result.runwayEnds.isEmpty())
      return;
  }
}

void AirportQuery::runwayEndByNames(map::MapResult& result, const QString& runwayName, const QString& airportIdent)
{
  if(!query::valid(Q_FUNC_INFO, runwayEndByNameQuery))
    return;

  QString rname(runwayName);
  if(rname.startsWith("RW"))
    rname.remove(0, 2);

  runwayEndByNameQuery->bindValue(":name", rname);
  runwayEndByNameQuery->bindValue(":airport", airportIdent);
  runwayEndByNameQuery->exec();
  while(runwayEndByNameQuery->next())
  {
    map::MapRunwayEnd end;
    mapTypesFactory->fillRunwayEnd(runwayEndByNameQuery->record(), end, navdata);
    result.runwayEnds.append(end);
  }
}

const QList<map::MapApron> *AirportQuery::getAprons(int airportId)
{
  if(!query::valid(Q_FUNC_INFO, apronQuery))
    return nullptr;

  if(apronCache.contains(airportId))
    return apronCache.object(airportId);
  else
  {
    apronQuery->bindValue(":airportId", airportId);
    apronQuery->exec();

    QList<map::MapApron> *aprons = new QList<map::MapApron>;
    while(apronQuery->next())
    {
      map::MapApron ap;

      ap.id = apronQuery->valueInt("apron_id");
      ap.surface = apronQuery->value("surface").toString();
      ap.drawSurface = apronQuery->value("is_draw_surface").toInt() > 0;

      if(!apronQuery->isNull("geometry"))
      {
        QByteArray bytes = apronQuery->value("geometry").toByteArray();

        if(!bytes.isEmpty())
        {
          // X-Plane specific - contains bezier points for apron and taxiways.
          atools::fs::common::XpGeometry geo(bytes);
          ap.geometry = geo.getGeometry();

          if(!ap.geometry.boundary.isEmpty())
            // Set position to first for validity check
            ap.position = ap.geometry.boundary.constFirst().node;
        }
      }

      // Decode vertices into a position list - FSX/P3D
      if(!apronQuery->isNull("vertices"))
      {
        QByteArray bytes = apronQuery->value("vertices").toByteArray();

        if(!bytes.isEmpty())
        {
          atools::fs::common::BinaryGeometry geo(bytes);
          geo.swapGeometry(ap.vertices);

          // Set position to first for validity check
          if(!ap.vertices.isEmpty())
            ap.position = ap.vertices.constFirst();
        }
      }

      aprons->append(ap);
    }

    // Revert apron drawing order for X-Plane - draw last in file first
    if(NavApp::isAirportDatabaseXPlane(navdata))
      std::reverse(aprons->begin(), aprons->end());

    apronCache.insert(airportId, aprons);
    return aprons;
  }
}

const QList<map::MapParking> *AirportQuery::getParkingsForAirport(int airportId)
{
  if(!query::valid(Q_FUNC_INFO, parkingQuery))
    return nullptr;

  if(parkingCache.contains(airportId))
    return parkingCache.object(airportId);
  else
  {
    parkingQuery->bindValue(":airportId", airportId);
    parkingQuery->exec();

    QList<map::MapParking> *ps = new QList<map::MapParking>;
    while(parkingQuery->next())
    {
      map::MapParking p;

      // Vehicle paths are filtered out in the compiler
      mapTypesFactory->fillParking(parkingQuery->record(), p);
      ps->append(p);
    }
    parkingCache.insert(airportId, ps);
    return ps;
  }
}

const QList<map::MapStart> *AirportQuery::getStartPositionsForAirport(int airportId)
{
  if(!query::valid(Q_FUNC_INFO, startQuery))
    return nullptr;

  if(startCache.contains(airportId))
    return startCache.object(airportId);
  else
  {
    startQuery->bindValue(":airportId", airportId);
    startQuery->exec();

    QList<map::MapStart> *ps = new QList<map::MapStart>;
    while(startQuery->next())
    {
      map::MapStart p;
      mapTypesFactory->fillStart(startQuery->record(), p);
      ps->append(p);
    }
    startCache.insert(airportId, ps);
    return ps;
  }
}

void AirportQuery::getStartByNameAndPos(map::MapStart& start, int airportId, const QString& runwayEndName, const ageo::Pos& position)
{
  for(const QString& rname : atools::fs::util::runwayNameZeroPrefixVariants(runwayEndName))
  {
    startByNameAndPos(start, airportId, rname, position);
    if(start.isValid())
      break;
  }
}

void AirportQuery::startByNameAndPos(map::MapStart& start, int airportId, const QString& runwayEndName, const ageo::Pos& position)
{
  // Get runway number for the first part of the query fetching start positions (before union)
  int number = runwayEndName.toInt();

  // No need to create a permanent query here since it is called rarely
  SqlQuery query(db);
  query.prepare(
    "select start_id, airport_id, type, heading, number, runway_name, altitude, lonx, laty from ("
    // Get start positions by number
    "select s.start_id, s.airport_id, s.type, s.heading, s.number, s.runway_name, s.altitude, s.lonx, s.laty "
    "from start s where s.airport_id = :airportId and s.number = :number "
    "union "
    // Get runway start positions by runway name
    "select s.start_id, s.airport_id, s.type, s.heading, s.number, s.runway_name, s.altitude, s.lonx, s.laty "
    "from start s "
    "where s.airport_id = :airportId and s.runway_name = :runwayName)");

  query.bindValue(":number", number);
  query.bindValue(":runwayName", runwayEndName);
  query.bindValue(":airportId", airportId);
  query.exec();

  // Get all start positions
  QVector<map::MapStart> starts;
  while(query.next())
  {
    map::MapStart s;
    mapTypesFactory->fillStart(query.record(), s);
    starts.append(s);
  }

  if(!starts.isEmpty())
  {
    // Now find the nearest since number is not unique for helipads and runways
    map::MapStart minStart;
    float minDistance = map::INVALID_DISTANCE_VALUE;
    for(const map::MapStart& s : qAsConst(starts))
    {
      float dist = position.distanceMeterTo(s.position);

      if(dist < minDistance)
      {
        minDistance = dist;
        minStart = s;
      }
    }
    start = minStart;
  }
  else
    start = map::MapStart();
}

void AirportQuery::getStartById(map::MapStart& start, int startId)
{
  if(!query::valid(Q_FUNC_INFO, startByIdQuery))
    return;

  startByIdQuery->bindValue(":id", startId);
  startByIdQuery->exec();

  if(startByIdQuery->next())
    mapTypesFactory->fillStart(startByIdQuery->record(), start);
  startByIdQuery->finish();
}

void AirportQuery::getParkingByNameNumberSuffix(QList<map::MapParking>& parkings, int airportId, const QString& name, int number,
                                                const QString& suffix)
{
  SqlQuery *query = parkingTypeNumberQuery;
  bool hasSuffix = false;
  if(!suffix.isEmpty() && parkingTypeNumberSuffixQuery != nullptr)
  {
    // Separate suffix currently only used by MSFS
    query = parkingTypeNumberSuffixQuery;
    hasSuffix = true;
  }

  if(!query::valid(Q_FUNC_INFO, query))
    return;

  query->bindValue(":airportId", airportId);
  if(name.isEmpty())
    // Use "like "%" if name is empty
    query->bindValue(":name", "%");
  else
    query->bindValue(":name", name);
  query->bindValue(":number", number);

  if(hasSuffix)
    query->bindValue(":suffix", suffix.isEmpty() ? "%" : suffix);

  query->exec();

  while(query->next())
  {
    map::MapParking parking;
    mapTypesFactory->fillParking(query->record(), parking);
    parkings.append(parking);
  }
}

void AirportQuery::getParkingByName(QList<map::MapParking>& parkings, int airportId, const QString& name,
                                    const ageo::Pos& sortByDistancePos)
{
  if(!query::valid(Q_FUNC_INFO, parkingNameQuery))
    return;

  parkingNameQuery->bindValue(":airportId", airportId);
  if(name.isEmpty())
    // Use "like "%" if name is empty
    parkingNameQuery->bindValue(":name", "%");
  else
    parkingNameQuery->bindValue(":name", name);
  parkingNameQuery->exec();

  while(parkingNameQuery->next())
  {
    map::MapParking parking;
    mapTypesFactory->fillParking(parkingNameQuery->record(), parking);
    parkings.append(parking);
  }
  maptools::sortByDistance(parkings, sortByDistancePos);
}

const QList<map::MapHelipad> *AirportQuery::getHelipads(int airportId)
{
  if(!query::valid(Q_FUNC_INFO, helipadQuery))
    return nullptr;

  if(helipadCache.contains(airportId))
    return helipadCache.object(airportId);
  else
  {
    helipadQuery->bindValue(":airportId", airportId);
    helipadQuery->exec();

    QList<map::MapHelipad> *hs = new QList<map::MapHelipad>;
    while(helipadQuery->next())
    {
      map::MapHelipad hp;
      mapTypesFactory->fillHelipad(helipadQuery->record(), hp);
      hs->append(hp);
    }
    helipadCache.insert(airportId, hs);
    return hs;
  }
}

const map::MapResultIndex *AirportQuery::getNearestProcAirports(const atools::geo::Pos& pos, const QString& ident, float distanceNm)
{
  const map::MapResultIndex *nearest = nearestProcAirportsInternal(pos, ident, distanceNm);
  if(nearest == nullptr || nearest->size() < 5)
    nearest = nearestProcAirportsInternal(pos, ident, distanceNm * 4.f);
  return nearest;
}

const map::MapResultIndex *AirportQuery::nearestProcAirportsInternal(const atools::geo::Pos& pos, const QString& ident, float distanceNm)
{
  const NearestCacheKeyAirport key(pos, distanceNm);

  map::MapResultIndex *result = nearestAirportCache.object(key);

  if(result == nullptr)
  {
    map::MapResult res;
    // Create a rectangle that roughly covers the requested region
    ageo::Rect rect(pos, ageo::nmToMeter(distanceNm), true /* fast */);

    query::fetchObjectsForRect(rect, airportByRectAndProcQuery, [ =, &res](atools::sql::SqlQuery *query) -> void {
      map::MapAirport obj;
      mapTypesFactory->fillAirport(query->record(), obj, true /* complete */, navdata, NavApp::isAirportDatabaseXPlane(navdata));
      if(obj.ident != ident)
        res.airports.append(obj);
    });

    result = new map::MapResultIndex;
    result->add(res);

    // Remove all that are too far away
    result->remove(pos, distanceNm);

    // Sort the rest by distance
    result->sort(pos);

    nearestAirportCache.insert(key, result);
  }
  return result;
}

void AirportQuery::bestRunwayEndAndAirport(map::MapRunwayEnd& runwayEnd, map::MapAirport& airport,
                                           const map::MapResultIndex& runwayAirports, const ageo::Pos& pos, float heading,
                                           float maxDistanceMeter, float maxHeadingDeviationDeg)
{
  map::MapRunway runway;
  ageo::LineDistance result;

  // Loop through the mixed list of distance ordered runways and airports - nearest is first
  for(const map::MapBase *base : runwayAirports)
  {
    float distanceMeter = 0.f;

    // Check if runway exceeds maximum distance ==========================
    const map::MapRunway *rw = base->asPtr<map::MapRunway>();
    if(rw != nullptr)
    {
      pos.distanceMeterToLine(rw->primaryPosition, rw->secondaryPosition, result);
      distanceMeter = std::abs(result.distance);

      if(distanceMeter < maxDistanceMeter)
      {
        // Get best matching runway end =========================
        map::MapRunwayEnd primaryEnd = getRunwayEndById(rw->primaryEndId);
        map::MapRunwayEnd secondaryEnd = getRunwayEndById(rw->secondaryEndId);

        if(maxHeadingDeviationDeg < map::INVALID_HEADING_VALUE)
        {
          // Check if either primary or secondary end matches by heading for fixed wing aircraft
          if(ageo::angleAbsDiff(heading, primaryEnd.heading) < maxHeadingDeviationDeg)
            runwayEnd = primaryEnd;
          else if(ageo::angleAbsDiff(heading, secondaryEnd.heading) < maxHeadingDeviationDeg)
            runwayEnd = secondaryEnd;
        }
        else
          // Ignore heading and look for nearest runway end for this runway
          runwayEnd = primaryEnd.position.distanceMeterTo(pos) < secondaryEnd.position.distanceMeterTo(pos) ? primaryEnd : secondaryEnd;

        if(runwayEnd.isValid())
        {
          // Distance is within limits and runway end found
          runway = *rw;
          qDebug() << Q_FUNC_INFO << "Found" << runwayEnd.name << runwayEnd << atools::geo::meterToNm(distanceMeter) << "NM";
          break;
        }
      }
      else
      {
        // Maximum distance exceeded - everything else (either airport or runway) is more far away
        qDebug() << Q_FUNC_INFO << "Distance exceeded"
                 << atools::geo::meterToNm(distanceMeter) << "<" << atools::geo::meterToNm(maxDistanceMeter);
        break;
      }

    } // if(rw != nullptr)
    else if(!(maxHeadingDeviationDeg < map::INVALID_HEADING_VALUE))
    {
      // Heading not given look for airports as well
      // Check if airport exceeds maximum distance ==========================
      const map::MapAirport *ap = base->asPtr<map::MapAirport>();
      if(ap != nullptr)
      {
        // Check airports for helicopters - list contains only runway-less airports
        distanceMeter = ap->position.distanceMeterTo(pos);
        if(distanceMeter < maxDistanceMeter)
        {
          // Distance is within limits and airport found
          airport = *ap;
          qDebug() << Q_FUNC_INFO << "Found" << airport.ident << airport << atools::geo::meterToNm(distanceMeter) << "NM";
          break;
        }
        else
        {
          // Maximum distance exceeded - everything else (airport or runway) is more far away
          qDebug() << Q_FUNC_INFO << "Distance exceeded"
                   << atools::geo::meterToNm(distanceMeter) << "<" << atools::geo::meterToNm(maxDistanceMeter);
          break;
        }
      }
    }
  } // for(const map::MapBase *base : runwayAirports)

  if(runwayEnd.isValid())
    // Get airport for runway end
    getAirportById(airport, runway.airportId);
  else if(airport.isValid())
    // No runway end but airport found - make airport object complete
    getAirportById(airport, airport.id);
}

void AirportQuery::getRunwaysAndAirports(map::MapResultIndex& runwayAirports, const ageo::Rect& rect, const ageo::Pos& pos, bool noRunway)
{
  // Get runways within rectangle =====================
  SqlQuery query(db);
  query.prepare("select * from runway where lonx between :leftx and :rightx and laty between :bottomy and :topy");

  for(const ageo::Rect& r : rect.splitAtAntiMeridian())
  {
    query::bindRect(r, &query);
    query.exec();

    while(query.next())
    {
      map::MapRunway runway;
      mapTypesFactory->fillRunway(query.record(), runway, true /* overview */);
      runwayAirports.add(map::MapResult::createFromMapBase(&runway));
    }
  }

  // Get airports without runways within rectangle too =====================
  if(noRunway)
  {
    query.prepare("select " % airportOverviewColumns(db).join(", ") % " from airport " %
                  "where lonx between :leftx and :rightx and laty between :bottomy and :topy and longest_runway_length = 0");

    bool xp = NavApp::isAirportDatabaseXPlane(navdata /* navdata */);
    for(const ageo::Rect& r : rect.splitAtAntiMeridian())
    {
      query::bindRect(r, &query);
      query.exec();

      while(query.next())
      {
        map::MapAirport airport;
        mapTypesFactory->fillAirport(query.record(), airport, false /* complete */, navdata, xp);
        if(!navdata)
          NavApp::getAirportQueryNav()->correctAirportProcedureFlag(airport);
        runwayAirports.add(map::MapResult::createFromMapBase(&airport));
      }
    }
  }

  // Sort by distance to airport or runway line nearest at beginning of list
  runwayAirports.sort(pos);
}

const QList<map::MapTaxiPath> *AirportQuery::getTaxiPaths(int airportId)
{
  if(!query::valid(Q_FUNC_INFO, taxiparthQuery))
    return nullptr;

  if(taxipathCache.contains(airportId))
    return taxipathCache.object(airportId);
  else
  {
    taxiparthQuery->bindValue(":airportId", airportId);
    taxiparthQuery->exec();

    QList<map::MapTaxiPath> *tps = new QList<map::MapTaxiPath>;
    while(taxiparthQuery->next())
    {
      // TODO should be moved to MapTypesFactory
      map::MapTaxiPath tp;
      tp.closed = taxiparthQuery->value("type").toString() == "CLOSED";
      tp.drawSurface = taxiparthQuery->value("is_draw_surface").toInt() > 0;
      tp.start = ageo::Pos(taxiparthQuery->value("start_lonx").toFloat(), taxiparthQuery->value("start_laty").toFloat());
      tp.end = ageo::Pos(taxiparthQuery->value("end_lonx").toFloat(), taxiparthQuery->value("end_laty").toFloat());
      tp.surface = taxiparthQuery->value("surface").toString();
      tp.name = taxiparthQuery->value("name").toString();
      tp.width = taxiparthQuery->value("width").toInt();

      tps->append(tp);
    }
    taxipathCache.insert(airportId, tps);
    return tps;
  }
}

const QList<map::MapRunway> *AirportQuery::getRunways(int airportId)
{
  if(!query::valid(Q_FUNC_INFO, runwaysQuery))
    return nullptr;

  if(runwayCache.contains(airportId))
    return runwayCache.object(airportId);
  else
  {
    runwaysQuery->bindValue(":airportId", airportId);
    runwaysQuery->exec();

    QList<map::MapRunway> *rs = new QList<map::MapRunway>;
    while(runwaysQuery->next())
    {
      map::MapRunway runway;
      mapTypesFactory->fillRunway(runwaysQuery->record(), runway, false);
      rs->append(runway);
    }

    // Sort to draw the hard/better runways last on top of other grass, turf, etc.
    using namespace std::placeholders;
    std::sort(rs->begin(), rs->end(), std::bind(&AirportQuery::runwayCompare, this, _1, _2));

    runwayCache.insert(airportId, rs);
    return rs;
  }
}

QStringList AirportQuery::getRunwayNames(int airportId)
{
  const QList<map::MapRunway> *aprunways = getRunways(airportId);
  QStringList runwayNames;
  if(aprunways != nullptr)
  {
    for(const map::MapRunway& runway : *aprunways)
      runwayNames << runway.primaryName << runway.secondaryName;
    runwayNames.sort();
  }
  return runwayNames;
}

map::MapRunwayEnd AirportQuery::getRunwayEndByName(int airportId, const QString& runway)
{
  map::MapRunwayEnd end;
  for(const QString& rname : atools::fs::util::runwayNameZeroPrefixVariants(runway))
  {
    end = runwayEndByName(airportId, rname);
    if(end.isValid())
      break;
  }
  return end;
}

map::MapRunwayEnd AirportQuery::runwayEndByName(int airportId, const QString& runway)
{
  const QList<map::MapRunway> *aprunways = getRunways(airportId);
  if(aprunways != nullptr)
  {
    for(const map::MapRunway& mr : *aprunways)
    {
      if(atools::fs::util::runwayEqual(mr.primaryName, runway))
        return getRunwayEndById(mr.primaryEndId);

      if(atools::fs::util::runwayEqual(mr.secondaryName, runway))
        return getRunwayEndById(mr.secondaryEndId);
    }
  }
  return map::MapRunwayEnd();
}

bool AirportQuery::getBestRunwayEndForPosAndCourse(map::MapRunwayEnd& runwayEnd, map::MapAirport& airport, const ageo::Pos& pos,
                                                   float trackTrue, bool helicopter)
{
  qDebug() << Q_FUNC_INFO << "pos" << pos << "trackTrue" << trackTrue << "helicopter" << helicopter;

  runwayEnd.reset();
  airport.reset();

  // Get all airports without runways and runways nearby ordered by distance between pos and runway line
  // Use inflated rectangle for query
  map::MapResultIndex runwayAirports;
  getRunwaysAndAirports(runwayAirports, ageo::Rect(pos, MAX_RUNWAY_DISTANCE_METER_3, true /* fast */), pos, true /* noRunway */);
  qDebug() << Q_FUNC_INFO << "Found" << runwayAirports.size() << "runways and/or airports";

  if(!runwayAirports.isEmpty())
  {
    // Get closest airport and runway that matches heading closely - first iteration
    // Heading is ignored for helicopter
    // Airport is also returned if runway end was found
    bestRunwayEndAndAirport(runwayEnd, airport, runwayAirports, pos, trackTrue,
                            MAX_RUNWAY_DISTANCE_METER, helicopter ? map::INVALID_HEADING_VALUE : MAX_HEADING_RUNWAY_DEVIATION_DEG);

    if(!airport.isValid())
    {
      // Get closest airport and runway that matches heading with larger distance and tolerances - second iteration
      qDebug() << Q_FUNC_INFO << "No airport found. Second iteration.";
      runwayEnd.reset();
      airport.reset();
      bestRunwayEndAndAirport(runwayEnd, airport, runwayAirports, pos, trackTrue,
                              MAX_RUNWAY_DISTANCE_METER_2, helicopter ? map::INVALID_HEADING_VALUE : MAX_HEADING_RUNWAY_DEVIATION_DEG_2);
    }

    if(!airport.isValid())
    {
      // Get closest airport and runway that matches heading with largest distance - third iteration
      // Ignore heading value and get closest runway end
      qDebug() << Q_FUNC_INFO << "No airport found. Third iteration.";
      runwayEnd.reset();
      airport.reset();
      bestRunwayEndAndAirport(runwayEnd, airport, runwayAirports, pos, trackTrue,
                              MAX_RUNWAY_DISTANCE_METER_3, helicopter ? map::INVALID_HEADING_VALUE : MAX_HEADING_RUNWAY_DEVIATION_DEG_3);
    }

    if(airport.isValid())
      qDebug() << Q_FUNC_INFO << "Found airport" << airport.ident << airport;

    if(runwayEnd.isValid())
      qDebug() << Q_FUNC_INFO << "Found runway end" << runwayEnd.name << runwayEnd;
  }

  return airport.isValid();
}

/* Compare runways to put betters ones (hard surface, longer) at the end of a list */
bool AirportQuery::runwayCompare(const map::MapRunway& r1, const map::MapRunway& r2)
{
  // The value returned indicates whether the element passed as first argument is
  // considered to go before the second
  int s1 = map::surfaceQuality(r1.surface);
  int s2 = map::surfaceQuality(r2.surface);
  if(s1 == s2)
    return r1.length < r2.length;
  else
    return s1 < s2;
}

QStringList AirportQuery::airportColumns(const atools::sql::SqlDatabase *db)
{
  // Common select statements
  QStringList airportQueryBase({
    "airport_id",
    "ident",
    "name",
    "rating",
    "has_avgas",
    "has_jetfuel", "has_tower_object",
    "tower_frequency", "atis_frequency", "awos_frequency", "asos_frequency", "unicom_frequency",
    "is_closed", "is_military", "is_addon",
    "num_apron", "num_taxi_path",
    "num_parking_gate", "num_parking_ga_ramp", "num_parking_cargo", "num_parking_mil_cargo", "num_parking_mil_combat",
    "num_runway_end_vasi", "num_runway_end_als", "num_runway_end_closed",
    "num_approach", "num_runway_hard", "num_runway_soft", "num_runway_water", "num_runway_light", "num_runway_end_ils",
    "num_helipad",
    "longest_runway_length", "longest_runway_heading",
    "mag_var",
    "tower_lonx", "tower_laty",
    "altitude",
    "lonx", "laty",
    "left_lonx", "top_laty", "right_lonx", "bottom_laty"
  });

  SqlUtil util(db);
  SqlRecord aprec = db->record("airport");
  if(aprec.contains("icao") && util.hasRows("airport", "icao is not null"))
    airportQueryBase.append("icao");
  if(aprec.contains("iata") && util.hasRows("airport", "iata is not null"))
    airportQueryBase.append("iata");
  if(aprec.contains("faa") && util.hasRows("airport", "faa is not null"))
    airportQueryBase.append("faa");
  if(aprec.contains("local") && util.hasRows("airport", "local is not null"))
    airportQueryBase.append("local");
  if(aprec.contains("region"))
    airportQueryBase.append("region");
  if(aprec.contains("flatten"))
    airportQueryBase.append("flatten");
  if(aprec.contains("is_3d"))
    airportQueryBase.append("is_3d");
  if(aprec.contains("transition_altitude"))
    airportQueryBase.append("transition_altitude");
  if(aprec.contains("transition_level"))
    airportQueryBase.append("transition_level");
  if(aprec.contains("type"))
    airportQueryBase.append("type");
  return airportQueryBase;
}

QStringList AirportQuery::airportOverviewColumns(const atools::sql::SqlDatabase *db)
{
  // Common select statements
  QStringList airportQueryBase({
    "airport_id", "ident", "name",
    "has_avgas", "has_jetfuel",
    "tower_frequency",
    "is_closed", "is_military", "is_addon", "rating",
    "num_runway_hard", "num_runway_soft", "num_runway_water", "num_helipad",
    "longest_runway_length", "longest_runway_heading", "mag_var",
    "lonx", "laty", "left_lonx", "top_laty", "right_lonx", "bottom_laty "
  });

  SqlRecord aprec = db->record("airport");
  if(aprec.contains("region"))
    airportQueryBase.append("region");
  if(aprec.contains("is_3d"))
    airportQueryBase.append("is_3d");

  if(aprec.contains("icao"))
    airportQueryBase.append("icao");
  if(aprec.contains("iata"))
    airportQueryBase.append("iata");
  if(aprec.contains("faa"))
    airportQueryBase.append("faa");
  if(aprec.contains("local"))
    airportQueryBase.append("local");

  return airportQueryBase;
}

void AirportQuery::initQueries()
{
  // Common where clauses
  static const QString whereRect("lonx between :leftx and :rightx and laty between :bottomy and :topy");
  static const QString whereLimit(" limit " % QString::number(map::MAX_MAP_OBJECTS));

  const QStringList airportQueryBase = airportColumns(db);
  const QStringList airportQueryBaseOverview = airportOverviewColumns(db);

  QString parkingQueryBase("parking_id, airport_id, type, name, airline_codes, number, radius, heading, has_jetway, lonx, laty ");
  bool parkingHasSuffix = db->record("parking").contains("suffix");
  if(parkingHasSuffix)
    parkingQueryBase.append(", suffix ");

  deInitQueries();

  airportByIdQuery = new SqlQuery(db);
  airportByIdQuery->prepare("select " % airportQueryBase.join(", ") % " from airport where airport_id = :id ");

  airportAdminByIdQuery = new SqlQuery(db);
  airportAdminByIdQuery->prepare("select city, state, country from airport where airport_id = :id ");

  airportProcByIdQuery = new SqlQuery(db);
  airportProcByIdQuery->prepare("select 1 from approach where airport_id = :id limit 1");

  procArrivalByAirportIdQuery = new SqlQuery(db);
  procArrivalByAirportIdQuery->prepare("select 1 from approach where airport_id = :id and  "
                                       "((type = 'GPS' and suffix = 'A') or (suffix <> 'D' or suffix is null)) limit 1");

  procDepartureByAirportIdQuery = new SqlQuery(db);
  procDepartureByAirportIdQuery->prepare("select 1 from approach where airport_id = :id and type = 'GPS' and suffix = 'D' limit 1");

  airportByIdentQuery = new SqlQuery(db);
  airportByIdentQuery->prepare("select " % airportQueryBase.join(", ") % " from airport where ident = :ident ");

  airportsByTruncatedIdentQuery = new SqlQuery(db);
  airportsByTruncatedIdentQuery->prepare("select " % airportQueryBase.join(", ") % " from airport where ident like :ident ");

  icaoCol = airportQueryBase.contains("icao");
  iataCol = airportQueryBase.contains("iata");
  faaCol = airportQueryBase.contains("faa");
  localCol = airportQueryBase.contains("local");

  QString sql("select " % airportQueryBase.join(", ") % " from airport where ");

  QStringList idents(" ident like :ident ");
  if(icaoCol)
    idents += " icao like :icao ";

  if(iataCol)
    idents += " iata like :iata ";

  if(faaCol)
    idents += " faa like :faa ";

  if(localCol)
    idents += " local like :local ";

  airportByOfficialQuery = new SqlQuery(db);
  airportByOfficialQuery->prepare(sql % idents.join(" or "));

  airportByPosQuery = new SqlQuery(db);
  airportByPosQuery->prepare("select " % airportQueryBase.join(", ") % " from airport  where " % whereRect % whereLimit);

  airportCoordsByIdentQuery = new SqlQuery(db);
  airportCoordsByIdentQuery->prepare("select lonx, laty from airport where ident = :ident ");

  airportCoordsByIdentOrIcaoQuery = new SqlQuery(db);
  airportCoordsByIdentOrIcaoQuery->prepare("select lonx, laty from airport where ident = :identicao or icao = :identicao ");

  airportByRectAndProcQuery = new SqlQuery(db);
  airportByRectAndProcQuery->prepare("select " % airportQueryBase.join(", ") % " from airport where " % whereRect %
                                     " and num_approach > 0 " % whereLimit);

  QString runwayEndQueryBase("e.runway_end_id, e.end_type, e.name, e.heading, e.left_vasi_pitch, e.right_vasi_pitch, e.is_pattern, "
                             "e.left_vasi_type, e.right_vasi_type, e.lonx, e.laty ");
  if(db->record("runway_end").contains("altitude"))
    runwayEndQueryBase.append(", e.altitude ");

  runwayEndByIdQuery = new SqlQuery(db);
  runwayEndByIdQuery->prepare("select " % runwayEndQueryBase % " from runway_end e where e.runway_end_id = :id");

  runwayEndByNameQuery = new SqlQuery(db);
  runwayEndByNameQuery->prepare(
    "select " % runwayEndQueryBase %
    "from runway r join runway_end e on (r.primary_end_id = e.runway_end_id or r.secondary_end_id = e.runway_end_id) "
    "join airport a on r.airport_id = a.airport_id where e.name = :name and a.ident = :airport");

  // Runways > 4000 feet for simplyfied runway overview
  runwayOverviewQuery = new SqlQuery(db);
  runwayOverviewQuery->prepare(
    "select length, heading, lonx, laty, primary_lonx, primary_laty, secondary_lonx, secondary_laty "
    "from runway where airport_id = :airportId and length > 4000 " % whereLimit);

  apronQuery = new SqlQuery(db);
  apronQuery->prepare("select * from apron where airport_id = :airportId");

  parkingQuery = new SqlQuery(db);
  parkingQuery->prepare("select " % parkingQueryBase % " from parking where airport_id = :airportId");

  // Start positions ordered by type (runway, helipad) and name
  startQuery = new SqlQuery(db);
  startQuery->prepare("select s.start_id, s.airport_id, s.type, s.heading, s.number, s.runway_name, s.altitude, s.lonx, s.laty "
                      "from start s where s.airport_id = :airportId order by s.type desc, s.runway_name");

  startByIdQuery = new SqlQuery(db);
  startByIdQuery->prepare("select start_id, airport_id, type, heading, number, runway_name, altitude, lonx, laty "
                          "from start s where start_id = :id");

  parkingTypeNumberQuery = new SqlQuery(db);
  parkingTypeNumberQuery->prepare("select " % parkingQueryBase %
                                  " from parking where airport_id = :airportId and name like :name and number = :number "
                                  " order by radius desc");

  if(parkingHasSuffix)
  {
    parkingTypeNumberSuffixQuery = new SqlQuery(db);
    parkingTypeNumberSuffixQuery->prepare("select " % parkingQueryBase %
                                          " from parking where airport_id = :airportId and name like :name and number = :number and "
                                          " suffix = :suffix order by radius desc");
  }

  parkingNameQuery = new SqlQuery(db);
  parkingNameQuery->prepare("select " % parkingQueryBase % " from parking where airport_id = :airportId and name like :name"
                                                           " order by radius desc");

  helipadQuery = new SqlQuery(db);
  helipadQuery->prepare(
    "select h.helipad_id, h.start_id, h.surface, h.type, h.length, h.width, h.airport_id, "
    " h.heading, h.is_transparent, h.is_closed, h.lonx, h.laty, s.number as start_number, s.runway_name as runway_name "
    " from helipad h "
    " left outer join start s on s.start_id = h.start_id "
    " where h.airport_id = :airportId");

  taxiparthQuery = new SqlQuery(db);
  taxiparthQuery->prepare(
    "select type, surface, width, name, is_draw_surface, start_type, end_type, "
    "start_lonx, start_laty, end_lonx, end_laty "
    "from taxi_path where airport_id = :airportId");

  // Runway joined with both runway ends
  runwaysQuery = new SqlQuery(db);
  runwaysQuery->prepare(
    "select r.*, p.name as primary_name, s.name as secondary_name, "
    "p.name as primary_name, s.name as secondary_name, "
    "r.primary_end_id, r.secondary_end_id, "
    "r.edge_light, "
    "p.offset_threshold as primary_offset_threshold,  p.has_closed_markings as primary_closed_markings, "
    "s.offset_threshold as secondary_offset_threshold,  s.has_closed_markings as secondary_closed_markings,"
    "p.blast_pad as primary_blast_pad,  p.overrun as primary_overrun, "
    "s.blast_pad as secondary_blast_pad,  s.overrun as secondary_overrun,"
    "r.primary_lonx, r.primary_laty, r.secondary_lonx, r.secondary_laty "
    "from runway r "
    "join runway_end p on r.primary_end_id = p.runway_end_id "
    "join runway_end s on r.secondary_end_id = s.runway_end_id "
    "where r.airport_id = :airportId");

  loadAirportProcedureCache();
}

void AirportQuery::deInitQueries()
{
  runwayCache.clear();
  apronCache.clear();
  taxipathCache.clear();
  parkingCache.clear();
  startCache.clear();
  helipadCache.clear();
  airportIdentCache.clear();
  airportIdCache.clear();
  airportFuzzyIdCache.clear();
  airportsWithProceduresIdent.clear();
  airportsWithProceduresIata.clear();
  nearestAirportCache.clear();

  ATOOLS_DELETE(runwayOverviewQuery);
  ATOOLS_DELETE(apronQuery);
  ATOOLS_DELETE(parkingQuery);
  ATOOLS_DELETE(startQuery);
  ATOOLS_DELETE(startByIdQuery);
  ATOOLS_DELETE(parkingTypeNumberQuery);
  ATOOLS_DELETE(parkingTypeNumberSuffixQuery);
  ATOOLS_DELETE(parkingNameQuery);
  ATOOLS_DELETE(helipadQuery);
  ATOOLS_DELETE(taxiparthQuery);
  ATOOLS_DELETE(runwaysQuery);
  ATOOLS_DELETE(airportByIdQuery);
  ATOOLS_DELETE(airportAdminByIdQuery);
  ATOOLS_DELETE(airportProcByIdQuery);
  ATOOLS_DELETE(procDepartureByAirportIdQuery);
  ATOOLS_DELETE(procArrivalByAirportIdQuery);
  ATOOLS_DELETE(airportByIdentQuery);
  ATOOLS_DELETE(airportsByTruncatedIdentQuery);
  ATOOLS_DELETE(airportByOfficialQuery);
  ATOOLS_DELETE(airportByPosQuery);
  ATOOLS_DELETE(airportCoordsByIdentQuery);
  ATOOLS_DELETE(airportCoordsByIdentOrIcaoQuery);
  ATOOLS_DELETE(airportByRectAndProcQuery);
  ATOOLS_DELETE(runwayEndByIdQuery);
  ATOOLS_DELETE(runwayEndByNameQuery);
}

QHash<int, QList<map::MapParking> > AirportQuery::getParkingCache() const
{
  QHash<int, QList<map::MapParking> > retval;

  const QList<int> keys = parkingCache.keys();
  for(int key : keys)
    retval.insert(key, *parkingCache.object(key));

  return retval;
}

QHash<int, QList<map::MapHelipad> > AirportQuery::getHelipadCache() const
{
  QHash<int, QList<map::MapHelipad> > retval;

  const QList<int> keys = helipadCache.keys();
  for(int key : keys)
    retval.insert(key, *helipadCache.object(key));

  return retval;
}
