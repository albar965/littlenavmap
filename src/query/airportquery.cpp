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

#include "query/airportquery.h"

#include "common/constants.h"
#include "common/maptypesfactory.h"
#include "common/maptools.h"
#include "fs/common/binarygeometry.h"
#include "sql/sqlquery.h"
#include "sql/sqldatabase.h"
#include "common/maptools.h"
#include "settings/settings.h"
#include "fs/common/xpgeometry.h"
#include "navapp.h"

#include <QDataStream>
#include <QRegularExpression>

using namespace Marble;
using namespace atools::sql;
using namespace atools::geo;
using map::MapAirport;
using map::MapVor;
using map::MapNdb;
using map::MapWaypoint;
using map::MapMarker;
using map::MapIls;
using map::MapParking;
using map::MapHelipad;

/* maximum difference in angle for aircraft to recognize the right runway */
const static float MAX_HEADING_RUNWAY_DEVIATION = 20.f;

AirportQuery::AirportQuery(QObject *parent, atools::sql::SqlDatabase *sqlDb, bool nav)
  : QObject(parent), navdata(nav), db(sqlDb)
{
  mapTypesFactory = new MapTypesFactory();
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  runwayCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "RunwayCache", 2000).toInt());
  apronCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "ApronCache", 1000).toInt());
  taxipathCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "TaxipathCache", 1000).toInt());
  parkingCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "ParkingCache", 1000).toInt());
  startCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "StartCache", 1000).toInt());
  helipadCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "HelipadCache", 1000).toInt());
  airportIdCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "AirportIdCache", 1000).toInt());
  airportIdentCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY + "AirportIdentCache", 1000).toInt());
}

AirportQuery::~AirportQuery()
{
  deInitQueries();
  delete mapTypesFactory;
}

void AirportQuery::getAirportAdminNamesById(int airportId, QString& city, QString& state, QString& country)
{
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
  map::MapAirport *ap = airportIdCache.object(airportId);

  if(ap != nullptr)
    airport = *ap;
  else
  {
    ap = new map::MapAirport;

    airportByIdQuery->bindValue(":id", airportId);
    airportByIdQuery->exec();
    if(airportByIdQuery->next())
      mapTypesFactory->fillAirport(airportByIdQuery->record(), *ap, true, navdata,
                                   NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::XPLANE11);
    airportByIdQuery->finish();

    airport = *ap;
    airportIdCache.insert(airportId, ap);
  }
}

void AirportQuery::getAirportByIdent(map::MapAirport& airport, const QString& ident)
{
  map::MapAirport *ap = airportIdentCache.object(ident);

  if(ap != nullptr)
    airport = *ap;
  else
  {
    ap = new map::MapAirport;

    airportByIdentQuery->bindValue(":ident", ident);
    airportByIdentQuery->exec();
    if(airportByIdentQuery->next())
      mapTypesFactory->fillAirport(airportByIdentQuery->record(), *ap, true, navdata,
                                   NavApp::getCurrentSimulatorDb() == atools::fs::FsPaths::XPLANE11);
    airportByIdentQuery->finish();

    airport = *ap;
    airportIdentCache.insert(ident, ap);
  }
}

Pos AirportQuery::getAirportCoordinatesByIdent(const QString& ident)
{
  Pos pos;
  airportCoordsByIdentQuery->bindValue(":ident", ident);
  airportCoordsByIdentQuery->exec();
  if(airportCoordsByIdentQuery->next())
    pos = Pos(airportCoordsByIdentQuery->value("lonx").toFloat(),
              airportCoordsByIdentQuery->value("laty").toFloat());
  airportCoordsByIdentQuery->finish();
  return pos;
}

bool AirportQuery::hasProcedures(const QString& ident) const
{
  return hasQueryByAirportIdent(*airportProcByIdentQuery, ident);
}

bool AirportQuery::hasAnyArrivalProcedures(const QString& ident) const
{
  return hasQueryByAirportIdent(*procArrivalByAirportIdentQuery, ident);
}

bool AirportQuery::hasDepartureProcedures(const QString& ident) const
{
  return hasQueryByAirportIdent(*procDepartureByAirportIdentQuery, ident);
}

bool AirportQuery::hasQueryByAirportIdent(atools::sql::SqlQuery& query, const QString& ident) const
{
  bool retval = false;
  query.bindValue(":ident", ident);
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
  runwayEndByIdQuery->bindValue(":id", id);
  runwayEndByIdQuery->exec();
  if(runwayEndByIdQuery->next())
    mapTypesFactory->fillRunwayEnd(runwayEndByIdQuery->record(), end, navdata);
  runwayEndByIdQuery->finish();
  return end;
}

void AirportQuery::getRunwayEndByNames(map::MapSearchResult& result, const QString& runwayName,
                                       const QString& airportIdent)
{
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

      ap.surface = apronQuery->value("surface").toString();
      ap.drawSurface = apronQuery->value("is_draw_surface").toInt() > 0;

      if(apronQuery->hasField("geometry"))
      {
        // X-Plane specific - contains bezier points for apron and taxiways.
        atools::fs::common::XpGeometry geo(apronQuery->value("geometry").toByteArray());
        ap.geometry = geo.getGeometry();
      }

      // Decode vertices into a position list - FSX/P3D
      if(!apronQuery->isNull("vertices"))
      {
        atools::fs::common::BinaryGeometry geo(apronQuery->value("vertices").toByteArray());
        geo.swapGeometry(ap.vertices);
      }

      aprons->append(ap);
    }
    apronCache.insert(airportId, aprons);
    return aprons;
  }
}

const QList<map::MapParking> *AirportQuery::getParkingsForAirport(int airportId)
{
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

void AirportQuery::getBestStartPositionForAirport(map::MapStart& start, int airportId, const QString& runwayName)
{
  start = map::MapStart();

  // No need to create a permanent query here since it is called rarely
  // Get runways ordered by length descending
  SqlQuery query(db);
  query.prepare(
    "select s.start_id, s.airport_id, s.type, s.heading, s.number, s.runway_name, s.altitude, s.lonx, s.laty, "
    "r.surface from start s left outer join runway_end e on s.runway_end_id = e.runway_end_id "
    "left outer join runway r on r.primary_end_id = e.runway_end_id "
    "where s.airport_id = :airportId "
    "order by r.length desc");
  query.bindValue(":airportId", airportId);
  query.exec();

  // Get runway by name if given
  if(!runwayName.isEmpty())
  {
    while(query.next())
    {
      if(runwayName == query.valueStr("runway_name"))
      {
        mapTypesFactory->fillStart(query.record(), start);
        break;
      }
    }
  }

  if(!start.isValid())
  {
    // Get best hard runway
    query.exec();

    // Get a runway with the best surface (hard)
    int bestSurfaceQuality = -1;
    while(query.next())
    {
      QString surface = query.valueStr("surface");

      int quality = map::surfaceQuality(surface);
      if(quality > bestSurfaceQuality || bestSurfaceQuality == -1)
      {
        bestSurfaceQuality = quality;
        mapTypesFactory->fillStart(query.record(), start);
      }
      if(map::isHardSurface(surface))
        break;
    }
  }
}

void AirportQuery::getStartByNameAndPos(map::MapStart& start, int airportId,
                                        const QString& runwayEndName, const atools::geo::Pos& position)
{
  // Get runway number for the first part of the query fetching start positions (before union)
  int number = runwayEndName.toInt();

  QString endName(runwayEndName);
  QString name, designator;
  if(map::runwayNameSplit(runwayEndName, &name, &designator))
    // It is a runway name - build correct name including leading zero
    endName = name + designator;

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
  query.bindValue(":runwayName", endName);
  query.bindValue(":airportId", airportId);
  query.exec();

  // Get all start positions
  QList<map::MapStart> starts;
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
    for(const map::MapStart& s : starts)
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
  startByIdQuery->bindValue(":id", startId);
  startByIdQuery->exec();

  if(startByIdQuery->next())
    mapTypesFactory->fillStart(startByIdQuery->record(), start);
  startByIdQuery->finish();
}

void AirportQuery::getParkingByNameAndNumber(QList<map::MapParking>& parkings, int airportId,
                                             const QString& name, int number)
{
  parkingTypeAndNumberQuery->bindValue(":airportId", airportId);
  if(name.isEmpty())
    // Use "like "%" if name is empty
    parkingTypeAndNumberQuery->bindValue(":name", "%");
  else
    parkingTypeAndNumberQuery->bindValue(":name", name);
  parkingTypeAndNumberQuery->bindValue(":number", number);
  parkingTypeAndNumberQuery->exec();

  while(parkingTypeAndNumberQuery->next())
  {
    map::MapParking parking;
    mapTypesFactory->fillParking(parkingTypeAndNumberQuery->record(), parking);
    parkings.append(parking);
  }
}

void AirportQuery::getParkingByName(QList<map::MapParking>& parkings, int airportId, const QString& name,
                                    const atools::geo::Pos& sortByDistancePos)
{
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

void AirportQuery::getBestRunwayEndAndAirport(map::MapRunwayEnd& runwayEnd, map::MapAirport& airport,
                                              const QVector<map::MapRunway>& runways, float heading)
{
  if(!runways.isEmpty())
  {
    // Loop through the list of distance ordered runways from getRunways
    for(const map::MapRunway& rw : runways)
    {
      map::MapRunwayEnd primaryEnd = getRunwayEndById(rw.primaryEndId);
      map::MapRunwayEnd secondaryEnd = getRunwayEndById(rw.secondaryEndId);

      // Check if either primary or secondary end matches by heading
      if(atools::geo::angleInRange(heading,
                                   atools::geo::normalizeCourse(primaryEnd.heading - MAX_HEADING_RUNWAY_DEVIATION),
                                   atools::geo::normalizeCourse(primaryEnd.heading + MAX_HEADING_RUNWAY_DEVIATION)))
      {
        qDebug() << "Found primary" << primaryEnd.name;
        runwayEnd = primaryEnd;
        break;
      }
      else if(atools::geo::angleInRange(heading,
                                        atools::geo::normalizeCourse(secondaryEnd.heading -
                                                                     MAX_HEADING_RUNWAY_DEVIATION),
                                        atools::geo::normalizeCourse(secondaryEnd.heading +
                                                                     MAX_HEADING_RUNWAY_DEVIATION)))
      {
        qDebug() << "Found secondary" << secondaryEnd.name;
        runwayEnd = secondaryEnd;
        break;
      }
    }

    // No runway end found - get at least the nearest airport
    getAirportById(airport, runways.first().airportId);

    if(!runwayEnd.isValid())
      qWarning() << Q_FUNC_INFO << "No runways found for takeoff/landing";
  }
}

void AirportQuery::getRunways(QVector<map::MapRunway>& runways, const atools::geo::Rect& rect,
                              const atools::geo::Pos& pos)
{
  SqlQuery query(db);
  query.prepare("select * from runway where lonx between :leftx and :rightx and laty between :bottomy and :topy");

  for(const Rect& r : rect.splitAtAntiMeridian())
  {
    query.bindValue(":leftx", r.getWest());
    query.bindValue(":rightx", r.getEast());
    query.bindValue(":bottomy", r.getSouth());
    query.bindValue(":topy", r.getNorth());
    query.exec();

    while(query.next())
    {
      map::MapRunway runway;
      mapTypesFactory->fillRunway(query.record(), runway, true /*overview*/);
      runways.append(runway);
    }
  }

  // Now sort the list of runways by distance to aircraft position and put the closest to the beginning of the list
  std::sort(runways.begin(), runways.end(), [pos](const map::MapRunway& r1, const map::MapRunway& r2) -> bool
  {
    LineDistance result1, result2;
    Line(r1.primaryPosition, r1.secondaryPosition).distanceMeterToLine(pos, result1);
    Line(r2.primaryPosition, r2.secondaryPosition).distanceMeterToLine(pos, result2);
    return result1.distance < result2.distance;
  });

  if(runways.isEmpty())
    qWarning() << Q_FUNC_INFO << "No runways found for takeoff/landing at" << pos << "inside" << rect;
}

const QList<map::MapTaxiPath> *AirportQuery::getTaxiPaths(int airportId)
{
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
      tp.start = Pos(taxiparthQuery->value("start_lonx").toFloat(), taxiparthQuery->value("start_laty").toFloat());
      tp.end = Pos(taxiparthQuery->value("end_lonx").toFloat(), taxiparthQuery->value("end_laty").toFloat());
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
  const QList<map::MapRunway> *aprunways = getRunways(airportId);
  if(aprunways != nullptr)
  {
    for(const map::MapRunway& mr : *aprunways)
    {
      if(mr.primaryName == runway)
        return getRunwayEndById(mr.primaryEndId);

      if(mr.secondaryName == runway)
        return getRunwayEndById(mr.secondaryEndId);
    }
  }
  return map::MapRunwayEnd();
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
    "num_runway_end_vasi", "num_runway_end_als", "num_boundary_fence", "num_runway_end_closed",
    "num_approach", "num_runway_hard", "num_runway_soft", "num_runway_water", "num_runway_light", "num_runway_end_ils",
    "num_helipad",
    "longest_runway_length", "longest_runway_heading",
    "mag_var",
    "tower_lonx", "tower_laty",
    "altitude",
    "lonx", "laty",
    "left_lonx", "top_laty", "right_lonx", "bottom_laty"
  });

  SqlRecord aprec = db->record("airport");
  if(aprec.contains("region"))
    airportQueryBase.append("region");
  if(aprec.contains("is_3d"))
    airportQueryBase.append("is_3d");
  if(aprec.contains("transition_altitude"))
    airportQueryBase.append("transition_altitude");
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

  SqlRecord aprec = db->record("airport_medium");
  if(aprec.contains("region"))
    airportQueryBase.append("region");
  if(aprec.contains("is_3d"))
    airportQueryBase.append("is_3d");
  return airportQueryBase;
}

void AirportQuery::initQueries()
{
  // Common where clauses
  static const QString whereRect("lonx between :leftx and :rightx and laty between :bottomy and :topy");
  static const QString whereIdentRegion("ident = :ident and region like :region");
  static const QString whereLimit("limit " + QString::number(queryRowLimit));

  QStringList const airportQueryBase = airportColumns(db);
  QStringList const airportQueryBaseOverview = airportOverviewColumns(db);

  static const QString parkingQueryBase(
    "parking_id, airport_id, type, name, airline_codes, number, radius, heading, has_jetway, lonx, laty ");

  deInitQueries();

  airportByIdQuery = new SqlQuery(db);
  airportByIdQuery->prepare("select " + airportQueryBase.join(", ") + " from airport where airport_id = :id ");

  airportAdminByIdQuery = new SqlQuery(db);
  airportAdminByIdQuery->prepare("select city, state, country from airport where airport_id = :id ");

  airportProcByIdentQuery = new SqlQuery(db);
  airportProcByIdentQuery->prepare("select 1 from airport where ident = :ident limit 1");

  procArrivalByAirportIdentQuery = new SqlQuery(db);
  procArrivalByAirportIdentQuery->prepare("select 1 from approach  "
                                          "where airport_ident = :ident and  "
                                          "((type = 'GPS' and suffix = 'A') or (suffix <> 'D' or suffix is null))");

  procDepartureByAirportIdentQuery = new SqlQuery(db);
  procDepartureByAirportIdentQuery->prepare("select 1 from approach "
                                            "where airport_ident = :ident and type = 'GPS' and suffix = 'D' limit 1");

  airportByIdentQuery = new SqlQuery(db);
  airportByIdentQuery->prepare("select " + airportQueryBase.join(", ") + " from airport where ident = :ident ");

  airportCoordsByIdentQuery = new SqlQuery(db);
  airportCoordsByIdentQuery->prepare("select lonx, laty from airport where ident = :ident ");

  runwayEndByIdQuery = new SqlQuery(db);
  runwayEndByIdQuery->prepare("select end_type, name, heading, lonx, laty from runway_end where runway_end_id = :id");

  runwayEndByNameQuery = new SqlQuery(db);
  runwayEndByNameQuery->prepare(
    "select e.end_type, e.name, e.heading, e.lonx, e.laty "
    "from runway r join runway_end e on (r.primary_end_id = e.runway_end_id or r.secondary_end_id = e.runway_end_id) "
    "join airport a on r.airport_id = a.airport_id "
    "where e.name = :name and a.ident = :airport");

  // Runways > 4000 feet for simplyfied runway overview
  runwayOverviewQuery = new SqlQuery(db);
  runwayOverviewQuery->prepare(
    "select length, heading, lonx, laty, primary_lonx, primary_laty, secondary_lonx, secondary_laty "
    "from runway where airport_id = :airportId and length > 4000 " + whereLimit);

  apronQuery = new SqlQuery(db);
  apronQuery->prepare(
    "select * from apron where airport_id = :airportId");

  parkingQuery = new SqlQuery(db);
  parkingQuery->prepare("select " + parkingQueryBase + " from parking where airport_id = :airportId");

  // Start positions ordered by type (runway, helipad) and name
  startQuery = new SqlQuery(db);
  startQuery->prepare(
    "select s.start_id, s.airport_id, s.type, s.heading, s.number, s.runway_name, s.altitude, s.lonx, s.laty "
    "from start s where s.airport_id = :airportId "
    "order by s.type desc, s.runway_name");

  startByIdQuery = new SqlQuery(db);
  startByIdQuery->prepare(
    "select start_id, airport_id, type, heading, number, runway_name, altitude, lonx, laty "
    "from start s where start_id = :id");

  parkingTypeAndNumberQuery = new SqlQuery(db);
  parkingTypeAndNumberQuery->prepare(
    "select " + parkingQueryBase +
    " from parking where airport_id = :airportId and name like :name and number = :number order by radius desc");

  parkingNameQuery = new SqlQuery(db);
  parkingNameQuery->prepare("select " + parkingQueryBase +
                            " from parking where airport_id = :airportId and name like :name order by radius desc");

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

  delete runwayOverviewQuery;
  runwayOverviewQuery = nullptr;

  delete apronQuery;
  apronQuery = nullptr;

  delete parkingQuery;
  parkingQuery = nullptr;

  delete startQuery;
  startQuery = nullptr;
  delete startByIdQuery;
  startByIdQuery = nullptr;

  delete parkingTypeAndNumberQuery;
  parkingTypeAndNumberQuery = nullptr;

  delete parkingNameQuery;
  parkingNameQuery = nullptr;

  delete helipadQuery;
  helipadQuery = nullptr;

  delete taxiparthQuery;
  taxiparthQuery = nullptr;

  delete runwaysQuery;
  runwaysQuery = nullptr;

  delete airportByIdQuery;
  airportByIdQuery = nullptr;

  delete airportAdminByIdQuery;
  airportAdminByIdQuery = nullptr;

  delete airportProcByIdentQuery;
  airportProcByIdentQuery = nullptr;

  delete procDepartureByAirportIdentQuery;
  procDepartureByAirportIdentQuery = nullptr;

  delete procArrivalByAirportIdentQuery;
  procArrivalByAirportIdentQuery = nullptr;

  delete airportByIdentQuery;
  airportByIdentQuery = nullptr;

  delete airportCoordsByIdentQuery;
  airportCoordsByIdentQuery = nullptr;

  delete runwayEndByIdQuery;
  runwayEndByIdQuery = nullptr;

  delete runwayEndByNameQuery;
  runwayEndByNameQuery = nullptr;
}

QHash<int, QList<map::MapParking> > AirportQuery::getParkingCache() const
{
  QHash<int, QList<map::MapParking> > retval;

  for(int key : parkingCache.keys())
    retval.insert(key, *parkingCache.object(key));

  return retval;
}

QHash<int, QList<map::MapHelipad> > AirportQuery::getHelipadCache() const
{
  QHash<int, QList<map::MapHelipad> > retval;

  for(int key : helipadCache.keys())
    retval.insert(key, *helipadCache.object(key));

  return retval;
}
