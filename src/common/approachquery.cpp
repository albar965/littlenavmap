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

// #define NO_CACHE

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
    approachIdForLegQuery->bindValue(":id", legId);
    approachIdForLegQuery->exec();
    if(approachIdForLegQuery->next())
    {
      const maptypes::MapApproachLegs *legs = getApproachLegs(airport, approachIdForLegQuery->value("id").toInt());
      if(legs != nullptr && approachLegIndex.contains(legId))
        // Use index to get leg
        return &legs->legs.at(approachLegIndex.value(legId).second);
    }
    approachIdForLegQuery->finish();
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
    transitionIdForLegQuery->bindValue(":id", legId);
    transitionIdForLegQuery->exec();
    if(transitionIdForLegQuery->next())
    {
      const maptypes::MapApproachLegs *legs =
        getTransitionLegs(airport, transitionIdForLegQuery->value("id").toInt());
      if(legs != nullptr && transitionLegIndex.contains(legId))
        return &legs->legs.at(transitionLegIndex.value(legId).second);
    }
    transitionIdForLegQuery->finish();
  }
  return nullptr;
}

maptypes::MapApproachLeg ApproachQuery::buildApproachLegEntry()
{
  maptypes::MapApproachLeg entry;
  entry.transitionId = -1;
  entry.legId = approachLegQuery->value("approach_leg_id").toInt();
  entry.missed = approachLegQuery->value("is_missed").toBool();
  buildLegEntry(approachLegQuery, entry);
  return entry;
}

maptypes::MapApproachLeg ApproachQuery::buildTransitionLegEntry()
{
  maptypes::MapApproachLeg entry;

  entry.legId = transitionLegQuery->value("transition_leg_id").toInt();
  // entry.dmeNavId = transitionLegQuery->value("dme_nav_id").toInt();
  // entry.dmeRadial = transitionLegQuery->value("dme_radial").toFloat();
  // entry.dmeDistance = transitionLegQuery->value("dme_distance").toFloat();
  // entry.dmeIdent = transitionLegQuery->value("dme_ident").toString();
  // if(!transitionLegQuery->isNull("dme_nav_id"))
  // {
  // entry.dme = mapQuery->getVorById(entry.dmeNavId);
  // entry.dmePos = entry.dme.position;
  // }

  entry.missed = false;
  buildLegEntry(transitionLegQuery, entry);
  return entry;
}

void ApproachQuery::buildLegEntry(atools::sql::SqlQuery *query, maptypes::MapApproachLeg& entry)
{
  entry.type = maptypes::legEnum(query->value("type").toString());

  entry.turnDirection = query->value("turn_direction").toString();
  entry.navId = query->value("fix_nav_id").toInt();
  entry.fixType = query->value("fix_type").toString();
  entry.fixIdent = query->value("fix_ident").toString();
  entry.fixRegion = query->value("fix_region").toString();
  // query->value("fix_airport_ident");
  entry.recNavId = query->value("recommended_fix_nav_id").toInt();
  entry.recFixType = query->value("recommended_fix_type").toString();
  entry.recFixIdent = query->value("recommended_fix_ident").toString();
  entry.recFixRegion = query->value("recommended_fix_region").toString();
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

maptypes::MapApproachLegs *ApproachQuery::buildApproachEntries(const maptypes::MapAirport& airport, int approachId)
{
#ifndef NO_CACHE
  if(approachCache.contains(approachId))
    return approachCache.object(approachId);
  else
#endif
  {
    qDebug() << "buildApproachEntries" << airport.ident << "approachId" << approachId;

    approachLegQuery->bindValue(":id", approachId);
    approachLegQuery->exec();

    maptypes::MapApproachLegs *entries = new maptypes::MapApproachLegs;
    entries->ref.airportId = airport.id;
    entries->ref.approachId = approachId;
    while(approachLegQuery->next())
    {
      entries->legs.append(buildApproachLegEntry());
      approachLegIndex.insert(entries->legs.last().legId, std::make_pair(approachId, entries->legs.size() - 1));
      entries->legs.last().approachId = approachId;
    }

    if(!entries->legs.isEmpty())
      approachCache.insert(approachId, entries);
    else
      approachCache.insert(approachId, nullptr);

    maptypes::MapApproachFullLegs legs(nullptr, entries);
    updateMagvar(airport, entries);

    // Prepare all leg coordinates
    processLegs(legs, false);

    // Calculate intercept terminators
    processCourseInterceptLegs(legs, false);

    updateBounding(entries);

    return entries;
  }
}

maptypes::MapApproachLegs *ApproachQuery::buildTransitionEntries(const maptypes::MapAirport& airport, int approachId,
                                                                 int transitionId)
{
#ifndef NO_CACHE
  if(transitionCache.contains(transitionId))
    return transitionCache.object(transitionId);
  else
#endif
  {
    qDebug() << "buildApproachEntries" << airport.ident << "approachId" << approachId
             << "transitionId" << transitionId;

    transitionLegQuery->bindValue(":id", transitionId);
    transitionLegQuery->exec();

    maptypes::MapApproachLegs *entries = new maptypes::MapApproachLegs;
    entries->ref.airportId = airport.id;
    entries->ref.approachId = approachId;
    entries->ref.transitionId = transitionId;
    while(transitionLegQuery->next())
    {
      entries->legs.append(buildTransitionLegEntry());
      transitionLegIndex.insert(entries->legs.last().legId, std::make_pair(transitionId, entries->legs.size() - 1));
      entries->legs.last().approachId = approachId;
      entries->legs.last().transitionId = transitionId;
    }

    if(!entries->legs.isEmpty())
      transitionCache.insert(transitionId, entries);
    else
      transitionCache.insert(transitionId, nullptr);

    maptypes::MapApproachFullLegs legs(entries, buildApproachEntries(airport, approachId));
    updateMagvar(airport, entries);

    // Prepare all leg coordinates
    processLegs(legs, true);

    // Calculate intercept terminators
    processCourseInterceptLegs(legs, true);

    updateBounding(entries);

    return entries;
  }
}

void ApproachQuery::processLegs(maptypes::MapApproachFullLegs& legs, bool transition)
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
    QString prevIdent = prevLeg != nullptr ? prevLeg->fixIdent : QString();
    maptypes::ApproachLegType type = leg.type;

    if(type == maptypes::ARC_TO_FIX)
    {
      curPos = leg.fixPos;
      leg.displayText << leg.recFixIdent + "/" + Unit::distNm(leg.rho, true, 20, true) + "/" +
      QLocale().toString(leg.theta) + (leg.trueCourse ? tr("°T") : tr("°M"));
      leg.remarks << tr("DME %1").arg(Unit::distNm(leg.rho, true, 20, true));
    }
    else if(type == maptypes::COURSE_TO_FIX)
    {
      Pos extended = leg.fixPos.endpointRhumb(atools::geo::nmToMeter(leg.dist),
                                              atools::geo::opposedCourseDeg(leg.legTrueCourse()));
      if(contains(prevType, {maptypes::TRACK_FROM_FIX_FROM_DISTANCE, maptypes::TRACK_FROM_FIX_TO_DME_DISTANCE,
                             maptypes::HEADING_TO_INTERCEPT, maptypes::COURSE_TO_INTERCEPT}))
      {
        if(extended.distanceMeterTo(lastPos) > atools::geo::nmToMeter(1.f))
          // Draw large bow to extended postition or allow intercept of leg
          lastPos = extended;
      }
      else
      {
        if(extended.distanceMeterTo(lastPos) > atools::geo::nmToMeter(1.f))
        {
          float crs = leg.legTrueCourse();
          Pos intr = Pos::intersectingRadials(extended, crs, lastPos, crs - 45.f).normalize();
          Pos intr2 = Pos::intersectingRadials(extended, crs, lastPos, crs + 45.f).normalize();
          Pos intersect;
          if(intr.distanceMeterTo(lastPos) < intr2.distanceMeterTo(lastPos))
            intersect = intr;
          else
            intersect = intr2;

          if(intersect.isValid())
          {
            atools::geo::CrossTrackStatus status;
            intersect.distanceMeterToLine(leg.fixPos, extended, status);

            float crs1 = lastPos.angleDegTo(extended);
            float crs2 = lastPos.angleDegTo(leg.fixPos);
            // "FD26" "D102H" from 0.563407 via 217.698 to 298.377 index 5

            float isectCrs = lastPos.angleDegTo(intersect);
            qDebug() << "======== fix" << leg.fixIdent << "prev" << prevIdent << "from"
                     << crs1 << "via" << isectCrs << "to" << crs2 << "index" << i;
            qDebug() << "======== intersect" << intersect;
            qDebug() << "======== extended" << extended;

            if(status == atools::geo::ALONG_TRACK)
            {
              qDebug() << "ALONG_TRACK";
              leg.intersectPos = intersect;
            }
            else if(status == atools::geo::BEFORE_START)
            {
              qDebug() << "BEFORE_START";
              // Fly to fix
            }
            else if(status == atools::geo::AFTER_END)
            {
              qDebug() << "AFTER_END";
              // Fly to start of leg
              lastPos = extended;
            }
            else
              qWarning() << "leg line type" << leg.type << "fix" << leg.fixIdent
                         << "invalid cross track"
                         << "approachId" << leg.approachId
                         << "transitionId" << leg.transitionId << "legId" << leg.legId;
          }
        }
      }

      curPos = leg.fixPos;
    }
    else if(contains(type, {maptypes::DIRECT_TO_FIX, maptypes::INITIAL_FIX, maptypes::TRACK_TO_FIX,
                            maptypes::CONSTANT_RADIUS_ARC, maptypes::PROCEDURE_TURN}))
    {
      curPos = leg.fixPos;
    }
    else if(contains(type,
                     {maptypes::COURSE_TO_ALTITUDE, maptypes::FIX_TO_ALTITUDE,
                      maptypes::HEADING_TO_ALTITUDE_TERMINATION}))
    {
      if(prevLeg != nullptr)
      {
        // TODO calculate by altitude
        Pos start = lastPos.isValid() ? lastPos : leg.fixPos;

        if(!lastPos.isValid())
          lastPos = start;
        curPos = start.endpoint(atools::geo::nmToMeter(3.f), leg.legTrueCourse()).normalize();
        leg.displayText << tr("Altitude");
      }
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
    }
    else if(type == maptypes::TRACK_FROM_FIX_FROM_DISTANCE)
    {
      if(!lastPos.isValid())
        lastPos = leg.fixPos;

      curPos = leg.fixPos.endpoint(atools::geo::nmToMeter(leg.dist), leg.legTrueCourse()).normalize();

      leg.displayText << leg.fixIdent + "/" + Unit::distNm(leg.dist, true, 20, true) + "/" +
      QLocale().toString(leg.course) + (leg.trueCourse ? tr("°T") : tr("°M"));
    }
    else if(contains(type, {maptypes::TRACK_FROM_FIX_TO_DME_DISTANCE, maptypes::COURSE_TO_DME_DISTANCE,
                            maptypes::HEADING_TO_DME_DISTANCE_TERMINATION}))
    {
      Pos start = lastPos.isValid() ? lastPos : (leg.fixPos.isValid() ? leg.fixPos : leg.recFixPos);

      Pos center = leg.recFixPos.isValid() ? leg.recFixPos : leg.fixPos;
      Line line(start, start.endpointRhumb(atools::geo::nmToMeter(leg.dist * 2), leg.legTrueCourse()));

      if(!lastPos.isValid())
        lastPos = start;
      Pos intersect = line.intersectionWithCircle(center, atools::geo::nmToMeter(leg.dist), 10.f);

      if(intersect.isValid())
        curPos = intersect;
      else
      {
        curPos = center;
        qWarning() << "leg line type" << type << "fix" << leg.fixIdent << "no intersectionWithCircle found"
                   << "approachId" << leg.approachId << "transitionId" << leg.transitionId << "legId" << leg.legId;
      }

      leg.displayText << leg.recFixIdent + "/" + Unit::distNm(leg.dist, true, 20, true) + "/" +
      QLocale().toString(leg.course) + (leg.trueCourse ? tr("°T") : tr("°M"));
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
    }
    else if(type == maptypes::HOLD_TO_ALTITUDE)
    {
      curPos = leg.fixPos;
      leg.displayText << tr("Altitude");
    }
    else if(type == maptypes::HOLD_TO_FIX)
    {
      curPos = leg.fixPos;
      leg.displayText << tr("Single");
    }
    else if(type == maptypes::HOLD_TO_MANUAL_TERMINATION)
    {
      curPos = leg.fixPos;
      leg.displayText << tr("Manual");
    }

    leg.line = Line(lastPos.isValid() ? lastPos : curPos, curPos);
    leg.original = leg.line;
    lastPos = curPos;
  }
}

void ApproachQuery::processCourseInterceptLegs(maptypes::MapApproachFullLegs& legs, bool transition)
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
        // TODO catch case of non intersecting legs properly
        bool nextIsArc = contains(nextLeg->type, {maptypes::ARC_TO_FIX, maptypes::CONSTANT_RADIUS_ARC});
        Pos start = prevLeg != nullptr ? prevLeg->original.getPos2() : leg.fixPos;
        Pos intersect;
        if(nextIsArc)
        {
          Line line(start, start.endpointRhumb(atools::geo::nmToMeter(200), leg.legTrueCourse()));
          intersect = line.intersectionWithCircle(nextLeg->recFixPos, atools::geo::nmToMeter(nextLeg->rho), 20);
        }
        else
        {
          float nextCourse;
          if((nextLeg->course == 0.f || nextLeg->type == maptypes::INITIAL_FIX) && secondNextLeg != nullptr)
            nextCourse = secondNextLeg->legTrueCourse();
          else
            nextCourse = nextLeg->legTrueCourse();

          intersect = Pos::intersectingRadials(start, leg.legTrueCourse(), nextLeg->original.getPos1(), nextCourse);
        }

        leg.line.setPos1(start);

        if(intersect.isValid() && intersect.distanceMeterTo(start) < atools::geo::nmToMeter(100.f))
        {
          leg.line.setPos2(intersect);
          if(nextIsArc)
            nextLeg->line.setPos1(intersect);

          leg.displayText << tr("Intercept");

          if(nextIsArc)
            leg.displayText << nextLeg->recFixIdent + "/" + Unit::distNm(nextLeg->rho, true, 20, true);
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

        leg.original = leg.line;
      }
    }
  }
}

void ApproachQuery::updateBounding(maptypes::MapApproachLegs *legs)
{
  if(legs != nullptr)
  {
    for(maptypes::MapApproachLeg& leg : legs->legs)
    {
      legs->bounding.extend(leg.fixPos);
      legs->bounding.extend(leg.line.getPos1());
      legs->bounding.extend(leg.line.getPos2());
      legs->bounding.extend(leg.original.getPos1());
      legs->bounding.extend(leg.original.getPos2());
    }
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
