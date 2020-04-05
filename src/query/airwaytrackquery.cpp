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

#include "query/airwaytrackquery.h"

#include "query/airwayquery.h"
#include "mapgui/maplayer.h"
#include "common/maptools.h"

using namespace Marble;
using namespace atools::sql;
using namespace atools::geo;

AirwayTrackQuery::AirwayTrackQuery(AirwayQuery *airwayQueryParam, AirwayQuery *trackQueryParam)
  : airwayQuery(airwayQueryParam), trackQuery(trackQueryParam)
{
}

AirwayTrackQuery::~AirwayTrackQuery()
{
}

void AirwayTrackQuery::getAirwaysForWaypoint(QList<map::MapAirway>& airways, int waypointId)
{
  if(useTracks)
    trackQuery->getAirwaysForWaypoint(airways, waypointId);
  airwayQuery->getAirwaysForWaypoint(airways, waypointId);
  maptools::removeDuplicatesById(airways);
}

void AirwayTrackQuery::getAirwayForWaypoints(map::MapAirway& airway, int waypointId1, int waypointId2,
                                             const QString& airwayName)
{
  airway = map::MapAirway();
  if(useTracks)
    trackQuery->getAirwayForWaypoints(airway, waypointId1, waypointId2, airwayName);

  if(!airway.isValid())
    airwayQuery->getAirwayForWaypoints(airway, waypointId1, waypointId2, airwayName);
}

void AirwayTrackQuery::getWaypointsForAirway(QList<map::MapWaypoint>& waypoints, const QString& airwayName,
                                             const QString& waypointIdent)
{
  if(useTracks)
    trackQuery->getWaypointsForAirway(waypoints, airwayName, waypointIdent);
  airwayQuery->getWaypointsForAirway(waypoints, airwayName, waypointIdent);
  maptools::removeDuplicatesById(waypoints);
}

void AirwayTrackQuery::getWaypointListForAirwayName(QList<map::MapAirwayWaypoint>& waypoints, const QString& airwayName,
                                                    int airwayFragment)
{
  if(useTracks)
    trackQuery->getWaypointListForAirwayName(waypoints, airwayName, airwayFragment);

  if(waypoints.isEmpty())
    airwayQuery->getWaypointListForAirwayName(waypoints, airwayName, airwayFragment);
}

void AirwayTrackQuery::getAirwayFull(QList<map::MapAirway>& airways, const QString& airwayName, int fragment)
{
  atools::geo::Rect boundingDummy;
  getAirwayFull(airways, boundingDummy, airwayName, fragment);
}

void AirwayTrackQuery::getAirwayFull(QList<map::MapAirway>& airways, atools::geo::Rect& bounding,
                                     const QString& airwayName, int fragment)
{
  if(useTracks)
    trackQuery->getAirwayFull(airways, bounding, airwayName, fragment);
  airwayQuery->getAirwayFull(airways, bounding, airwayName, fragment);
  maptools::removeDuplicatesById(airways);
}

map::MapAirway AirwayTrackQuery::getAirwayById(int airwayId)
{
  map::MapAirway airway;
  getAirwayById(airway, airwayId);
  return airway;
}

void AirwayTrackQuery::getAirwayById(map::MapAirway& airway, int airwayId)
{
  airway = map::MapAirway();

  if(useTracks && !airway.isValid())
    trackQuery->getAirwayById(airway, airwayId);

  if(!airway.isValid())
    airwayQuery->getAirwayById(airway, airwayId);
}

void AirwayTrackQuery::getAirwaysByName(QList<map::MapAirway>& airways, const QString& name)
{
  if(useTracks)
    trackQuery->getAirwaysByName(airways, name);
  airwayQuery->getAirwaysByName(airways, name);
  maptools::removeDuplicatesById(airways);
}

void AirwayTrackQuery::getAirwayByNameAndWaypoint(map::MapAirway& airway, const QString& airwayName,
                                                  const QString& waypoint1,
                                                  const QString& waypoint2)
{
  if(airwayName.isEmpty() || waypoint1.isEmpty() || waypoint2.isEmpty())
    return;

  airway = map::MapAirway();
  if(useTracks && !airway.isValid())
    trackQuery->getAirwayByNameAndWaypoint(airway, airwayName, waypoint1, waypoint2);

  if(!airway.isValid())
    airwayQuery->getAirwayByNameAndWaypoint(airway, airwayName, waypoint1, waypoint2);
}

void AirwayTrackQuery::getAirways(QList<map::MapAirway>& airways, const GeoDataLatLonBox& rect,
                                  const MapLayer *mapLayer, bool lazy)
{
  if(mapLayer->isAirway())
  {
    const QList<map::MapAirway> *aw = airwayQuery->getAirways(rect, mapLayer, lazy);
    if(aw != nullptr)
      airways.append(*aw);
  }
}

void AirwayTrackQuery::getTracks(QList<map::MapAirway>& airways, const GeoDataLatLonBox& rect,
                                 const MapLayer *mapLayer, bool lazy)
{
  if(useTracks && mapLayer->isTrack())
  {
    const QList<map::MapAirway> *aw = trackQuery->getAirways(rect, mapLayer, lazy);
    if(aw != nullptr)
      airways.append(*aw);
  }
}

void AirwayTrackQuery::initQueries()
{
  trackQuery->initQueries();
  airwayQuery->initQueries();
}

void AirwayTrackQuery::deInitQueries()
{
  trackQuery->deInitQueries();
  airwayQuery->deInitQueries();
}

void AirwayTrackQuery::postTrackLoad()
{
  trackQuery->clearCache();
}

void AirwayTrackQuery::deleteChildren()
{
  delete trackQuery;
  trackQuery = nullptr;

  delete airwayQuery;
  airwayQuery = nullptr;
}
