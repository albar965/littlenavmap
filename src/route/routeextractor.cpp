/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel alex@littlenavmap.org
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

#include "route/routeextractor.h"
#include "routing/routefinder.h"

RouteExtractor::RouteExtractor(const atools::routing::RouteFinder *routeFinderParam)
  : routeFinder(routeFinderParam)
{

}

void RouteExtractor::extractRoute(QVector<RouteEntry>& route, float& distanceMeter) const
{
  distanceMeter = 0.f;

  QVector<atools::routing::RouteLeg> routeLegs;
  routeFinder->extractLegs(routeLegs, distanceMeter);

  for(const atools::routing::RouteLeg& leg : routeLegs)
  {
    RouteEntry entry;
    entry.ref.id = leg.navId;
    entry.airwayId = leg.airwayId;
    entry.ref.type = toMapObjectType(leg.type);
    route.append(entry);
  }
}

/* Convert internal network type to MapObjectTypes for extract route */
map::MapObjectTypes RouteExtractor::toMapObjectType(atools::routing::NodeType type) const
{
  switch(type)
  {
    case atools::routing::WAYPOINT_JET:
    case atools::routing::WAYPOINT_VICTOR:
    case atools::routing::WAYPOINT_BOTH:
    case atools::routing::WAYPOINT_NAMED:
    case atools::routing::WAYPOINT_UNNAMED:
      return map::WAYPOINT;

    case atools::routing::VOR:
    case atools::routing::VORDME:
    case atools::routing::DME:
      return map::VOR;

    case atools::routing::NDB:
      return map::NDB;

    case atools::routing::DEPARTURE:
    case atools::routing::DESTINATION:
      return map::USERPOINTROUTE;

    case atools::routing::NONE:
      break;
  }
  return map::NONE;
}
