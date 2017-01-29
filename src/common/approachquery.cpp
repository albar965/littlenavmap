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
#include "geo/calculations.h"
#include "common/unit.h"
#include "geo/line.h"

#include "sql/sqlquery.h"

using atools::sql::SqlQuery;
using atools::geo::Pos;
using atools::geo::Rect;
using atools::geo::Line;

ApproachQuery::ApproachQuery(atools::sql::SqlDatabase *sqlDb, MapQuery *mapQueryParam)
  : db(sqlDb), mapQuery(mapQueryParam)
{
}

ApproachQuery::~ApproachQuery()
{
  deInitQueries();
}

const maptypes::MapApproachLegs *ApproachQuery::getApproachLegs(const maptypes::MapAirport& airport, int approachId)
{
  std::function<maptypes::MapApproachLeg(SqlQuery *query)> func =
    std::bind(&ApproachQuery::buildApproachLegEntry, this, std::placeholders::_1);

  maptypes::MapApproachLegs *entries =
    buildEntries(airport, approachCache, approachLegIndex, approachQuery, approachId, func, false);
  return entries;
}

const maptypes::MapApproachLegs *ApproachQuery::getTransitionLegs(const maptypes::MapAirport& airport,
                                                                  int transitionId)
{
  std::function<maptypes::MapApproachLeg(SqlQuery *query)> func =
    std::bind(&ApproachQuery::buildTransitionLegEntry, this, std::placeholders::_1);

  maptypes::MapApproachLegs *entries =
    buildEntries(airport, transitionCache, transitionLegIndex, transitionQuery, transitionId, func, true);
  return entries;
}

const maptypes::MapApproachLeg *ApproachQuery::getApproachLeg(const maptypes::MapAirport& airport, int legId)
{
  if(approachLegIndex.contains(legId))
  {
    // Already in index
    std::pair<int, int> val = approachLegIndex.value(legId);

    // Ensure it is in the cache - reload if needed
    const maptypes::MapApproachLegs *legs = getApproachLegs(airport, val.first);
    if(legs != nullptr)
      return &legs->legs.at(approachLegIndex.value(legId).second);
  }
  else
  {
    // Get approach ID for leg
    approachLegQuery->bindValue(":id", legId);
    approachLegQuery->exec();
    if(approachLegQuery->next())
    {
      const maptypes::MapApproachLegs *legs =
        getApproachLegs(airport, approachLegQuery->value("id").toInt());
      if(legs != nullptr && approachLegIndex.contains(legId))
        return &legs->legs.at(approachLegIndex.value(legId).second);
    }
    approachLegQuery->finish();
  }
  return nullptr;
}

const maptypes::MapApproachLeg *ApproachQuery::getTransitionLeg(const maptypes::MapAirport& airport, int legId)
{
  if(transitionLegIndex.contains(legId))
  {
    // Already in index
    std::pair<int, int> val = transitionLegIndex.value(legId);

    // Ensure it is in the cache - reload if needed
    const maptypes::MapApproachLegs *legs = getTransitionLegs(airport, val.first);

    if(legs != nullptr)
      return &legs->legs.at(transitionLegIndex.value(legId).second);
  }
  else
  {
    // Get transition ID for leg
    transitionLegQuery->bindValue(":id", legId);
    transitionLegQuery->exec();
    if(transitionLegQuery->next())
    {
      const maptypes::MapApproachLegs *legs =
        getTransitionLegs(airport, transitionLegQuery->value("id").toInt());
      if(legs != nullptr && transitionLegIndex.contains(legId))
        return &legs->legs.at(transitionLegIndex.value(legId).second);
    }
    transitionLegQuery->finish();
  }
  return nullptr;
}

void ApproachQuery::postProcessLegList(maptypes::MapApproachFullLegs& legs)
{
  if(legs.isEmpty())
    return;

  // Assumptions: 3.5 nm per min
  // Climb 500 ft/min
  // Intercept 30 for localizers and 30-45 for others
  for(int i = 0; i < legs.size(); ++i)
  {
    maptypes::MapApproachLeg& leg = legs[i];
    maptypes::MapApproachLeg *prevLeg = nullptr;
    if(i > 0)
      prevLeg = &legs[i - 1];

    if(leg.type == "AF" || // Arc to fix
       leg.type == "CF" || // Course to fix
       leg.type == "DF" || // Direct to fix
       leg.type == "IF" || // Initial fix
       leg.type == "TF" || // Track to fix
       leg.type == "RF" || // Constant radius arc
       leg.type == "PI") // Procedure turn
    {
      leg.displayPos = leg.fixPos;
    }
    else if(leg.type == "CA" || // Course to altitude
            leg.type == "FA" || // Fix to altitude
            leg.type == "VA") // Heading to altitude termination
    {
      if(prevLeg != nullptr)
      {
        Pos pos = prevLeg->fixPos.isValid() ? prevLeg->fixPos : prevLeg->displayPos;
        leg.displayPos = pos.endpoint(atools::geo::nmToMeter(3.f), leg.course + leg.magvar).normalize();
        leg.displayText << tr("Altitude");
      }
    }
    else if(leg.type == "CI" || // Course to intercept
            leg.type == "VI") // Heading to intercept
    {
      // TODO line intersection
      // TODO check if next is arc or not

    }
    else if(leg.type == "CR" || // Course to radial termination
            leg.type == "VR") // Heading to radial termination
    {
      // TODO line intersection
    }
    else if(leg.type == "FC") // Track from fix from distance
    {
      leg.displayPos = leg.fixPos.endpoint(atools::geo::nmToMeter(leg.dist), leg.course + leg.magvar).normalize();
      leg.displayText << leg.fixIdent + "/" + Unit::distNm(leg.dist, true, 20, true) + "/" +
      QLocale().toString(leg.course) + (leg.trueCourse ? tr("°T") : tr("°M"));
    }
    else if(leg.type == "FD" || // Track from fix to DME distance
            leg.type == "CD" || // Course to DME distance
            leg.type == "VD") // Heading to DME distance termination
    {
      Pos start = leg.fixPos;
      if(prevLeg != nullptr)
        start = prevLeg->displayPos;

      Pos center = leg.recFixPos.isValid() ? leg.recFixPos : leg.fixPos;
      Line line(start, start.endpointRhumb(atools::geo::nmToMeter(leg.dist * 2), leg.course + leg.magvar));
      leg.displayPos = line.intersectionWithCircle(center, atools::geo::nmToMeter(leg.dist), 10.f);

      leg.displayText << leg.recFixIdent + "/" + Unit::distNm(leg.dist, true, 20, true) + "/" +
      QLocale().toString(leg.course) + (leg.trueCourse ? tr("°T") : tr("°M"));
    }
    else if(leg.type == "FM" || // From fix to manual termination
            leg.type == "VM") // Heading to manual termination
    {
      if(prevLeg != nullptr)
      {
        Pos pos = prevLeg->fixPos.isValid() ? prevLeg->fixPos : prevLeg->displayPos;
        leg.displayPos = pos.endpoint(atools::geo::nmToMeter(3.f), leg.course + leg.magvar).normalize();
        leg.displayText << tr("Manual");
      }
    }
    else if(leg.type == "HA") // Hold to altitude
    {
      leg.displayPos = leg.fixPos;
      leg.displayText << tr("Altitude");
    }
    else if(leg.type == "HF") // Hold to fix
    {
      leg.displayPos = leg.fixPos;
      leg.displayText << tr("Single");
    }
    else if(leg.type == "HM") // Hold to manual termination
    {
      leg.displayPos = leg.fixPos;
      leg.displayText << tr("Manual");
    }
  }
}

void ApproachQuery::updateBounding(maptypes::MapApproachLegs *legs)
{
  if(legs != nullptr)
  {
    for(maptypes::MapApproachLeg& leg : legs->legs)
    {
      if(leg.fixPos.isValid())
      {
        if(!legs->bounding.isValid())
          legs->bounding = Rect(leg.fixPos);
        else
          legs->bounding.extend(leg.fixPos);
      }
      if(leg.displayPos.isValid())
      {
        if(!legs->bounding.isValid())
          legs->bounding = Rect(leg.displayPos);
        else
          legs->bounding.extend(leg.displayPos);
      }
    }
  }
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

  float alt1 = query->value("altitude1").toFloat();
  float alt2 = query->value("altitude2").toFloat();

  if(!query->isNull("alt_descriptor") && (alt1 > 0.f || alt2 > 0.f))
  {
    QString descriptor = query->value("alt_descriptor").toString();

    if(descriptor == "A")
      entry.altRestriction.descriptor = maptypes::MapAltRestriction::AT;
    else if(descriptor == "+")
      entry.altRestriction.descriptor = maptypes::MapAltRestriction::AT_OR_ABOVE;
    else if(descriptor == "-")
      entry.altRestriction.descriptor = maptypes::MapAltRestriction::AT_OR_BELOW;
    else if(descriptor == "B")
      entry.altRestriction.descriptor = maptypes::MapAltRestriction::BETWEEN;
    else
      entry.altRestriction.descriptor = maptypes::MapAltRestriction::AT;

    entry.altRestriction.alt1 = alt1;
    entry.altRestriction.alt2 = alt2;
  }
  else
  {
    entry.altRestriction.descriptor = maptypes::MapAltRestriction::NONE;
    entry.altRestriction.alt1 = 0.f;
    entry.altRestriction.alt2 = 0.f;
  }

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

  if(entry.recFixType == "W" || entry.recFixType == "TW")
  {
    entry.recWaypoint = mapQuery->getWaypointById(entry.recNavId);
    entry.recFixPos = entry.recWaypoint.position;
  }
  else if(entry.recFixType == "N" || entry.recFixType == "TN")
  {
    entry.recNdb = mapQuery->getNdbById(entry.recNavId);
    entry.recFixPos = entry.recNdb.position;
  }
  else if(entry.recFixType == "V")
  {
    entry.recVor = mapQuery->getVorById(entry.recNavId);
    entry.recFixPos = entry.recVor.position;
  }
  else if(entry.recFixType == "L")
  {
    entry.recIls = mapQuery->getIlsById(entry.recNavId);
    entry.recFixPos = entry.recIls.position;
  }
  else if(entry.recFixType == "R")
  {
    entry.recRunwayEnd = mapQuery->getRunwayEndById(entry.recNavId);
    entry.recFixPos = entry.recRunwayEnd.position;
  }
}

void ApproachQuery::updateMagvar(const maptypes::MapAirport& airport, maptypes::MapApproachLegs *legs)
{
  for(maptypes::MapApproachLeg& leg : legs->legs)
    leg.magvar = airport.magvar;
}

maptypes::MapApproachLegs *ApproachQuery::buildEntries(const maptypes::MapAirport& airport,
                                                       QCache<int, maptypes::MapApproachLegs>& cache,
                                                       QHash<int, std::pair<int, int> >& index,
                                                       SqlQuery *query, int id,
                                                       FactoryFunctionType factoryFunction,
                                                       bool isTransition)
{
  if(cache.contains(id))
    return cache.object(id);
  else
  {
    query->bindValue(":id", id);
    query->exec();

    maptypes::MapApproachLegs *entries = new maptypes::MapApproachLegs;
    while(query->next())
    {
      entries->legs.append(factoryFunction(query));
      index.insert(entries->legs.last().legId, std::make_pair(id, entries->legs.size() - 1));
    }

    if(!entries->legs.isEmpty())
      cache.insert(id, entries);
    else
      cache.insert(id, nullptr);

    updateMagvar(airport, entries);

    if(isTransition)
    {
      maptypes::MapApproachFullLegs legs(entries, getApproachLegs(airport, entries->legs.first().approachId));
      postProcessLegList(legs);
    }
    else
    {
      maptypes::MapApproachFullLegs legs(nullptr, entries);
      postProcessLegList(legs);
    }
    updateBounding(entries);

    return entries;
  }
}

void ApproachQuery::initQueries()
{
  deInitQueries();

  approachQuery = new SqlQuery(db);
  approachQuery->prepare("select * from approach_leg where approach_id = :id "
                         "order by approach_leg_id");

  transitionQuery = new SqlQuery(db);
  transitionQuery->prepare("select * from transition_leg where transition_id = :id "
                           "order by transition_leg_id");

  approachLegQuery = new SqlQuery(db);
  approachLegQuery->prepare("select approach_id as id from approach_leg where approach_leg_id = :id");

  transitionLegQuery = new SqlQuery(db);
  transitionLegQuery->prepare("select transition_id as id from transition_leg where transition_leg_id = :id");
}

void ApproachQuery::deInitQueries()
{
  approachCache.clear();
  delete approachQuery;
  approachQuery = nullptr;

  transitionCache.clear();
  delete transitionQuery;
  transitionQuery = nullptr;

  approachLegIndex.clear();
  delete approachLegQuery;
  approachLegQuery = nullptr;

  transitionLegIndex.clear();
  delete transitionLegQuery;
  transitionLegQuery = nullptr;
}
