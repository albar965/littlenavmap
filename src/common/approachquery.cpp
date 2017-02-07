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

// #define DEBUG_NO_CACHE

using atools::sql::SqlQuery;
using atools::geo::Pos;
using atools::geo::Rect;
using atools::geo::Line;
using atools::contains;

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
  return buildApproachEntries(airport, approachId);
}

const maptypes::MapApproachLegs *ApproachQuery::getTransitionLegs(const maptypes::MapAirport& airport,
                                                                      int transitionId)
{
  // Get approach id for this transition
  int approachId = -1;
  approachIdForTransQuery->bindValue(":id", transitionId);
  approachIdForTransQuery->exec();
  if(approachIdForTransQuery->next())
    approachId = approachIdForTransQuery->value("approach_id").toInt();

  return buildTransitionEntries(airport, approachId, transitionId);
}

const maptypes::MapApproachLeg *ApproachQuery::getApproachLeg(const maptypes::MapAirport& airport, int legId)
{
#ifndef DEBUG_NO_CACHE
  if(approachLegIndex.contains(legId))
  {
    // Already in index
    std::pair<int, int> val = approachLegIndex.value(legId);

    // Ensure it is in the cache - reload if needed
    const maptypes::MapApproachLegs *legs = getApproachLegs(airport, val.first);
    if(legs != nullptr)
      return &legs->at(approachLegIndex.value(legId).second);
  }
  else
#endif
  {
    // Get approach ID for leg
    approachIdForLegQuery->bindValue(":id", legId);
    approachIdForLegQuery->exec();
    if(approachIdForLegQuery->next())
    {
      const maptypes::MapApproachLegs *legs = getApproachLegs(airport, approachIdForLegQuery->value("id").toInt());
      if(legs != nullptr && approachLegIndex.contains(legId))
        // Use index to get leg
        return &legs->at(approachLegIndex.value(legId).second);
    }
    approachIdForLegQuery->finish();
  }
  qWarning() << "approach leg with id" << legId << "not found";
  return nullptr;
}

const maptypes::MapApproachLeg *ApproachQuery::getTransitionLeg(const maptypes::MapAirport& airport, int legId)
{
#ifndef DEBUG_NO_CACHE
  if(transitionLegIndex.contains(legId))
  {
    // Already in index
    std::pair<int, int> val = transitionLegIndex.value(legId);

    // Ensure it is in the cache - reload if needed
    const maptypes::MapApproachLegs *legs = getTransitionLegs(airport, val.first);

    if(legs != nullptr)
      return &legs->at(transitionLegIndex.value(legId).second);
  }
  else
#endif
  {
    // Get transition ID for leg
    transitionIdForLegQuery->bindValue(":id", legId);
    transitionIdForLegQuery->exec();
    if(transitionIdForLegQuery->next())
    {
      const maptypes::MapApproachLegs *legs = getTransitionLegs(airport,
                                                                    transitionIdForLegQuery->value("id").toInt());
      if(legs != nullptr && transitionLegIndex.contains(legId))
        return &legs->at(transitionLegIndex.value(legId).second);
    }
    transitionIdForLegQuery->finish();
  }
  qWarning() << "transition leg with id" << legId << "not found";
  return nullptr;
}

maptypes::MapApproachLeg ApproachQuery::buildApproachLegEntry()
{
  maptypes::MapApproachLeg leg;
  leg.legId = approachLegQuery->value("approach_leg_id").toInt();
  leg.missed = approachLegQuery->value("is_missed").toBool();
  buildLegEntry(approachLegQuery, leg);
  return leg;
}

maptypes::MapApproachLeg ApproachQuery::buildTransitionLegEntry()
{
  maptypes::MapApproachLeg leg;

  leg.legId = transitionLegQuery->value("transition_leg_id").toInt();

  // entry.dmeNavId = transitionLegQuery->value("dme_nav_id").toInt();
  // entry.dmeRadial = transitionLegQuery->value("dme_radial").toFloat();
  // entry.dmeDistance = transitionLegQuery->value("dme_distance").toFloat();
  // entry.dmeIdent = transitionLegQuery->value("dme_ident").toString();
  // if(!transitionLegQuery->isNull("dme_nav_id"))
  // {
  // entry.dme = mapQuery->getVorById(entry.dmeNavId);
  // entry.dmePos = entry.dme.position;
  // }

  leg.missed = false;
  buildLegEntry(transitionLegQuery, leg);
  return leg;
}

void ApproachQuery::buildLegEntry(atools::sql::SqlQuery *query, maptypes::MapApproachLeg& leg)
{
  leg.type = maptypes::approachLegEnum(query->value("type").toString());

  leg.turnDirection = query->value("turn_direction").toString();
  leg.navId = query->value("fix_nav_id").toInt();
  leg.fixType = query->value("fix_type").toString();
  leg.fixIdent = query->value("fix_ident").toString();
  leg.fixRegion = query->value("fix_region").toString();
  // query->value("fix_airport_ident");
  leg.recNavId = query->value("recommended_fix_nav_id").toInt();
  leg.recFixType = query->value("recommended_fix_type").toString();
  leg.recFixIdent = query->value("recommended_fix_ident").toString();
  leg.recFixRegion = query->value("recommended_fix_region").toString();
  leg.flyover = query->value("is_flyover").toBool();
  leg.trueCourse = query->value("is_true_course").toBool();
  leg.course = query->value("course").toFloat();
  leg.distance = query->value("distance").toFloat();
  leg.time = query->value("time").toFloat();
  leg.theta = query->value("theta").toFloat();
  leg.rho = query->value("rho").toFloat();

  leg.calculatedDistance = 0.f;
  leg.calculatedCourse = 0.f;
  leg.disabled = false;

  float alt1 = query->value("altitude1").toFloat();
  float alt2 = query->value("altitude2").toFloat();

  if(!query->isNull("alt_descriptor") && (alt1 > 0.f || alt2 > 0.f))
  {
    QString descriptor = query->value("alt_descriptor").toString();

    if(descriptor == "A")
      leg.altRestriction.descriptor = maptypes::MapAltRestriction::AT;
    else if(descriptor == "+")
      leg.altRestriction.descriptor = maptypes::MapAltRestriction::AT_OR_ABOVE;
    else if(descriptor == "-")
      leg.altRestriction.descriptor = maptypes::MapAltRestriction::AT_OR_BELOW;
    else if(descriptor == "B")
      leg.altRestriction.descriptor = maptypes::MapAltRestriction::BETWEEN;
    else
      leg.altRestriction.descriptor = maptypes::MapAltRestriction::AT;

    leg.altRestriction.alt1 = alt1;
    leg.altRestriction.alt2 = alt2;
  }
  else
  {
    leg.altRestriction.descriptor = maptypes::MapAltRestriction::NONE;
    leg.altRestriction.alt1 = 0.f;
    leg.altRestriction.alt2 = 0.f;
  }

  leg.magvar = maptypes::INVALID_MAGVAR;

  // Load full navaid information for fix an set fix position
  if(leg.fixType == "W" || leg.fixType == "TW")
  {
    mapQuery->getMapObjectById(leg.navaids, maptypes::WAYPOINT, leg.navId);
    if(!leg.navaids.waypoints.isEmpty())
    {
      leg.fixPos = leg.navaids.waypoints.first().position;
      leg.magvar = leg.navaids.waypoints.first().magvar;
    }
  }
  else if(leg.fixType == "V")
  {
    mapQuery->getMapObjectById(leg.navaids, maptypes::VOR, leg.navId);
    if(!leg.navaids.vors.isEmpty())
    {
      leg.fixPos = leg.navaids.vors.first().position;
      leg.magvar = leg.navaids.vors.first().magvar;
    }
  }
  else if(leg.fixType == "N" || leg.fixType == "TN")
  {
    mapQuery->getMapObjectById(leg.navaids, maptypes::NDB, leg.navId);
    if(!leg.navaids.ndbs.isEmpty())
    {
      leg.fixPos = leg.navaids.ndbs.first().position;
      leg.magvar = leg.navaids.ndbs.first().magvar;
    }
  }
  else if(leg.fixType == "L")
  {
    mapQuery->getMapObjectById(leg.navaids, maptypes::ILS, leg.navId);
    if(!leg.navaids.ils.isEmpty())
    {
      leg.fixPos = leg.navaids.ils.first().position;
      leg.magvar = leg.navaids.ils.first().magvar;
    }
  }
  else if(leg.fixType == "R")
  {
    mapQuery->getMapObjectById(leg.navaids, maptypes::RUNWAYEND, leg.navId);
    leg.fixPos = leg.navaids.runwayEnds.isEmpty() ? Pos() : leg.navaids.runwayEnds.first().position;
  }

  // Load navaid information for recommended fix and set fix position
  maptypes::MapSearchResult rn;
  if(leg.recFixType == "W" || leg.recFixType == "TW")
  {
    mapQuery->getMapObjectById(rn, maptypes::WAYPOINT, leg.recNavId);
    if(!rn.waypoints.isEmpty())
    {
      leg.recFixPos = rn.waypoints.first().position;
      if(leg.magvar == maptypes::INVALID_MAGVAR)
        leg.magvar = rn.waypoints.first().magvar;
    }
  }
  else if(leg.recFixType == "V")
  {
    mapQuery->getMapObjectById(rn, maptypes::VOR, leg.recNavId);
    if(!rn.vors.isEmpty())
    {
      leg.recFixPos = rn.vors.first().position;
      if(leg.magvar == maptypes::INVALID_MAGVAR)
        leg.magvar = rn.vors.first().magvar;
    }
  }
  else if(leg.recFixType == "N" || leg.recFixType == "TN")
  {
    mapQuery->getMapObjectById(rn, maptypes::NDB, leg.recNavId);
    if(!rn.ndbs.isEmpty())
    {
      leg.recFixPos = rn.ndbs.first().position;
      if(leg.magvar == maptypes::INVALID_MAGVAR)
        leg.magvar = rn.ndbs.first().magvar;
    }
  }
  else if(leg.recFixType == "L")
  {
    mapQuery->getMapObjectById(rn, maptypes::ILS, leg.recNavId);
    if(!rn.ils.isEmpty())
    {
      leg.recFixPos = rn.ils.first().position;
      if(leg.magvar == maptypes::INVALID_MAGVAR)
        leg.magvar = rn.ils.first().magvar;
    }
  }
  else if(leg.recFixType == "R")
  {
    mapQuery->getMapObjectById(rn, maptypes::RUNWAYEND, leg.recNavId);
    leg.recFixPos = rn.runwayEnds.isEmpty() ? Pos() : rn.runwayEnds.first().position;
  }
}

void ApproachQuery::updateMagvar(const maptypes::MapAirport& airport, maptypes::MapApproachLegs *legs)
{
  for(maptypes::MapApproachLeg& leg : legs->alegs)
  {
    if(leg.magvar == maptypes::INVALID_MAGVAR)
      leg.magvar = airport.magvar;
  }

  for(maptypes::MapApproachLeg& leg : legs->tlegs)
  {
    if(leg.magvar == maptypes::INVALID_MAGVAR)
      leg.magvar = airport.magvar;
  }
}

maptypes::MapApproachLegs *ApproachQuery::buildApproachEntries(const maptypes::MapAirport& airport, int approachId)
{
#ifndef DEBUG_NO_CACHE
  if(approachCache.contains(approachId))
    return approachCache.object(approachId);
  else
#endif
  {
    qDebug() << "buildApproachEntries" << airport.ident << "approachId" << approachId;

    approachLegQuery->bindValue(":id", approachId);
    approachLegQuery->exec();

    maptypes::MapApproachLegs *legs = new maptypes::MapApproachLegs;
    legs->ref.airportId = airport.id;
    legs->ref.approachId = approachId;
    legs->ref.transitionId = -1;
    while(approachLegQuery->next())
    {
      legs->alegs.append(buildApproachLegEntry());
      approachLegIndex.insert(legs->alegs.last().legId, std::make_pair(approachId, legs->size() - 1));
      legs->alegs.last().approachId = approachId;
    }

    updateMagvar(airport, legs);

    // Prepare all leg coordinates
    processLegs(*legs, false);

    // Calculate intercept terminators
    processCourseInterceptLegs(*legs, false);

    updateBounding(*legs);

    if(!legs->isEmpty())
      approachCache.insert(approachId, legs);
    else
      approachCache.insert(approachId, nullptr);

    return legs;
  }
}

maptypes::MapApproachLegs *ApproachQuery::buildTransitionEntries(const maptypes::MapAirport& airport,
                                                                     int approachId, int transitionId)
{
#ifndef DEBUG_NO_CACHE
  if(transitionCache.contains(transitionId))
    return transitionCache.object(transitionId);
  else
#endif
  {
    qDebug() << "buildApproachEntries" << airport.ident << "approachId" << approachId
             << "transitionId" << transitionId;

    transitionLegQuery->bindValue(":id", transitionId);
    transitionLegQuery->exec();

    maptypes::MapApproachLegs *legs = new maptypes::MapApproachLegs;
    legs->ref.airportId = airport.id;
    legs->ref.approachId = approachId;
    legs->ref.transitionId = transitionId;

    while(transitionLegQuery->next())
    {
      legs->tlegs.append(buildTransitionLegEntry());
      transitionLegIndex.insert(legs->tlegs.last().legId, std::make_pair(transitionId, legs->size() - 1));
      legs->tlegs.last().approachId = approachId;
      legs->tlegs.last().transitionId = transitionId;
    }

    // Add a full copy of the approach because approach legs will be modified
    maptypes::MapApproachLegs *approach = buildApproachEntries(airport, approachId);
    if(approach != nullptr)
      legs->alegs = approach->alegs;

    updateMagvar(airport, legs);

    // Prepare all leg coordinates
    processLegs(*legs, true);

    // Calculate intercept terminators
    processCourseInterceptLegs(*legs, true);

    updateBounding(*legs);

    if(!legs->isEmpty())
      transitionCache.insert(transitionId, legs);
    else
      transitionCache.insert(transitionId, nullptr);

    return legs;
  }
}

void ApproachQuery::processLegs(maptypes::MapApproachLegs& legs, bool transition)
{
  // Assumptions: 3.5 nm per min
  // Climb 500 ft/min
  // Intercept 30 for localizers and 30-45 for others
  Pos lastPos;
  for(int i = 0; i < legs.size(); ++i)
  {
    if((transition && !legs.isTransition(i)) || (!transition && legs.isTransition(i)))
      continue;

    Pos curPos;
    maptypes::MapApproachLeg& leg = legs[i];
    maptypes::MapApproachLeg *prevLeg = i > 0 ? &legs[i - 1] : nullptr;
    maptypes::ApproachLegType prevType = prevLeg != nullptr ? prevLeg->type : maptypes::INVALID_LEG_TYPE;
    maptypes::ApproachLegType type = leg.type;

    if(type == maptypes::ARC_TO_FIX)
    {
      curPos = leg.fixPos;
      leg.displayText << leg.recFixIdent + "/" + Unit::distNm(leg.rho, true, 20, true) + "/" +
      QLocale().toString(leg.theta) + (leg.trueCourse ? tr("°T") : tr("°M"));
      leg.remarks << tr("DME %1").arg(Unit::distNm(leg.rho, true, 20, true));

      // TODO calculate more precise
      leg.calculatedDistance = atools::geo::meterToNm(lastPos.distanceMeterTo(curPos)) * 3.14f;
    }
    else if(type == maptypes::COURSE_TO_FIX)
    {
      // Calculate the leading extended position to the fix
      Pos extended = leg.fixPos.endpoint(atools::geo::nmToMeter(leg.distance),
                                         atools::geo::opposedCourseDeg(leg.legTrueCourse())).normalize();
      if(contains(prevType, {maptypes::TRACK_FROM_FIX_FROM_DISTANCE, maptypes::TRACK_FROM_FIX_TO_DME_DISTANCE,
                             maptypes::HEADING_TO_INTERCEPT, maptypes::COURSE_TO_INTERCEPT}))
      {
        if(extended.distanceMeterTo(lastPos) > atools::geo::nmToMeter(1.f))
          // Draw large bow to extended postition or allow intercept of leg
          lastPos = extended;

        // TODO calculate more precise - assume half circle
        leg.calculatedDistance = atools::geo::meterToNm(lastPos.distanceMeterTo(leg.fixPos)) * 3.14f;
      }
      else
      {
        atools::geo::CrossTrackStatus status;
        float dist = lastPos.distanceMeterToLine(extended, leg.fixPos, status);

        if(dist > atools::geo::nmToMeter(1.f))
        {
          // Extended position leading towards the fix which is far away from last fix - calculate an intercept position
          float crs = leg.legTrueCourse();

          // Try left or right
          Pos intr = Pos::intersectingRadials(extended, crs, lastPos, crs - 45.f).normalize();
          Pos intr2 = Pos::intersectingRadials(extended, crs, lastPos, crs + 45.f).normalize();
          Pos intersect;
          // Use whatever course is shorter
          if(intr.distanceMeterTo(lastPos) < intr2.distanceMeterTo(lastPos))
            intersect = intr;
          else
            intersect = intr2;

          if(intersect.isValid())
          {
            atools::geo::CrossTrackStatus status;
            intersect.distanceMeterToLine(leg.fixPos, extended, status);

            if(status == atools::geo::ALONG_TRACK)
              leg.interceptPos = intersect;
            else if(status == atools::geo::BEFORE_START)
              ;  // Fly to fix
            else if(status == atools::geo::AFTER_END)
              // Fly to start of leg
              lastPos = extended;
            else
              qWarning() << "leg line type" << leg.type << "fix" << leg.fixIdent
                         << "invalid cross track"
                         << "approachId" << leg.approachId
                         << "transitionId" << leg.transitionId << "legId" << leg.legId;
          }
        }
        if(leg.interceptPos.isValid())
        {
          leg.calculatedDistance = atools::geo::meterToNm(lastPos.distanceMeterTo(leg.interceptPos) +
                                                          leg.interceptPos.distanceMeterTo(leg.fixPos));
          leg.displayText << tr("Intercept");
        }
        else
          leg.calculatedDistance = atools::geo::meterToNm(lastPos.distanceMeterTo(leg.fixPos));
      }

      curPos = leg.fixPos;
    }
    else if(contains(type, {maptypes::DIRECT_TO_FIX, maptypes::INITIAL_FIX, maptypes::TRACK_TO_FIX,
                            maptypes::CONSTANT_RADIUS_ARC}))
    {
      curPos = leg.fixPos;
      if(type != maptypes::INITIAL_FIX)
        leg.calculatedDistance = atools::geo::meterToNm(lastPos.distanceMeterTo(curPos));
    }
    else if(type == maptypes::PROCEDURE_TURN)
    {
      float course;
      if(leg.turnDirection == "L")
        // Turn right and then turn 180 deg left
        course = leg.legTrueCourse() - 45.f;
      else
        // Turn left and then turn 180 deg right
        course = leg.legTrueCourse() + 45.f;

      // curPos = leg.fixPos;
      curPos = leg.fixPos.endpoint(atools::geo::nmToMeter(leg.distance), course);

      // TODO calculate more precise
      leg.calculatedDistance = atools::geo::meterToNm(lastPos.distanceMeterTo(curPos)) * 2.f;
    }
    else if(contains(type, {maptypes::COURSE_TO_ALTITUDE, maptypes::FIX_TO_ALTITUDE,
                            maptypes::HEADING_TO_ALTITUDE_TERMINATION}))
    {
      if(prevLeg != nullptr)
      {
        // TODO calculate by altitude
        Pos start = lastPos.isValid() ? lastPos : leg.fixPos;

        if(!lastPos.isValid())
          lastPos = start;
        curPos = start.endpoint(atools::geo::nmToMeter(3.f), leg.legTrueCourse()).normalize();
        leg.calculatedDistance = 3.f;
        leg.displayText << tr("Altitude");
      }
      else
        qWarning() << "leg line type" << type << "fix" << leg.fixIdent << "no previous leg found"
                   << "approachId" << leg.approachId << "transitionId" << leg.transitionId << "legId" << leg.legId;
    }
    else if(contains(type, {maptypes::COURSE_TO_RADIAL_TERMINATION, maptypes::HEADING_TO_RADIAL_TERMINATION}))
    {
      Pos start = lastPos.isValid() ? lastPos : leg.fixPos;
      if(!lastPos.isValid())
        lastPos = start;

      Pos center = leg.recFixPos.isValid() ? leg.recFixPos : leg.fixPos;

      Pos intersect = Pos::intersectingRadials(start, leg.legTrueCourse(), center, leg.theta + leg.magvar);

      if(intersect.isValid())
        curPos = intersect;
      else
      {
        curPos = center;
        qWarning() << "leg line type" << type << "fix" << leg.fixIdent << "no intersectingRadials found"
                   << "approachId" << leg.approachId << "transitionId" << leg.transitionId << "legId" << leg.legId;
      }

      leg.displayText << leg.recFixIdent + "/" + QLocale().toString(leg.theta);
      leg.calculatedDistance = atools::geo::meterToNm(lastPos.distanceMeterTo(curPos));
    }
    else if(type == maptypes::TRACK_FROM_FIX_FROM_DISTANCE)
    {
      if(!lastPos.isValid())
        lastPos = leg.fixPos;

      curPos = leg.fixPos.endpoint(atools::geo::nmToMeter(leg.distance), leg.legTrueCourse()).normalize();

      leg.displayText << leg.fixIdent + "/" + Unit::distNm(leg.distance, true, 20, true) + "/" +
      QLocale().toString(leg.course) + (leg.trueCourse ? tr("°T") : tr("°M"));
      leg.calculatedDistance = leg.distance;
    }
    else if(contains(type, {maptypes::TRACK_FROM_FIX_TO_DME_DISTANCE, maptypes::COURSE_TO_DME_DISTANCE,
                            maptypes::HEADING_TO_DME_DISTANCE_TERMINATION}))
    {
      Pos start = lastPos.isValid() ? lastPos : (leg.fixPos.isValid() ? leg.fixPos : leg.recFixPos);

      Pos center = leg.recFixPos.isValid() ? leg.recFixPos : leg.fixPos;
      Line line(start, start.endpoint(atools::geo::nmToMeter(leg.distance * 2), leg.legTrueCourse()));

      if(!lastPos.isValid())
        lastPos = start;
      Pos intersect = line.intersectionWithCircle(center, atools::geo::nmToMeter(leg.distance), 10.f);

      if(intersect.isValid())
        curPos = intersect;
      else
      {
        curPos = center;
        qWarning() << "leg line type" << type << "fix" << leg.fixIdent << "no intersectionWithCircle found"
                   << "approachId" << leg.approachId << "transitionId" << leg.transitionId << "legId" << leg.legId;
      }

      leg.displayText << leg.recFixIdent + "/" + Unit::distNm(leg.distance, true, 20, true) + "/" +
      QLocale().toString(leg.course) + (leg.trueCourse ? tr("°T") : tr("°M"));

      leg.calculatedDistance = atools::geo::meterToNm(lastPos.distanceMeterTo(curPos));
    }
    else if(contains(type, {maptypes::FROM_FIX_TO_MANUAL_TERMINATION, maptypes::HEADING_TO_MANUAL_TERMINATION}))
    {
      if(prevLeg != nullptr)
      {
        Pos start = prevLeg->fixPos.isValid() ? prevLeg->fixPos : prevLeg->line.getPos2();
        if(!lastPos.isValid())
          lastPos = start;
        curPos = start.endpoint(atools::geo::nmToMeter(3.f), leg.legTrueCourse()).normalize();
        leg.displayText << tr("Manual");
      }
      else
        qWarning() << "leg line type" << type << "fix" << leg.fixIdent << "no previous leg found"
                   << "approachId" << leg.approachId << "transitionId" << leg.transitionId << "legId" << leg.legId;

      leg.calculatedDistance = 3.f;
    }
    else if(type == maptypes::HOLD_TO_ALTITUDE)
    {
      curPos = leg.fixPos;
      leg.displayText << tr("Altitude");
      leg.calculatedDistance = atools::geo::meterToNm(lastPos.distanceMeterTo(curPos));

    }
    else if(type == maptypes::HOLD_TO_FIX)
    {
      curPos = leg.fixPos;
      leg.displayText << tr("Single");
      leg.calculatedDistance = atools::geo::meterToNm(lastPos.distanceMeterTo(curPos));
    }
    else if(type == maptypes::HOLD_TO_MANUAL_TERMINATION)
    {
      curPos = leg.fixPos;
      leg.displayText << tr("Manual");
      leg.calculatedDistance = atools::geo::meterToNm(lastPos.distanceMeterTo(curPos));
    }

    leg.line = Line(lastPos.isValid() ? lastPos : curPos, curPos);
    if(!leg.line.isValid())
      qWarning() << "leg line type" << type << "fix" << leg.fixIdent << "invalid line"
                 << "approachId" << leg.approachId << "transitionId" << leg.transitionId << "legId" << leg.legId;
    lastPos = curPos;
  }
}

void ApproachQuery::processCourseInterceptLegs(maptypes::MapApproachLegs& legs, bool transition)
{
  for(int i = 0; i < legs.size(); ++i)
  {
    if((transition && !legs.isTransition(i)) || (!transition && legs.isTransition(i)))
      continue;

    maptypes::MapApproachLeg& leg = legs[i];
    maptypes::MapApproachLeg *prevLeg = i > 0 ? &legs[i - 1] : nullptr;
    maptypes::MapApproachLeg *nextLeg = i < legs.size() - 1 ? &legs[i + 1] : nullptr;
    maptypes::MapApproachLeg *secondNextLeg = i < legs.size() - 2 ? &legs[i + 2] : nullptr;

    if(contains(leg.type, {maptypes::COURSE_TO_INTERCEPT, maptypes::HEADING_TO_INTERCEPT}))
    {
      if(nextLeg != nullptr)
      {
        maptypes::MapApproachLeg *next = nextLeg->type == maptypes::INITIAL_FIX ? secondNextLeg : nextLeg;

        if(nextLeg->type == maptypes::INITIAL_FIX)
          // Do not show the cut-off initial fix
          nextLeg->disabled = true;

        if(next != nullptr)
        {
          // TODO catch case of non intersecting legs properly
          bool nextIsArc = contains(next->type, {maptypes::ARC_TO_FIX, maptypes::CONSTANT_RADIUS_ARC});
          Pos start = prevLeg != nullptr ? prevLeg->line.getPos2() : leg.fixPos;
          Pos intersect;
          if(nextIsArc)
          {
            Line line(start, start.endpoint(atools::geo::nmToMeter(200), leg.legTrueCourse()));
            intersect = line.intersectionWithCircle(next->recFixPos, atools::geo::nmToMeter(next->rho), 20);
          }
          else
          {

            float nextCourse;
            if(next->course == 0.f)
              nextCourse = next->legTrueCourse();
            else
              nextCourse = next->legTrueCourse();

            intersect = Pos::intersectingRadials(start, leg.legTrueCourse(), next->line.getPos1(), nextCourse);
          }

          leg.line.setPos1(start);

          if(intersect.isValid() && intersect.distanceMeterTo(start) < atools::geo::nmToMeter(200.f))
          {
            leg.line.setPos2(intersect);
            // if(nextIsArc)
            next->line.setPos1(intersect);

            leg.displayText << tr("Intercept");

            if(nextIsArc)
              leg.displayText << next->recFixIdent + "/" + Unit::distNm(next->rho, true, 20, true);
            else
              leg.displayText << tr("Leg");
          }
          else
          {
            qWarning() << "leg line type" << leg.type << "fix" << leg.fixIdent
                       << "no intersectingRadials/intersectionWithCircle found"
                       << "approachId" << leg.approachId << "transitionId" << leg.transitionId << "legId" << leg.legId;
            leg.line.setPos2(nextLeg->line.getPos1());
          }
        }
      }
    }
  }
}

void ApproachQuery::updateBounding(maptypes::MapApproachLegs& legs)
{
  for(int i = 0; i < legs.size(); i++)
  {
    const maptypes::MapApproachLeg& leg = legs.at(i);
    legs.bounding.extend(leg.fixPos);
    legs.bounding.extend(leg.interceptPos);
    legs.bounding.extend(leg.line.boundingRect());
  }
}

void ApproachQuery::initQueries()
{
  deInitQueries();

  approachLegQuery = new SqlQuery(db);
  approachLegQuery->prepare("select * from approach_leg where approach_id = :id "
                            "order by approach_leg_id");

  transitionLegQuery = new SqlQuery(db);
  transitionLegQuery->prepare("select * from transition_leg where transition_id = :id "
                              "order by transition_leg_id");

  approachIdForLegQuery = new SqlQuery(db);
  approachIdForLegQuery->prepare("select approach_id as id from approach_leg where approach_leg_id = :id");

  transitionIdForLegQuery = new SqlQuery(db);
  transitionIdForLegQuery->prepare("select transition_id as id from transition_leg where transition_leg_id = :id");

  approachIdForTransQuery = new SqlQuery(db);
  approachIdForTransQuery->prepare("select approach_id from transition where transition_id = :id");
}

void ApproachQuery::deInitQueries()
{
  approachCache.clear();
  delete approachLegQuery;
  approachLegQuery = nullptr;

  transitionCache.clear();
  delete transitionLegQuery;
  transitionLegQuery = nullptr;

  approachLegIndex.clear();
  delete approachIdForLegQuery;
  approachIdForLegQuery = nullptr;

  transitionLegIndex.clear();
  delete transitionIdForLegQuery;
  transitionIdForLegQuery = nullptr;

  delete approachIdForTransQuery;
  approachIdForTransQuery = nullptr;
}
