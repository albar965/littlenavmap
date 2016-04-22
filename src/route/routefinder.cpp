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

#include "routefinder.h"
#include "routenetwork.h"

const float MAX_COST_FACTOR = 1000000.f;
const float COST_FACTOR_UNREACHABLE_RADIONAV = 1000000.f;
const float COST_FACTOR_UNREACHABLE_RADIONAV_SINGLE = 100.f;
const float COST_FACTOR_NDB = 1.2f;

using nw::Node;
using atools::geo::Pos;

RouteFinder::RouteFinder(RouteNetwork *routeNetwork)
  : network(routeNetwork)
{

}

RouteFinder::~RouteFinder()
{

}

void RouteFinder::calculateRoute(const atools::geo::Pos& from, const atools::geo::Pos& to,
                                 QVector<maptypes::MapObjectRef>& route)
{
  network->addStartAndDestinationNodes(from, to);
  Node startNode = network->getStartNode();
  Node destNode = network->getDestinationNode();

  if(startNode.edges.isEmpty())
    return;

  openNodesHeap.push(startNode, costEstimate(startNode, destNode));
  nodeCosts[startNode.id] = 0.f;

  Node currentNode;
  bool found = false;
  while(!openNodesHeap.isEmpty())
  {
    // Contains known nodes
    openNodesHeap.pop(currentNode);

    if(currentNode.id == destNode.id)
    {
      found = true;
      break;
    }

    // Contains nodes with known shortest path
    closedNodes.insert(currentNode.id);

    // Work on successors
    expandNode(currentNode, destNode);
  }

  if(found)
  {
    // Build route
    int predId = currentNode.id;
    while(predId != -1)
    {
      int navId;
      nw::NodeType type;
      network->getNavIdAndTypeForNode(predId, navId, type);

      if(type != nw::START && type != nw::DESTINATION)
        route.append({navId, toMapObjectType(type)});

      predId = nodePredecessor.value(predId, -1);
    }
  }
}

void RouteFinder::expandNode(const nw::Node& currentNode, const nw::Node& destNode)
{
  QList<Node> successors;

  network->getNeighbours(currentNode, successors);

  for(const Node& successor : successors)
  {
    if(closedNodes.contains(successor.id))
      continue;

    // newNodeCosts / g = all costs from start to current node
    float newNodeCosts = nodeCosts.value(currentNode.id) + cost(currentNode, successor);

    if(newNodeCosts >= nodeCosts.value(successor.id) && openNodesHeap.contains(successor))
      continue;

    nodePredecessor[successor.id] = currentNode.id;
    nodeCosts[successor.id] = newNodeCosts;

    // Costs from start to successor + estimate to destination = sort order in heap
    float f = newNodeCosts + costEstimate(successor, destNode);

    if(openNodesHeap.contains(successor))
      openNodesHeap.change(successor, f);
    else
      openNodesHeap.push(successor, f);
  }
}

float RouteFinder::cost(const nw::Node& currentNode, const nw::Node& successor)
{
  float dist = currentNode.pos.distanceMeterTo(successor.pos);

  if(currentNode.range + successor.range < dist)
    dist *= COST_FACTOR_UNREACHABLE_RADIONAV;
  else if(currentNode.range < dist)
    dist *= COST_FACTOR_UNREACHABLE_RADIONAV_SINGLE;

  if(successor.type == nw::NDB)
    dist *= COST_FACTOR_NDB;
  return dist;
}

float RouteFinder::costEstimate(const nw::Node& currentNode, const nw::Node& successor)
{
  // Use largest factor to allow underestimate
  return currentNode.pos.distanceMeterTo(successor.pos) * MAX_COST_FACTOR;
}

maptypes::MapObjectTypes RouteFinder::toMapObjectType(nw::NodeType type)
{
  switch(type)
  {
    case nw::VOR:
    case nw::VORDME:
    case nw::DME:
      return maptypes::VOR;

    case nw::NDB:
      return maptypes::NDB;

    case nw::START:
    case nw::DESTINATION:
      return maptypes::USER;

    case nw::NONE:
      break;
  }
  return maptypes::NONE;
}
