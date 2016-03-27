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
#include "coordinateconverter.h"
#include "maplayer.h"
#include "common/maptools.h"

#include <algorithm>
#include <functional>
#include <QSqlRecord>
#include <marble/GeoDataLatLonBox.h>

using namespace Marble;
using namespace atools::sql;
using namespace atools::geo;

const double MapQuery::RECT_INFLATION_FACTOR = 0.3;
const double MapQuery::RECT_INFLATION_ADD = 0.1;
const int QUERY_ROW_LIMIT = 5000;

MapQuery::MapQuery(atools::sql::SqlDatabase *sqlDb)
  : db(sqlDb)
{

}

MapQuery::~MapQuery()
{
  deInitQueries();
}

void MapQuery::getAirportById(maptypes::MapAirport& airport, int airportId)
{
  airportByIdQuery->bindValue(":id", airportId);
  airportByIdQuery->exec();
  while(airportByIdQuery->next())
    fillMapAirport(airportByIdQuery, airport, true);
}

void MapQuery::getAirwaysForWaypoint(int waypointId, QList<maptypes::MapAirway>& airways)
{
  airwayByWaypointIdQuery->bindValue(":id", waypointId);
  airwayByWaypointIdQuery->exec();
  while(airwayByWaypointIdQuery->next())
  {
    maptypes::MapAirway airway;
    fillAirway(airwayByWaypointIdQuery, airway);
    airways.append(airway);
  }
}

void MapQuery::getMapObject(maptypes::MapSearchResult& result, maptypes::MapObjectTypes type,
                            const QString& ident, const QString& region)
{
  if(type == maptypes::AIRPORT)
  {
    airportByIdentQuery->bindValue(":ident", ident);
    airportByIdentQuery->exec();
    while(airportByIdentQuery->next())
    {
      maptypes::MapAirport *ap = new maptypes::MapAirport;
      fillMapAirport(airportByIdentQuery, *ap, true);
      result.airports.append(ap);
      result.needsDelete = true;
    }
  }
  else if(type == maptypes::VOR)
  {
    vorByIdentQuery->bindValue(":ident", ident);
    vorByIdentQuery->bindValue(":region", region.isEmpty() ? "%" : region);
    vorByIdentQuery->exec();
    while(vorByIdentQuery->next())
    {
      maptypes::MapVor *vor = new maptypes::MapVor;
      fillMapVor(vorByIdentQuery, *vor);
      result.vors.append(vor);
      result.needsDelete = true;
    }
  }
  else if(type == maptypes::NDB)
  {
    ndbByIdentQuery->bindValue(":ident", ident);
    ndbByIdentQuery->bindValue(":region", region.isEmpty() ? "%" : region);
    ndbByIdentQuery->exec();
    while(ndbByIdentQuery->next())
    {
      maptypes::MapNdb *ndb = new maptypes::MapNdb;
      fillMapNdb(ndbByIdentQuery, *ndb);
      result.ndbs.append(ndb);
      result.needsDelete = true;
    }
  }
  else if(type == maptypes::WAYPOINT)
  {
    waypointByIdentQuery->bindValue(":ident", ident);
    waypointByIdentQuery->bindValue(":region", region.isEmpty() ? "%" : region);
    waypointByIdentQuery->exec();
    while(waypointByIdentQuery->next())
    {
      maptypes::MapWaypoint *wp = new maptypes::MapWaypoint;
      fillMapWaypoint(waypointByIdentQuery, *wp);
      result.waypoints.append(wp);
      result.needsDelete = true;
    }
  }
}

void MapQuery::getNearestObjects(const CoordinateConverter& conv, const MapLayer *mapLayer,
                                 maptypes::MapObjectTypes types,
                                 int xs, int ys, int screenDistance,
                                 maptypes::MapSearchResult& result)
{
  using maptools::insertSortedByDistance;
  using maptools::insertSortedByTowerDistance;
  using namespace maptypes;

  if(mapLayer == nullptr)
    return;

  int x, y;
  if(mapLayer->isAirport() && types.testFlag(maptypes::AIRPORT))
    for(int i = airportCache.list.size() - 1; i >= 0; i--)
    {
    const MapAirport& airport = airportCache.list.at(i);

    if(airport.isVisible(types))
    {
      if(conv.wToS(airport.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.airports, &result.airportIds, xs, ys, &airport);

      if(conv.wToS(airport.towerCoords, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByTowerDistance(conv, result.towers, xs, ys, &airport);
    }
    }

  if(mapLayer->isVor() && types.testFlag(maptypes::VOR))
    for(int i = vorCache.list.size() - 1; i >= 0; i--)
    {
      const MapVor& vor = vorCache.list.at(i);
      if(conv.wToS(vor.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.vors, &result.vorIds, xs, ys, &vor);
    }

  if(mapLayer->isNdb() && types.testFlag(maptypes::NDB))
    for(int i = ndbCache.list.size() - 1; i >= 0; i--)
    {
      const MapNdb& ndb = ndbCache.list.at(i);
      if(conv.wToS(ndb.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.ndbs, &result.ndbIds, xs, ys, &ndb);
    }

  if(mapLayer->isWaypoint() && types.testFlag(maptypes::WAYPOINT))
    for(int i = waypointCache.list.size() - 1; i >= 0; i--)
    {
      const MapWaypoint& wp = waypointCache.list.at(i);
      if(conv.wToS(wp.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.waypoints, &result.waypointIds, xs, ys, &wp);
    }

  if(mapLayer->isMarker() && types.testFlag(maptypes::MARKER))
    for(int i = markerCache.list.size() - 1; i >= 0; i--)
    {
      const MapMarker& wp = markerCache.list.at(i);
      if(conv.wToS(wp.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.markers, nullptr, xs, ys, &wp);
    }

  if(mapLayer->isIls() && types.testFlag(maptypes::ILS))
    for(int i = ilsCache.list.size() - 1; i >= 0; i--)
    {
      const MapIls& wp = ilsCache.list.at(i);
      if(conv.wToS(wp.position, x, y))
        if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
          insertSortedByDistance(conv, result.ils, nullptr, xs, ys, &wp);
    }

  if(mapLayer->isAirportDiagram())
  {
    for(int id : parkingCache.keys())
    {
      QList<MapParking> *parkings = parkingCache.object(id);
      for(const MapParking& p : *parkings)
        if(conv.wToS(p.position, x, y))
          if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
            insertSortedByDistance(conv, result.parkings, nullptr, xs, ys, &p);
    }

    for(int id : helipadCache.keys())
    {
      QList<MapHelipad> *helipads = helipadCache.object(id);
      for(const MapHelipad& p : *helipads)
        if(conv.wToS(p.position, x, y))
          if((atools::geo::manhattanDistance(x, y, xs, ys)) < screenDistance)
            insertSortedByDistance(conv, result.helipads, nullptr, xs, ys, &p);
    }
  }
}

const QList<maptypes::MapAirport> *MapQuery::getAirports(const Marble::GeoDataLatLonBox& rect,
                                                         const MapLayer *mapLayer, bool lazy)
{
  if(mapLayer == nullptr)
    return nullptr;

  if(airportCache.handleCache(rect, mapLayer, lazy))
    qDebug() << "MapQuery airports cache miss";

  switch(mapLayer->getDataSource())
  {
    case layer::ALL:
      airportByRectQuery->bindValue(":minlength", mapLayer->getMinRunwayLength());
      return fetchAirports(rect, airportByRectQuery, lazy, true);

    case layer::MEDIUM:
      return fetchAirports(rect, airportMediumByRectQuery, lazy, false);

    case layer::LARGE:
      return fetchAirports(rect, airportLargeByRectQuery, lazy, false);

  }
  return nullptr;
}

const QList<maptypes::MapWaypoint> *MapQuery::getWaypoints(const GeoDataLatLonBox& rect,
                                                           const MapLayer *mapLayer, bool lazy)
{
  if(mapLayer == nullptr)
    return nullptr;

  if(waypointCache.handleCache(rect, mapLayer, lazy))
    qDebug() << "MapQuery waypoints cache miss";

  if(waypointCache.list.isEmpty() && !lazy)
    for(const GeoDataLatLonBox& r : splitAtAntiMeridian(rect))
    {
      bindCoordinatePointInRect(r, waypointsByRectQuery);
      waypointsByRectQuery->exec();
      while(waypointsByRectQuery->next())
      {
        maptypes::MapWaypoint wp;
        fillMapWaypoint(waypointsByRectQuery, wp);
        waypointCache.list.append(wp);
      }
    }
  return &waypointCache.list;
}

const QList<maptypes::MapVor> *MapQuery::getVors(const GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                                 bool lazy)
{
  if(mapLayer == nullptr)
    return nullptr;

  if(vorCache.handleCache(rect, mapLayer, lazy))
    qDebug() << "MapQuery vor cache miss";

  if(vorCache.list.isEmpty() && !lazy)
    for(const GeoDataLatLonBox& r : splitAtAntiMeridian(rect))
    {
      bindCoordinatePointInRect(r, vorsByRectQuery);
      vorsByRectQuery->exec();
      while(vorsByRectQuery->next())
      {
        maptypes::MapVor vor;
        fillMapVor(vorsByRectQuery, vor);
        vorCache.list.append(vor);
      }
    }
  return &vorCache.list;
}

const QList<maptypes::MapNdb> *MapQuery::getNdbs(const GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                                 bool lazy)
{
  if(mapLayer == nullptr)
    return nullptr;

  if(ndbCache.handleCache(rect, mapLayer, lazy))
    qDebug() << "MapQuery ndb cache miss";

  if(ndbCache.list.isEmpty() && !lazy)
    for(const GeoDataLatLonBox& r : splitAtAntiMeridian(rect))
    {
      bindCoordinatePointInRect(r, ndbsByRectQuery);
      ndbsByRectQuery->exec();
      while(ndbsByRectQuery->next())
      {
        maptypes::MapNdb ndb;
        fillMapNdb(ndbsByRectQuery, ndb);
        ndbCache.list.append(ndb);
      }
    }
  return &ndbCache.list;
}

const QList<maptypes::MapMarker> *MapQuery::getMarkers(const GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                                       bool lazy)
{
  if(mapLayer == nullptr)
    return nullptr;

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
        marker.id = markersByRectQuery->value("marker_id").toInt();
        marker.type = markersByRectQuery->value("type").toString();
        marker.heading = static_cast<int>(std::roundf(markersByRectQuery->value("heading").toFloat()));
        marker.position = Pos(markersByRectQuery->value("lonx").toFloat(), markersByRectQuery->value(
                                "laty").toFloat());
        markerCache.list.append(marker);
      }
    }
  return &markerCache.list;
}

const QList<maptypes::MapIls> *MapQuery::getIls(const GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                                bool lazy)
{
  if(mapLayer == nullptr)
    return nullptr;

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

        ils.id = ilsByRectQuery->value("ils_id").toInt();
        ils.ident = ilsByRectQuery->value("ident").toString();
        ils.name = ilsByRectQuery->value("name").toString();
        ils.heading = ilsByRectQuery->value("loc_heading").toFloat();
        ils.width = ilsByRectQuery->value("loc_width").toFloat();
        ils.magvar = ilsByRectQuery->value("mag_var").toFloat();
        ils.slope = ilsByRectQuery->value("gs_pitch").toFloat();

        ils.frequency = ilsByRectQuery->value("frequency").toInt();
        ils.range = ilsByRectQuery->value("range").toInt();
        ils.dme = ilsByRectQuery->value("dme_range").toInt() > 0;

        ils.position = Pos(ilsByRectQuery->value("lonx").toFloat(), ilsByRectQuery->value("laty").toFloat());
        ils.pos1 = Pos(ilsByRectQuery->value("end1_lonx").toFloat(), ilsByRectQuery->value(
                         "end1_laty").toFloat());
        ils.pos2 = Pos(ilsByRectQuery->value("end2_lonx").toFloat(), ilsByRectQuery->value(
                         "end2_laty").toFloat());
        ils.posmid =
          Pos(ilsByRectQuery->value("end_mid_lonx").toFloat(), ilsByRectQuery->value("end_mid_laty").toFloat());

        ils.bounding = Rect(ils.position);
        ils.bounding.extend(ils.pos1);
        ils.bounding.extend(ils.pos2);

        ilsCache.list.append(ils);
      }
    }
  return &ilsCache.list;
}

const QList<maptypes::MapAirway> *MapQuery::getAirways(const GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                                       bool lazy)
{
  if(mapLayer == nullptr)
    return nullptr;

  if(airwayCache.handleCache(rect, mapLayer, lazy))
    qDebug() << "MapQuery airway cache miss";

  if(airwayCache.list.isEmpty() && !lazy)
    for(const GeoDataLatLonBox& r : splitAtAntiMeridian(rect))
    {
      bindCoordinatePointInRect(r, airwayByRectQuery);
      airwayByRectQuery->exec();
      while(airwayByRectQuery->next())
      {
        maptypes::MapAirway airway;
        fillAirway(airwayByRectQuery, airway);
        airwayCache.list.append(airway);
      }
    }
  return &airwayCache.list;
}

const QList<maptypes::MapAirport> *MapQuery::fetchAirports(const Marble::GeoDataLatLonBox& rect,
                                                           atools::sql::SqlQuery *query, bool lazy,
                                                           bool complete)
{
  if(airportCache.list.isEmpty() && !lazy)
    for(const GeoDataLatLonBox& r : splitAtAntiMeridian(rect))
    {
      bindCoordinatePointInRect(r, query);
      query->exec();
      while(query->next())
      {
        maptypes::MapAirport ap;
        fillMapAirport(query, ap, complete);
        airportCache.list.append(ap);
      }
    }
  return &airportCache.list;
}

void MapQuery::fillMapAirport(const atools::sql::SqlQuery *query, maptypes::MapAirport& ap, bool complete)
{
  QSqlRecord rec = query->record();

  ap.id = query->value("airport_id").toInt();
  ap.ident = query->value("ident").toString();
  ap.name = query->value("name").toString();
  ap.longestRunwayLength = query->value("longest_runway_length").toInt();
  ap.longestRunwayHeading = static_cast<int>(std::roundf(query->value("longest_runway_heading").toFloat()));

  if(rec.contains("has_tower_object"))
    ap.towerCoords = Pos(query->value("tower_lonx").toFloat(), query->value("tower_laty").toFloat());

  if(rec.contains("tower_frequency"))
    ap.towerFrequency = query->value("tower_frequency").toInt();
  if(rec.contains("atis_frequency"))
    ap.atisFrequency = query->value("atis_frequency").toInt();
  if(rec.contains("awos_frequency"))
    ap.awosFrequency = query->value("awos_frequency").toInt();
  if(rec.contains("asos_frequency"))
    ap.asosFrequency = query->value("asos_frequency").toInt();
  if(rec.contains("unicom_frequency"))
    ap.unicomFrequency = query->value("unicom_frequency").toInt();

  if(rec.contains("altitude"))
    ap.altitude = static_cast<int>(std::roundf(query->value("altitude").toFloat()));

  ap.flags = getFlags(query);

  if(complete)
    ap.flags |= maptypes::AP_COMPLETE;

  ap.magvar = query->value("mag_var").toFloat();
  ap.position = Pos(query->value("lonx").toFloat(), query->value("laty").toFloat());
  ap.bounding = Rect(query->value("left_lonx").toFloat(), query->value("top_laty").toFloat(),
                     query->value("right_lonx").toFloat(), query->value("bottom_laty").toFloat());

  ap.valid = true;
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

const QList<maptypes::MapParking> *MapQuery::getParking(int airportId)
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

      QString type = parkingQuery->value("type").toString();
      if(type != "VEHICLES")
      {
        p.type = type;
        p.name = parkingQuery->value("name").toString();

        p.position = Pos(parkingQuery->value("lonx").toFloat(), parkingQuery->value("laty").toFloat());
        p.jetway = parkingQuery->value("has_jetway").toInt() > 0;
        p.number = parkingQuery->value("number").toInt();

        p.heading = static_cast<int>(std::roundf(parkingQuery->value("heading").toFloat()));
        p.radius = static_cast<int>(std::roundf(parkingQuery->value("radius").toFloat()));

        ps->append(p);
      }
    }
    parkingCache.insert(airportId, ps);
    return ps;
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

Pos MapQuery::getNavTypePos(int navSearchId)
{
  SqlQuery query(db);
  query.prepare("select lonx, laty from nav_search where nav_search_id = :id");
  query.bindValue(":id", navSearchId);
  query.exec();
  if(query.next())
    return atools::geo::Pos(query.value("lonx").toFloat(), query.value("laty").toFloat());

  return atools::geo::Pos();
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
      if(type != "RUNWAY" && type != "VEHICLE")
      {
        tp.start = Pos(taxiparthQuery->value("start_lonx").toFloat(), taxiparthQuery->value(
                         "start_laty").toFloat()),
        tp.end = Pos(taxiparthQuery->value("end_lonx").toFloat(), taxiparthQuery->value("end_laty").toFloat()),
        tp.surface = taxiparthQuery->value("surface").toString();
        tp.name = taxiparthQuery->value("name").toString();
        tp.width = taxiparthQuery->value("width").toInt();

        tps->append(tp);
      }
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

maptypes::MapAirportFlags MapQuery::flag(const atools::sql::SqlQuery *query, const QString& field,
                                         maptypes::MapAirportFlags flag)
{
  if(!query->record().contains(field) || query->isNull(field))
    return maptypes::AP_NONE;
  else
    return query->value(field).toInt() > 0 ? flag : maptypes::AP_NONE;
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

maptypes::MapAirportFlags MapQuery::getFlags(const atools::sql::SqlQuery *query)
{
  using namespace maptypes;

  MapAirportFlags flags = 0;
  flags |= flag(query, "num_helipad", AP_HELIPAD);
  flags |= flag(query, "rating", AP_SCENERY);
  flags |= flag(query, "has_avgas", AP_AVGAS);
  flags |= flag(query, "has_jetfuel", AP_JETFUEL);
  flags |= flag(query, "tower_frequency", AP_TOWER);
  flags |= flag(query, "is_closed", AP_CLOSED);
  flags |= flag(query, "is_military", AP_MIL);
  flags |= flag(query, "is_addon", AP_ADDON);
  flags |= flag(query, "num_approach", AP_APPR);
  flags |= flag(query, "num_runway_hard", AP_HARD);
  flags |= flag(query, "num_runway_soft", AP_SOFT);
  flags |= flag(query, "num_runway_water", AP_WATER);
  flags |= flag(query, "num_runway_light", AP_LIGHT);
  flags |= flag(query, "num_runway_end_ils", AP_ILS);

  return flags;
}

void MapQuery::initQueries()
{
  static QString whereRect("lonx between :leftx and :rightx and laty between :bottomy and :topy");
  static QString whereIdentRegion("ident = :ident and region like :region");
  static QString whereLimit("limit " + QString::number(QUERY_ROW_LIMIT));

  static QString airportQueryBase(
    "select airport_id, ident, name, rating, "
    "has_avgas, has_jetfuel, has_tower_object, "
    "tower_frequency, atis_frequency, awos_frequency, asos_frequency, unicom_frequency, "
    "is_closed, is_military, is_addon,"
    "num_approach, num_runway_hard, num_runway_soft, num_runway_water, "
    "num_runway_light, num_runway_end_ils, num_helipad, "
    "longest_runway_length, longest_runway_heading, mag_var, "
    "tower_lonx, tower_laty, altitude, lonx, laty, left_lonx, top_laty, right_lonx, bottom_laty "
    "from airport ");

  QString airwayQueryBase(
    "select route_id, route_name, route_type, route_fragment_no, sequence_no, from_waypoint_id, to_waypoint_id, "
    "minimum_altitude, from_lonx, from_laty, to_lonx, to_laty "
    "from route ");

  static QString waypointQueryBase("select waypoint_id, ident, region, type, mag_var, lonx, laty "
                                   "from waypoint");

  static QString vorQueryBase(
    "select vor_id, ident, name, region, type, name, frequency, range, dme_only, dme_altitude, "
    "mag_var, lonx, laty "
    "from vor ");
  static QString ndbQueryBase(
    "select ndb_id, ident, name, region, type, name, frequency, range, mag_var, lonx, laty "
    "from ndb ");

  deInitQueries();

  airportByIdQuery = new SqlQuery(db);
  airportByIdQuery->prepare(airportQueryBase + " where airport_id = :id ");

  airportByIdentQuery = new SqlQuery(db);
  airportByIdentQuery->prepare(airportQueryBase + " where ident = :ident ");

  vorByIdentQuery = new SqlQuery(db);
  vorByIdentQuery->prepare(vorQueryBase + " where " + whereIdentRegion);

  ndbByIdentQuery = new SqlQuery(db);
  ndbByIdentQuery->prepare(ndbQueryBase + " where " + whereIdentRegion);

  waypointByIdentQuery = new SqlQuery(db);
  waypointByIdentQuery->prepare(waypointQueryBase + " where " + whereIdentRegion);

  airportByRectQuery = new SqlQuery(db);
  airportByRectQuery->prepare(
    airportQueryBase + " where " + whereRect +
    " and longest_runway_length >= :minlength order by rating asc, longest_runway_length "
    + whereLimit);

  airportMediumByRectQuery = new SqlQuery(db);
  airportMediumByRectQuery->prepare(
    "select airport_id, ident, name, rating, "
    "has_avgas, has_jetfuel, "
    "tower_frequency, "
    "is_closed, is_military, is_addon,"
    "num_runway_hard, num_runway_soft, num_runway_water, num_helipad, "
    "longest_runway_length, longest_runway_heading, mag_var, "
    "lonx, laty, left_lonx, top_laty, right_lonx, bottom_laty "
    "from airport_medium where " + whereRect + " order by longest_runway_length" + "  " + whereLimit);

  airportLargeByRectQuery = new SqlQuery(db);
  airportLargeByRectQuery->prepare(
    "select airport_id, ident, name, rating, "
    "has_avgas, has_jetfuel, "
    "tower_frequency, "
    "is_closed, is_military, is_addon,"
    "num_runway_hard, num_runway_soft, num_runway_water, "
    "longest_runway_length, longest_runway_heading, mag_var, "
    "lonx, laty, left_lonx, top_laty, right_lonx, bottom_laty "
    "from airport_large where " + whereRect + " " + whereLimit);

  runwayOverviewQuery = new SqlQuery(db);
  runwayOverviewQuery->prepare(
    "select length, heading, lonx, laty, primary_lonx, primary_laty, secondary_lonx, secondary_laty "
    "from runway where airport_id = :airportId and length > 4000 " + whereLimit);

  apronQuery = new SqlQuery(db);
  apronQuery->prepare(
    "select surface, is_draw_surface, vertices "
    "from apron where airport_id = :airportId");

  parkingQuery = new SqlQuery(db);
  parkingQuery->prepare(
    "select type, name, number, radius, heading, has_jetway, lonx, laty "
    "from parking where airport_id = :airportId");

  helipadQuery = new SqlQuery(db);
  helipadQuery->prepare(
    "select surface, type, length, width, heading, is_closed, lonx, laty "
    "from helipad where airport_id = :airportId");

  taxiparthQuery = new SqlQuery(db);
  taxiparthQuery->prepare(
    "select type, surface, width, name, is_draw_surface, start_type, end_type, "
    "start_lonx, start_laty, end_lonx, end_laty "
    "from taxi_path where airport_id = :airportId");

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
  waypointsByRectQuery->prepare(waypointQueryBase + " where " + whereRect + " " + whereLimit);

  vorsByRectQuery = new SqlQuery(db);
  vorsByRectQuery->prepare(vorQueryBase + " where " + whereRect + " " + whereLimit);

  ndbsByRectQuery = new SqlQuery(db);
  ndbsByRectQuery->prepare(ndbQueryBase + " where " + whereRect + " " + whereLimit);

  markersByRectQuery = new SqlQuery(db);
  markersByRectQuery->prepare(
    "select marker_id, type, heading, lonx, laty "
    "from marker "
    "where " + whereRect + " " + whereLimit);

  ilsByRectQuery = new SqlQuery(db);
  ilsByRectQuery->prepare(
    "select ils_id, ident, name, mag_var, loc_heading, gs_pitch, frequency, range, dme_range, loc_width, "
    "end1_lonx, end1_laty, end_mid_lonx, end_mid_laty, end2_lonx, end2_laty, lonx, laty "
    "from ils where " + whereRect + " " + whereLimit);

  airwayByRectQuery = new SqlQuery(db);
  airwayByRectQuery->prepare(
    airwayQueryBase + " where " +
    "not (right_lonx < :leftx or left_lonx > :rightx or bottom_laty > :topy or top_laty < :bottomy) ");

  airwayByWaypointIdQuery = new SqlQuery(db);
  airwayByWaypointIdQuery->prepare(airwayQueryBase + " where from_waypoint_id = :id or to_waypoint_id = :id");
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
  delete airwayByWaypointIdQuery;
  airwayByWaypointIdQuery = nullptr;

  delete airportByIdentQuery;
  airportByIdentQuery = nullptr;
  delete vorByIdentQuery;
  vorByIdentQuery = nullptr;
  delete ndbByIdentQuery;
  ndbByIdentQuery = nullptr;
  delete waypointByIdentQuery;
  waypointByIdentQuery = nullptr;
}

void MapQuery::fillMapVor(const atools::sql::SqlQuery *query, maptypes::MapVor& vor)
{
  vor.id = query->value("vor_id").toInt();
  vor.ident = query->value("ident").toString();
  vor.region = query->value("region").toString();
  vor.name = query->value("name").toString();
  vor.type = query->value("type").toString();
  vor.frequency = query->value("frequency").toInt();
  vor.range = query->value("range").toInt();
  vor.dmeOnly = query->value("dme_only").toInt() > 0;
  vor.hasDme = !query->value("dme_altitude").isNull();
  vor.magvar = query->value("mag_var").toFloat();
  vor.position = Pos(query->value("lonx").toFloat(), query->value("laty").toFloat());
}

void MapQuery::fillMapNdb(const atools::sql::SqlQuery *query, maptypes::MapNdb& ndb)
{
  ndb.id = query->value("ndb_id").toInt();
  ndb.ident = query->value("ident").toString();
  ndb.region = query->value("region").toString();
  ndb.name = query->value("name").toString();
  ndb.type = query->value("type").toString();
  ndb.frequency = query->value("frequency").toInt();
  ndb.range = query->value("range").toInt();
  ndb.magvar = query->value("mag_var").toFloat();
  ndb.position = Pos(query->value("lonx").toFloat(), query->value("laty").toFloat());
}

void MapQuery::fillMapWaypoint(const atools::sql::SqlQuery *query, maptypes::MapWaypoint& wp)
{
  wp.id = query->value("waypoint_id").toInt();
  wp.ident = query->value("ident").toString();
  wp.region = query->value("region").toString();
  wp.type = query->value("type").toString();
  wp.magvar = query->value("mag_var").toFloat();
  wp.position = Pos(query->value("lonx").toFloat(), query->value("laty").toFloat());
}

void MapQuery::fillAirway(const atools::sql::SqlQuery *query, maptypes::MapAirway& airway)
{
  airway.id = query->value("route_id").toInt();
  airway.type = query->value("route_type").toString();
  airway.name = query->value("route_name").toString();
  airway.minalt = query->value("minimum_altitude").toInt();
  airway.fragment = query->value("route_fragment_no").toInt();
  airway.sequence = query->value("sequence_no").toInt();
  airway.fromWpId = query->value("from_waypoint_id").toInt();
  airway.toWpId = query->value("to_waypoint_id").toInt();
  airway.from = Pos(query->value("from_lonx").toFloat(),
                    query->value("from_laty").toFloat());
  airway.to = Pos(query->value("to_lonx").toFloat(),
                  query->value("to_laty").toFloat());
  airway.bounding = Rect(airway.from);
  airway.bounding.extend(airway.to);
}
