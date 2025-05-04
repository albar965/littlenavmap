/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#include "query/airwayquery.h"

#include "common/constants.h"
#include "common/mapresult.h" // Needed for delete operator
#include "common/maptypesfactory.h"
#include "mapgui/maplayer.h"
#include "settings/settings.h"
#include "sql/sqldatabase.h"
#include "sql/sqlutil.h"

#include <QStringBuilder>

using namespace Marble;
using namespace atools::sql;
using namespace atools::geo;

static double queryRectInflationFactor = 0.2;
static double queryRectInflationIncrement = 0.1;
int AirwayQuery::queryMaxRowsAirways = map::MAX_MAP_OBJECTS;

AirwayQuery::AirwayQuery(SqlDatabase *sqlDbNav, bool trackParam)
  : dbNav(sqlDbNav), track(trackParam)
{
  mapTypesFactory = new MapTypesFactory();
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  queryRectInflationFactor = settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "QueryRectInflationFactor", 0.3).toDouble();
  queryRectInflationIncrement = settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "QueryRectInflationIncrement", 0.1).toDouble();
  queryMaxRowsAirways = settings.getAndStoreValue(lnm::SETTINGS_MAPQUERY % "AirwayQueryRowLimitAw", map::MAX_MAP_OBJECTS).toInt();
}

AirwayQuery::~AirwayQuery()
{
  deInitQueries();
  delete mapTypesFactory;
}

void AirwayQuery::getAirwaysForWaypoint(QList<map::MapAirway>& airways, int waypointId)
{
  if(!query::valid(Q_FUNC_INFO, airwayByWaypointIdQuery))
    return;

  airwayByWaypointIdQuery->bindValue(":id", waypointId);
  airwayByWaypointIdQuery->exec();
  while(airwayByWaypointIdQuery->next())
  {
    map::MapAirway airway;
    mapTypesFactory->fillAirwayOrTrack(airwayByWaypointIdQuery->record(), airway, track);
    airways.append(airway);
  }
}

void AirwayQuery::getAirwaysForWaypoints(QList<map::MapAirway>& airways, int waypointId1, int waypointId2,
                                         const QString& airwayName)
{
  QList<map::MapAirway> temp;
  getAirwaysForWaypoint(temp, waypointId1);

  for(const map::MapAirway& a : qAsConst(airways))
  {
    if((airwayName.isEmpty() || a.name == airwayName) &&
       (waypointId2 == a.fromWaypointId || waypointId2 == a.toWaypointId))
      airways.append(a);
  }
}

void AirwayQuery::getWaypointsForAirway(QList<map::MapWaypoint>& waypoints, const QString& airwayName,
                                        const QString& waypointIdent)
{
  if(!query::valid(Q_FUNC_INFO, airwayWaypointByIdentQuery))
    return;

  airwayWaypointByIdentQuery->bindValue(":waypoint", waypointIdent.isEmpty() ? "%" : waypointIdent);
  airwayWaypointByIdentQuery->bindValue(":airway", airwayName.isEmpty() ? "%" : airwayName);
  airwayWaypointByIdentQuery->exec();
  while(airwayWaypointByIdentQuery->next())
  {
    map::MapWaypoint waypoint;
    mapTypesFactory->fillWaypoint(airwayWaypointByIdentQuery->record(), waypoint, track);
    waypoints.append(waypoint);
  }
}

void AirwayQuery::getWaypointListForAirwayName(QList<map::MapAirwayWaypoint>& waypoints, const QString& airwayName,
                                               int airwayFragment)
{
  if(!query::valid(Q_FUNC_INFO, airwayWaypointsQuery))
    return;

  airwayWaypointsQuery->bindValue(":name", airwayName);
  airwayWaypointsQuery->exec();

  // Collect records first
  QVector<SqlRecord> records;
  while(airwayWaypointsQuery->next())
    records.append(airwayWaypointsQuery->record());

  QString fragmentCol = track ? "track_fragment_no" : "airway_fragment_no";

  for(int i = 0; i < records.size(); i++)
  {
    const SqlRecord& rec = records.at(i);

    int fragment = rec.valueInt(fragmentCol, 0);

    if(airwayFragment != -1 && airwayFragment != fragment)
      continue;

    // Check if the next fragment is different
    int nextFragment = i < records.size() - 1 ? records.at(i + 1).valueInt(fragmentCol, 0) : -1;

    map::MapAirwayWaypoint aw;
    aw.airwayFragmentId = fragment;
    aw.seqNum = rec.valueInt("sequence_no");
    aw.airwayId = rec.valueInt(airwayIdCol);

    // Add from waypoint
    aw.waypoint = waypointById(rec.valueInt("from_waypoint_id"));
    waypoints.append(aw);

    if(i == records.size() - 1 || fragment != nextFragment)
    {
      // Add to waypoint if this is the last one or if the fragment is about to change
      aw.waypoint = waypointById(rec.valueInt("to_waypoint_id"));
      waypoints.append(aw);
    }
  }
}

map::MapWaypoint AirwayQuery::waypointById(int id)
{
  map::MapWaypoint wp;
  if(!query::valid(Q_FUNC_INFO, waypointByIdQuery))
    return wp;

  waypointByIdQuery->bindValue(":id", id);
  waypointByIdQuery->exec();
  if(waypointByIdQuery->next())
    mapTypesFactory->fillWaypoint(waypointByIdQuery->record(), wp, track);
  waypointByIdQuery->finish();
  return wp;
}

void AirwayQuery::getAirwayFull(QList<map::MapAirway>& airways, atools::geo::Rect& bounding, const QString& airwayName,
                                int fragment)
{
  if(!query::valid(Q_FUNC_INFO, airwayFullQuery))
    return;

  airwayFullQuery->bindValue(":name", airwayName);
  airwayFullQuery->bindValue(":fragment", fragment);
  airwayFullQuery->exec();
  while(airwayFullQuery->next())
  {
    map::MapAirway airway;
    mapTypesFactory->fillAirwayOrTrack(airwayFullQuery->record(), airway, track);
    bounding.extend(airway.bounding);
    airways.append(airway);
  }
}

map::MapAirway AirwayQuery::getAirwayById(int airwayId)
{
  map::MapAirway airway;
  getAirwayById(airway, airwayId);
  return airway;
}

void AirwayQuery::getAirwayById(map::MapAirway& airway, int airwayId)
{
  if(!query::valid(Q_FUNC_INFO, airwayByIdQuery))
    return;

  airwayByIdQuery->bindValue(":id", airwayId);
  airwayByIdQuery->exec();
  if(airwayByIdQuery->next())
    mapTypesFactory->fillAirwayOrTrack(airwayByIdQuery->record(), airway, track);
  airwayByIdQuery->finish();

}

void AirwayQuery::getAirwaysByName(QList<map::MapAirway>& airways, const QString& name)
{
  if(!query::valid(Q_FUNC_INFO, airwayByNameQuery))
    return;

  airwayByNameQuery->bindValue(":name", name);
  airwayByNameQuery->exec();
  while(airwayByNameQuery->next())
  {
    map::MapAirway airway;
    mapTypesFactory->fillAirwayOrTrack(airwayByNameQuery->record(), airway, track);
    airways.append(airway);
  }
}

void AirwayQuery::getAirwaysByNameAndWaypoint(QList<map::MapAirway>& airways, const QString& airwayName,
                                              const QString& waypoint1, const QString& waypoint2)
{
  if(airwayName.isEmpty() || waypoint1.isEmpty())
    return;

  if(!query::valid(Q_FUNC_INFO, airwayByNameAndWaypointQuery))
    return;

  QList<map::MapAirway> *airwaysObj = airwayByNameCache.object({airwayName, waypoint1, waypoint2});
  if(airwaysObj != nullptr)
    // Copy airways from cache
    airways = *airwaysObj;
  else
  {
    airwaysObj = new QList<map::MapAirway>(airways);

    // Query from database
    airwayByNameAndWaypointQuery->bindValue(":airway", airwayName.isEmpty() ? "%" : airwayName);
    airwayByNameAndWaypointQuery->bindValue(":ident1", waypoint1);
    airwayByNameAndWaypointQuery->bindValue(":ident2", waypoint2.isEmpty() ? "%" : waypoint2);
    airwayByNameAndWaypointQuery->exec();
    while(airwayByNameAndWaypointQuery->next())
    {
      map::MapAirway airway;
      mapTypesFactory->fillAirwayOrTrack(airwayByNameAndWaypointQuery->record(), airway, track);
      airwaysObj->append(airway);
    }

    // Add to the cache
    airwayByNameCache.insert({airwayName, waypoint1, waypoint2}, airwaysObj);

    // Return copy
    airways = *airwaysObj;
  }
}

const QList<map::MapAirway> *AirwayQuery::getAirways(const GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy)
{
  if(!query::valid(Q_FUNC_INFO, airwayByRectQuery))
    return nullptr;

  airwayCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                          [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
  {
    return curLayer->hasSameQueryParametersAirwayTrack(newLayer);
  });

  if(airwayCache.list.isEmpty() && !lazy)
  {
    QSet<int> ids;
    for(const GeoDataLatLonBox& box : query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      query::bindRect(box, airwayByRectQuery);
      airwayByRectQuery->exec();
      while(airwayByRectQuery->next())
      {
        if(ids.contains(airwayByRectQuery->valueInt(airwayIdCol)))
          continue;

        map::MapAirway airway;
        mapTypesFactory->fillAirwayOrTrack(airwayByRectQuery->record(), airway, track);
        airwayCache.list.append(airway);
        ids.insert(airway.id);
      }
    }
  }
  airwayCache.validate(queryMaxRowsAirways);
  return &airwayCache.list;
}

void AirwayQuery::initQueries()
{
  airwayTable = track ? "track" : "airway";
  airwayIdCol = track ? "track_id" : "airway_id";
  airwayNameCol = track ? "track_name" : "airway_name";
  waypointIdCol = track ? "trackpoint_id" : "waypoint_id";
  waypointTable = track ? "trackpoint" : "waypoint";
  prefix = track ? "track_" : "airway_";

  // Columns for airway query ==================================
  QString airwayQueryBase = airwayIdCol % ", " % airwayNameCol;
  if(track)
    airwayQueryBase += ", track_type, track_fragment_no, sequence_no, airway_id, from_waypoint_id, to_waypoint_id, "
                       "airway_direction, airway_minimum_altitude, airway_maximum_altitude, "
                       "altitude_levels_east, altitude_levels_west, "
                       "from_lonx, from_laty, to_lonx, to_laty ";
  else
    airwayQueryBase += ", airway_type, airway_fragment_no, sequence_no, from_waypoint_id, to_waypoint_id, "
                       "direction, minimum_altitude, maximum_altitude, from_lonx, from_laty, to_lonx, to_laty ";

  if(atools::sql::SqlUtil(dbNav).hasTableAndColumn(airwayTable, "route_type"))
    airwayQueryBase.append(", route_type");

  // Columns for waypoint query ==================================
  QString waypointQueryBase = waypointIdCol % ", ident, region, type, num_victor_airway, num_jet_airway, mag_var, lonx, laty ";
  if(atools::sql::SqlUtil(dbNav).hasTableAndColumn(waypointTable, "name"))
    waypointQueryBase += ", name";

  if(atools::sql::SqlUtil(dbNav).hasTableAndColumn(waypointTable, "arinc_type"))
    waypointQueryBase += ", arinc_type";

  deInitQueries();

  waypointByIdQuery = new SqlQuery(dbNav);
  waypointByIdQuery->prepare("select " % waypointQueryBase % " from " % waypointTable %
                             " where " % waypointIdCol % " = :id");

  // Get all that are crossing the anti meridian too and filter them out from the query result
  airwayByRectQuery = new SqlQuery(dbNav);
  airwayByRectQuery->prepare(
    "select " % airwayQueryBase % ", right_lonx, left_lonx, bottom_laty, top_laty from " % airwayTable % " where " %
    "not (right_lonx < :leftx or left_lonx > :rightx or bottom_laty > :topy or top_laty < :bottomy) "
    "or right_lonx < left_lonx");

  airwayByWaypointIdQuery = new SqlQuery(dbNav);
  airwayByWaypointIdQuery->prepare(
    "select " % airwayQueryBase % " from " % airwayTable % " where from_waypoint_id = :id or to_waypoint_id = :id");

  airwayByNameAndWaypointQuery = new SqlQuery(dbNav);
  airwayByNameAndWaypointQuery->prepare(
    "select " % airwayQueryBase %
    " from " % airwayTable % " a join " % waypointTable % " wf on a.from_waypoint_id = wf." % waypointIdCol %
    " join " % waypointTable % " wt on a.to_waypoint_id = wt." % waypointIdCol %
    " where a." % airwayNameCol % " like :airway and ((wf.ident = :ident1 and wt.ident like :ident2) or "
                                  " (wt.ident = :ident1 and wf.ident like :ident2))");

  airwayByIdQuery = new SqlQuery(dbNav);
  airwayByIdQuery->prepare("select " % airwayQueryBase % " from " % airwayTable %
                           " where " % airwayIdCol % " = :id");

  airwayWaypointByIdentQuery = new SqlQuery(dbNav);
  airwayWaypointByIdentQuery->prepare("select " % waypointQueryBase %
                                      " from " % waypointTable %
                                      " w join " % airwayTable %
                                      " a on w." % waypointIdCol %
                                      " = a.from_waypoint_id where w.ident = :waypoint and a." %
                                      airwayNameCol % " = :airway union select " % waypointQueryBase %
                                      " from " % waypointTable %
                                      " w join " % airwayTable %
                                      " a on w." % waypointIdCol %
                                      " = a.to_waypoint_id where w.ident = :waypoint and a." %
                                      airwayNameCol % " = :airway");

  airwayByNameQuery = new SqlQuery(dbNav);
  airwayByNameQuery->prepare("select " % airwayQueryBase % " from " % airwayTable %
                             " where " % airwayNameCol % " = :name");

  airwayWaypointsQuery = new SqlQuery(dbNav);
  airwayWaypointsQuery->prepare("select " % airwayQueryBase %
                                " from " % airwayTable %
                                " where " % airwayNameCol %
                                " = :name order by " % prefix % "fragment_no, sequence_no");

  airwayFullQuery = new SqlQuery(dbNav);
  airwayFullQuery->prepare("select " % airwayQueryBase %
                           " from " % airwayTable %
                           " where " % prefix % "fragment_no = :fragment and " % airwayNameCol % " = :name");
}

void AirwayQuery::deInitQueries()
{
  clearCache();

  delete airwayByRectQuery;
  airwayByRectQuery = nullptr;

  delete airwayByWaypointIdQuery;
  airwayByWaypointIdQuery = nullptr;
  delete airwayByNameAndWaypointQuery;
  airwayByNameAndWaypointQuery = nullptr;
  delete airwayByIdQuery;
  airwayByIdQuery = nullptr;

  delete airwayWaypointByIdentQuery;
  airwayWaypointByIdentQuery = nullptr;

  delete waypointByIdQuery;
  waypointByIdQuery = nullptr;

  delete airwayByNameQuery;
  airwayByNameQuery = nullptr;

  delete airwayWaypointsQuery;
  airwayWaypointsQuery = nullptr;

  delete airwayFullQuery;
  airwayFullQuery = nullptr;
}

void AirwayQuery::clearCache()
{
  airwayCache.clear();
  airwayByNameCache.clear();
  nearestNavaidCache.clear();
}
