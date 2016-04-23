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

class RouteNetwork;

class RouteFinder
{
public:
  RouteFinder(RouteNetwork *routeNetwork);
  virtual ~RouteFinder();

  void calculateRoute(const atools::geo::Pos& from, const atools::geo::Pos& to,
                      QVector<maptypes::MapObjectRef>& route);

private:
  const float MAX_COST = std::numeric_limits<float>::max();

  RouteNetwork *network;
  void expandNode(const nw::Node& node, const nw::Node& destNode);

  float cost(const nw::Node& node, const nw::Node& successor, int distanceMeter);
  float costEstimate(const nw::Node& currentNode, const nw::Node& successor);

  Heap<nw::Node> openNodesHeap;
  QSet<int> closedNodes;
  QHash<int, float> nodeCosts;
  QHash<int, int> nodePredecessor;
  maptypes::MapObjectTypes toMapObjectType(nw::NodeType type);

};

#endif // ROUTEFINDER_H
