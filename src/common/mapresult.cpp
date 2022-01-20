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

#include "common/mapresult.h"

#include "geo/calculations.h"
#include "options/optiondata.h"
#include "common/maptools.h"

namespace map {

MapResult& MapResult::clear(const MapTypes& types)
{
  if(types.testFlag(map::AIRPORT))
  {
    airports.clear();
    airportIds.clear();
  }

  if(types.testFlag(map::AIRPORT_MSA))
  {
    airportMsa.clear();
    airportMsaIds.clear();
  }

  if(types.testFlag(map::WAYPOINT))
  {
    waypoints.clear();
    waypointIds.clear();
  }

  if(types.testFlag(map::VOR))
  {
    vors.clear();
    vorIds.clear();
  }

  if(types.testFlag(map::NDB))
  {
    ndbs.clear();
    ndbIds.clear();
  }

  if(types.testFlag(map::HOLDING))
  {
    holdings.clear();
    holdingIds.clear();
  }

  if(types.testFlag(map::AIRWAY))
    airways.clear();

  if(types.testFlag(map::AIRSPACE))
    airspaces.clear();

  if(types.testFlag(map::RUNWAYEND))
    runwayEnds.clear();

  if(types.testFlag(map::ILS))
    ils.clear();

  if(types.testFlag(map::AIRSPACE))
    airspaces.clear();

  if(types.testFlag(map::USERPOINTROUTE))
    userpointsRoute.clear();

  if(types.testFlag(map::USERPOINT))
  {
    userpoints.clear();
    userpointIds.clear();
  }

  if(types.testFlag(map::LOGBOOK))
    logbookEntries.clear();

  if(types.testFlag(map::PROCEDURE_POINT))
    procPoints.clear();

  if(types.testFlag(map::AIRCRAFT_AI))
    aiAircraft.clear();

  if(types.testFlag(map::AIRCRAFT))
    userAircraft.clear();

  if(types.testFlag(map::AIRCRAFT_ONLINE))
  {
    onlineAircraft.clear();
    onlineAircraftIds.clear();
  }

  // Marks
  if(types.testFlag(MARK_RANGE))
    rangeMarks.clear();

  if(types.testFlag(MARK_DISTANCE))
    distanceMarks.clear();

  if(types.testFlag(MARK_HOLDING))
    holdingMarks.clear();

  if(types.testFlag(MARK_PATTERNS))
    patternMarks.clear();

  if(types.testFlag(MARK_MSA))
    msaMarks.clear();

  return *this;
}

template<typename TYPE>
void MapResult::clearAllButFirst(QList<TYPE>& list)
{
  while(list.size() > 1)
    list.removeLast();
}

template<typename TYPE>
void MapResult::removeInvalid(QList<TYPE>& list, QSet<int> *ids)
{
  if(ids != nullptr)
  {
    for(const TYPE& type : list)
    {
      if(!type.isValid())
        ids->remove(type.id);
    }
  }

  list.erase(std::remove_if(list.begin(), list.end(), [](const TYPE& type) -> bool {
      return !type.isValid();
    }), list.end());
}

MapResult& MapResult::clearAllButFirst(const MapTypes& types)
{
  if(types.testFlag(map::AIRPORT))
  {
    clearAllButFirst(airports);
    airportIds.clear();
  }

  if(types.testFlag(map::AIRPORT_MSA))
  {
    clearAllButFirst(airportMsa);
    airportMsaIds.clear();
  }

  if(types.testFlag(map::WAYPOINT))
  {
    clearAllButFirst(waypoints);
    waypointIds.clear();
  }

  if(types.testFlag(map::VOR))
  {
    clearAllButFirst(vors);
    vorIds.clear();
  }

  if(types.testFlag(map::NDB))
  {
    clearAllButFirst(ndbs);
    ndbIds.clear();
  }

  if(types.testFlag(map::HOLDING))
  {
    clearAllButFirst(holdings);
    holdingIds.clear();
  }

  if(types.testFlag(map::AIRWAY))
    clearAllButFirst(airways);

  if(types.testFlag(map::RUNWAYEND))
    clearAllButFirst(runwayEnds);

  if(types.testFlag(map::ILS))
    clearAllButFirst(ils);

  if(types.testFlag(map::AIRSPACE))
    clearAllButFirst(airspaces);

  if(types.testFlag(map::USERPOINTROUTE))
    clearAllButFirst(userpointsRoute);

  if(types.testFlag(map::USERPOINT))
  {
    clearAllButFirst(userpoints);
    userpointIds.clear();
  }

  if(types.testFlag(map::LOGBOOK))
    clearAllButFirst(logbookEntries);

  if(types.testFlag(map::PROCEDURE_POINT))
    clearAllButFirst(procPoints);

  if(types.testFlag(map::AIRCRAFT_AI))
    clearAllButFirst(aiAircraft);

  // if(types.testFlag(map::AIRCRAFT))
  // userAircraft.clear();

  if(types.testFlag(map::AIRCRAFT_ONLINE))
  {
    clearAllButFirst(onlineAircraft);
    onlineAircraftIds.clear();
  }

  // Marks
  if(types.testFlag(MARK_RANGE))
    clearAllButFirst(rangeMarks);

  if(types.testFlag(MARK_DISTANCE))
    clearAllButFirst(distanceMarks);

  if(types.testFlag(MARK_HOLDING))
    clearAllButFirst(holdingMarks);

  if(types.testFlag(MARK_PATTERNS))
    clearAllButFirst(patternMarks);

  if(types.testFlag(MARK_MSA))
    clearAllButFirst(msaMarks);

  return *this;
}

void MapResult::moveOnlineAirspacesToFront()
{
  QList<MapAirspace> list;
  for(const MapAirspace& a: airspaces)
  {
    if(a.isOnline())
      list.append(a);
  }
  for(const MapAirspace& a: airspaces)
  {
    if(!a.isOnline())
      list.append(a);
  }
  airspaces = list;
}

MapResult MapResult::moveOnlineAirspacesToFront() const
{
  MapResult retval(*this);
  retval.moveOnlineAirspacesToFront();
  return retval;
}

bool MapResult::hasSimNavUserAirspaces() const
{
  for(const map::MapAirspace& airspace : airspaces)
  {
    if(!airspace.isOnline())
      return true;
  }
  return false;
}

bool MapResult::hasOnlineAirspaces() const
{
  for(const map::MapAirspace& airspace : airspaces)
  {
    if(airspace.isOnline())
      return true;
  }
  return false;
}

const map::MapAirspace *MapResult::firstSimNavUserAirspace() const
{
  QList<map::MapAirspace>::const_iterator it =
    std::find_if(airspaces.begin(), airspaces.end(), [](const map::MapAirspace& a) -> bool
    {
      return !a.isOnline();
    });

  if(it != airspaces.end())
    return &(*it);

  return nullptr;
}

const map::MapAirspace *MapResult::firstOnlineAirspace() const
{
  QList<map::MapAirspace>::const_iterator it =
    std::find_if(airspaces.begin(), airspaces.end(), [](const map::MapAirspace& a) -> bool
    {
      return a.isOnline();
    });

  if(it != airspaces.end())
    return &(*it);

  return nullptr;
}

int MapResult::numSimNavUserAirspaces() const
{
  int num = 0;
  for(const map::MapAirspace& airspace : airspaces)
    num += !airspace.isOnline();
  return num;
}

int MapResult::numOnlineAirspaces() const
{
  int num = 0;
  for(const map::MapAirspace& airspace : airspaces)
    num += airspace.isOnline();
  return num;
}

QList<map::MapAirspace> MapResult::getSimNavUserAirspaces() const
{
  QList<map::MapAirspace> retval;
  for(const map::MapAirspace& airspace : airspaces)
  {
    if(!airspace.isOnline())
      retval.append(airspace);
  }
  return retval;
}

QList<map::MapAirspace> MapResult::getOnlineAirspaces() const
{
  QList<map::MapAirspace> retval;
  for(const map::MapAirspace& airspace : airspaces)
  {
    if(airspace.isOnline())
      retval.append(airspace);
  }
  return retval;
}

QString MapResult::objectText(MapTypes type, int elideName) const
{
  QString str;
  if(type == map::AIRPORT && hasAirports())
    str = map::airportTextShort(airports.constFirst(), elideName);
  else if(type == map::AIRPORT_MSA && hasAirportMsa())
    str = map::airportMsaTextShort(airportMsa.constFirst());
  else if(type == map::VOR && hasVor())
    str = map::vorText(vors.constFirst(), elideName);
  else if(type == map::NDB && hasNdb())
    str = map::ndbText(ndbs.constFirst(), elideName);
  else if(type == map::WAYPOINT && hasWaypoints())
    str = map::waypointText(waypoints.constFirst());
  else if(type == map::USERPOINT && hasUserpoints())
    str = map::userpointText(userpoints.constFirst(), elideName);
  else if(type == map::LOGBOOK && hasLogEntries())
    str = map::logEntryText(logbookEntries.constFirst());
  else if(type == map::PROCEDURE_POINT && hasProcedurePoints())
    str = map::procedurePointTextShort(procPoints.constFirst());
  else if(type == map::AIRCRAFT_ONLINE && hasOnlineAircraft())
    str = onlineAircraft.constFirst().getIdent();
  else if(type == map::AIRSPACE && hasOnlineAirspaces())
    str = getOnlineAirspaces().constFirst().getIdent();
  else if(type == MARK_RANGE)
    str = map::rangeMarkText(rangeMarks.constFirst());
  else if(type == MARK_DISTANCE)
    str = map::distanceMarkText(distanceMarks.constFirst());
  else if(type == MARK_HOLDING)
    str = map::holdingMarkText(holdingMarks.constFirst());
  else if(type == MARK_PATTERNS)
    str = map::patternMarkText(patternMarks.constFirst());
  else if(type == MARK_MSA)
    str = map::msaMarkText(msaMarks.constFirst());
  return str;
}

void MapResult::removeInvalid()
{
  removeInvalid(airports, &airportIds);
  removeInvalid(runwayEnds);
  removeInvalid(towers);
  removeInvalid(parkings);
  removeInvalid(helipads);
  removeInvalid(waypoints, &waypointIds);
  removeInvalid(vors, &waypointIds);
  removeInvalid(ndbs, &ndbIds);
  removeInvalid(markers);
  removeInvalid(ils);
  removeInvalid(airways);
  removeInvalid(airspaces);
  removeInvalid(userpointsRoute);
  removeInvalid(userpoints, &userpointIds);
  removeInvalid(logbookEntries);
  removeInvalid(aiAircraft);
  removeInvalid(onlineAircraft, &onlineAircraftIds);
  removeInvalid(patternMarks);
  removeInvalid(rangeMarks);
  removeInvalid(distanceMarks);
  removeInvalid(holdings, &holdingIds);
  removeInvalid(airportMsa, &airportMsaIds);
  removeInvalid(procPoints);
  removeInvalid(holdingMarks);
  removeInvalid(msaMarks);
}

void MapResult::clearNavdataAirspaces()
{
  QList<map::MapAirspace>::iterator it = std::remove_if(airspaces.begin(), airspaces.end(),
                                                        [](const map::MapAirspace& airspace) -> bool
    {
      return !airspace.isOnline();
    });
  if(it != airspaces.end())
    airspaces.erase(it, airspaces.end());
}

void MapResult::clearOnlineAirspaces()
{
  QList<map::MapAirspace>::iterator it = std::remove_if(airspaces.begin(), airspaces.end(),
                                                        [](const map::MapAirspace& airspace) -> bool
    {
      return airspace.isOnline();
    });
  if(it != airspaces.end())
    airspaces.erase(it, airspaces.end());
}

const atools::geo::Pos& MapResult::getPosition(const std::initializer_list<MapTypes>& types) const
{
  for(const MapTypes& type : types)
  {
    if(!isEmpty(type))
    {
      if(type == map::AIRPORT)
        return airports.constFirst().getPosition();
      else if(type == map::AIRPORT_MSA)
        return airportMsa.constFirst().getPosition();
      else if(type == map::WAYPOINT)
        return waypoints.constFirst().getPosition();
      else if(type == map::VOR)
        return vors.constFirst().getPosition();
      else if(type == map::NDB)
        return ndbs.constFirst().getPosition();
      else if(type == map::AIRWAY)
        return airways.constFirst().getPosition();
      else if(type == map::RUNWAYEND)
        return runwayEnds.constFirst().getPosition();
      else if(type == map::ILS)
        return ils.constFirst().getPosition();
      else if(type == map::HOLDING)
        return holdings.constFirst().getPosition();
      else if(type == map::AIRSPACE)
        return airspaces.constFirst().getPosition();
      else if(type == map::USERPOINTROUTE)
        return userpointsRoute.constFirst().getPosition();
      else if(type == map::USERPOINT)
        return userpoints.constFirst().getPosition();
      else if(type == map::LOGBOOK)
        return logbookEntries.constFirst().getPosition();
      else if(type == map::PROCEDURE_POINT)
        return procPoints.constFirst().getPosition();
      else if(type == map::AIRCRAFT)
        return userAircraft.getPosition();
      else if(type == map::AIRCRAFT_AI)
        return aiAircraft.constFirst().getPosition();
      else if(type == map::AIRCRAFT_ONLINE)
        return onlineAircraft.constFirst().getPosition();
      else if(type == MARK_RANGE)
        return rangeMarks.constFirst().getPosition();
      else if(type == MARK_DISTANCE)
        return distanceMarks.constFirst().getPosition();
      else if(type == MARK_HOLDING)
        return holdingMarks.constFirst().getPosition();
      else if(type == MARK_PATTERNS)
        return patternMarks.constFirst().getPosition();
      else if(type == MARK_MSA)
        return msaMarks.constFirst().getPosition();

    }
  }
  return atools::geo::EMPTY_POS;
}

QString MapResult::getIdent(const std::initializer_list<MapTypes>& types) const
{
  for(const MapTypes& type : types)
  {
    if(!isEmpty(type))
    {
      if(type == map::AIRPORT)
        return airports.constFirst().ident;
      else if(type == map::AIRPORT_MSA)
        return airportMsa.constFirst().navIdent;
      else if(type == map::WAYPOINT)
        return waypoints.constFirst().ident;
      else if(type == map::VOR)
        return vors.constFirst().ident;
      else if(type == map::NDB)
        return ndbs.constFirst().ident;
      else if(type == map::AIRWAY)
        return airways.constFirst().name;
      else if(type == map::RUNWAYEND)
        return runwayEnds.constFirst().name;
      else if(type == map::ILS)
        return ils.constFirst().ident;
      else if(type == map::HOLDING)
        return holdings.constFirst().navIdent;
      else if(type == map::AIRSPACE)
        return airspaces.constFirst().name;
      else if(type == map::USERPOINTROUTE)
        return userpointsRoute.constFirst().ident;
      else if(type == map::USERPOINT)
        return userpoints.constFirst().ident;
      else if(type == map::LOGBOOK)
        return logbookEntries.constFirst().departureIdent;
      else if(type == map::PROCEDURE_POINT)
        return procPoints.constFirst().getIdent();
      else if(type == map::AIRCRAFT)
        return userAircraft.getAircraft().getAirplaneRegistration();
      else if(type == map::AIRCRAFT_AI)
        return aiAircraft.constFirst().getAircraft().getAirplaneRegistration();
      else if(type == map::AIRCRAFT_ONLINE)
        return onlineAircraft.constFirst().getAircraft().getAirplaneRegistration();
      else if(type == MARK_RANGE)
        return rangeMarks.constFirst().text;
      else if(type == MARK_DISTANCE)
        return distanceMarks.constFirst().text;
      else if(type == MARK_HOLDING)
        return holdingMarks.constFirst().holding.navIdent;
      else if(type == MARK_PATTERNS)
        return patternMarks.constFirst().airportIcao;
      else if(type == MARK_MSA)
        return msaMarks.constFirst().msa.navIdent;
    }
  }
  return QString();
}

QString MapResult::getRegion(const std::initializer_list<MapTypes>& types) const
{
  for(const MapTypes& type : types)
  {
    if(!isEmpty(type))
    {
      if(type == map::AIRPORT)
        return airports.constFirst().region;
      else if(type == map::AIRPORT_MSA)
        return airportMsa.constFirst().region;
      else if(type == map::WAYPOINT)
        return waypoints.constFirst().region;
      else if(type == map::VOR)
        return vors.constFirst().region;
      else if(type == map::NDB)
        return ndbs.constFirst().region;
      else if(type == map::ILS)
        return ils.constFirst().region;
      else if(type == map::USERPOINTROUTE)
        return userpointsRoute.constFirst().region;
      else if(type == map::USERPOINT)
        return userpoints.constFirst().region;
    }
  }
  return QString();
}

bool MapResult::getIdAndType(int& id, MapTypes& type, const std::initializer_list<MapTypes>& types) const
{
  id = -1;
  type = NONE;

  for(const MapTypes& t : types)
  {
    if(!isEmpty(t))
    {
      if(t == map::AIRPORT)
        id = airports.constFirst().getId();
      else if(t == map::AIRPORT_MSA)
        id = airportMsa.constFirst().getId();
      else if(t == map::WAYPOINT)
        id = waypoints.constFirst().getId();
      else if(t == map::VOR)
        id = vors.constFirst().getId();
      else if(t == map::NDB)
        id = ndbs.constFirst().getId();
      else if(t == map::AIRWAY)
        id = airways.constFirst().getId();
      else if(t == map::RUNWAYEND)
        id = runwayEnds.constFirst().getId();
      else if(t == map::ILS)
        id = ils.constFirst().getId();
      else if(t == map::HOLDING)
        id = holdings.constFirst().getId();
      else if(t == map::AIRSPACE)
        id = airspaces.constFirst().getId();
      else if(t == map::USERPOINTROUTE)
        id = userpointsRoute.constFirst().getId();
      else if(t == map::USERPOINT)
        id = userpoints.constFirst().getId();
      else if(t == map::LOGBOOK)
        id = logbookEntries.constFirst().getId();
      else if(t == map::PROCEDURE_POINT)
        id = procPoints.constFirst().getId();
      else if(t == map::AIRCRAFT)
        id = userAircraft.getId();
      else if(t == map::AIRCRAFT_AI)
        id = aiAircraft.constFirst().getId();
      else if(t == map::AIRCRAFT_ONLINE)
        id = onlineAircraft.constFirst().getId();
      else if(t == map::MARK_RANGE)
        id = rangeMarks.constFirst().getId();
      else if(t == map::MARK_DISTANCE)
        id = distanceMarks.constFirst().getId();
      else if(t == map::MARK_HOLDING)
        id = holdingMarks.constFirst().getId();
      else if(t == map::MARK_PATTERNS)
        id = patternMarks.constFirst().getId();
      else if(t == map::MARK_MSA)
        id = msaMarks.constFirst().getId();

      if(id != -1)
      {
        type = t;
        break;
      }
    }
  }
  return id != -1;
}

MapResult& MapResult::addFromMapBase(const MapBase *base)
{
  if(base != nullptr)
  {
    if(base->getType().testFlag(map::AIRPORT))
      airports.append(base->asObj<map::MapAirport>());
    else if(base->getType().testFlag(map::AIRPORT_MSA))
      airportMsa.append(base->asObj<map::MapAirportMsa>());
    else if(base->getType().testFlag(map::WAYPOINT))
      waypoints.append(base->asObj<map::MapWaypoint>());
    else if(base->getType().testFlag(map::VOR))
      vors.append(base->asObj<map::MapVor>());
    else if(base->getType().testFlag(map::NDB))
      ndbs.append(base->asObj<map::MapNdb>());
    else if(base->getType().testFlag(map::AIRWAY))
      airways.append(base->asObj<map::MapAirway>());
    else if(base->getType().testFlag(map::RUNWAYEND))
      runwayEnds.append(base->asObj<map::MapRunwayEnd>());
    else if(base->getType().testFlag(map::ILS))
      ils.append(base->asObj<map::MapIls>());
    else if(base->getType().testFlag(map::HOLDING))
      holdings.append(base->asObj<map::MapHolding>());
    else if(base->getType().testFlag(map::AIRSPACE))
      airspaces.append(base->asObj<map::MapAirspace>());
    else if(base->getType().testFlag(map::USERPOINTROUTE))
      userpointsRoute.append(base->asObj<map::MapUserpointRoute>());
    else if(base->getType().testFlag(map::USERPOINT))
      userpoints.append(base->asObj<map::MapUserpoint>());
    else if(base->getType().testFlag(map::LOGBOOK))
      logbookEntries.append(base->asObj<map::MapLogbookEntry>());
    else if(base->getType().testFlag(map::PROCEDURE_POINT))
      procPoints.append(base->asObj<map::MapProcedurePoint>());
    else if(base->getType().testFlag(map::AIRCRAFT))
      userAircraft = base->asObj<map::MapUserAircraft>();
    else if(base->getType().testFlag(map::AIRCRAFT_AI))
      aiAircraft.append(base->asObj<map::MapAiAircraft>());
    else if(base->getType().testFlag(map::AIRCRAFT_ONLINE))
      onlineAircraft.append(base->asObj<map::MapOnlineAircraft>());
    else if(base->getType().testFlag(map::MARK_RANGE))
      rangeMarks.append(base->asObj<map::RangeMarker>());
    else if(base->getType().testFlag(map::MARK_DISTANCE))
      distanceMarks.append(base->asObj<map::DistanceMarker>());
    else if(base->getType().testFlag(map::MARK_HOLDING))
      holdingMarks.append(base->asObj<map::HoldingMarker>());
    else if(base->getType().testFlag(map::MARK_PATTERNS))
      patternMarks.append(base->asObj<map::PatternMarker>());
    else if(base->getType().testFlag(map::MARK_MSA))
      msaMarks.append(base->asObj<map::MsaMarker>());
  }
  return *this;
}

MapResult MapResult::createFromMapBase(const MapBase *base)
{
  return MapResult().addFromMapBase(base);
}

int MapResult::size(const MapTypes& types) const
{
  int totalSize = 0;
  totalSize += types.testFlag(map::AIRPORT) ? airports.size() : 0;
  totalSize += types.testFlag(map::AIRPORT_MSA) ? airportMsa.size() : 0;
  totalSize += types.testFlag(map::WAYPOINT) ? waypoints.size() : 0;
  totalSize += types.testFlag(map::VOR) ? vors.size() : 0;
  totalSize += types.testFlag(map::NDB) ? ndbs.size() : 0;
  totalSize += types.testFlag(map::AIRWAY) ? airways.size() : 0;
  totalSize += types.testFlag(map::RUNWAYEND) ? runwayEnds.size() : 0;
  totalSize += types.testFlag(map::ILS) ? ils.size() : 0;
  totalSize += types.testFlag(map::HOLDING) ? holdings.size() : 0;
  totalSize += types.testFlag(map::AIRSPACE) ? airspaces.size() : 0;
  totalSize += types.testFlag(map::USERPOINTROUTE) ? userpointsRoute.size() : 0;
  totalSize += types.testFlag(map::USERPOINT) ? userpoints.size() : 0;
  totalSize += types.testFlag(map::LOGBOOK) ? logbookEntries.size() : 0;
  totalSize += types.testFlag(map::PROCEDURE_POINT) ? procPoints.size() : 0;
  totalSize += types.testFlag(map::AIRCRAFT) ? userAircraft.isValid() : 0;
  totalSize += types.testFlag(map::AIRCRAFT_AI) ? aiAircraft.size() : 0;
  totalSize += types.testFlag(map::AIRCRAFT_ONLINE) ? onlineAircraft.size() : 0;
  totalSize += types.testFlag(map::MARK_RANGE) ? rangeMarks.size() : 0;
  totalSize += types.testFlag(map::MARK_DISTANCE) ? distanceMarks.size() : 0;
  totalSize += types.testFlag(map::MARK_HOLDING) ? holdingMarks.size() : 0;
  totalSize += types.testFlag(map::MARK_PATTERNS) ? patternMarks.size() : 0;
  totalSize += types.testFlag(map::MARK_MSA) ? msaMarks.size() : 0;

  return totalSize;
}

QDebug operator<<(QDebug out, const map::MapResult& record)
{
  QDebugStateSaver saver(out);

  if(!record.airports.isEmpty())
  {
    out << "Airport[";
    for(const map::MapAirport& obj :  record.airports)
      out << obj.id << obj.ident << obj.routeIndex << ",";
    out << "]";
  }
  if(!record.airportMsa.isEmpty())
  {
    out << "AirportMSA[";
    for(const map::MapAirportMsa& obj :  record.airportMsa)
      out << obj.id << obj.airportIdent << obj.navIdent << ",";
    out << "]";
  }
  if(!record.runwayEnds.isEmpty())
  {
    out << "RunwayEnd[";
    for(const map::MapRunwayEnd& obj :  record.runwayEnds)
      out << obj.name << ",";
    out << "]";
  }
  if(!record.parkings.isEmpty())
  {
    out << "Parking[";
    for(const map::MapParking& obj :  record.parkings)
      out << obj.id << obj.name << obj.number << obj.type << ",";
    out << "]";
  }
  if(!record.waypoints.isEmpty())
  {
    out << "Waypoint[";
    for(const map::MapWaypoint& obj :  record.waypoints)
      out << obj.id << obj.ident << obj.region << obj.routeIndex << ",";
    out << "]";
  }
  if(!record.vors.isEmpty())
  {
    out << "VOR[";
    for(const map::MapVor& obj :  record.vors)
      out << obj.id << obj.ident << obj.region << obj.routeIndex << ",";
    out << "]";
  }
  if(!record.ndbs.isEmpty())
  {
    out << "NDB[";
    for(const map::MapNdb& obj :  record.ndbs)
      out << obj.id << obj.ident << obj.region << obj.routeIndex << ",";
    out << "]";
  }
  if(!record.ils.isEmpty())
  {
    out << "ILS[";
    for(const map::MapIls& obj :  record.ils)
      out << obj.id << obj.ident << ",";
    out << "]";
  }
  if(!record.airways.isEmpty())
  {
    out << "Airway[";
    for(const map::MapAirway& obj :  record.airways)
      out << obj.id << obj.name << ",";
    out << "]";
  }
  if(!record.airspaces.isEmpty())
  {
    out << "Airspace[";
    for(const map::MapAirspace& obj :  record.airspaces)
      out << obj.id << obj.name << "src" << obj.src << ",";
    out << "]";
  }
  if(!record.userpointsRoute.isEmpty())
  {
    out << "UserpointRoute[";
    for(const map::MapUserpointRoute& obj :  record.userpointsRoute)
      out << obj.id << obj.ident << obj.routeIndex << ",";
    out << "]";
  }
  if(!record.userpoints.isEmpty())
  {
    out << "Userpoint[";
    for(const map::MapUserpoint& obj :  record.userpoints)
      out << obj.id << obj.name << ",";
    out << "]";
  }

  if(record.userAircraft.isValid())
  {
    out << "Useraircraft[";
    out << record.userAircraft.id << record.userAircraft.getAircraft().getAirplaneRegistration()
        << record.userAircraft.getAircraft().getAirplaneModel()
        << "shadow" << record.userAircraft.getAircraft().isOnlineShadow() << ",";
    out << "]";
  }

  if(!record.aiAircraft.isEmpty())
  {
    out << "AI aircraft[";
    for(const map::MapAiAircraft& obj :  record.aiAircraft)
      out << obj.id << obj.getAircraft().getAirplaneRegistration()
          << obj.getAircraft().getAirplaneModel()
          << "shadow" << obj.getAircraft().isOnlineShadow() << ",";
    out << "]";
  }

  if(!record.onlineAircraft.isEmpty())
  {
    out << "Online aircraft[";
    for(const map::MapOnlineAircraft& obj :  record.onlineAircraft)
      out << obj.id << obj.getAircraft().getAirplaneRegistration()
          << obj.getAircraft().getAirplaneModel()
          << "shadow" << obj.getAircraft().isOnlineShadow() << ",";
    out << "]";
  }
  return out;
}

MapResultIndex& MapResultIndex::add(const MapResult& resultParam, const MapTypes& types)
{
  if(types.testFlag(AIRPORT))
  {
    result.airports.append(resultParam.airports);
    addAll(result.airports);
  }
  if(types.testFlag(AIRPORT_MSA))
  {
    result.airportMsa.append(resultParam.airportMsa);
    addAll(result.airportMsa);
  }
  if(types.testFlag(RUNWAYEND))
  {
    result.runwayEnds.append(resultParam.runwayEnds);
    addAll(result.runwayEnds);
  }
  if(types.testFlag(PARKING))
  {
    result.parkings.append(resultParam.parkings);
    addAll(result.parkings);
  }
  if(types.testFlag(HELIPAD))
  {
    result.helipads.append(resultParam.helipads);
    addAll(result.helipads);
  }
  if(types.testFlag(WAYPOINT))
  {
    result.waypoints.append(resultParam.waypoints);
    addAll(result.waypoints);
  }
  if(types.testFlag(VOR))
  {
    result.vors.append(resultParam.vors);
    addAll(result.vors);
  }
  if(types.testFlag(NDB))
  {
    result.ndbs.append(resultParam.ndbs);
    addAll(result.ndbs);
  }
  if(types.testFlag(MARKER))
  {
    result.markers.append(resultParam.markers);
    addAll(result.markers);
  }
  if(types.testFlag(ILS))
  {
    result.ils.append(resultParam.ils);
    addAll(result.ils);
  }
  if(types.testFlag(HOLDING))
  {
    result.holdings.append(resultParam.holdings);
    addAll(result.holdings);
  }
  if(types.testFlag(AIRWAY))
  {
    result.airways.append(resultParam.airways);
    addAll(result.airways);
  }
  if(types.testFlag(AIRSPACE))
  {
    result.airspaces.append(resultParam.airspaces);
    addAll(result.airspaces);
  }
  if(types.testFlag(USERPOINTROUTE))
  {
    result.userpointsRoute.append(resultParam.userpointsRoute);
    addAll(result.userpointsRoute);
  }
  if(types.testFlag(USERPOINT))
  {
    result.userpoints.append(resultParam.userpoints);
    addAll(result.userpoints);
  }
  if(types.testFlag(LOGBOOK))
  {
    result.logbookEntries.append(resultParam.logbookEntries);
    addAll(result.logbookEntries);
  }
  if(types.testFlag(PROCEDURE_POINT))
  {
    result.procPoints.append(resultParam.procPoints);
    addAll(result.procPoints);
  }

  // Aircraft ===========
  if(types.testFlag(AIRCRAFT))
  {
    result.userAircraft = resultParam.userAircraft;
    if(result.userAircraft.isValid())
      append(&result.userAircraft);
  }
  if(types.testFlag(AIRCRAFT_AI))
  {
    result.aiAircraft.append(resultParam.aiAircraft);
    addAll(result.aiAircraft);
  }
  if(types.testFlag(AIRCRAFT_ONLINE))
  {
    result.onlineAircraft.append(resultParam.onlineAircraft);
    addAll(result.onlineAircraft);
  }

  // Markers ========================
  if(types.testFlag(MARK_RANGE))
  {
    result.rangeMarks.append(resultParam.rangeMarks);
    addAll(result.rangeMarks);
  }
  if(types.testFlag(MARK_DISTANCE))
  {
    result.distanceMarks.append(resultParam.distanceMarks);
    addAll(result.distanceMarks);
  }
  if(types.testFlag(MARK_HOLDING))
  {
    result.holdingMarks.append(resultParam.holdingMarks);
    addAll(result.holdingMarks);
  }
  if(types.testFlag(MARK_PATTERNS))
  {
    result.patternMarks.append(resultParam.patternMarks);
    addAll(result.patternMarks);
  }
  if(types.testFlag(MARK_MSA))
  {
    result.msaMarks.append(resultParam.msaMarks);
    addAll(result.msaMarks);
  }

  return *this;
}

MapResultIndex& MapResultIndex::addRef(const MapResult& resultParam, const MapTypes& types)
{
  if(types.testFlag(AIRPORT))
    addAll(resultParam.airports);
  if(types.testFlag(AIRPORT_MSA))
    addAll(resultParam.airportMsa);
  if(types.testFlag(RUNWAYEND))
    addAll(resultParam.runwayEnds);
  if(types.testFlag(PARKING))
    addAll(resultParam.parkings);
  if(types.testFlag(HELIPAD))
    addAll(resultParam.helipads);
  if(types.testFlag(WAYPOINT))
    addAll(resultParam.waypoints);
  if(types.testFlag(VOR))
    addAll(resultParam.vors);
  if(types.testFlag(NDB))
    addAll(resultParam.ndbs);
  if(types.testFlag(MARKER))
    addAll(resultParam.markers);
  if(types.testFlag(ILS))
    addAll(resultParam.ils);
  if(types.testFlag(HOLDING))
    addAll(resultParam.holdings);
  if(types.testFlag(AIRWAY))
    addAll(resultParam.airways);
  if(types.testFlag(AIRSPACE))
    addAll(resultParam.airspaces);
  if(types.testFlag(USERPOINTROUTE))
    addAll(resultParam.userpointsRoute);
  if(types.testFlag(USERPOINT))
    addAll(resultParam.userpoints);
  if(types.testFlag(LOGBOOK))
    addAll(resultParam.logbookEntries);
  if(types.testFlag(PROCEDURE_POINT))
    addAll(resultParam.procPoints);

  // Aircraft ===========
  if(types.testFlag(AIRCRAFT) && resultParam.userAircraft.isValid())
    append(&resultParam.userAircraft);

  if(types.testFlag(AIRCRAFT_AI))
    addAll(resultParam.aiAircraft);
  if(types.testFlag(AIRCRAFT_ONLINE))
    addAll(resultParam.onlineAircraft);

  if(types.testFlag(MARK_RANGE))
    addAll(resultParam.rangeMarks);
  if(types.testFlag(MARK_DISTANCE))
    addAll(resultParam.distanceMarks);
  if(types.testFlag(MARK_HOLDING))
    addAll(resultParam.holdingMarks);
  if(types.testFlag(MARK_PATTERNS))
    addAll(resultParam.patternMarks);
  if(types.testFlag(MARK_MSA))
    addAll(resultParam.msaMarks);

  return *this;
}

MapResultIndex& MapResultIndex::sort(const QVector<MapTypes>& types, const MapResultIndex::SortFunction& sortFunc)
{
  if(size() <= 1)
    // Nothing to sort
    return *this;

  if(types.isEmpty() && sortFunc)
    // No types - sort only by callback
    std::sort(begin(), end(), sortFunc);
  else if(!types.isEmpty())
  {
    // Sort by types - create hash map of priorities
    QHash<MapTypes, int> priorities;
    for(int i = 0; i < types.size(); i++)
    {
      if(types.at(i) != map::NONE)
        priorities.insert(types.at(i), i);
    }

    // Sort by type priority and second by callback if set
    std::sort(begin(), end(), [&priorities, &sortFunc](const MapBase *obj1, const MapBase *obj2) -> bool
      {
        int p1 = priorities.value(obj1->getType(), std::numeric_limits<int>::max());
        int p2 = priorities.value(obj2->getType(), std::numeric_limits<int>::max());
        if(p1 == p2 && sortFunc)
          return sortFunc(obj1, obj2);

        return p1 < p2;
      });
  }
  return *this;
}

MapResultIndex& MapResultIndex::sort(const atools::geo::Pos& pos, bool sortNearToFar)
{
  if(isEmpty() || !pos.isValid())
    // Nothing to sort
    return *this;

  std::sort(begin(), end(),
            [pos, sortNearToFar](const MapBase *obj1, const MapBase *obj2) -> bool
    {
      bool res = obj1->getPosition().distanceMeterTo(pos) < obj2->getPosition().distanceMeterTo(pos);
      return sortNearToFar ? res : !res;
    });
  return *this;
}

MapResultIndex& MapResultIndex::remove(const atools::geo::Pos& pos, float maxDistanceNm)
{
  if(isEmpty() || !pos.isValid())
    return *this;

  float maxMeter = atools::geo::nmToMeter(maxDistanceNm);

  auto it = std::remove_if(begin(), end(), [ = ](const MapBase *obj) -> bool
    {
      return obj->position.distanceMeterTo(pos) > maxMeter;
    });

  if(it != end())
    erase(it, end());
  return *this;
}

} // namespace map
