/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#include "mapgui/mapquery.h"

#include "sql/sqlquery.h"
#include "geo/rect.h"
#include "geo/calculations.h"
#include "common/coordinateconverter.h"
#include "maplayer.h"
#include "common/maptools.h"

#include <algorithm>
#include <functional>
#include <marble/GeoDataLatLonBox.h>
#include <common/maptypesfactory.h>

using namespace Marble;
using namespace atools::sql;
using namespace atools::geo;

MapQuery::MapQuery(QObject *parent, atools::sql::SqlDatabase *sqlDb)
  : QObject(parent), db(sqlDb)
{
  mapTypesFactory = new MapTypesFactory();
}

MapQuery::~MapQuery()
{
  deInitQueries();
  delete mapTypesFactory;
}

void MapQuery::getAirportAdminById(int airportId, QString& city, QString& state, QString& country)
{
  airportAdminByIdQuery->bindValue(":id", airportId);
  airportAdminByIdQuery->exec();
  if(airportAdminByIdQuery->next())
  {
    city = airportAdminByIdQuery->value("city").toString();
    state = airportAdminByIdQuery->value("state").toString();
    country = airportAdminByIdQuery->value("country").toString();
  }
}

void MapQuery::getAirportById(maptypes::MapAirport& airport, int airportId)
{
  airportByIdQuery->bindValue(":id", airportId);
  airportByIdQuery->exec();
  if(airportByIdQuery->next())
    mapTypesFactory->fillAirport(airportByIdQuery->record(), airport, true);
}

void MapQuery::getVorForWaypoint(maptypes::MapVor& vor, int waypointId)
{
  vorByWaypointIdQuery->bindValue(":id", waypointId);
  vorByWaypointIdQuery->exec();
  if(vorByWaypointIdQuery->next())
    mapTypesFactory->fillVor(vorByWaypointIdQuery->record(), vor);
}

void MapQuery::getNdbForWaypoint(maptypes::MapNdb& ndb, int waypointId)
{
  ndbByWaypointIdQuery->bindValue(":id", waypointId);
  ndbByWaypointIdQuery->exec();
  if(ndbByWaypointIdQuery->next())
    mapTypesFactory->fillNdb(ndbByWaypointIdQuery->record(), ndb);
}

void MapQuery::getAirwaysForWaypoint(QList<maptypes::MapAirway>& airways, int waypointId)
{
  airwayByWaypointIdQuery->bindValue(":id", waypointId);
  airwayByWaypointIdQuery->exec();
  while(airwayByWaypointIdQuery->next())
  {
    maptypes::MapAirway airway;
    mapTypesFactory->fillAirway(airwayByWaypointIdQuery->record(), airway);
    airways.append(airway);
  }
}

void MapQuery::getAirwayById(maptypes::MapAirway& airway, int airwayId)
{
  airwayByIdQuery->bindValue(":id", airwayId);
  airwayByIdQuery->exec();
  if(airwayByIdQuery->next())
    mapTypesFactory->fillAirway(airwayByIdQuery->record(), airway);
}

// TODO no delete needed here
void MapQuery::getMapObjectByIdent(maptypes::MapSearchResult& result, maptypes::MapObjectTypes type,
                                   const QString& ident, const QString& region)
{
  if(type == maptypes::AIRPORT)
  {
    airportByIdentQuery->bindValue(":ident", ident);
    airportByIdentQuery->exec();
    while(airportByIdentQuery->next())
    {
      maptypes::MapAirport ap;
      mapTypesFactory->fillAirport(airportByIdentQuery->record(), ap, true);
      result.airports.append(ap);
    }
  }
  else if(type == maptypes::VOR)
  {
    vorByIdentQuery->bindValue(":ident", ident);
    vorByIdentQuery->bindValue(":region", region.isEmpty() ? "%" : region);
    vorByIdentQuery->exec();
    while(vorByIdentQuery->next())
    {
      maptypes::MapVor vor;
      mapTypesFactory->fillVor(vorByIdentQuery->record(), vor);
      result.vors.append(vor);
    }
  }
  else if(type == maptypes::NDB)
  {
    ndbByIdentQuery->bindValue(":ident", ident);
    ndbByIdentQuery->bindValue(":region", region.isEmpty() ? "%" : region);
    ndbByIdentQuery->exec();
    while(ndbByIdentQuery->next())
    {
      maptypes::MapNdb ndb;
      mapTypesFactory->fillNdb(ndbByIdentQuery->record(), ndb);
      result.ndbs.append(ndb);
    }
  }
  else if(type == maptypes::WAYPOINT)
  {
    waypointByIdentQuery->bindValue(":ident", ident);
    waypointByIdentQuery->bindValue(":region", region.isEmpty() ? "%" : region);
    waypointByIdentQuery->exec();
    while(waypointByIdentQuery->next())
    {
      maptypes::MapWaypoint wp;
      mapTypesFactory->fillWaypoint(waypointByIdentQuery->record(), wp);
      result.waypoints.append(wp);
    }
  }
}

void MapQuery::getMapObjectById(maptypes::MapSearchResult& result, maptypes::MapObjectTypes type, int id)
{
  if(type == maptypes::AIRPORT)
  {
    airportByIdQuery->bindValue(":id", id);
    airportByIdQuery->exec();
    if(airportByIdQuery->next())
    {
      maptypes::MapAirport ap;
      mapTypesFactory->fillAirport(airportByIdQuery->record(), ap, true);
      result.airports.append(ap);
    }
  }
  else if(type == maptypes::VOR)
  {
    vorByIdQuery->bindValue(":id", id);
    vorByIdQuery->exec();
    if(vorByIdQuery->next())
    {
      maptypes::MapVor vor;
      mapTypesFactory->fillVor(vorByIdQuery->record(), vor);
      result.vors.append(vor);
    }
  }
  else if(type == maptypes::NDB)
  {
    ndbByIdQuery->bindValue(":id", id);
    ndbByIdQuery->exec();
    if(ndbByIdQuery->next())
    {
      maptypes::MapNdb ndb;
      mapTypesFactory->fillNdb(ndbByIdQuery->record(), ndb);
      result.ndbs.append(ndb);
    }
  }
  else if(type == maptypes::WAYPOINT)
  {
    waypointByIdQuery->bindValue(":id", id);
    waypointByIdQuery->exec();
    if(waypointByIdQuery->next())
    {
      maptypes::MapWaypoint wp;
      mapTypesFactory->fillWaypoint(waypointByIdQuery->record(), wp);
      result.waypoints.append(wp);
    }
  }
}

// TODO no delete needed here
void MapQuery::getNearestObjects(const CoordinateConverter& conv, const MapLayer *mapLayer,
                                 bool airportDiagram, maptypes::MapObjectTypes types,
                                 int xs, int ys, int screenDistance,
                                 maptypes::MapSearchResult& result)
{
  using maptools::insertSortedByDistance;
  using maptools::insertSortedByTowerDistance;
  using namespace maptypes;

  int x, y;
  if(mapLayer->isAirport() && types.testFlag(maptypes::AIRPORT))
    for(int i = airportCache.list.size() - 1; i >= 0; i--)
    {
    const MapAirport& airport = airportCache.list.at(i);

    if(airport.isVisible(types))
    {
      if(conv.wToS(airport.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.airports, &result.airportIds, xs, ys, airport);

      if(airportDiagram)
        if(conv.wToS(airport.towerCoords, x, y))
          if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
            insertSortedByTowerDistance(conv, result.towers, xs, ys, airport);
    }
    }

  if(mapLayer->isVor() && types.testFlag(maptypes::VOR))
    for(int i = vorCache.list.size() - 1; i >= 0; i--)
    {
      const MapVor& vor = vorCache.list.at(i);
      if(conv.wToS(vor.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.vors, &result.vorIds, xs, ys, vor);
    }

  if(mapLayer->isNdb() && types.testFlag(maptypes::NDB))
    for(int i = ndbCache.list.size() - 1; i >= 0; i--)
    {
      const MapNdb& ndb = ndbCache.list.at(i);
      if(conv.wToS(ndb.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.ndbs, &result.ndbIds, xs, ys, ndb);
    }

  if(mapLayer->isWaypoint() && types.testFlag(maptypes::WAYPOINT))
    for(int i = waypointCache.list.size() - 1; i >= 0; i--)
    {
      const MapWaypoint& wp = waypointCache.list.at(i);
      if(conv.wToS(wp.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.waypoints, &result.waypointIds, xs, ys, wp);
    }

  if(mapLayer->isAirway())
    for(int i = waypointCache.list.size() - 1; i >= 0; i--)
    {
      const MapWaypoint& wp = waypointCache.list.at(i);
      if((wp.hasVictor && types.testFlag(maptypes::AIRWAYV)) ||
         (wp.hasJet && types.testFlag(maptypes::AIRWAYJ)))
        if(conv.wToS(wp.position, x, y))
          if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
            insertSortedByDistance(conv, result.waypoints, &result.waypointIds, xs, ys, wp);
    }

  if(mapLayer->isMarker() && types.testFlag(maptypes::MARKER))
    for(int i = markerCache.list.size() - 1; i >= 0; i--)
    {
      const MapMarker& wp = markerCache.list.at(i);
      if(conv.wToS(wp.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.markers, nullptr, xs, ys, wp);
    }

  if(mapLayer->isIls() && types.testFlag(maptypes::ILS))
    for(int i = ilsCache.list.size() - 1; i >= 0; i--)
    {
      const MapIls& wp = ilsCache.list.at(i);
      if(conv.wToS(wp.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.ils, nullptr, xs, ys, wp);
    }

  if(airportDiagram)
  {
    for(int id : parkingCache.keys())
    {
      QList<MapParking> *parkings = parkingCache.object(id);
      for(const MapParking& p : *parkings)
        if(conv.wToS(p.position, x, y))
          if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
            insertSortedByDistance(conv, result.parkings, nullptr, xs, ys, p);
    }

    for(int id : helipadCache.keys())
    {
      QList<MapHelipad> *helipads = helipadCache.object(id);
      for(const MapHelipad& p : *helipads)
        if(conv.wToS(p.position, x, y))
          if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
            insertSortedByDistance(conv, result.helipads, nullptr, xs, ys, p);
    }
  }
}

const QList<maptypes::MapAirport> *MapQuery::getAirports(const Marble::GeoDataLatLonBox& rect,
                                                         const MapLayer *mapLayer, bool lazy)
{
  if(airportCache.handleCache(rect, mapLayer, lazy))
    qDebug() << "MapQuery airports cache miss";

  switch(mapLayer->getDataSource())
  {
    case layer::ALL:
      airportByRectQuery->bindValue(":minlength", mapLayer->getMinRunwayLength());
      return fetchAirports(rect, airportByRectQuery, true, lazy, false);

    case layer::MEDIUM:
      return fetchAirports(rect, airportMediumByRectQuery, false, lazy, true);

    case layer::LARGE:
      return fetchAirports(rect, airportLargeByRectQuery, false, lazy, true);

  }
  return nullptr;
}

const QList<maptypes::MapWaypoint> *MapQuery::getWaypoints(const GeoDataLatLonBox& rect,
                                                           const MapLayer *mapLayer, bool lazy)
{
  if(waypointCache.handleCache(rect, mapLayer, lazy))
    qDebug() << "MapQuery waypoints cache miss";

  if(waypointCache.list.isEmpty() && !lazy)
  {
    for(const GeoDataLatLonBox& r : splitAtAntiMeridian(rect))
    {
      bindCoordinatePointInRect(r, waypointsByRectQuery);
      waypointsByRectQuery->exec();
      while(waypointsByRectQuery->next())
      {
        maptypes::MapWaypoint wp;
        mapTypesFactory->fillWaypoint(waypointsByRectQuery->record(), wp);
        waypointCache.list.append(wp);
      }
    }
    checkOverflow(waypointCache.list, maptypes::WAYPOINT);
  }
  return &waypointCache.list;
}

const QList<maptypes::MapVor> *MapQuery::getVors(const GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                                 bool lazy)
{
  if(vorCache.handleCache(rect, mapLayer, lazy))
    qDebug() << "MapQuery vor cache miss";

  if(vorCache.list.isEmpty() && !lazy)
  {
    for(const GeoDataLatLonBox& r : splitAtAntiMeridian(rect))
    {
      bindCoordinatePointInRect(r, vorsByRectQuery);
      vorsByRectQuery->exec();
      while(vorsByRectQuery->next())
      {
        maptypes::MapVor vor;
        mapTypesFactory->fillVor(vorsByRectQuery->record(), vor);
        vorCache.list.append(vor);
      }
    }
    checkOverflow(vorCache.list, maptypes::VOR);
  }
  return &vorCache.list;
}

const QList<maptypes::MapNdb> *MapQuery::getNdbs(const GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                                 bool lazy)
{
  if(ndbCache.handleCache(rect, mapLayer, lazy))
    qDebug() << "MapQuery ndb cache miss";

  if(ndbCache.list.isEmpty() && !lazy)
  {
    for(const GeoDataLatLonBox& r : splitAtAntiMeridian(rect))
    {
      bindCoordinatePointInRect(r, ndbsByRectQuery);
      ndbsByRectQuery->exec();
      while(ndbsByRectQuery->next())
      {
        maptypes::MapNdb ndb;
        mapTypesFactory->fillNdb(ndbsByRectQuery->record(), ndb);
        ndbCache.list.append(ndb);
      }
    }
    checkOverflow(ndbCache.list, maptypes::NDB);
  }
  return &ndbCache.list;
}

const QList<maptypes::MapMarker> *MapQuery::getMarkers(const GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                                       bool lazy)
{
  if(markerCache.handleCache(rect, mapLayer, lazy))
    qDebug() << "MapQuery marker cache miss";

  if(markerCache.list.isEmpty() && !lazy)
    for(const GeoDataLatLonBox& r : splitAtAntiMeridian(rect))
    {
      bindCoordinatePointInRect(r, markersByRectQuery);
      markersByRectQuery->exec();
      while(markersByRectQuery->next())
      {
        maptypes::MapMarker marker;
        mapTypesFactory->fillMarker(markersByRectQuery->record(), marker);
        markerCache.list.append(marker);
      }
    }
  return &markerCache.list;
}

const QList<maptypes::MapIls> *MapQuery::getIls(const GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                                bool lazy)
{
  if(ilsCache.handleCache(rect, mapLayer, lazy))
    qDebug() << "MapQuery ils cache miss";

  if(ilsCache.list.isEmpty() && !lazy)
    for(const GeoDataLatLonBox& r : splitAtAntiMeridian(rect))
    {
      bindCoordinatePointInRect(r, ilsByRectQuery);
      ilsByRectQuery->exec();
      while(ilsByRectQuery->next())
      {
        maptypes::MapIls ils;
        mapTypesFactory->fillIls(ilsByRectQuery->record(), ils);
        ilsCache.list.append(ils);
      }
    }
  return &ilsCache.list;
}

const QList<maptypes::MapAirway> *MapQuery::getAirways(const GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                                       bool lazy)
{
  if(airwayCache.handleCache(rect, mapLayer, lazy))
    qDebug() << "MapQuery airway cache miss";

  if(airwayCache.list.isEmpty() && !lazy)
  {
    for(const GeoDataLatLonBox& r : splitAtAntiMeridian(rect))
    {
      bindCoordinatePointInRect(r, airwayByRectQuery);
      airwayByRectQuery->exec();
      while(airwayByRectQuery->next())
      {
        maptypes::MapAirway airway;
        mapTypesFactory->fillAirway(airwayByRectQuery->record(), airway);
        airwayCache.list.append(airway);
      }
    }
    checkOverflow(airwayCache.list, maptypes::AIRWAY);
  }
  return &airwayCache.list;
}

const QList<maptypes::MapAirport> *MapQuery::fetchAirports(const Marble::GeoDataLatLonBox& rect,
                                                           atools::sql::SqlQuery *query, bool reverse,
                                                           bool lazy, bool overview)
{
  if(airportCache.list.isEmpty() && !lazy)
  {
    for(const GeoDataLatLonBox& r : splitAtAntiMeridian(rect))
    {
      bindCoordinatePointInRect(r, query);
      query->exec();
      while(query->next())
      {
        maptypes::MapAirport ap;
        if(overview)
          mapTypesFactory->fillAirportForOverview(query->record(), ap);
        else
          mapTypesFactory->fillAirport(query->record(), ap, true);

        if(reverse)
          airportCache.list.prepend(ap);
        else
          airportCache.list.append(ap);
      }
    }
    checkOverflow(airportCache.list, maptypes::AIRPORT);
  }
  return &airportCache.list;
}

const QList<maptypes::MapRunway> *MapQuery::getRunwaysForOverview(int airportId)
{
  if(runwayOverwiewCache.contains(airportId))
    return runwayOverwiewCache.object(airportId);
  else
  {
    qDebug() << "runwaysOverwiew cache miss";
    using atools::geo::Pos;

    runwayOverviewQuery->bindValue(":airportId", airportId);
    runwayOverviewQuery->exec();

    QList<maptypes::MapRunway> *rws = new QList<maptypes::MapRunway>;
    while(runwayOverviewQuery->next())
    {
      maptypes::MapRunway r =
      {
        QString(),
        QString(),
        QString(),
        QString(),
        runwayOverviewQuery->value("length").toInt(),
        static_cast<int>(std::roundf(runwayOverviewQuery->value("heading").toFloat())),
        0,
        0,
        0,
        Pos(runwayOverviewQuery->value("lonx").toFloat(),
            runwayOverviewQuery->value("laty").toFloat()),
        Pos(runwayOverviewQuery->value("primary_lonx").toFloat(),
            runwayOverviewQuery->value("primary_laty").toFloat()),
        Pos(runwayOverviewQuery->value("secondary_lonx").toFloat(),
            runwayOverviewQuery->value("secondary_laty").toFloat()),
        false,
        false
      };
      rws->append(r);
    }
    runwayOverwiewCache.insert(airportId, rws);
    return rws;
  }
}

const QList<maptypes::MapApron> *MapQuery::getAprons(int airportId)
{
  if(apronCache.contains(airportId))
    return apronCache.object(airportId);
  else
  {
    qDebug() << "aprons cache miss";
    apronQuery->bindValue(":airportId", airportId);
    apronQuery->exec();

    QList<maptypes::MapApron> *aps = new QList<maptypes::MapApron>;
    while(apronQuery->next())
    {
      maptypes::MapApron ap;

      ap.surface = apronQuery->value("surface").toString();
      ap.drawSurface = apronQuery->value("is_draw_surface").toInt() > 0;

      QString vertices = apronQuery->value("vertices").toString();
      QStringList vertexList = vertices.split(",");
      for(QString vertex : vertexList)
      {
        QStringList ordinates = vertex.split(" ", QString::SkipEmptyParts);

        if(ordinates.size() == 2)
          ap.vertices.append(ordinates.at(0).toFloat(), ordinates.at(1).toFloat());
      }
      aps->append(ap);
    }
    apronCache.insert(airportId, aps);
    return aps;
  }
}

const QList<maptypes::MapParking> *MapQuery::getParkingsForAirport(int airportId)
{
  if(parkingCache.contains(airportId))
    return parkingCache.object(airportId);
  else
  {
    qDebug() << "parkings cache miss";
    parkingQuery->bindValue(":airportId", airportId);
    parkingQuery->exec();

    QList<maptypes::MapParking> *ps = new QList<maptypes::MapParking>;
    while(parkingQuery->next())
    {
      maptypes::MapParking p;

      if(parkingQuery->value("type").toString() != "VEHICLES")
      {
        mapTypesFactory->fillParking(parkingQuery->record(), p);
        ps->append(p);
      }
    }
    parkingCache.insert(airportId, ps);
    return ps;
  }
}

void MapQuery::getParkingByNameAndNumber(QList<maptypes::MapParking>& parkings, int airportId,
                                         const QString& name, int number)
{
  parkingTypeAndNumberQuery->bindValue(":airportId", airportId);
  if(name.isEmpty())
    parkingTypeAndNumberQuery->bindValue(":name", "%");
  else
    parkingTypeAndNumberQuery->bindValue(":name", name);
  parkingTypeAndNumberQuery->bindValue(":number", number);
  parkingTypeAndNumberQuery->exec();

  while(parkingTypeAndNumberQuery->next())
  {
    maptypes::MapParking parking;
    mapTypesFactory->fillParking(parkingTypeAndNumberQuery->record(), parking);
    parkings.append(parking);
  }
}

const QList<maptypes::MapHelipad> *MapQuery::getHelipads(int airportId)
{
  if(helipadCache.contains(airportId))
    return helipadCache.object(airportId);
  else
  {
    qDebug() << "helipads cache miss";
    helipadQuery->bindValue(":airportId", airportId);
    helipadQuery->exec();

    QList<maptypes::MapHelipad> *hs = new QList<maptypes::MapHelipad>;
    while(helipadQuery->next())
    {
      maptypes::MapHelipad hp;

      hp.position = Pos(helipadQuery->value("lonx").toFloat(), helipadQuery->value("laty").toFloat()),
      hp.width = helipadQuery->value("width").toInt();
      hp.length = helipadQuery->value("length").toInt();
      hp.heading = static_cast<int>(std::roundf(helipadQuery->value("heading").toFloat()));
      hp.surface = helipadQuery->value("surface").toString();
      hp.type = helipadQuery->value("type").toString();
      hp.transparent = helipadQuery->value("is_transparent").toInt() > 0;
      hp.closed = helipadQuery->value("is_closed").toInt() > 0;

      hs->append(hp);
    }
    helipadCache.insert(airportId, hs);
    return hs;
  }
}

Rect MapQuery::getAirportRect(int airportId)
{
  SqlQuery query(db);
  query.prepare("select left_lonx, top_laty, right_lonx, bottom_laty from airport where airport_id = :id");
  query.bindValue(":id", airportId);
  query.exec();
  if(query.next())
    return atools::geo::Rect(query.value("left_lonx").toFloat(), query.value("top_laty").toFloat(),
                             query.value("right_lonx").toFloat(), query.value("bottom_laty").toFloat());

  return atools::geo::Rect();
}

const QList<maptypes::MapTaxiPath> *MapQuery::getTaxiPaths(int airportId)
{
  if(taxipathCache.contains(airportId))
    return taxipathCache.object(airportId);
  else
  {
    qDebug() << "taxipaths cache miss";
    taxiparthQuery->bindValue(":airportId", airportId);
    taxiparthQuery->exec();

    QList<maptypes::MapTaxiPath> *tps = new QList<maptypes::MapTaxiPath>;
    while(taxiparthQuery->next())
    {
      maptypes::MapTaxiPath tp;
      QString type = taxiparthQuery->value("type").toString();
      tp.drawSurface = taxiparthQuery->value("is_draw_surface").toInt() > 0;
      tp.start = Pos(taxiparthQuery->value("start_lonx").toFloat(), taxiparthQuery->value(
                       "start_laty").toFloat()),
      tp.end = Pos(taxiparthQuery->value("end_lonx").toFloat(), taxiparthQuery->value("end_laty").toFloat()),
      tp.surface = taxiparthQuery->value("surface").toString();
      tp.name = taxiparthQuery->value("name").toString();
      tp.width = taxiparthQuery->value("width").toInt();

      tps->append(tp);
    }
    taxipathCache.insert(airportId, tps);
    return tps;
  }
}

const QList<maptypes::MapRunway> *MapQuery::getRunways(int airportId)
{
  if(runwayCache.contains(airportId))
    return runwayCache.object(airportId);
  else
  {
    qDebug() << "runways cache miss";
    runwaysQuery->bindValue(":airportId", airportId);
    runwaysQuery->exec();

    // TODO delete
    QList<maptypes::MapRunway> *rs = new QList<maptypes::MapRunway>;
    while(runwaysQuery->next())
    {
      maptypes::MapRunway r =
      {
        runwaysQuery->value("surface").toString(),
        runwaysQuery->value("primary_name").toString(),
        runwaysQuery->value("secondary_name").toString(),
        runwaysQuery->value("edge_light").toString(),
        runwaysQuery->value("length").toInt(),
        static_cast<int>(std::roundf(runwaysQuery->value("heading").toFloat())),
        runwaysQuery->value("width").toInt(),
        runwaysQuery->value("primary_offset_threshold").toInt(),
        runwaysQuery->value("secondary_offset_threshold").toInt(),
        Pos(runwaysQuery->value("lonx").toFloat(), runwaysQuery->value("laty").toFloat()),
        Pos(runwaysQuery->value("primary_lonx").toFloat(),
            runwaysQuery->value("primary_laty").toFloat()),
        Pos(runwaysQuery->value("secondary_lonx").toFloat(),
            runwaysQuery->value("secondary_laty").toFloat()),
        runwaysQuery->value("primary_closed_markings").toInt() > 0,
        runwaysQuery->value("secondary_closed_markings").toInt() > 0
      };
      rs->append(r);
    }

    // Sort to draw the hard runways last
    using namespace std::placeholders;
    std::sort(rs->begin(), rs->end(), std::bind(&MapQuery::runwayCompare, this, _1, _2));

    runwayCache.insert(airportId, rs);
    return rs;
  }
}

bool MapQuery::runwayCompare(const maptypes::MapRunway& r1, const maptypes::MapRunway& r2)
{
  // The value returned indicates whether the element passed as first argument is
  // considered to go before the second
  if(r1.isHard() && r2.isHard())
    return r1.length < r2.length;
  else
    return r1.isSoft() && r2.isHard();
}

void MapQuery::bindCoordinatePointInRect(const Marble::GeoDataLatLonBox& rect, atools::sql::SqlQuery *query,
                                         const QString& prefix)
{
  query->bindValue(":" + prefix + "leftx", rect.west(GeoDataCoordinates::Degree));
  query->bindValue(":" + prefix + "rightx", rect.east(GeoDataCoordinates::Degree));
  query->bindValue(":" + prefix + "bottomy", rect.south(GeoDataCoordinates::Degree));
  query->bindValue(":" + prefix + "topy", rect.north(GeoDataCoordinates::Degree));
}

QList<Marble::GeoDataLatLonBox> MapQuery::splitAtAntiMeridian(const Marble::GeoDataLatLonBox& rect)
{
  GeoDataLatLonBox newRect = rect;
  inflateRect(newRect,
              newRect.width(GeoDataCoordinates::Degree) * RECT_INFLATION_FACTOR + RECT_INFLATION_ADD,
              newRect.height(GeoDataCoordinates::Degree) * RECT_INFLATION_FACTOR + RECT_INFLATION_ADD);

  if(newRect.crossesDateLine())
  {
    GeoDataLatLonBox westOf;
    westOf.setBoundaries(newRect.north(GeoDataCoordinates::Degree),
                         newRect.south(GeoDataCoordinates::Degree),
                         180.,
                         newRect.west(GeoDataCoordinates::Degree), GeoDataCoordinates::Degree);

    GeoDataLatLonBox eastOf;
    eastOf.setBoundaries(newRect.north(GeoDataCoordinates::Degree),
                         newRect.south(GeoDataCoordinates::Degree),
                         newRect.east(GeoDataCoordinates::Degree),
                         -180., GeoDataCoordinates::Degree);

    return QList<GeoDataLatLonBox>({westOf, eastOf});
  }
  else
    return QList<GeoDataLatLonBox>({newRect});
}

void MapQuery::inflateRect(Marble::GeoDataLatLonBox& rect, double width, double height)
{
  rect.setNorth(std::min(rect.north(GeoDataCoordinates::Degree) + height, 89.), GeoDataCoordinates::Degree);
  rect.setSouth(std::max(rect.south(GeoDataCoordinates::Degree) - height, -89.), GeoDataCoordinates::Degree);
  rect.setWest(std::max(rect.west(GeoDataCoordinates::Degree) - width, -179.), GeoDataCoordinates::Degree);
  rect.setEast(std::min(rect.east(GeoDataCoordinates::Degree) + width, 179.), GeoDataCoordinates::Degree);
}

template<typename TYPE>
void MapQuery::checkOverflow(const QList<TYPE>& list, maptypes::MapObjectTypes type)
{
  if(list.size() >= QUERY_ROW_LIMIT)
    emit resultTruncated(type, QUERY_ROW_LIMIT);
  else
    emit resultTruncated(type, 0);
}

void MapQuery::initQueries()
{
  static QString whereRect("lonx between :leftx and :rightx and laty between :bottomy and :topy");
  static QString whereIdentRegion("ident = :ident and region like :region");
  static QString whereLimit("limit " + QString::number(QUERY_ROW_LIMIT));

  static QString airportQueryBase(
    "select airport_id, ident, name, "
    "has_avgas, has_jetfuel, has_tower_object, "
    "tower_frequency, atis_frequency, awos_frequency, asos_frequency, unicom_frequency, "
    "is_closed, is_military, is_addon, num_apron, num_taxi_path, "
    "num_parking_gate,  num_parking_ga_ramp,  num_parking_cargo,  num_parking_mil_cargo,  num_parking_mil_combat, "
    "num_runway_end_vasi,  num_runway_end_als,  num_boundary_fence, num_runway_end_closed, "
    "num_approach, num_runway_hard, num_runway_soft, num_runway_water, "
    "num_runway_light, num_runway_end_ils, num_helipad, "
    "longest_runway_length, longest_runway_heading, mag_var, "
    "tower_lonx, tower_laty, altitude, lonx, laty, left_lonx, top_laty, right_lonx, bottom_laty ");

  static QString airportQueryBaseOverview(
    "select airport_id, ident, name, "
    "has_avgas, has_jetfuel, "
    "tower_frequency, "
    "is_closed, is_military, is_addon, rating, "
    "num_runway_hard, num_runway_soft, num_runway_water, num_helipad, "
    "longest_runway_length, longest_runway_heading, mag_var, "
    "lonx, laty, left_lonx, top_laty, right_lonx, bottom_laty ");

  static QString airwayQueryBase(
    "select airway_id, airway_name, airway_type, airway_fragment_no, sequence_no, from_waypoint_id, to_waypoint_id, "
    "minimum_altitude, from_lonx, from_laty, to_lonx, to_laty ");

  static QString waypointQueryBase(
    "select waypoint_id, ident, region, type, num_victor_airway, num_jet_airway, "
    "mag_var, lonx, laty ");

  static QString vorQueryBase(
    "select vor_id, ident, name, region, type, name, frequency, range, dme_only, dme_altitude, "
    "mag_var, altitude, lonx, laty ");
  static QString ndbQueryBase(
    "select ndb_id, ident, name, region, type, name, frequency, range, mag_var, altitude, lonx, laty ");

  static QString parkingQueryBase(
    "select parking_id, airport_id, type, name, number, radius, heading, has_jetway, lonx, laty ");

  deInitQueries();

  airportByIdQuery = new SqlQuery(db);
  airportByIdQuery->prepare(airportQueryBase + " from airport where airport_id = :id ");

  airportAdminByIdQuery = new SqlQuery(db);
  airportAdminByIdQuery->prepare("select city, state, country from airport where airport_id = :id ");

  airportByIdentQuery = new SqlQuery(db);
  airportByIdentQuery->prepare(airportQueryBase + " from airport where ident = :ident ");

  vorByIdentQuery = new SqlQuery(db);
  vorByIdentQuery->prepare(vorQueryBase + " from vor where " + whereIdentRegion);

  ndbByIdentQuery = new SqlQuery(db);
  ndbByIdentQuery->prepare(ndbQueryBase + " from ndb where " + whereIdentRegion);

  waypointByIdentQuery = new SqlQuery(db);
  waypointByIdentQuery->prepare(waypointQueryBase + " from waypoint where " + whereIdentRegion);

  vorByIdQuery = new SqlQuery(db);
  vorByIdQuery->prepare(vorQueryBase + " from vor where vor_id = :id");

  ndbByIdQuery = new SqlQuery(db);
  ndbByIdQuery->prepare(ndbQueryBase + " from ndb where ndb_id = :id");

  vorByWaypointIdQuery = new SqlQuery(db);
  vorByWaypointIdQuery->prepare(vorQueryBase +
                                " from vor where vor_id in "
                                "(select nav_id from waypoint w where w.waypoint_id = :id)");

  ndbByWaypointIdQuery = new SqlQuery(db);
  ndbByWaypointIdQuery->prepare(ndbQueryBase +
                                " from ndb where ndb_id in "
                                "(select nav_id from waypoint w where w.waypoint_id = :id)");

  waypointByIdQuery = new SqlQuery(db);
  waypointByIdQuery->prepare(waypointQueryBase + " from waypoint where waypoint_id = :id");

  airportByRectQuery = new SqlQuery(db);
  airportByRectQuery->prepare(
    airportQueryBase + " from airport where " + whereRect +
    " and longest_runway_length >= :minlength order by rating desc, longest_runway_length desc "
    + whereLimit);

  airportMediumByRectQuery = new SqlQuery(db);
  airportMediumByRectQuery->prepare(
    airportQueryBaseOverview + "from airport_medium where " + whereRect + " " + whereLimit);

  airportLargeByRectQuery = new SqlQuery(db);
  airportLargeByRectQuery->prepare(
    airportQueryBaseOverview + "from airport_large where " + whereRect + " " + whereLimit);

  runwayOverviewQuery = new SqlQuery(db);
  runwayOverviewQuery->prepare(
    "select length, heading, lonx, laty, primary_lonx, primary_laty, secondary_lonx, secondary_laty "
    "from runway where airport_id = :airportId and length > 4000 " + whereLimit);

  apronQuery = new SqlQuery(db);
  apronQuery->prepare(
    "select surface, is_draw_surface, vertices "
    "from apron where airport_id = :airportId");

  parkingQuery = new SqlQuery(db);
  parkingQuery->prepare(parkingQueryBase + " from parking where airport_id = :airportId");

  parkingTypeAndNumberQuery = new SqlQuery(db);
  parkingTypeAndNumberQuery->prepare(
    parkingQueryBase +
    " from parking where airport_id = :airportId and name like :name and number = :number order by radius desc");

  helipadQuery = new SqlQuery(db);
  helipadQuery->prepare(
    "select surface, type, length, width, heading, is_transparent, is_closed, lonx, laty "
    "from helipad where airport_id = :airportId");

  taxiparthQuery = new SqlQuery(db);
  taxiparthQuery->prepare(
    "select type, surface, width, name, is_draw_surface, start_type, end_type, "
    "start_lonx, start_laty, end_lonx, end_laty "
    "from taxi_path where airport_id = :airportId and type not in ('RUNWAY', 'VEHICLE')");

  runwaysQuery = new SqlQuery(db);
  runwaysQuery->prepare(
    "select length, heading, width, surface, lonx, laty, p.name as primary_name, s.name as secondary_name, "
    "edge_light, "
    "p.offset_threshold as primary_offset_threshold,  p.has_closed_markings as primary_closed_markings, "
    "s.offset_threshold as secondary_offset_threshold,  s.has_closed_markings as secondary_closed_markings,"
    "primary_lonx, primary_laty, secondary_lonx, secondary_laty "
    "from runway "
    "join runway_end p on primary_end_id = p.runway_end_id "
    "join runway_end s on secondary_end_id = s.runway_end_id "
    "where airport_id = :airportId");

  waypointsByRectQuery = new SqlQuery(db);
  waypointsByRectQuery->prepare(waypointQueryBase + " from waypoint where " + whereRect + " " + whereLimit);

  vorsByRectQuery = new SqlQuery(db);
  vorsByRectQuery->prepare(vorQueryBase + " from vor where " + whereRect + " " + whereLimit);

  ndbsByRectQuery = new SqlQuery(db);
  ndbsByRectQuery->prepare(ndbQueryBase + " from ndb where " + whereRect + " " + whereLimit);

  markersByRectQuery = new SqlQuery(db);
  markersByRectQuery->prepare(
    "select marker_id, type, heading, lonx, laty "
    "from marker "
    "where " + whereRect + " " + whereLimit);

  ilsByRectQuery = new SqlQuery(db);
  ilsByRectQuery->prepare(
    "select ils_id, ident, name, mag_var, loc_heading, gs_pitch, frequency, range, dme_range, loc_width, "
    "end1_lonx, end1_laty, end_mid_lonx, end_mid_laty, end2_lonx, end2_laty, altitude, lonx, laty "
    "from ils where " + whereRect + " " + whereLimit);

  airwayByRectQuery = new SqlQuery(db);
  airwayByRectQuery->prepare(
    airwayQueryBase + " from airway where " +
    "not (right_lonx < :leftx or left_lonx > :rightx or bottom_laty > :topy or top_laty < :bottomy) ");

  airwayByWaypointIdQuery = new SqlQuery(db);
  airwayByWaypointIdQuery->prepare(
    airwayQueryBase + " from airway where from_waypoint_id = :id or to_waypoint_id = :id");

  airwayByIdQuery = new SqlQuery(db);
  airwayByIdQuery->prepare(airwayQueryBase + " from airway where airway_id = :id");
}

void MapQuery::deInitQueries()
{
  delete airportByRectQuery;
  airportByRectQuery = nullptr;
  delete airportMediumByRectQuery;
  airportMediumByRectQuery = nullptr;
  delete airportLargeByRectQuery;
  airportLargeByRectQuery = nullptr;

  delete runwayOverviewQuery;
  runwayOverviewQuery = nullptr;
  delete apronQuery;
  apronQuery = nullptr;
  delete parkingQuery;
  parkingQuery = nullptr;
  delete parkingTypeAndNumberQuery;
  parkingTypeAndNumberQuery = nullptr;
  delete helipadQuery;
  helipadQuery = nullptr;
  delete taxiparthQuery;
  taxiparthQuery = nullptr;
  delete runwaysQuery;
  runwaysQuery = nullptr;

  delete waypointsByRectQuery;
  waypointsByRectQuery = nullptr;
  delete vorsByRectQuery;
  vorsByRectQuery = nullptr;
  delete ndbsByRectQuery;
  ndbsByRectQuery = nullptr;
  delete markersByRectQuery;
  markersByRectQuery = nullptr;
  delete ilsByRectQuery;
  ilsByRectQuery = nullptr;
  delete airwayByRectQuery;
  airwayByRectQuery = nullptr;

  delete airportByIdQuery;
  airportByIdQuery = nullptr;
  delete airportAdminByIdQuery;
  airportAdminByIdQuery = nullptr;

  delete airwayByWaypointIdQuery;
  airwayByWaypointIdQuery = nullptr;
  delete airwayByIdQuery;
  airwayByIdQuery = nullptr;

  delete airportByIdentQuery;
  airportByIdentQuery = nullptr;

  delete vorByIdentQuery;
  vorByIdentQuery = nullptr;
  delete ndbByIdentQuery;
  ndbByIdentQuery = nullptr;
  delete waypointByIdentQuery;
  waypointByIdentQuery = nullptr;

  delete vorByIdQuery;
  vorByIdQuery = nullptr;
  delete ndbByIdQuery;
  ndbByIdQuery = nullptr;

  delete vorByWaypointIdQuery;
  vorByWaypointIdQuery = nullptr;
  delete ndbByWaypointIdQuery;
  ndbByWaypointIdQuery = nullptr;

  delete waypointByIdQuery;
  waypointByIdQuery = nullptr;
}
