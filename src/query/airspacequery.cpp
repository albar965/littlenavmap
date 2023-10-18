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

#include "query/airspacequery.h"

#include "atools.h"
#include "common/constants.h"
#include "common/maptypesfactory.h"
#include "fs/common/binarygeometry.h"
#include "mapgui/maplayer.h"
#include "settings/settings.h"
#include "sql/sqldatabase.h"
#include "sql/sqlutil.h"

#include <QFileInfo>
#include <QStringBuilder>

using namespace Marble;
using namespace atools::sql;
using namespace atools::geo;

static double queryRectInflationFactor = 0.2;
static double queryRectInflationIncrement = 0.1;
int AirspaceQuery::queryMaxRows = map::MAX_MAP_OBJECTS;

AirspaceQuery::AirspaceQuery(SqlDatabase *sqlDb, map::MapAirspaceSources src)
  : db(sqlDb), source(src)
{
  mapTypesFactory = new MapTypesFactory();
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  airspaceLineCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "AirspaceLineCache", 10000).toInt());
  onlineCenterGeoCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "OnlineCenterGeoCache", 10000).toInt());
  onlineCenterGeoFileCache.setMaxCost(settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "OnlineCenterGeoFileCache", 10000).toInt());

  queryRectInflationFactor = settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "QueryRectInflationFactor", 0.3).toDouble();
  queryRectInflationIncrement = settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "QueryRectInflationIncrement", 0.1).toDouble();
  queryMaxRows = settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "AirspaceQueryRowLimit", map::MAX_MAP_OBJECTS).toInt();
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
  if(!query::valid(Q_FUNC_INFO, airspaceByIdQuery))
    return false;

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
  if(!query::valid(Q_FUNC_INFO, airspaceByIdQuery))
    return;

  airspaceByIdQuery->bindValue(":id", airspaceId);
  airspaceByIdQuery->exec();
  if(airspaceByIdQuery->next())
    mapTypesFactory->fillAirspace(airspaceByIdQuery->record(), airspace, source);

  airspaceByIdQuery->finish();
}

const QList<map::MapAirspace> *AirspaceQuery::getAirspaces(const GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                                           const map::MapAirspaceFilter& filter, float flightPlanAltitude,
                                                           bool lazy, bool& overflow)
{
  airspaceCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                            [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
  {
    return curLayer->hasSameQueryParametersAirspace(newLayer);
  });

  if(filter != lastAirspaceFilter || atools::almostNotEqual(lastFlightplanAltitude, flightPlanAltitude))
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

      // Select query and assign altitude limits ======================================
      SqlQuery *query = nullptr;
      int minAlt = map::MapAirspaceFilter::MIN_AIRSPACE_ALT, maxAlt = map::MapAirspaceFilter::MAX_AIRSPACE_ALT;
      if(filter.flags.testFlag(map::AIRSPACE_ALTITUDE_ALL))
        // No altitude query =========
        query = airspaceByRectQuery;
      else if(filter.flags.testFlag(map::AIRSPACE_ALTITUDE_FLIGHTPLAN))
      {
        // One altitude query =========
        minAlt = maxAlt = atools::roundToInt(flightPlanAltitude);
        query = airspaceByRectAltQuery;
      }
      else if(filter.flags.testFlag(map::AIRSPACE_ALTITUDE_SET))
      {
        // Altitude range query =========
        minAlt = filter.minAltitudeFt;

        // Use unlimited for the maximum value if not equal
        if(filter.minAltitudeFt != filter.maxAltitudeFt && filter.maxAltitudeFt == map::MapAirspaceFilter::MAX_AIRSPACE_ALT)
          maxAlt = 100000;
        else
          maxAlt = filter.maxAltitudeFt;

        // Can use single alt query if values are equal
        query = maxAlt == minAlt ? airspaceByRectAltQuery : airspaceByRectAltRangeQuery;
      }

      if(query::valid(Q_FUNC_INFO, query))
      {
        QSet<int> ids;

        // Get the airspace objects without geometry
        for(const GeoDataLatLonBox& r : query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
        {
          for(const QString& typeStr : qAsConst(typeStrings))
          {
            query::bindRect(r, query);
            query->bindValue(":type", typeStr);

            // Bind altitude values ===========================
            if(query == airspaceByRectAltQuery)
              query->bindValue(":alt", minAlt);
            else if(query == airspaceByRectAltRangeQuery)
            {
              query->bindValue(":minalt", minAlt);
              query->bindValue(":maxalt", maxAlt);
            }

            // Run query ===========================================
            query->exec();
            while(query->next())
            {
              // Avoid double airspaces which can happen if they cross the date boundary
              if(ids.contains(query->valueInt("boundary_id")))
                continue;

              if(hasMultipleCode && filter.flags.testFlag(map::AIRSPACE_NO_MULTIPLE_Z) && query->valueStr("multiple_code") == "Z")
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
  }
  overflow = airspaceCache.validate(queryMaxRows);
  return &airspaceCache.list;
}

void AirspaceQuery::airspaceGeometry(LineString *lines, const QByteArray& bytes)
{
  atools::fs::common::BinaryGeometry geometry(bytes);
  geometry.swapGeometry(*lines);
}

const LineString *AirspaceQuery::getAirspaceGeometryById(int airspaceId)
{
  if(!query::valid(Q_FUNC_INFO, airspaceLinesByIdQuery))
    return nullptr;

  if(airspaceLineCache.contains(airspaceId))
    return airspaceLineCache.object(airspaceId);
  else
  {
    LineString *linestring = new LineString;

    airspaceLinesByIdQuery->bindValue(":id", airspaceId);
    airspaceLinesByIdQuery->exec();
    if(airspaceLinesByIdQuery->next())
      airspaceGeometry(linestring, airspaceLinesByIdQuery->value("geometry").toByteArray());
    airspaceLinesByIdQuery->finish();
    airspaceLineCache.insert(airspaceId, linestring);

    return linestring;
  }
}

const LineString *AirspaceQuery::getAirspaceGeometryByFile(QString callsign)
{
  if(airspaceGeoByFileQuery != nullptr)
  {
    if(onlineCenterGeoFileCache.contains(callsign))
    {
      // Return nullptr if empty - empty objects in cache indicate object not present
      const LineString *lineString = onlineCenterGeoFileCache.object(callsign);
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
          airspaceGeometry(lineString, airspaceGeoByFileQuery->value("geometry").toByteArray());
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

const LineString *AirspaceQuery::getAirspaceGeometryByName(QString callsign, const QString& facilityType)
{
  callsign.replace('-', '_').replace("__", "_");

  const LineString *geometry = airspaceGeometryByNameInternal(callsign, facilityType);
  if(geometry == nullptr)
  {
    QStringList callsignSplit = callsign.split('_');

    // Work on middle section
    if(callsignSplit.size() > 2)
    {
      if(callsign.section('_', 1, 1).size() > 1)
      {
        // Shorten middle initial: "EDGG_DKB_CTR" to "EDGG_D_CTR"
        callsignSplit[1] = callsignSplit[1].mid(1);
        geometry = airspaceGeometryByNameInternal(callsignSplit.join('_'), facilityType);
      }

      if(geometry == nullptr)
      {
        // Remove middle initial "EDGG_D_CTR" to "EDGG_CTR"
        callsignSplit.removeAt(1);
        geometry = airspaceGeometryByNameInternal(callsignSplit.join('_'), facilityType);
      }
    }
  }
  return geometry;
}

const LineString *AirspaceQuery::airspaceGeometryByNameInternal(const QString& callsign, const QString& facilityType)
{
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << callsign << facilityType;
#endif

  if(airspaceGeoByNameQuery != nullptr)
  {
    if(onlineCenterGeoCache.contains(callsign))
    {
      // Return nullptr if empty - empty objects in cache indicate object not present
      const LineString *lineString = onlineCenterGeoCache.object(callsign);
      if(lineString != nullptr && !lineString->isEmpty())
        return lineString;
    }
    else
    {
      LineString *lineString = new LineString();

      // Check if the airspace name matches the callsign
      airspaceGeoByNameQuery->bindValue(":name", callsign);
      airspaceGeoByNameQuery->bindValue(":type", facilityType.isEmpty() ? "%" : facilityType);
      airspaceGeoByNameQuery->exec();

      if(airspaceGeoByNameQuery->next())
        airspaceGeometry(lineString, airspaceGeoByNameQuery->value("geometry").toByteArray());

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

    hasMultipleCode = rec.contains("multiple_code");
    if(hasMultipleCode)
      airspaceQueryBase += ", multiple_code ";

    if(rec.contains("restrictive_type"))
      airspaceQueryBase += ", restrictive_type ";
    if(rec.contains("restrictive_designation"))
      airspaceQueryBase += ", restrictive_designation ";
  }

  deInitQueries();

  airspaceByIdQuery = new SqlQuery(db);
  airspaceByIdQuery->prepare("select " % airspaceQueryBase % " from " % table % " where " % id % " = :id");

  if(!(source & map::AIRSPACE_SRC_ONLINE))
  {
    airspaceInfoQuery = new SqlQuery(db);
    airspaceInfoQuery->prepare("select * from boundary "
                               "join bgl_file on boundary.file_id = bgl_file.bgl_file_id "
                               "join scenery_area on bgl_file.scenery_area_id = scenery_area.scenery_area_id "
                               "where boundary_id = :id");
  }

  // Get all that are crossing the anti meridian too and filter them out from the query result
  QString airspaceRect = " (not (max_lonx < :leftx or min_lonx > :rightx or "
                         "min_laty > :topy or max_laty < :bottomy) or max_lonx < min_lonx) ";

  airspaceByRectQuery = new SqlQuery(db);
  airspaceByRectQuery->prepare("select " % airspaceQueryBase % "from " % table % " where " % airspaceRect % " and type like :type");

  airspaceByRectAltRangeQuery = new SqlQuery(db);
  airspaceByRectAltRangeQuery->prepare("select " % airspaceQueryBase % " from " % table %
                                       " where " % airspaceRect % " and type like :type and " %
                                       "((:minalt <= max_altitude and :maxalt >= min_altitude) or "
                                       "(max_altitude = 0 and :maxalt >= min_altitude))");

  airspaceByRectAltQuery = new SqlQuery(db);
  airspaceByRectAltQuery->prepare("select " % airspaceQueryBase % " from " % table %
                                  " where " % airspaceRect % " and type like :type and " %
                                  "((:alt <= max_altitude and :alt >= min_altitude) or "
                                  "(max_altitude = 0 and :alt >= min_altitude))");

  airspaceLinesByIdQuery = new SqlQuery(db);
  airspaceLinesByIdQuery->prepare("select geometry from " % table % " where " % id % " = :id");

  // Queries for online center boundary matches
  if(!(source & map::AIRSPACE_SRC_ONLINE))
  {
    airspaceGeoByNameQuery = new SqlQuery(db);
    airspaceGeoByNameQuery->prepare("select geometry from " % table % " where name like :name and type like :type");

    airspaceGeoByFileQuery = new SqlQuery(db);
    airspaceGeoByFileQuery->prepare("select b.geometry, f.filepath from " % table %
                                    " b join bgl_file f on b.file_id = f.bgl_file_id where f.filepath like :filepath");
  }
}

void AirspaceQuery::deInitQueries()
{
  clearCache();

  delete airspaceByRectQuery;
  airspaceByRectQuery = nullptr;

  delete airspaceByRectAltRangeQuery;
  airspaceByRectAltRangeQuery = nullptr;

  delete airspaceByRectAltQuery;
  airspaceByRectAltQuery = nullptr;

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
