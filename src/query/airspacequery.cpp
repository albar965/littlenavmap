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

#include "query/airspacequery.h"

#include "common/constants.h"
#include "common/maptypesfactory.h"
#include "common/maptools.h"
#include "fs/common/binarygeometry.h"
#include "sql/sqlquery.h"
#include "query/airportquery.h"
#include "navapp.h"
#include "common/maptools.h"
#include "settings/settings.h"
#include "fs/common/xpgeometry.h"
#include "db/databasemanager.h"

#include <QDataStream>
#include <QRegularExpression>

using namespace Marble;
using namespace atools::sql;
using namespace atools::geo;

static double queryRectInflationFactor = 0.2;
static double queryRectInflationIncrement = 0.1;
int AirspaceQuery::queryMaxRows = 5000;

AirspaceQuery::AirspaceQuery(QObject *parent, SqlDatabase *sqlDb, bool onlineSchema)
  : QObject(parent), db(sqlDb), online(onlineSchema)
{
  mapTypesFactory = new MapTypesFactory();
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  airspaceLineCache.setMaxCost(settings.getAndStoreValue(
                                 lnm::SETTINGS_MAPQUERY + "AirspaceLineCache", 10000).toInt());

  queryRectInflationFactor = settings.getAndStoreValue(
    lnm::SETTINGS_MAPQUERY + "QueryRectInflationFactor", 0.3).toDouble();
  queryRectInflationIncrement = settings.getAndStoreValue(
    lnm::SETTINGS_MAPQUERY + "QueryRectInflationIncrement", 0.1).toDouble();
  queryMaxRows = settings.getAndStoreValue(
    lnm::SETTINGS_MAPQUERY + "QueryRowLimit", 5000).toInt();
}

AirspaceQuery::~AirspaceQuery()
{
  deInitQueries();
  delete mapTypesFactory;
}

map::MapAirspace AirspaceQuery::getAirspaceById(int airspaceId)
{
  map::MapAirspace airspace;
  getAirspaceById(airspace, airspaceId);
  return airspace;
}

SqlRecord AirspaceQuery::getAirspaceRecordById(int airspaceId)
{
  SqlQuery query(db);
  query.prepare("select * from atc where atc_id = :id");
  query.bindValue(":id", airspaceId);
  query.exec();
  if(query.next())
    return query.record();
  else
    return SqlRecord();
}

void AirspaceQuery::getAirspaceById(map::MapAirspace& airspace, int airspaceId)
{
  airspaceByIdQuery->bindValue(":id", airspaceId);
  airspaceByIdQuery->exec();
  if(airspaceByIdQuery->next())
    mapTypesFactory->fillAirspace(airspaceByIdQuery->record(), airspace, online);
  airspaceByIdQuery->finish();
}

const QList<map::MapAirspace> *AirspaceQuery::getAirspaces(const GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                                           map::MapAirspaceFilter filter, float flightPlanAltitude,
                                                           bool lazy)
{
  airspaceCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                            [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
  {
    return curLayer->hasSameQueryParametersAirspace(newLayer);
  });

  if(filter.types != lastAirspaceFilter.types || filter.flags != lastAirspaceFilter.flags ||
     atools::almostNotEqual(lastFlightplanAltitude, flightPlanAltitude))
  {
    // Need a few more parameters to clear the cache which is different to other map features
    airspaceCache.list.clear();
    lastAirspaceFilter = filter;
    lastFlightplanAltitude = flightPlanAltitude;
  }

  if(airspaceCache.list.isEmpty() && !lazy)
  {
    QStringList typeStrings;

    if(filter.types != map::AIRSPACE_NONE)
    {
      // Build a list of query strings based on the bitfield
      if(filter.types == map::AIRSPACE_ALL)
        typeStrings.append("%");
      else
      {
        for(int i = 0; i <= map::MAP_AIRSPACE_TYPE_BITS; i++)
        {
          map::MapAirspaceTypes t(1 << i);
          if(filter.types & t)
            typeStrings.append(map::airspaceTypeToDatabase(t));
        }
      }

      SqlQuery *query = nullptr;
      int alt;
      if(filter.flags & map::AIRSPACE_AT_FLIGHTPLAN)
      {
        query = airspaceByRectAtAltQuery;
        alt = atools::roundToInt(flightPlanAltitude);
      }
      else if(filter.flags & map::AIRSPACE_BELOW_10000)
      {
        query = airspaceByRectBelowAltQuery;
        alt = 10000;
      }
      else if(filter.flags & map::AIRSPACE_BELOW_18000)
      {
        query = airspaceByRectBelowAltQuery;
        alt = 18000;
      }
      else if(filter.flags & map::AIRSPACE_ABOVE_10000)
      {
        query = airspaceByRectAboveAltQuery;
        alt = 10000;
      }
      else if(filter.flags & map::AIRSPACE_ABOVE_18000)
      {
        query = airspaceByRectAboveAltQuery;
        alt = 18000;
      }
      else
      {
        query = airspaceByRectQuery;
        alt = 0;
      }

      QSet<int> ids;

      // qDebug() << rect.toString(GeoDataCoordinates::Degree);

      // Get the airspace objects without geometry
      for(const GeoDataLatLonBox& r :
          query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
      {
        // qDebug() << r.toString(GeoDataCoordinates::Degree);

        for(const QString& typeStr : typeStrings)
        {
          query::bindCoordinatePointInRect(r, query);
          query->bindValue(":type", typeStr);

          if(alt > 0)
            query->bindValue(":alt", alt);

          // qDebug() << "==================== query" << endl << query->getFullQueryString();

          query->exec();
          while(query->next())
          {
            // Avoid double airspaces which can happen if they cross the date boundary
            if(ids.contains(query->valueInt("boundary_id")))
              continue;

            // qreal north, qreal south, qreal east, qreal west
            if(rect.intersects(GeoDataLatLonBox(query->valueFloat("max_laty"), query->valueFloat("min_laty"),
                                                query->valueFloat("max_lonx"), query->valueFloat("min_lonx"),
                                                GeoDataCoordinates::GeoDataCoordinates::Degree)))
            {
              map::MapAirspace airspace;
              mapTypesFactory->fillAirspace(query->record(), airspace, online);
              airspaceCache.list.append(airspace);

              ids.insert(airspace.id);
            }
          }
        }
      }

      // Sort by importance
      std::sort(airspaceCache.list.begin(), airspaceCache.list.end(),
                [](const map::MapAirspace& airspace1, const map::MapAirspace& airspace2) -> bool
      {
        return map::airspaceDrawingOrder(airspace1.type) < map::airspaceDrawingOrder(airspace2.type);
      });
    }
  }
  airspaceCache.validate(queryMaxRows);
  return &airspaceCache.list;
}

const LineString *AirspaceQuery::getAirspaceGeometry(int boundaryId)
{
  if(airspaceLineCache.contains(boundaryId))
    return airspaceLineCache.object(boundaryId);
  else
  {
    LineString *lines = new LineString;

    airspaceLinesByIdQuery->bindValue(":id", boundaryId);
    airspaceLinesByIdQuery->exec();
    if(airspaceLinesByIdQuery->next())
    {
      atools::fs::common::BinaryGeometry geometry(airspaceLinesByIdQuery->value("geometry").toByteArray());
      geometry.swapGeometry(*lines);

      // qDebug() << *lines;
    }

    airspaceLineCache.insert(boundaryId, lines);

    return lines;
  }
}

void AirspaceQuery::initQueries()
{
  QString airspaceQueryBase, table, id;
  if(online)
  {
    // Use modified result rows from online atc table
    table = "atc";
    id = "atc_id";
    airspaceQueryBase =
      "atc_id as boundary_id, type, com_type, callsign, name, frequency as com_frequency, "
      "server, facility_type, visual_range, atis, atis_time, max_lonx, max_laty, min_lonx, min_laty, "
      "9999999 as max_altitude, "
      "max_lonx, max_laty, 0 as min_altitude, min_lonx, min_laty ";
  }
  else
  {
    // Usual offline boundaries (Navigraph, etc.)
    table = "boundary";
    id = "boundary_id";
    airspaceQueryBase =
      "boundary_id, type, name, com_type, com_frequency, com_name, "
      "min_altitude_type, max_altitude_type, max_altitude, max_lonx, max_laty, min_altitude, min_lonx, min_laty ";
  }

  deInitQueries();

  airspaceByIdQuery = new SqlQuery(db);
  airspaceByIdQuery->prepare("select " + airspaceQueryBase + " from " + table + " where " + id + " = :id");

  // Get all that are crossing the anti meridian too and filter them out from the query result
  QString airspaceRect =
    " (not (max_lonx < :leftx or min_lonx > :rightx or "
    "min_laty > :topy or max_laty < :bottomy) or max_lonx < min_lonx) and ";

  airspaceByRectQuery = new SqlQuery(db);
  airspaceByRectQuery->prepare(
    "select " + airspaceQueryBase + "from " + table +
    " where " + airspaceRect + " type like :type");

  airspaceByRectBelowAltQuery = new SqlQuery(db);
  airspaceByRectBelowAltQuery->prepare(
    "select " + airspaceQueryBase + "from " + table +
    " where " + airspaceRect + " type like :type and min_altitude < :alt");

  airspaceByRectAboveAltQuery = new SqlQuery(db);
  airspaceByRectAboveAltQuery->prepare(
    "select " + airspaceQueryBase + "from " + table +
    " where " + airspaceRect + " type like :type and max_altitude > :alt");

  airspaceByRectAtAltQuery = new SqlQuery(db);
  airspaceByRectAtAltQuery->prepare(
    "select " + airspaceQueryBase + "from " + table +
    " where "
    "not (max_lonx < :leftx or min_lonx > :rightx or "
    "min_laty > :topy or max_laty < :bottomy) and "
    "type like :type and "
    ":alt between min_altitude and max_altitude");

  airspaceLinesByIdQuery = new SqlQuery(db);
  airspaceLinesByIdQuery->prepare("select geometry from " + table + " where " + id + " = :id");

}

void AirspaceQuery::deInitQueries()
{
  clearCache();

  delete airspaceByRectQuery;
  airspaceByRectQuery = nullptr;
  delete airspaceByRectBelowAltQuery;
  airspaceByRectBelowAltQuery = nullptr;
  delete airspaceByRectAboveAltQuery;
  airspaceByRectAboveAltQuery = nullptr;
  delete airspaceByRectAtAltQuery;
  airspaceByRectAtAltQuery = nullptr;

  delete airspaceLinesByIdQuery;
  airspaceLinesByIdQuery = nullptr;
  delete airspaceByIdQuery;
  airspaceByIdQuery = nullptr;

}

void AirspaceQuery::clearCache()
{
  airspaceCache.clear();
  airspaceLineCache.clear();
}
