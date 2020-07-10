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

#include "common/proctypes.h"
#include "geo/calculations.h"
#include "options/optiondata.h"

namespace map {

void MapSearchResult::clear(const MapObjectTypes& types)
{
  if(types.testFlag(map::AIRPORT))
  {
    airports.clear();
    airportIds.clear();
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

  if(types.testFlag(map::AIRCRAFT_AI))
    aiAircraft.clear();

  if(types.testFlag(map::AIRCRAFT))
    userAircraft.clear();

  if(types.testFlag(map::AIRCRAFT_ONLINE))
  {
    onlineAircraft.clear();
    onlineAircraftIds.clear();
  }
}

template<typename T>
void MapSearchResult::clearAllButFirst(QList<T>& list)
{
  while(list.size() > 1)
    list.removeLast();
}

void MapSearchResult::clearAllButFirst(const MapObjectTypes& types)
{
  if(types.testFlag(map::AIRPORT))
  {
    clearAllButFirst(airports);
    airportIds.clear();
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

  if(types.testFlag(map::AIRCRAFT_AI))
    clearAllButFirst(aiAircraft);

  // if(types.testFlag(map::AIRCRAFT))
  // userAircraft.clear();

  if(types.testFlag(map::AIRCRAFT_ONLINE))
  {
    clearAllButFirst(onlineAircraft);
    onlineAircraftIds.clear();
  }
}

void MapSearchResult::moveOnlineAirspacesToFront()
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

MapSearchResult MapSearchResult::moveOnlineAirspacesToFront() const
{
  MapSearchResult retval(*this);
  retval.moveOnlineAirspacesToFront();
  return retval;
}

bool MapSearchResult::hasSimNavUserAirspaces() const
{
  for(const map::MapAirspace& airspace : airspaces)
  {
    if(!airspace.isOnline())
      return true;
  }
  return false;
}

bool MapSearchResult::hasOnlineAirspaces() const
{
  for(const map::MapAirspace& airspace : airspaces)
  {
    if(airspace.isOnline())
      return true;
  }
  return false;
}

const map::MapAirspace *MapSearchResult::firstSimNavUserAirspace() const
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

const map::MapAirspace *MapSearchResult::firstOnlineAirspace() const
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

int MapSearchResult::numSimNavUserAirspaces() const
{
  int num = 0;
  for(const map::MapAirspace& airspace : airspaces)
    num += !airspace.isOnline();
  return num;
}

int MapSearchResult::numOnlineAirspaces() const
{
  int num = 0;
  for(const map::MapAirspace& airspace : airspaces)
    num += airspace.isOnline();
  return num;
}

QList<map::MapAirspace> MapSearchResult::getSimNavUserAirspaces() const
{
  QList<map::MapAirspace> retval;
  for(const map::MapAirspace& airspace : airspaces)
  {
    if(!airspace.isOnline())
      retval.append(airspace);
  }
  return retval;
}

QList<map::MapAirspace> MapSearchResult::getOnlineAirspaces() const
{
  QList<map::MapAirspace> retval;
  for(const map::MapAirspace& airspace : airspaces)
  {
    if(airspace.isOnline())
      retval.append(airspace);
  }
  return retval;
}

void MapSearchResult::clearNavdataAirspaces()
{
  QList<map::MapAirspace>::iterator it = std::remove_if(airspaces.begin(), airspaces.end(),
                                                        [](const map::MapAirspace& airspace) -> bool
    {
      return !airspace.isOnline();
    });
  if(it != airspaces.end())
    airspaces.erase(it, airspaces.end());
}

void MapSearchResult::clearOnlineAirspaces()
{
  QList<map::MapAirspace>::iterator it = std::remove_if(airspaces.begin(), airspaces.end(),
                                                        [](const map::MapAirspace& airspace) -> bool
    {
      return airspace.isOnline();
    });
  if(it != airspaces.end())
    airspaces.erase(it, airspaces.end());
}

const atools::geo::Pos& MapSearchResult::getPosition(const std::initializer_list<MapObjectTypes>& types) const
{
  for(const MapObjectTypes& type : types)
  {
    if(!isEmpty(type))
    {
      if(type == map::AIRPORT)
        return airports.first().getPosition();
      else if(type == map::WAYPOINT)
        return waypoints.first().getPosition();
      else if(type == map::VOR)
        return vors.first().getPosition();
      else if(type == map::NDB)
        return ndbs.first().getPosition();
      else if(type == map::AIRWAY)
        return airways.first().getPosition();
      else if(type == map::RUNWAYEND)
        return runwayEnds.first().getPosition();
      else if(type == map::ILS)
        return ils.first().getPosition();
      else if(type == map::AIRSPACE)
        return airspaces.first().getPosition();
      else if(type == map::USERPOINTROUTE)
        return userpointsRoute.first().getPosition();
      else if(type == map::USERPOINT)
        return userpoints.first().getPosition();
      else if(type == map::LOGBOOK)
        return logbookEntries.first().getPosition();
      else if(type == map::AIRCRAFT)
        return userAircraft.getPosition();
      else if(type == map::AIRCRAFT_AI)
        return aiAircraft.first().getPosition();
      else if(type == map::AIRCRAFT_ONLINE)
        return onlineAircraft.first().getPosition();
    }
  }
  return atools::geo::EMPTY_POS;
}

QString MapSearchResult::getIdent(const std::initializer_list<MapObjectTypes>& types) const
{
  for(const MapObjectTypes& type : types)
  {
    if(!isEmpty(type))
    {
      if(type == map::AIRPORT)
        return airports.first().ident;
      else if(type == map::WAYPOINT)
        return waypoints.first().ident;
      else if(type == map::VOR)
        return vors.first().ident;
      else if(type == map::NDB)
        return ndbs.first().ident;
      else if(type == map::AIRWAY)
        return airways.first().name;
      else if(type == map::RUNWAYEND)
        return runwayEnds.first().name;
      else if(type == map::ILS)
        return ils.first().ident;
      else if(type == map::AIRSPACE)
        return airspaces.first().name;
      else if(type == map::USERPOINTROUTE)
        return userpointsRoute.first().ident;
      else if(type == map::USERPOINT)
        return userpoints.first().ident;
      else if(type == map::LOGBOOK)
        return logbookEntries.first().departureIdent;
      else if(type == map::AIRCRAFT)
        return userAircraft.getAircraft().getAirplaneRegistration();
      else if(type == map::AIRCRAFT_AI)
        return aiAircraft.first().getAircraft().getAirplaneRegistration();
      else if(type == map::AIRCRAFT_ONLINE)
        return onlineAircraft.first().getAircraft().getAirplaneRegistration();
    }
  }
  return QString();
}

bool MapSearchResult::getIdAndType(int& id, MapObjectTypes& type,
                                   const std::initializer_list<MapObjectTypes>& types) const
{
  id = -1;
  type = NONE;

  for(const MapObjectTypes& t : types)
  {
    if(!isEmpty(t))
    {
      if(t == map::AIRPORT)
      {
        id = airports.first().getId();
        type = t;
        break;
      }
      else if(t == map::WAYPOINT)
      {
        id = waypoints.first().getId();
        type = t;
        break;
      }
      else if(t == map::VOR)
      {
        id = vors.first().getId();
        type = t;
        break;
      }
      else if(t == map::NDB)
      {
        id = ndbs.first().getId();
        type = t;
        break;
      }
      else if(t == map::AIRWAY)
      {
        id = airways.first().getId();
        type = t;
        break;
      }
      else if(t == map::RUNWAYEND)
      {
        id = runwayEnds.first().getId();
        type = t;
        break;
      }
      else if(t == map::ILS)
      {
        id = ils.first().getId();
        type = t;
        break;
      }
      else if(t == map::AIRSPACE)
      {
        id = airspaces.first().getId();
        type = t;
        break;
      }
      else if(t == map::USERPOINTROUTE)
      {
        id = userpointsRoute.first().getId();
        type = t;
        break;
      }
      else if(t == map::USERPOINT)
      {
        id = userpoints.first().getId();
        type = t;
        break;
      }
      else if(t == map::LOGBOOK)
      {
        id = logbookEntries.first().getId();
        type = t;
        break;
      }
      else if(t == map::AIRCRAFT)
      {
        id = userAircraft.getId();
        type = t;
        break;
      }
      else if(t == map::AIRCRAFT_AI)
      {
        id = aiAircraft.first().getId();
        type = t;
        break;
      }
      else if(t == map::AIRCRAFT_ONLINE)
      {
        id = onlineAircraft.first().getId();
        type = t;
        break;
      }
    }
  }
  return id != -1;
}

MapSearchResult& MapSearchResult::fromMapBase(const MapBase *base)
{
  if(base != nullptr)
  {
    if(base->getType().testFlag(map::AIRPORT))
      airports.append(base->asObj<map::MapAirport>());
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
    else if(base->getType().testFlag(map::AIRSPACE))
      airspaces.append(base->asObj<map::MapAirspace>());
    else if(base->getType().testFlag(map::USERPOINTROUTE))
      userpointsRoute.append(base->asObj<map::MapUserpointRoute>());
    else if(base->getType().testFlag(map::USERPOINT))
      userpoints.append(base->asObj<map::MapUserpoint>());
    else if(base->getType().testFlag(map::LOGBOOK))
      logbookEntries.append(base->asObj<map::MapLogbookEntry>());
    else if(base->getType().testFlag(map::AIRCRAFT))
      userAircraft = base->asObj<map::MapUserAircraft>();
    else if(base->getType().testFlag(map::AIRCRAFT_AI))
      aiAircraft.append(base->asObj<map::MapAiAircraft>());
    else if(base->getType().testFlag(map::AIRCRAFT_ONLINE))
      onlineAircraft.append(base->asObj<map::MapOnlineAircraft>());
  }
  return *this;
}

int MapSearchResult::size(const MapObjectTypes& types) const
{
  int totalSize = 0;
  totalSize += types.testFlag(map::AIRPORT) ? airports.size() : 0;
  totalSize += types.testFlag(map::WAYPOINT) ? waypoints.size() : 0;
  totalSize += types.testFlag(map::VOR) ? vors.size() : 0;
  totalSize += types.testFlag(map::NDB) ? ndbs.size() : 0;
  totalSize += types.testFlag(map::AIRWAY) ? airways.size() : 0;
  totalSize += types.testFlag(map::RUNWAYEND) ? runwayEnds.size() : 0;
  totalSize += types.testFlag(map::ILS) ? ils.size() : 0;
  totalSize += types.testFlag(map::AIRSPACE) ? airspaces.size() : 0;
  totalSize += types.testFlag(map::USERPOINTROUTE) ? userpointsRoute.size() : 0;
  totalSize += types.testFlag(map::USERPOINT) ? userpoints.size() : 0;
  totalSize += types.testFlag(map::LOGBOOK) ? logbookEntries.size() : 0;
  totalSize += types.testFlag(map::AIRCRAFT) ? userAircraft.isValid() : 0;
  totalSize += types.testFlag(map::AIRCRAFT_AI) ? aiAircraft.size() : 0;
  totalSize += types.testFlag(map::AIRCRAFT_ONLINE) ? onlineAircraft.size() : 0;
  return totalSize;
}

QDebug operator<<(QDebug out, const map::MapSearchResult& record)
{
  QDebugStateSaver saver(out);

  if(!record.airports.isEmpty())
  {
    out << "Airport[";
    for(const map::MapAirport& obj :  record.airports)
      out << obj.id << obj.ident << obj.routeIndex << ",";
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

MapSearchResultIndex& MapSearchResultIndex::add(const MapSearchResult& resultParam, const MapObjectTypes& types)
{
  if(types.testFlag(AIRPORT))
  {
    result.airports.append(resultParam.airports);
    addAll(result.airports);
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
  return *this;
}

MapSearchResultIndex& MapSearchResultIndex::addRef(const MapSearchResult& resultParam, const MapObjectTypes& types)
{
  if(types.testFlag(AIRPORT))
    addAll(resultParam.airports);
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

  // Aircraft ===========
  if(types.testFlag(AIRCRAFT) && resultParam.userAircraft.isValid())
    append(&resultParam.userAircraft);

  if(types.testFlag(AIRCRAFT_AI))
    addAll(resultParam.aiAircraft);
  if(types.testFlag(AIRCRAFT_ONLINE))
    addAll(resultParam.onlineAircraft);
  return *this;
}

MapSearchResultIndex& MapSearchResultIndex::sort(const QVector<MapObjectTypes>& types,
                                                 const MapSearchResultIndex::SortFunction& sortFunc)
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
    QHash<MapObjectTypes, int> priorities;
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

MapSearchResultIndex& MapSearchResultIndex::sort(const atools::geo::Pos& pos, bool sortNearToFar)
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

MapSearchResultIndex& MapSearchResultIndex::remove(const atools::geo::Pos& pos, float maxDistanceNm)
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
