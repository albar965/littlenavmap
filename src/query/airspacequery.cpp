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

#include "query/airspacequery.h"

#include "common/constants.h"
#include "common/maptypesfactory.h"
#include "mapgui/maplayer.h"
#include "sql/sqlutil.h"
#include "fs/common/binarygeometry.h"
#include "common/maptools.h"
#include "settings/settings.h"
#include "db/databasemanager.h"

#include <QFileInfo>

using namespace Marble;
using namespace atools::sql;
using namespace atools::geo;

static double queryRectInflationFactor = 0.2;
static double queryRectInflationIncrement = 0.1;
int AirspaceQuery::queryMaxRows = 5000;

AirspaceQuery::AirspaceQuery(SqlDatabase *sqlDb, map::MapAirspaceSources src)
  : db(sqlDb), source(src)
{
  mapTypesFactory = new MapTypesFactory();
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  airspaceLineCache.setMaxCost(settings.getAndStoreValue(
                                 lnm::SETTINGS_MAPQUERY + "AirspaceLineCache", 10000).toInt());
  onlineCenterGeoCache.setMaxCost(settings.getAndStoreValue(
                                    lnm::SETTINGS_MAPQUERY + "OnlineCenterGeoCache", 10000).toInt());
  onlineCenterGeoFileCache.setMaxCost(settings.getAndStoreValue(
                                        lnm::SETTINGS_MAPQUERY + "OnlineCenterGeoFileCache", 10000).toInt());

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

bool AirspaceQuery::hasAirspaceById(int airspaceId)
{
  bool retval = false;
  airspaceByIdQuery->bindValue(":id", airspaceId);
  airspaceByIdQuery->exec();
  if(airspaceByIdQuery->next())
    retval = true;
  airspaceByIdQuery->finish();
  return retval;
}

SqlRecord AirspaceQuery::getOnlineAirspaceRecordById(int airspaceId)
{
  if(source == map::AIRSPACE_SRC_ONLINE)
  {
    SqlQuery query(db);
    query.prepare("select * from atc where atc_id = :id");
    query.bindValue(":id", airspaceId);
    query.exec();
    if(query.next())
      return query.record();
  }
  return SqlRecord();
}

void AirspaceQuery::getAirspaceById(map::MapAirspace& airspace, int airspaceId)
{
  airspaceByIdQuery->bindValue(":id", airspaceId);
  airspaceByIdQuery->exec();
  if(airspaceByIdQuery->next())
  {
    mapTypesFactory->fillAirspace(airspaceByIdQuery->record(), airspace, source);
  }
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
          query::bindRect(r, query);
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

            if(hasFirUir)
            {
              // Database has new FIR/UIR types - filter out the old deprecated centers
              QString name = query->valueStr("name");
              if(name.contains("(FIR)") || name.contains("(UIR)") || name.contains("(FIR/UIR)"))
                continue;
            }

            map::MapAirspace airspace;
            mapTypesFactory->fillAirspace(query->record(), airspace, source);
            airspaceCache.list.append(airspace);

            ids.insert(airspace.id);
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

const LineString *AirspaceQuery::getAirspaceGeometryByName(int airspaceId)
{
  if(airspaceLineCache.contains(airspaceId))
    return airspaceLineCache.object(airspaceId);
  else
  {
    LineString *lines = new LineString;

    airspaceLinesByIdQuery->bindValue(":id", airspaceId);
    airspaceLinesByIdQuery->exec();
    if(airspaceLinesByIdQuery->next())
    {
      atools::fs::common::BinaryGeometry geometry(airspaceLinesByIdQuery->value("geometry").toByteArray());
      geometry.swapGeometry(*lines);
    }
    airspaceLinesByIdQuery->finish();
    airspaceLineCache.insert(airspaceId, lines);

    return lines;
  }
}

LineString *AirspaceQuery::getAirspaceGeometryByFile(QString callsign)
{
  if(airspaceGeoByFileQuery != nullptr)
  {
    if(onlineCenterGeoFileCache.contains(callsign))
    {
      // Return nullptr if empty - empty objects in cache indicate object not present
      LineString *lineString = onlineCenterGeoFileCache.object(callsign);
      if(lineString != nullptr && !lineString->isEmpty())
        return lineString;
    }
    else
    {
      LineString *lineString = new LineString();
      callsign = callsign.trimmed().toUpper();

      // Do a pattern query and check for basename matches later
      airspaceGeoByFileQuery->bindValue(":filepath", "%" + callsign + "%");
      airspaceGeoByFileQuery->exec();

      while(airspaceGeoByFileQuery->next())
      {
        QFileInfo fi(airspaceGeoByFileQuery->valueStr("filepath").trimmed());
        QString basename = fi.baseName().toUpper().trimmed();

        // Check if the basename matches the callsign
        if(basename == callsign.toUpper())
        {
          atools::fs::common::BinaryGeometry geo(airspaceGeoByFileQuery->value("geometry").toByteArray());
          geo.swapGeometry(*lineString);
          break;
        }
      }
      airspaceGeoByFileQuery->finish();
      onlineCenterGeoFileCache.insert(callsign, lineString);
      if(!lineString->isEmpty())
        return lineString;
    }
  }
  return nullptr;
}

LineString *AirspaceQuery::getAirspaceGeometryByName(const QString& callsign, const QString& facilityType)
{
  Q_UNUSED(facilityType)

  if(airspaceGeoByNameQuery != nullptr)
  {
    if(onlineCenterGeoCache.contains(callsign))
    {
      // Return nullptr if empty - empty objects in cache indicate object not present
      LineString *lineString = onlineCenterGeoCache.object(callsign);
      if(lineString != nullptr && !lineString->isEmpty())
        return lineString;
    }
    else
    {
      LineString *lineString = new LineString();

      // Check if the airspace name matches the callsign
      airspaceGeoByNameQuery->bindValue(":name", callsign);
      airspaceGeoByNameQuery->bindValue(":type", "%"); // Not used yet
      airspaceGeoByNameQuery->exec();

      if(airspaceGeoByNameQuery->next())
      {
        atools::fs::common::BinaryGeometry geo(airspaceGeoByNameQuery->value("geometry").toByteArray());
        geo.swapGeometry(*lineString);
      }
      airspaceGeoByNameQuery->finish();
      onlineCenterGeoCache.insert(callsign, lineString);
      if(!lineString->isEmpty())
        return lineString;
    }
  }
  return nullptr;
}

SqlRecord AirspaceQuery::getAirspaceInfoRecordById(int airspaceId)
{
  SqlRecord retval;
  if(airspaceInfoQuery != nullptr)
  {
    airspaceInfoQuery->bindValue(":id", airspaceId);
    airspaceInfoQuery->exec();
    if(airspaceInfoQuery->next())
      retval = airspaceInfoQuery->record();
    airspaceInfoQuery->finish();
  }
  return retval;
}

void AirspaceQuery::updateAirspaceStatus()
{
  if(source & map::AIRSPACE_SRC_ONLINE)
    hasAirspaces = SqlUtil(db).hasTableAndRows("atc");
  else
    hasAirspaces = SqlUtil(db).hasTableAndRows("boundary");

  if(source & map::AIRSPACE_SRC_NAV && hasAirspaces)
  {
    // Check if the database contains the new FIR/UIR types which are preferred before FIR/UIR center types
    SqlQuery query("select count(1) from boundary where type in ('FIR', 'UIR')", db);
    query.exec();
    hasFirUir = query.next() && query.valueInt(0) > 0;
  }
  else
    hasFirUir = false;
}

void AirspaceQuery::initQueries()
{
  updateAirspaceStatus();

  QString airspaceQueryBase, table, id;
  if(source == map::AIRSPACE_SRC_ONLINE)
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

    SqlRecord rec = db->record("boundary");
    if(rec.contains("time_code"))
      airspaceQueryBase += ", time_code ";
    if(rec.contains("multiple_code"))
      airspaceQueryBase += ", multiple_code ";

    if(rec.contains("restrictive_type"))
      airspaceQueryBase += ", restrictive_type ";
    if(rec.contains("restrictive_designation"))
      airspaceQueryBase += ", restrictive_designation ";
  }

  deInitQueries();

  airspaceByIdQuery = new SqlQuery(db);
  airspaceByIdQuery->prepare("select " + airspaceQueryBase + " from " + table + " where " + id + " = :id");

  if(!(source & map::AIRSPACE_SRC_ONLINE))
  {
    airspaceInfoQuery = new SqlQuery(db);
    airspaceInfoQuery->prepare("select * from boundary "
                               "join bgl_file on boundary.file_id = bgl_file.bgl_file_id "
                               "join scenery_area on bgl_file.scenery_area_id = scenery_area.scenery_area_id "
                               "where boundary_id = :id");
  }

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

  // Queries for online center boundary matches
  if(!(source & map::AIRSPACE_SRC_ONLINE))
  {
    airspaceGeoByNameQuery = new SqlQuery(db);
    airspaceGeoByNameQuery->prepare("select geometry from " + table + " where name = :name and type like :type");

    airspaceGeoByFileQuery = new SqlQuery(db);
    airspaceGeoByFileQuery->prepare("select b.geometry, f.filepath from " + table +
                                    " b join bgl_file f on b.file_id = f.bgl_file_id where f.filepath like :filepath");
  }
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

  delete airspaceGeoByNameQuery;
  airspaceGeoByNameQuery = nullptr;

  delete airspaceGeoByFileQuery;
  airspaceGeoByFileQuery = nullptr;

  delete airspaceByIdQuery;
  airspaceByIdQuery = nullptr;

  delete airspaceInfoQuery;
  airspaceInfoQuery = nullptr;
}

void AirspaceQuery::clearCache()
{
  airspaceCache.clear();
  airspaceLineCache.clear();
  onlineCenterGeoCache.clear();
  onlineCenterGeoFileCache.clear();

  updateAirspaceStatus();
}
