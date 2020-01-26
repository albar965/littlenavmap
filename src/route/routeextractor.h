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

#ifndef LNM_ROUTEEXTRACTOR_H
#define LNM_ROUTEEXTRACTOR_H

#include "common/maptypes.h"
#include "routing/routenetwork.h"

namespace atools {
namespace routing {
class RouteFinder;
}
}

/* Used when fetching the route points after calculation. Adds airway id to node */
struct RouteEntry
{
  map::MapObjectRef ref;
  int airwayId;
};

class RouteExtractor
{
public:
  RouteExtractor(atools::routing::RouteFinder *routeFinderParam);

  /* Extract route points and total distance if calculateRoute was successfull.
   * From and to are not included in the list */
  void extractRoute(QVector<RouteEntry>& route, float& distanceMeter);

private:
  map::MapObjectTypes toMapObjectType(atools::routing::NodeType type);

  atools::routing::RouteFinder *routeFinder;
};

#endif // LNM_ROUTEEXTRACTOR_H
