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

#include "common/mapresult.h"

#include "geo/calculations.h"

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

  if(types.testFlag(map::RUNWAY))
    runways.clear();

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
void MapResult::removeInvalid(QList<TYPE>& list, QSet<int> *ids)
{
  if(ids != nullptr)
  {
    // Remove from id list
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

template<typename TYPE>
void MapResult::removeNoRouteIndex(QList<TYPE>& list, QSet<int> *ids)
{
  if(ids != nullptr)
  {
    // Remove from id list
    for(const TYPE& type : list)
    {
      if(type.routeIndex == -1)
        ids->remove(type.id);
    }
  }

  list.erase(std::remove_if(list.begin(), list.end(), [](const TYPE& type) -> bool {
      return type.routeIndex == -1;
    }), list.end());
}

template<class TYPE>
void MapResult::setRouteIndex(QList<TYPE>& list, const MapTypes& types, const MapTypes& type, int routeIndex)
{
  if(types.testFlag(type) && !isEmpty(type))
  {
    for(TYPE& t : list)
      t.routeIndex = routeIndex;
  }
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

  if(types.testFlag(map::RUNWAY))
    clearAllButFirst(runways);

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

MapResult& MapResult::clearRouteIndex(const MapTypes& types)
{
  setRouteIndex(airports, types, map::AIRPORT);
  setRouteIndex(waypoints, types, map::WAYPOINT);
  setRouteIndex(vors, types, map::VOR);
  setRouteIndex(ndbs, types, map::NDB);
  setRouteIndex(userpointsRoute, types, map::USERPOINTROUTE);
  setRouteIndex(procPoints, types, map::PROCEDURE_POINT);

  return *this;
}

void MapResult::moveOnlineAirspacesToFront()
{
  QList<MapAirspace> list;
  for(const MapAirspace& a: qAsConst(airspaces))
  {
    if(a.isOnline())
      list.append(a);
  }
  for(const MapAirspace& a: qAsConst(airspaces))
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
  for(const map::MapAirspace& airspace : qAsConst(airspaces))
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
    std::find_if(airspaces.constBegin(), airspaces.constEnd(), [](const map::MapAirspace& a) -> bool
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
    std::find_if(airspaces.constBegin(), airspaces.constEnd(), [](const map::MapAirspace& a) -> bool
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

const QList<map::MapAirspace> MapResult::getSimNavUserAirspaces() const
{
  QList<map::MapAirspace> retval;
  for(const map::MapAirspace& airspace : airspaces)
  {
    if(!airspace.isOnline())
      retval.append(airspace);
  }
  return retval;
}

const QList<map::MapAirspace> MapResult::getOnlineAirspaces() const
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
  removeInvalid(runways);
  removeInvalid(towers);
  removeInvalid(parkings);
  removeInvalid(helipads);
  removeInvalid(waypoints, &waypointIds);
  removeInvalid(vors, &vorIds);
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

void MapResult::removeNoRouteIndex()
{
  removeNoRouteIndex(airports, &airportIds);
  removeNoRouteIndex(waypoints, &waypointIds);
  removeNoRouteIndex(vors, &vorIds);
  removeNoRouteIndex(ndbs, &ndbIds);
  removeNoRouteIndex(userpointsRoute);
  removeNoRouteIndex(procPoints);
}

void MapResult::clearNavdataAirspaces()
{
  airspaces.erase(std::remove_if(airspaces.begin(), airspaces.end(), [](const map::MapAirspace& airspace) -> bool
    {
      return !airspace.isOnline();
    }), airspaces.end());
}

void MapResult::clearOnlineAirspaces()
{
  airspaces.erase(std::remove_if(airspaces.begin(), airspaces.end(), [](const map::MapAirspace& airspace) -> bool
    {
      return airspace.isOnline();
    }), airspaces.end());
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
      else if(type == map::RUNWAY)
        return runways.constFirst().getPosition();
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
      else if(type == map::RUNWAY)
        return runways.constFirst().primaryName + "/" + runways.constFirst().secondaryName;
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
    id = getId(t);

    if(id != -1)
    {
      type = t;
      break;
    }
  }
  return id != -1;
}

map::MapRef MapResult::getRef(const std::initializer_list<MapTypes>& types) const
{
  int id = -1;
  map::MapTypes type = map::NONE;
  if(getIdAndType(id, type, types))
    return map::MapRef(id, type);
  else
    return map::MapRef();
}

int MapResult::getId(const map::MapTypes& type) const
{
  if(!isEmpty(type))
  {
    if(type == map::AIRPORT)
      return airports.constFirst().getId();
    else if(type == map::AIRPORT_MSA)
      return airportMsa.constFirst().getId();
    else if(type == map::WAYPOINT)
      return waypoints.constFirst().getId();
    else if(type == map::VOR)
      return vors.constFirst().getId();
    else if(type == map::NDB)
      return ndbs.constFirst().getId();
    else if(type == map::AIRWAY)
      return airways.constFirst().getId();
    else if(type == map::RUNWAYEND)
      return runwayEnds.constFirst().getId();
    else if(type == map::RUNWAY)
      return runways.constFirst().getId();
    else if(type == map::ILS)
      return ils.constFirst().getId();
    else if(type == map::HOLDING)
      return holdings.constFirst().getId();
    else if(type == map::AIRSPACE)
      return airspaces.constFirst().getId();
    else if(type == map::USERPOINTROUTE)
      return userpointsRoute.constFirst().getId();
    else if(type == map::USERPOINT)
      return userpoints.constFirst().getId();
    else if(type == map::LOGBOOK)
      return logbookEntries.constFirst().getId();
    else if(type == map::PROCEDURE_POINT)
      return procPoints.constFirst().getId();
    else if(type == map::AIRCRAFT)
      return userAircraft.getId();
    else if(type == map::AIRCRAFT_AI)
      return aiAircraft.constFirst().getId();
    else if(type == map::AIRCRAFT_ONLINE)
      return onlineAircraft.constFirst().getId();
    else if(type == map::MARK_RANGE)
      return rangeMarks.constFirst().getId();
    else if(type == map::MARK_DISTANCE)
      return distanceMarks.constFirst().getId();
    else if(type == map::MARK_HOLDING)
      return holdingMarks.constFirst().getId();
    else if(type == map::MARK_PATTERNS)
      return patternMarks.constFirst().getId();
    else if(type == map::MARK_MSA)
      return msaMarks.constFirst().getId();
  }
  return -1;
}

int MapResult::getRouteIndex(const map::MapTypes& type) const
{
  if(!isEmpty(type))
  {
    if(type == map::AIRPORT)
      return airports.constFirst().routeIndex;
    else if(type == map::WAYPOINT)
      return waypoints.constFirst().routeIndex;
    else if(type == map::VOR)
      return vors.constFirst().routeIndex;
    else if(type == map::NDB)
      return ndbs.constFirst().routeIndex;
    else if(type == map::USERPOINTROUTE)
      return userpointsRoute.constFirst().routeIndex;
    else if(type == map::PROCEDURE_POINT)
      return procPoints.constFirst().routeIndex;
  }
  return -1;
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
    else if(base->getType().testFlag(map::RUNWAY))
      runways.append(base->asObj<map::MapRunway>());
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
  totalSize += types.testFlag(map::RUNWAY) ? runways.size() : 0;
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
  if(!record.runways.isEmpty())
  {
    out << "Runway[";
    for(const map::MapRunway& obj :  record.runways)
      out << obj.primaryName << "/" << obj.secondaryName << ",";
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
  addToIndexRangeIf(resultParam.airports, result.airports, types);
  addToIndexRangeIf(resultParam.airportMsa, result.airportMsa, types);
  addToIndexRangeIf(resultParam.runwayEnds, result.runwayEnds, types);
  addToIndexRangeIf(resultParam.runways, result.runways, types);
  addToIndexRangeIf(resultParam.parkings, result.parkings, types);
  addToIndexRangeIf(resultParam.helipads, result.helipads, types);
  addToIndexRangeIf(resultParam.waypoints, result.waypoints, types);
  addToIndexRangeIf(resultParam.vors, result.vors, types);
  addToIndexRangeIf(resultParam.ndbs, result.ndbs, types);
  addToIndexRangeIf(resultParam.markers, result.markers, types);
  addToIndexRangeIf(resultParam.ils, result.ils, types);
  addToIndexRangeIf(resultParam.holdings, result.holdings, types);
  addToIndexRangeIf(resultParam.airways, result.airways, types);
  addToIndexRangeIf(resultParam.airspaces, result.airspaces, types);
  addToIndexRangeIf(resultParam.userpointsRoute, result.userpointsRoute, types);
  addToIndexRangeIf(resultParam.userpoints, result.userpoints, types);
  addToIndexRangeIf(resultParam.logbookEntries, result.logbookEntries, types);
  addToIndexRangeIf(resultParam.procPoints, result.procPoints, types);

  // Aircraft ===========
  if(types.testFlag(AIRCRAFT) && resultParam.userAircraft.isValid())
  {
    result.userAircraft = resultParam.userAircraft;
    append(&result.userAircraft);
  }
  addToIndexRangeIf(resultParam.aiAircraft, result.aiAircraft, types);
  addToIndexRangeIf(resultParam.onlineAircraft, result.onlineAircraft, types);

  // Markers ========================
  addToIndexRangeIf(resultParam.rangeMarks, result.rangeMarks, types);
  addToIndexRangeIf(resultParam.distanceMarks, result.distanceMarks, types);
  addToIndexRangeIf(resultParam.holdingMarks, result.holdingMarks, types);
  addToIndexRangeIf(resultParam.patternMarks, result.patternMarks, types);
  addToIndexRangeIf(resultParam.msaMarks, result.msaMarks, types);

  return *this;
}

MapResultIndex& MapResultIndex::addRef(const MapResult& resultParam, const MapTypes& types)
{
  addToIndexIf(resultParam.airports, types);
  addToIndexIf(resultParam.airportMsa, types);
  addToIndexIf(resultParam.runwayEnds, types);
  addToIndexIf(resultParam.runways, types);
  addToIndexIf(resultParam.parkings, types);
  addToIndexIf(resultParam.helipads, types);
  addToIndexIf(resultParam.waypoints, types);
  addToIndexIf(resultParam.vors, types);
  addToIndexIf(resultParam.ndbs, types);
  addToIndexIf(resultParam.markers, types);
  addToIndexIf(resultParam.ils, types);
  addToIndexIf(resultParam.holdings, types);
  addToIndexIf(resultParam.airways, types);
  addToIndexIf(resultParam.airspaces, types);
  addToIndexIf(resultParam.userpointsRoute, types);
  addToIndexIf(resultParam.userpoints, types);
  addToIndexIf(resultParam.logbookEntries, types);
  addToIndexIf(resultParam.procPoints, types);

  // Aircraft ===========
  if(types.testFlag(AIRCRAFT) && resultParam.userAircraft.isValid())
    append(&resultParam.userAircraft);
  addToIndexIf(resultParam.aiAircraft, types);
  addToIndexIf(resultParam.onlineAircraft, types);

  // User features ===========
  addToIndexIf(resultParam.rangeMarks, types);
  addToIndexIf(resultParam.distanceMarks, types);
  addToIndexIf(resultParam.holdingMarks, types);
  addToIndexIf(resultParam.patternMarks, types);
  addToIndexIf(resultParam.msaMarks, types);

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
  if(size() <= 1 || !pos.isValid())
    // Nothing to sort
    return *this;

  std::sort(begin(), end(), [pos, sortNearToFar](const MapBase *obj1, const MapBase *obj2) -> bool
    {
      float dist1, dist2;
      atools::geo::LineDistance lineDist;

      const map::MapRunway *rw = obj1->asPtr<map::MapRunway>();
      if(rw != nullptr)
      {
        // Distance to runway line
        pos.distanceMeterToLine(rw->primaryPosition, rw->secondaryPosition, lineDist);
        dist1 = std::abs(lineDist.distance);
      }
      else
        // Distance to center position
        dist1 = obj1->getPosition().distanceMeterTo(pos);

      rw = obj2->asPtr<map::MapRunway>();
      if(rw != nullptr)
      {
        // Distance to runway line
        pos.distanceMeterToLine(rw->primaryPosition, rw->secondaryPosition, lineDist);
        dist2 = std::abs(lineDist.distance);
      }
      else
        // Distance to center position
        dist2 = obj2->getPosition().distanceMeterTo(pos);

      return sortNearToFar ? dist1<dist2 : dist1> dist2;
    });

  return *this;
}

MapResultIndex& MapResultIndex::remove(const atools::geo::Pos& pos, float maxDistanceNm)
{
  if(isEmpty() || !pos.isValid())
    return *this;

  float maxMeter = atools::geo::nmToMeter(maxDistanceNm);

  auto it = std::remove_if(begin(), end(), [maxMeter, &pos](const MapBase *obj) -> bool
    {
      return obj->position.distanceMeterTo(pos) > maxMeter;
    });

  if(it != end())
    erase(it, end());
  return *this;
}

} // namespace map
