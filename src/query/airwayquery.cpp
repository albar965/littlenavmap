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

#include "query/airwayquery.h"

#include "common/constants.h"
#include "common/maptypesfactory.h"
#include "common/maptools.h"
#include "common/proctypes.h"
#include "query/airportquery.h"
#include "settings/settings.h"
#include "sql/sqldatabase.h"

using namespace Marble;
using namespace atools::sql;
using namespace atools::geo;

static double queryRectInflationFactor = 0.2;
static double queryRectInflationIncrement = 0.1;
int AirwayQuery::queryMaxRows = 5000;

AirwayQuery::AirwayQuery(SqlDatabase *sqlDbNav)
  : dbNav(sqlDbNav)
{
  mapTypesFactory = new MapTypesFactory();
  atools::settings::Settings& settings = atools::settings::Settings::instance();

  queryRectInflationFactor = settings.getAndStoreValue(
    lnm::SETTINGS_MAPQUERY + "QueryRectInflationFactor", 0.3).toDouble();
  queryRectInflationIncrement = settings.getAndStoreValue(
    lnm::SETTINGS_MAPQUERY + "QueryRectInflationIncrement", 0.1).toDouble();
  queryMaxRows = settings.getAndStoreValue(
    lnm::SETTINGS_MAPQUERY + "QueryRowLimit", 5000).toInt();
}

AirwayQuery::~AirwayQuery()
{
  deInitQueries();
  delete mapTypesFactory;
}

void AirwayQuery::getAirwaysForWaypoint(QList<map::MapAirway>& airways, int waypointId)
{
  airwayByWaypointIdQuery->bindValue(":id", waypointId);
  airwayByWaypointIdQuery->exec();
  while(airwayByWaypointIdQuery->next())
  {
    map::MapAirway airway;
    mapTypesFactory->fillAirway(airwayByWaypointIdQuery->record(), airway);
    airways.append(airway);
  }
}

void AirwayQuery::getWaypointsForAirway(QList<map::MapWaypoint>& waypoints, const QString& airwayName,
                                        const QString& waypointIdent)
{
  airwayWaypointByIdentQuery->bindValue(":waypoint", waypointIdent.isEmpty() ? "%" : waypointIdent);
  airwayWaypointByIdentQuery->bindValue(":airway", airwayName.isEmpty() ? "%" : airwayName);
  airwayWaypointByIdentQuery->exec();
  while(airwayWaypointByIdentQuery->next())
  {
    map::MapWaypoint waypoint;
    mapTypesFactory->fillWaypoint(airwayWaypointByIdentQuery->record(), waypoint);
    waypoints.append(waypoint);
  }
}

void AirwayQuery::getWaypointListForAirwayName(QList<map::MapAirwayWaypoint>& waypoints, const QString& airwayName)
{
  airwayWaypointsQuery->bindValue(":name", airwayName);
  airwayWaypointsQuery->exec();

  // Collect records first
  SqlRecordVector records;
  while(airwayWaypointsQuery->next())
    records.append(airwayWaypointsQuery->record());

  for(int i = 0; i < records.size(); i++)
  {
    const SqlRecord& rec = records.at(i);

    int fragment = rec.valueInt("airway_fragment_no");
    // Check if the next fragment is different
    int nextFragment = i < records.size() - 1 ?
                       records.at(i + 1).valueInt("airway_fragment_no") : -1;

    map::MapAirwayWaypoint aw;
    aw.airwayFragmentId = fragment;
    aw.seqNum = rec.valueInt("sequence_no");
    aw.airwayId = rec.valueInt("airway_id");

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
  waypointByIdQuery->bindValue(":id", id);
  waypointByIdQuery->exec();
  if(waypointByIdQuery->next())
    mapTypesFactory->fillWaypoint(waypointByIdQuery->record(), wp);
  waypointByIdQuery->finish();
  return wp;
}

void AirwayQuery::getAirwayFull(QList<map::MapAirway>& airways, atools::geo::Rect& bounding, const QString& airwayName,
                                int fragment)
{
  airwayFullQuery->bindValue(":name", airwayName);
  airwayFullQuery->bindValue(":fragment", fragment);
  airwayFullQuery->exec();
  while(airwayFullQuery->next())
  {
    map::MapAirway airway;
    mapTypesFactory->fillAirway(airwayFullQuery->record(), airway);
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
  airwayByIdQuery->bindValue(":id", airwayId);
  airwayByIdQuery->exec();
  if(airwayByIdQuery->next())
    mapTypesFactory->fillAirway(airwayByIdQuery->record(), airway);
  airwayByIdQuery->finish();

}

void AirwayQuery::getAirwaysByName(QList<map::MapAirway>& airways, const QString& name)
{
  airwayByNameQuery->bindValue(":name", name);
  airwayByNameQuery->exec();
  while(airwayByNameQuery->next())
  {
    map::MapAirway airway;
    mapTypesFactory->fillAirway(airwayByNameQuery->record(), airway);
    airways.append(airway);
  }
}

void AirwayQuery::getAirwayByNameAndWaypoint(map::MapAirway& airway, const QString& airwayName,
                                             const QString& waypoint1,
                                             const QString& waypoint2)
{
  if(airwayName.isEmpty() || waypoint1.isEmpty() || waypoint2.isEmpty())
    return;

  airwayByNameAndWaypointQuery->bindValue(":airway", airwayName);
  airwayByNameAndWaypointQuery->bindValue(":ident1", waypoint1);
  airwayByNameAndWaypointQuery->bindValue(":ident2", waypoint2);
  airwayByNameAndWaypointQuery->exec();
  if(airwayByNameAndWaypointQuery->next())
    mapTypesFactory->fillAirway(airwayByNameAndWaypointQuery->record(), airway);
  airwayByNameAndWaypointQuery->finish();
}

const QList<map::MapAirway> *AirwayQuery::getAirways(const GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy)
{
  airwayCache.updateCache(rect, mapLayer, queryRectInflationFactor, queryRectInflationIncrement, lazy,
                          [](const MapLayer *curLayer, const MapLayer *newLayer) -> bool
  {
    return curLayer->hasSameQueryParametersAirway(newLayer);
  });

  if(airwayCache.list.isEmpty() && !lazy)
  {
    QSet<int> ids;
    for(const GeoDataLatLonBox& r :
        query::splitAtAntiMeridian(rect, queryRectInflationFactor, queryRectInflationIncrement))
    {
      query::bindRect(r, airwayByRectQuery);
      airwayByRectQuery->exec();
      while(airwayByRectQuery->next())
      {
        if(ids.contains(airwayByRectQuery->valueInt("airway_id")))
          continue;

        map::MapAirway airway;
        mapTypesFactory->fillAirway(airwayByRectQuery->record(), airway);
        airwayCache.list.append(airway);
        ids.insert(airway.id);
      }
    }
  }
  airwayCache.validate(queryMaxRows);
  return &airwayCache.list;
}

void AirwayQuery::initQueries()
{
  QString airwayQueryBase(
    "airway_id, airway_name, airway_type, airway_fragment_no, sequence_no, from_waypoint_id, to_waypoint_id, "
    "direction, minimum_altitude, maximum_altitude, from_lonx, from_laty, to_lonx, to_laty ");

  SqlRecord aprec = dbNav->record("airway");
  if(aprec.contains("route_type"))
    airwayQueryBase.append(", route_type");

  static const QString waypointQueryBase(
    "waypoint_id, ident, region, type, num_victor_airway, num_jet_airway, "
    "mag_var, lonx, laty ");

  deInitQueries();

  waypointByIdQuery = new SqlQuery(dbNav);
  waypointByIdQuery->prepare("select " + waypointQueryBase + " from waypoint where waypoint_id = :id");

  // Get all that are crossing the anti meridian too and filter them out from the query result
  airwayByRectQuery = new SqlQuery(dbNav);
  airwayByRectQuery->prepare(
    "select " + airwayQueryBase + ", right_lonx, left_lonx, bottom_laty, top_laty from airway where " +
    "not (right_lonx < :leftx or left_lonx > :rightx or bottom_laty > :topy or top_laty < :bottomy) "
    "or right_lonx < left_lonx");

  airwayByWaypointIdQuery = new SqlQuery(dbNav);
  airwayByWaypointIdQuery->prepare(
    "select " + airwayQueryBase + " from airway where from_waypoint_id = :id or to_waypoint_id = :id");

  airwayByNameAndWaypointQuery = new SqlQuery(dbNav);
  airwayByNameAndWaypointQuery->prepare(
    "select " + airwayQueryBase +
    " from airway a join waypoint wf on a.from_waypoint_id = wf.waypoint_id "
    "join waypoint wt on a.to_waypoint_id = wt.waypoint_id "
    "where a.airway_name = :airway and ((wf.ident = :ident1 and wt.ident = :ident2) or "
    " (wt.ident = :ident1 and wf.ident = :ident2))");

  airwayByIdQuery = new SqlQuery(dbNav);
  airwayByIdQuery->prepare("select " + airwayQueryBase + " from airway where airway_id = :id");

  airwayWaypointByIdentQuery = new SqlQuery(dbNav);
  airwayWaypointByIdentQuery->prepare("select " + waypointQueryBase +
                                      " from waypoint w "
                                      " join airway a on w.waypoint_id = a.from_waypoint_id "
                                      "where w.ident = :waypoint and a.airway_name = :airway"
                                      " union "
                                      "select " + waypointQueryBase +
                                      " from waypoint w "
                                      " join airway a on w.waypoint_id = a.to_waypoint_id "
                                      "where w.ident = :waypoint and a.airway_name = :airway");

  airwayByNameQuery = new SqlQuery(dbNav);
  airwayByNameQuery->prepare("select " + airwayQueryBase + " from airway where airway_name = :name");

  airwayWaypointsQuery = new SqlQuery(dbNav);
  airwayWaypointsQuery->prepare("select " + airwayQueryBase + " from airway where airway_name = :name "
                                                              " order by airway_fragment_no, sequence_no");

  airwayFullQuery = new SqlQuery(dbNav);
  airwayFullQuery->prepare("select " + airwayQueryBase +
                           " from airway where airway_fragment_no = :fragment and airway_name = :name");

}

void AirwayQuery::deInitQueries()
{
  airwayCache.clear();

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
