/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#include "common/approachquery.h"
#include "mapgui/mapquery.h"

#include "sql/sqlquery.h"

#include <functional>

using atools::sql::SqlQuery;
using atools::geo::Pos;

const maptypes::MapApproachLegList *buildEntries(QCache<int, maptypes::MapApproachLegList>& cache,
                                                 SqlQuery *query, int approachId,
                                                 std::function<maptypes::MapApproachLeg(
                                                                 SqlQuery *query)> factoryFunction)
{
  if(cache.contains(approachId))
    return cache.object(approachId);
  else
  {
    query->bindValue(":id", approachId);
    query->exec();

    maptypes::MapApproachLegList *entries = new maptypes::MapApproachLegList;
    while(query->next())
      entries->legs.append(factoryFunction(query));

    if(!entries->legs.isEmpty())
      cache.insert(approachId, entries);
    else
      cache.insert(approachId, nullptr);

    return entries;
  }
}

const maptypes::MapApproachLeg *buildEntry(QCache<int, maptypes::MapApproachLeg>& cache,
                                           SqlQuery *query, int legId,
                                           std::function<maptypes::MapApproachLeg(
                                                           SqlQuery *query)> factoryFunction)
{
  if(cache.contains(legId))
    return cache.object(legId);
  else
  {
    query->bindValue(":id", legId);
    query->exec();

    maptypes::MapApproachLeg *leg = new maptypes::MapApproachLeg;
    if(query->next())
      *leg = factoryFunction(query);

    cache.insert(legId, leg);

    return leg;
  }
}

ApproachQuery::ApproachQuery(atools::sql::SqlDatabase *sqlDb, MapQuery *mapQueryParam)
  : db(sqlDb), mapQuery(mapQueryParam)
{
}

ApproachQuery::~ApproachQuery()
{
  deInitQueries();
}

const maptypes::MapApproachLegList *ApproachQuery::getApproachLegs(int approachId)
{
  std::function<maptypes::MapApproachLeg(SqlQuery *query)> func =
    std::bind(&ApproachQuery::buildApproachLegEntry, this, std::placeholders::_1);

  return buildEntries(approachCache, approachQuery, approachId, func);
}

const maptypes::MapApproachLegList *ApproachQuery::getTransitionLegs(int transitionId)
{
  std::function<maptypes::MapApproachLeg(SqlQuery *query)> func =
    std::bind(&ApproachQuery::buildTransitionLegEntry, this, std::placeholders::_1);

  return buildEntries(transitionCache, transitionQuery, transitionId, func);
}

const maptypes::MapApproachLeg *ApproachQuery::getApproachLeg(int legId)
{
  std::function<maptypes::MapApproachLeg(SqlQuery *query)> func =
    std::bind(&ApproachQuery::buildApproachLegEntry, this, std::placeholders::_1);

  return buildEntry(approachLegCache, approachLegQuery, legId, func);
}

const maptypes::MapApproachLeg *ApproachQuery::getTransitionLeg(int legId)
{
  std::function<maptypes::MapApproachLeg(SqlQuery *query)> func =
    std::bind(&ApproachQuery::buildTransitionLegEntry, this, std::placeholders::_1);

  return buildEntry(transitionLegCache, transitionLegQuery, legId, func);
}

maptypes::MapApproachLeg ApproachQuery::buildApproachLegEntry(atools::sql::SqlQuery *query)
{
  maptypes::MapApproachLeg entry;
  entry.approachId = query->value("approach_id").toInt();
  entry.transitionId = -1;
  entry.legId = query->value("approach_leg_id").toInt();
  entry.missed = query->value("is_missed").toBool();
  buildLegEntry(query, entry);
  return entry;
}

maptypes::MapApproachLeg ApproachQuery::buildTransitionLegEntry(atools::sql::SqlQuery *query)
{
  maptypes::MapApproachLeg entry;
  entry.approachId = -1;
  entry.transitionId = query->value("transition_id").toInt();
  entry.legId = query->value("transition_leg_id").toInt();
  entry.missed = false;
  buildLegEntry(query, entry);
  return entry;
}

void ApproachQuery::buildLegEntry(atools::sql::SqlQuery *query, maptypes::MapApproachLeg& entry)
{
  entry.type = query->value("type").toString();
  entry.altDescriptor = query->value("alt_descriptor").toString();
  entry.turnDirection = query->value("turn_direction").toString();
  entry.navId = query->value("fix_nav_id").toInt();
  entry.fixType = query->value("fix_type").toString();
  entry.fixIdent = query->value("fix_ident").toString();
  // query->value("fix_region");
  // query->value("fix_airport_ident");
  entry.recNavId = query->value("recommended_fix_nav_id").toInt();
  entry.recFixType = query->value("recommended_fix_type").toString();
  entry.recFixIdent = query->value("recommended_fix_ident").toString();
  // query->value("recommended_fix_region");
  entry.flyover = query->value("is_flyover").toBool();
  entry.trueCourse = query->value("is_true_course").toBool();
  entry.course = query->value("course").toFloat();
  entry.dist = query->value("distance").toFloat();
  entry.time = query->value("time").toFloat();
  entry.theta = query->value("theta").toFloat();
  entry.rho = query->value("rho").toFloat();
  entry.alt1 = query->value("altitude1").toFloat();
  entry.alt2 = query->value("altitude2").toFloat();

  if(entry.fixType == "W" || entry.fixType == "TW")
  {
    entry.waypoint = mapQuery->getWaypointById(entry.navId);
    entry.fixPos = entry.waypoint.position;
  }
  else if(entry.fixType == "N" || entry.fixType == "TN")
  {
    entry.ndb = mapQuery->getNdbById(entry.navId);
    entry.fixPos = entry.ndb.position;
  }
  else if(entry.fixType == "V")
  {
    entry.vor = mapQuery->getVorById(entry.navId);
    entry.fixPos = entry.vor.position;
  }
  else if(entry.fixType == "L")
  {
    entry.ils = mapQuery->getIlsById(entry.navId);
    entry.fixPos = entry.ils.position;
  }
  else if(entry.fixType == "R")
  {
    entry.runwayEnd = mapQuery->getRunwayEndById(entry.navId);
    entry.fixPos = entry.runwayEnd.position;
  }
}

void ApproachQuery::initQueries()
{
  deInitQueries();

  approachQuery = new SqlQuery(db);
  approachQuery->prepare("select * from approach_leg where approach_id = :id");

  transitionQuery = new SqlQuery(db);
  transitionQuery->prepare("select * from transition_leg where transition_id = :id");

  approachLegQuery = new SqlQuery(db);
  approachLegQuery->prepare("select * from approach_leg where approach_leg_id = :id");

  transitionLegQuery = new SqlQuery(db);
  transitionLegQuery->prepare("select * from transition_leg where transition_leg_id = :id");
}

void ApproachQuery::deInitQueries()
{
  approachCache.clear();
  delete approachQuery;
  approachQuery = nullptr;

  transitionCache.clear();
  delete transitionQuery;
  transitionQuery = nullptr;

  approachLegCache.clear();
  delete approachLegQuery;
  approachLegQuery = nullptr;

  transitionLegCache.clear();
  delete transitionLegQuery;
  transitionLegQuery = nullptr;
}
