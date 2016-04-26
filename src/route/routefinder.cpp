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
#include "geo/calculations.h"

using nw::Node;
using nw::Edge;
using atools::geo::Pos;

RouteFinder::RouteFinder(RouteNetwork *routeNetwork)
  : network(routeNetwork), openNodesHeap(5000)
{
  closedNodes.reserve(10000);
  nodeCosts.reserve(10000);
  nodePredecessor.reserve(10000);
  nodeAirwayId.reserve(10000);

  successorNodes.reserve(500);
  successorEdges.reserve(500);
}

RouteFinder::~RouteFinder()
{

}

void RouteFinder::calculateRoute(const atools::geo::Pos& from, const atools::geo::Pos& to,
                                 QVector<rf::RouteEntry>& route, int flownAltitude)
{
  altitude = flownAltitude;
  network->addStartAndDestinationNodes(from, to);
  Node startNode = network->getStartNode();
  Node destNode = network->getDestinationNode();

  int numNodesTotal = network->getNumberOfNodesDatabase();

  if(startNode.edges.isEmpty())
    return;

  openNodesHeap.push(startNode, 0.f);
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

    if(closedNodes.size() > numNodesTotal / 2)
      // If we read too much nodes routing will fail
      break;

    // Work on successors
    expandNode(currentNode, destNode);
  }

  qDebug() << "heap size" << openNodesHeap.size();
  qDebug() << "close nodes size" << closedNodes.size();

  if(found)
  {
    route.reserve(500);
    // Build route
    int predId = currentNode.id;
    while(predId != -1)
    {
      int navId;
      nw::Type type;
      network->getNavIdAndTypeForNode(predId, navId, type);

      if(type != nw::START && type != nw::DESTINATION)
      {
        rf::RouteEntry entry;
        entry.ref = {navId, toMapObjectType(type)};
        entry.airwayId = nodeAirwayId.value(predId, -1);
        route.prepend(entry);
      }

      predId = nodePredecessor.value(predId, -1);
    }
  }
  else
    qDebug() << "No route found";

  qDebug() << "num nodes database" << network->getNumberOfNodesDatabase()
           << "num nodes cache" << network->getNumberOfNodesCache();
}

void RouteFinder::expandNode(const nw::Node& currentNode, const nw::Node& destNode)
{
  successorNodes.clear();
  successorEdges.clear();
  network->getNeighbours(currentNode, successorNodes, successorEdges);

  for(int i = 0; i < successorNodes.size(); i++)
  {
    const Node& successor = successorNodes.at(i);

    if(closedNodes.contains(successor.id))
      continue;

    const Edge& edge = successorEdges.at(i);

    if(altitude > 0 && edge.minAltFt > 0 && altitude < edge.minAltFt)
      continue;

    int distanceMeter = edge.distanceMeter;

    if(distanceMeter == 0)
      // No distance given for airways - have to calculate this here
      distanceMeter = static_cast<int>(currentNode.pos.distanceMeterTo(successor.pos) + 0.5f);

    float successorEdgeCosts = cost(currentNode, successor, distanceMeter);
    float successorNodeCosts = nodeCosts.value(currentNode.id) + successorEdgeCosts;

    if(successorNodeCosts >= nodeCosts.value(successor.id) && openNodesHeap.contains(successor))
      // New path is not cheaper
      continue;

    nodeAirwayId[successor.id] = successorEdges.at(i).airwayId;
    nodePredecessor[successor.id] = currentNode.id;
    nodeCosts[successor.id] = successorNodeCosts;

    // Costs from start to successor + estimate to destination = sort order in heap
    float totalCost = successorNodeCosts + costEstimate(successor, destNode);

    if(openNodesHeap.contains(successor))
      openNodesHeap.change(successor, totalCost);
    else
      openNodesHeap.push(successor, totalCost);
  }
}

float RouteFinder::cost(const nw::Node& currentNode, const nw::Node& successorNode, int distanceMeter)
{
  float costs = distanceMeter;

  if(currentNode.type == nw::START || successorNode.type == nw::DESTINATION)
    costs *= COST_FACTOR_FORCE_CLOSE_NODES;

  if((currentNode.range != 0 || successorNode.range != 0) &&
     currentNode.range + successorNode.range < distanceMeter)
    costs *= COST_FACTOR_UNREACHABLE_RADIONAV;

  if(successorNode.type == nw::DME)
    costs *= COST_FACTOR_DME;
  else if(successorNode.type == nw::VOR)
    costs *= COST_FACTOR_VOR;
  else if(successorNode.type == nw::NDB)
    costs *= COST_FACTOR_NDB;

  return costs;
}

float RouteFinder::costEstimate(const nw::Node& currentNode, const nw::Node& successor)
{
  // return 0.f;
  // Use largest factor to allow underestimate
  return currentNode.pos.distanceMeterTo(successor.pos);
}

maptypes::MapObjectTypes RouteFinder::toMapObjectType(nw::Type type)
{
  switch(type)
  {
    case nw::WAYPOINT_JET:
    case nw::WAYPOINT_VICTOR:
    case nw::WAYPOINT_BOTH:
      return maptypes::WAYPOINT;

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
