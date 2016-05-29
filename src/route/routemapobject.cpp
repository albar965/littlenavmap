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
#include "geo/calculations.h"
#include "common/formatter.h"
#include <QRegularExpression>
#include "atools.h"

#include <fs/pln/flightplan.h>

#include <marble/ElevationModel.h>

using namespace atools::geo;

const QString EMPTY_STR;
const QString UNKNOWN_STR("Unknown");

const QRegularExpression USER_WP_ID("[A-Za-z_]+([0-9]+)");

const QRegularExpression PARKING_TO_NAME_AND_NUM("([A-Za-z_ ]*)([0-9]+)");

RouteMapObject::RouteMapObject(atools::fs::pln::Flightplan *parentFlightplan,
                               const Marble::ElevationModel *elevationModel)
  : flightplan(parentFlightplan), elevation(elevationModel)
{

}

RouteMapObject::~RouteMapObject()
{

}

template<typename TYPE>
TYPE findMapObject(const QList<TYPE>& waypoints, const atools::fs::pln::FlightplanEntry *entry, bool& found)
{
  if(!waypoints.isEmpty())
  {
    TYPE nearest = waypoints.first();
    if(waypoints.size() > 1)
    {
      float distance = 99999999.f;
      for(const TYPE& obj : waypoints)
      {
        float d = entry->getPosition().distanceSimpleTo(obj.position);
        if(d < distance)
        {
          distance = d;
          nearest = obj;
        }
      }
    }
    found = true;
    return nearest;
  }
  found = false;
  return TYPE();
}

void RouteMapObject::loadFromAirport(atools::fs::pln::FlightplanEntry *planEntry,
                                     const maptypes::MapAirport& newAirport,
                                     const RouteMapObject *predRouteMapObj)
{
  entry = planEntry;
  predecessor = predRouteMapObj != nullptr;
  type = maptypes::AIRPORT;
  airport = newAirport;

  updateDistAndCourse(predRouteMapObj);
  valid = true;
}

void RouteMapObject::loadFromDatabaseByEntry(atools::fs::pln::FlightplanEntry *planEntry, MapQuery *query,
                                             const RouteMapObject *predRouteMapObj)
{
  entry = planEntry;

  maptypes::MapSearchResult res;

  predecessor = predRouteMapObj != nullptr;

  QString region = entry->getIcaoRegion();

  if(region == "KK") // Invalid route finder stuff
    region.clear();

  bool found;
  switch(entry->getWaypointType())
  {
    case atools::fs::pln::entry::UNKNOWN:
      break;
    case atools::fs::pln::entry::AIRPORT:
      query->getMapObjectByIdent(res, maptypes::AIRPORT, entry->getIcaoIdent());
      if(!res.airports.isEmpty())
      {
        type = maptypes::AIRPORT;
        airport = res.airports.first();
        valid = true;

        if(!flightplan->getDepartureParkingName().isEmpty() && predRouteMapObj == nullptr)
        {
          // Resolve parking if first airport
          QRegularExpressionMatch match = PARKING_TO_NAME_AND_NUM.match(flightplan->getDepartureParkingName());
          QString name = match.captured(1).trimmed().toUpper().replace(" ", "_");
          int number = QString(match.captured(2)).toInt();
          QList<maptypes::MapParking> parkings;
          query->getParkingByNameAndNumber(parkings, airport.id, name, number);

          if(parkings.isEmpty())
          {
            qWarning() << "Found no parking spots";
            flightplan->setDepartureParkingName(QString());
          }
          else
          {
            if(parkings.size() > 1)
              qWarning() << "Found multiple parking spots";

            parking = parkings.first();
            flightplan->setDepartureParkingName(maptypes::parkingNameForFlightplan(parking));
          }
        }
      }
      break;
    case atools::fs::pln::entry::INTERSECTION:
      {
        query->getMapObjectByIdent(res, maptypes::WAYPOINT, entry->getIcaoIdent(), region);
        const maptypes::MapWaypoint& obj = findMapObject(res.waypoints, planEntry, found);
        if(found)
        {
          type = maptypes::WAYPOINT;
          waypoint = obj;
          valid = waypoint.position.distanceMeterTo(entry->getPosition()) < 10000.f;
          if(valid)
          {
            entry->setIcaoRegion(waypoint.region);
            entry->setIcaoIdent(waypoint.ident);
            entry->setPosition(waypoint.position);
          }
        }
        break;
      }
    case atools::fs::pln::entry::VOR:
      {
        query->getMapObjectByIdent(res, maptypes::VOR, entry->getIcaoIdent(), region);
        const maptypes::MapVor& obj = findMapObject(res.vors, planEntry, found);
        if(found)
        {
          type = maptypes::VOR;
          vor = obj;
          valid = vor.position.distanceMeterTo(entry->getPosition()) < 10000.f;
          if(valid)
          {
            entry->setIcaoRegion(vor.region);
            entry->setIcaoIdent(vor.ident);
            entry->setPosition(vor.position);
          }
        }
        break;
      }
    case atools::fs::pln::entry::NDB:
      {
        query->getMapObjectByIdent(res, maptypes::NDB, entry->getIcaoIdent(), region);
        const maptypes::MapNdb& obj = findMapObject(res.ndbs, planEntry, found);
        if(found)
        {
          type = maptypes::NDB;
          ndb = obj;
          valid = ndb.position.distanceMeterTo(entry->getPosition()) < 10000.f;
          if(valid)
          {
            entry->setIcaoRegion(ndb.region);
            entry->setIcaoIdent(ndb.ident);
            entry->setPosition(ndb.position);
          }
        }
        break;
      }
    case atools::fs::pln::entry::USER:
      valid = true;
      type = maptypes::USER;
      userpointNum = QString(USER_WP_ID.match(entry->getWaypointId()).captured(1)).toInt();
      entry->setIcaoIdent(QString());
      entry->setWaypointId("WP" + QString::number(userpointNum));
      break;
  }

  if(!valid)
    type = maptypes::INVALID;

  updateDistAndCourse(predRouteMapObj);
}

void RouteMapObject::update(const RouteMapObject *predRouteMapObj)
{
  updateDistAndCourse(predRouteMapObj);
}

void RouteMapObject::updateParking(const maptypes::MapParking& departureParking)
{
  parking = departureParking;
}

void RouteMapObject::updateDistAndCourse(const RouteMapObject *predRouteMapObj)
{
  if(predRouteMapObj != nullptr)
  {
    const Pos& prevPos = predRouteMapObj->getPosition();

    distanceTo = meterToNm(getPosition().distanceMeterTo(prevPos));
    distanceToRhumb = meterToNm(getPosition().distanceMeterToRhumb(prevPos));

    float magvar = getMagvar();
    if(magvar == 0.f)
      magvar = predRouteMapObj->getMagvar();

    courseTo = normalizeCourse(predRouteMapObj->getPosition().angleDegTo(getPosition()) + magvar);
    courseRhumbTo = normalizeCourse(predRouteMapObj->getPosition().angleDegToRhumb(getPosition()) + magvar);

    // QList<Marble::GeoDataCoordinates> elev =
    // elevation->heightProfile(prevPos.getLonX(), prevPos.getLatY(),
    // getPosition().getLonX(), getPosition().getLatY());

    // groundAltitude = 0.f;
    // for(const Marble::GeoDataCoordinates& e : elev)
    // {
    // float altFeet = static_cast<float>(atools::geo::meterToFeet(e.altitude()));
    // if(altFeet > groundAltitude)
    // groundAltitude = altFeet;
    // }
  }
  else
  {
    // groundAltitude = static_cast<float>(atools::geo::meterToFeet(
    // elevation->height(getPosition().getLonX(), getPosition().getLatY())));
    predecessor = false;
    distanceTo = 0.f;
    distanceToRhumb = 0.f;
    courseTo = 0.f;
    courseRhumbTo = 0.f;
  }
}

int RouteMapObject::getId() const
{
  if(type == maptypes::INVALID)
    return -1;

  switch(entry->getWaypointType())
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

float RouteMapObject::getMagvar() const
{
  if(type == maptypes::INVALID)
    return -1;

  switch(entry->getWaypointType())
  {
    case atools::fs::pln::entry::UNKNOWN:
      return 0.f;

    case atools::fs::pln::entry::AIRPORT:
      return airport.magvar;

    case atools::fs::pln::entry::INTERSECTION:
      return waypoint.magvar;

    case atools::fs::pln::entry::VOR:
      return vor.magvar;

    case atools::fs::pln::entry::NDB:
      return ndb.magvar;

    case atools::fs::pln::entry::USER:
      return 0.f;
  }
  return 0.f;
}

int RouteMapObject::getRange() const
{
  if(type == maptypes::INVALID)
    return -1;

  switch(entry->getWaypointType())
  {
    case atools::fs::pln::entry::UNKNOWN:
    case atools::fs::pln::entry::AIRPORT:
    case atools::fs::pln::entry::INTERSECTION:
    case atools::fs::pln::entry::USER:
      return -1;

    case atools::fs::pln::entry::VOR:
      return vor.range;

    case atools::fs::pln::entry::NDB:
      return ndb.range;
  }
  return -1;
}

bool RouteMapObject::isUser()
{
  return entry->getWaypointType() == atools::fs::pln::entry::USER;
}

const atools::geo::Pos& RouteMapObject::getPosition() const
{
  if(type == maptypes::INVALID)
  {
    if(entry->getPosition().isValid())
      return entry->getPosition();
    else
      return atools::geo::EMPTY_POS;
  }

  switch(entry->getWaypointType())
  {
    case atools::fs::pln::entry::UNKNOWN:
      return atools::geo::EMPTY_POS;

    case atools::fs::pln::entry::AIRPORT:
      return airport.position;

    case atools::fs::pln::entry::INTERSECTION:
      return waypoint.position;

    case atools::fs::pln::entry::VOR:
      return vor.position;

    case atools::fs::pln::entry::NDB:
      return ndb.position;

    case atools::fs::pln::entry::USER:
      return entry->getPosition();
  }
  return atools::geo::EMPTY_POS;
}

QString RouteMapObject::getIdent() const
{
  if(type == maptypes::INVALID)
    return entry->getIcaoIdent();

  switch(entry->getWaypointType())
  {
    case atools::fs::pln::entry::UNKNOWN:
      return UNKNOWN_STR;

    case atools::fs::pln::entry::USER:
      return "User " + QString::number(userpointNum);

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
  if(type == maptypes::INVALID)
    return entry->getIcaoRegion();

  switch(entry->getWaypointType())
  {
    case atools::fs::pln::entry::UNKNOWN:
    case atools::fs::pln::entry::AIRPORT:
    case atools::fs::pln::entry::USER:
      return entry->getIcaoRegion();

    case atools::fs::pln::entry::INTERSECTION:
      return waypoint.region;

    case atools::fs::pln::entry::VOR:
      return vor.region;

    case atools::fs::pln::entry::NDB:
      return ndb.region;
  }
  return EMPTY_STR;
}

QString RouteMapObject::getName() const
{
  if(type == maptypes::INVALID)
    return EMPTY_STR;

  switch(entry->getWaypointType())
  {
    case atools::fs::pln::entry::UNKNOWN:
    case atools::fs::pln::entry::USER:
    case atools::fs::pln::entry::INTERSECTION:
      return EMPTY_STR;

    case atools::fs::pln::entry::AIRPORT:
      return airport.name;

    case atools::fs::pln::entry::VOR:
      return atools::capString(vor.name);

    case atools::fs::pln::entry::NDB:
      return atools::capString(ndb.name);
  }
  return EMPTY_STR;
}

int RouteMapObject::getFrequency() const
{
  if(type == maptypes::INVALID)
    return 0;

  switch(entry->getWaypointType())
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
