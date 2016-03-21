/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#include "routemapobject.h"
#include "mapgui/mapquery.h"

static QString EMPTY_STR;

RouteMapObject::RouteMapObject()
{

}

template<typename TYPE>
const TYPE *findMapObject(const QList<const TYPE *> waypoints, const atools::fs::pln::FlightplanEntry& entry)
{
  if(!waypoints.isEmpty())
  {
    const TYPE *nearest = waypoints.first();
    if(waypoints.size() > 1)
    {
      float distance = 99999999.f;
      for(const TYPE *obj : waypoints)
      {
        float d = entry.getPosition().distanceSimpleTo(obj->position);
        if(d < distance)
        {
          distance = d;
          nearest = obj;
        }
      }
    }
    return nearest;
  }
  return nullptr;
}

RouteMapObject::RouteMapObject(const atools::fs::pln::FlightplanEntry& planEntry, MapQuery *query)
  : entry(planEntry)
{
  maptypes::MapSearchResult res;

  QString region = entry.getIcaoRegion();

  if(region == "KK") // Invalid route finder stuff
    region.clear();

  switch(entry.getWaypointType())
  {
    case atools::fs::pln::entry::UNKNOWN:
      break;
    case atools::fs::pln::entry::AIRPORT:
      query->getMapObject(res, maptypes::AIRPORT, entry.getIcaoIdent());
      if(!res.airports.isEmpty())
      {
        type = maptypes::AIRPORT;
        airport = *res.airports.first();
        valid = true;
      }
      break;
    case atools::fs::pln::entry::INTERSECTION:
      {
        query->getMapObject(res, maptypes::WAYPOINT, entry.getIcaoIdent(), region);
        const maptypes::MapWaypoint *obj = findMapObject(res.waypoints, planEntry);
        if(obj != nullptr)
        {
          type = maptypes::WAYPOINT;
          waypoint = *obj;
          valid = waypoint.position.distanceMeterTo(entry.getPosition()) < 10000.f;
        }
        break;
      }
    case atools::fs::pln::entry::VOR:
      {
        query->getMapObject(res, maptypes::VOR, entry.getIcaoIdent(), region);
        const maptypes::MapVor *obj = findMapObject(res.vors, planEntry);
        if(obj != nullptr)
        {
          type = maptypes::VOR;
          vor = *obj;
          valid = vor.position.distanceMeterTo(entry.getPosition()) < 10000.f;
        }
        break;
      }
    case atools::fs::pln::entry::NDB:
      {
        query->getMapObject(res, maptypes::NDB, entry.getIcaoIdent(), region);
        const maptypes::MapNdb *obj = findMapObject(res.ndbs, planEntry);
        if(obj != nullptr)
        {
          type = maptypes::NDB;
          ndb = *obj;
          valid = ndb.position.distanceMeterTo(entry.getPosition()) < 10000.f;
        }
        break;
      }
    case atools::fs::pln::entry::USER:
      valid = true;
      break;
  }

  res.deleteAllObjects();
}

RouteMapObject::~RouteMapObject()
{

}

int RouteMapObject::getId() const
{
  if(!valid)
    return -1;

  switch(entry.getWaypointType())
  {
    case atools::fs::pln::entry::UNKNOWN:
      return -1;

    case atools::fs::pln::entry::AIRPORT:
      return airport.id;

    case atools::fs::pln::entry::INTERSECTION:
      return waypoint.id;

    case atools::fs::pln::entry::VOR:
      return vor.id;

    case atools::fs::pln::entry::NDB:
      return ndb.id;

    case atools::fs::pln::entry::USER:
      return -1;
  }
  return -1;
}

const atools::geo::Pos& RouteMapObject::getPosition() const
{
  if(!valid)
  {
    if(entry.getPosition().isValid())
      return entry.getPosition();
    else
      return atools::geo::Pos::EMPTY_POS;
  }

  switch(entry.getWaypointType())
  {
    case atools::fs::pln::entry::UNKNOWN:
      return atools::geo::Pos::EMPTY_POS;

    case atools::fs::pln::entry::AIRPORT:
      return airport.position;

    case atools::fs::pln::entry::INTERSECTION:
      return waypoint.position;

    case atools::fs::pln::entry::VOR:
      return vor.position;

    case atools::fs::pln::entry::NDB:
      return ndb.position;

    case atools::fs::pln::entry::USER:
      return entry.getPosition();
  }
  return atools::geo::Pos::EMPTY_POS;
}

const QString& RouteMapObject::getIdent() const
{
  if(!valid)
    return EMPTY_STR;

  switch(entry.getWaypointType())
  {
    case atools::fs::pln::entry::UNKNOWN:
    case atools::fs::pln::entry::USER:
      return EMPTY_STR;

    case atools::fs::pln::entry::AIRPORT:
      return airport.ident;

    case atools::fs::pln::entry::INTERSECTION:
      return waypoint.ident;

    case atools::fs::pln::entry::VOR:
      return vor.ident;

    case atools::fs::pln::entry::NDB:
      return ndb.ident;
  }
  return EMPTY_STR;
}

const QString& RouteMapObject::getRegion() const
{
  if(!valid)
    return EMPTY_STR;

  switch(entry.getWaypointType())
  {
    case atools::fs::pln::entry::UNKNOWN:
    case atools::fs::pln::entry::AIRPORT:
    case atools::fs::pln::entry::USER:
      return EMPTY_STR;

    case atools::fs::pln::entry::INTERSECTION:
      return waypoint.region;

    case atools::fs::pln::entry::VOR:
      return vor.region;

    case atools::fs::pln::entry::NDB:
      return ndb.region;
  }
  return EMPTY_STR;
}

const QString& RouteMapObject::getName() const
{
  if(!valid)
    return EMPTY_STR;

  switch(entry.getWaypointType())
  {
    case atools::fs::pln::entry::UNKNOWN:
    case atools::fs::pln::entry::USER:
    case atools::fs::pln::entry::INTERSECTION:
      return EMPTY_STR;

    case atools::fs::pln::entry::AIRPORT:
      return airport.name;

    case atools::fs::pln::entry::VOR:
      return vor.name;

    case atools::fs::pln::entry::NDB:
      return ndb.name;
  }
  return EMPTY_STR;
}

int RouteMapObject::getFrequency() const
{
  if(!valid)
    return 0;

  switch(entry.getWaypointType())
  {
    case atools::fs::pln::entry::UNKNOWN:
    case atools::fs::pln::entry::USER:
    case atools::fs::pln::entry::INTERSECTION:
    case atools::fs::pln::entry::AIRPORT:
      return 0;

    case atools::fs::pln::entry::VOR:
      return vor.frequency;

    case atools::fs::pln::entry::NDB:
      return ndb.frequency;
  }
  return 0;
}
