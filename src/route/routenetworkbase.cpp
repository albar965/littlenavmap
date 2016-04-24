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

#include "routenetworkbase.h"

#include <sql/sqldatabase.h>
#include <sql/sqlquery.h>
#include <sql/sqlutil.h>

#include "geo/calculations.h"
#include <geo/pos.h>
#include <geo/rect.h>

#include <QElapsedTimer>

using atools::sql::SqlDatabase;
using atools::sql::SqlQuery;
using atools::geo::Pos;
using atools::geo::Rect;

using namespace nw;

RouteNetworkBase::RouteNetworkBase(atools::sql::SqlDatabase *sqlDb)
  : db(sqlDb)
{
  nodes.reserve(60000);
  destinationNodePredecessors.reserve(1000);
}

RouteNetworkBase::~RouteNetworkBase()
{
}

int RouteNetworkBase::getNumberOfNodesDatabase()
{
  if(numNodesDb == -1)
    numNodesDb = atools::sql::SqlUtil(db).rowCount("route_node_radio");
  return numNodesDb;
}

int RouteNetworkBase::getNumberOfNodesCache() const
{
  return nodes.size();
}

void RouteNetworkBase::setMode(nw::Modes routeMode)
{
  mode = routeMode;
}

void RouteNetworkBase::clear()
{
  startNodeRect = atools::geo::EMPTY_RECT;
  destinationNodeRect = atools::geo::EMPTY_RECT;
  startPos = atools::geo::EMPTY_POS;
  destinationPos = atools::geo::EMPTY_POS;
  nodes.clear();
  destinationNodePredecessors.clear();
  numNodesDb = -1;
}

void RouteNetworkBase::getNeighbours(const nw::Node& from, QVector<nw::Node>& neighbours,
                                     QVector<int>& distancesMeter)
{
  neighbours.reserve(1000);
  distancesMeter.reserve(1000);
  for(const Edge& e : from.edges)
  {
    neighbours.append(fetchNode(e.toNodeId));
    distancesMeter.append(e.distanceMeter);
  }
}

void RouteNetworkBase::addStartAndDestinationNodes(const atools::geo::Pos& from, const atools::geo::Pos& to)
{
  qDebug() << "adding start and  destination to network";

  if(startPos == from && destinationPos == to)
    return;

  if(destinationPos != to)
  {
    // Remove all references to destination node
    cleanDestNodeEdges();

    destinationPos = to;

    // Add destination first so it can be added to start successors
    destinationNodeRect = Rect(to, atools::geo::nmToMeter(200));
    fetchNode(to.getLonX(), to.getLatY(), false, DESTINATION_NODE_ID);

    for(int id : nodes.keys())
      addDestNodeEdges(nodes[id]);
  }

  if(startPos != from)
  {
    startPos = from;
    startNodeRect = Rect(from, atools::geo::nmToMeter(200));
    fetchNode(from.getLonX(), from.getLatY(), true, START_NODE_ID);
  }
  qDebug() << "adding start and  destination to network done";
}

nw::Node RouteNetworkBase::getStartNode() const
{
  return nodes.value(START_NODE_ID);
}

nw::Node RouteNetworkBase::getDestinationNode() const
{
  return nodes.value(DESTINATION_NODE_ID);
}

void RouteNetworkBase::fillNode(const QSqlRecord& rec, nw::Node& node)
{
  node.id = rec.value("node_id").toInt();
  node.type = static_cast<nw::NodeType>(rec.value("type").toInt());
  node.range = rec.value("range").toInt();
  node.pos.setLonX(rec.value("lonx").toFloat());
  node.pos.setLatY(rec.value("laty").toFloat());
}

bool RouteNetworkBase::checkType(nw::NodeType type)
{
  switch(type)
  {
    case nw::WAYPOINT_VICTOR:
    case nw::WAYPOINT_JET:
    case nw::WAYPOINT_BOTH:
      return mode & ROUTE_JET || mode & ROUTE_VICTOR;

    case nw::VOR:
      return mode & ROUTE_VOR;

    case nw::VORDME:
      return mode & ROUTE_VORDME;

    case nw::DME:
      return mode & ROUTE_DME;

    case nw::NDB:
      return mode & ROUTE_NDB;

    case nw::START:
    case nw::DESTINATION:
      return true;

    case nw::NONE:
      break;
  }

  return false;
}

void RouteNetworkBase::bindCoordRect(const atools::geo::Rect& rect, atools::sql::SqlQuery *query)
{
  query->bindValue(":leftx", rect.getWest());
  query->bindValue(":rightx", rect.getEast());
  query->bindValue(":bottomy", rect.getSouth());
  query->bindValue(":topy", rect.getNorth());
}

void RouteNetworkBase::cleanDestNodeEdges()
{
  // Remove all references to destination node
  for(int i : destinationNodePredecessors)
  {
    if(nodes.contains(i))
    {
      if(nodes[i].edges.removeAll(Edge(DESTINATION_NODE_ID, 0)) == 0)
        qDebug() << "No node destination predecessors deleted for node" << nodes.value(i).id;
    }
    else
      qDebug() << "No node destination found" << nodes.value(i).id;
  }
  destinationNodePredecessors.clear();
}

void RouteNetworkBase::addDestNodeEdges(nw::Node& node)
{
  if(destinationNodeRect.contains(node.pos) && !destinationNodePredecessors.contains(node.id) &&
     node.id != DESTINATION_NODE_ID)
  {
    // Near destination - add as successor
    node.edges.append(Edge(DESTINATION_NODE_ID,
                           static_cast<int>(node.pos.distanceMeterTo(destinationPos) + 0.5f)));
    // Remember for later cleanup
    destinationNodePredecessors.insert(node.id);
  }
}
