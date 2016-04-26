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

#ifndef ROUTEFINDER_H
#define ROUTEFINDER_H

#include "common/maptypes.h"
#include "heap.h"
#include "route/routenetwork.h"

namespace rf {
struct RouteEntry
{
  maptypes::MapObjectRef ref;
  int airwayId;
};

}

class RouteFinder
{
public:
  RouteFinder(RouteNetwork *routeNetwork);
  virtual ~RouteFinder();

  void calculateRoute(const atools::geo::Pos& from, const atools::geo::Pos& to,
                      QVector<rf::RouteEntry>& route, int flownAltitude);

private:
  // Force algortihm to use close waypoints near start and destination
  const float COST_FACTOR_FORCE_CLOSE_NODES = 1.5;
  // Increase costs to force reception of at least one radio navaid along the route
  const float COST_FACTOR_UNREACHABLE_RADIONAV = 2.f;
  // Try to avoid NDBs
  const float COST_FACTOR_NDB = 1.5f;
  // Try to avoid VORs (no DME)
  const float COST_FACTOR_VOR = 1.2f;
  // Avoid DMEs
  const float COST_FACTOR_DME = 4.f;

  int altitude = 0;

  RouteNetwork *network;
  void expandNode(const nw::Node& node, const nw::Node& destNode);

  float cost(const nw::Node& node, const nw::Node& successorNode, int distanceMeter);
  float costEstimate(const nw::Node& currentNode, const nw::Node& successor);

  Heap<nw::Node> openNodesHeap;
  QSet<int> closedNodes;
  QHash<int, float> nodeCosts;
  QHash<int, int> nodePredecessor;
  QHash<int, int> nodeAirwayId;
  maptypes::MapObjectTypes toMapObjectType(nw::Type type);

  QVector<int> airwayAltitudes;

  // For RouteNetwork::getNeighbours
  QVector<nw::Node> successorNodes;
  QVector<nw::Edge> successorEdges;
};

#endif // ROUTEFINDER_H
