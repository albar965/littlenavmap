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

#include "routenetwork.h"

#include <sql/sqldatabase.h>
#include <sql/sqlquery.h>

#include "geo/calculations.h"
#include <geo/pos.h>
#include <geo/rect.h>

#include <QElapsedTimer>

using atools::sql::SqlDatabase;
using atools::sql::SqlQuery;
using atools::geo::Pos;
using atools::geo::Rect;

int nw::qHash(const nw::Node& node)
{
  return node.id;
}

bool nw::Node::operator==(const nw::Node& other) const
{
  return id == other.id && type == other.type;
}

bool nw::Node::operator!=(const nw::Node& other) const
{
  return !operator==(other);
}

using namespace nw;

RouteNetwork::RouteNetwork(atools::sql::SqlDatabase *sqlDb)
  : db(sqlDb)
{
  initQueries();
}

RouteNetwork::~RouteNetwork()
{
  deInitQueries();
}

void RouteNetwork::setMode(nw::Modes routeMode)
{
  mode = routeMode;
}

void RouteNetwork::initQueries()
{

}

void RouteNetwork::deInitQueries()
{

}

void RouteNetwork::clear()
{
  startNodeRect = Rect();
  destinationNodeRect = Rect();
  nodes.clear();
}

void RouteNetwork::getNeighbours(const nw::Node& from, QList<nw::Node>& neighbours)
{
  QElapsedTimer t;
  t.start();

  for(int id : from.edges)
    neighbours.append(fetchNode(id));

  // qDebug() << "Time for getNeighbours query" << t.elapsed() << " ms";
}

nw::Node RouteNetwork::getNodeByNavId(int id, nw::NodeType type)
{
  SqlQuery nodeQuery(db);
  nodeQuery.prepare("select node_id from route_node_radio where nav_id = :id and type = :type");
  nodeQuery.bindValue(":id", id);
  nodeQuery.bindValue(":type", type);
  nodeQuery.exec();

  if(nodeQuery.next())
    return fetchNode(nodeQuery.value("node_id").toInt());

  return Node();
}

void RouteNetwork::getNavIdAndTypeForNode(int nodeId, int& navId, nw::NodeType& type)
{
  if(nodeId == START_NODE_ID)
  {
    type = START;
    navId = -1;
  }
  else if(nodeId == DESTINATION_NODE_ID)
  {
    type = DESTINATION;
    navId = -1;
  }
  else
  {
    SqlQuery nodeQuery(db);
    nodeQuery.prepare("select nav_id, type from route_node_radio where node_id = :id");
    nodeQuery.bindValue(":id", nodeId);
    nodeQuery.exec();

    if(nodeQuery.next())
    {
      navId = nodeQuery.value("nav_id").toInt();
      type = static_cast<nw::NodeType>(nodeQuery.value("type").toInt());
    }
    else
    {
      navId = -1;
      type = nw::NONE;
    }
  }
}

nw::Node RouteNetwork::getNodeById(int id)
{
  return fetchNode(id);
}

void RouteNetwork::addStartAndDestinationNodes(const atools::geo::Pos& from, const atools::geo::Pos& to)
{
  // Add destination first so it can be added to start successors
  destinationNodeRect = Rect(to, atools::geo::nmToMeter(200));
  fetchNode(to.getLonX(), to.getLatY(), false, DESTINATION_NODE_ID);

  startNodeRect = Rect(from, atools::geo::nmToMeter(200));
  fetchNode(from.getLonX(), from.getLatY(), true, START_NODE_ID);
}

nw::Node RouteNetwork::getStartNode()
{
  return nodes.value(START_NODE_ID);
}

nw::Node RouteNetwork::getDestinationNode()
{
  return nodes.value(DESTINATION_NODE_ID);
}

nw::Node RouteNetwork::fetchNode(float lonx, float laty, bool loadSuccessors, int id)
{
  Node node;
  node.id = id;
  node.range = -1;

  if(id == START_NODE_ID)
    node.type = START;
  else if(id == DESTINATION_NODE_ID)
    node.type = DESTINATION;
  else
    node.type = NONE;

  node.lonx = lonx;
  node.laty = laty;

  if(loadSuccessors)
  {
    if(destinationNodeRect.contains(Pos(node.lonx, node.laty)))
      // Near destination - add as successor
      node.edges.append(DESTINATION_NODE_ID);

    SqlQuery nearestStmt(db);
    nearestStmt.prepare(
      "select node_id, type from route_node_radio "
      "where lonx between :leftx and :rightx and laty between :bottomy and :topy");

    int maxRange = atools::geo::nmToMeter(200);
    Rect queryRect(Pos(lonx, laty), maxRange);

    for(const Rect& rect : queryRect.splitAtAntiMeridian())
    {
      bindCoordRect(rect, nearestStmt);
      nearestStmt.exec();
      while(nearestStmt.next())
      {
        int nodeId = nearestStmt.value("node_id").toInt();
        if(!node.edges.contains(nodeId) &&
           checkType(static_cast<nw::NodeType>(nearestStmt.value("type").toInt())))
          node.edges.append(nodeId);
      }
    }
  }

  nodes.insert(node.id, node);

  return node;
}

nw::Node RouteNetwork::fetchNode(int id)
{
  if(nodes.contains(id))
    return nodes.value(id);

  SqlQuery nodeQuery(db);
  nodeQuery.prepare("select type, range, lonx, laty from route_node_radio where node_id = :id");
  nodeQuery.bindValue(":id", id);
  nodeQuery.exec();

  if(nodeQuery.next())
  {
    nw::Node node;
    fillNode(nodeQuery.record(), node);
    node.id = id;

    if(destinationNodeRect.contains(Pos(node.lonx, node.laty)))
      // Near destination - add as successor
      node.edges.append(DESTINATION_NODE_ID);

    SqlQuery edgeQueryTo(db);
    edgeQueryTo.prepare("select to_node_id, to_node_type from route_edge_radio "
                        "where from_node_id = :id");

    edgeQueryTo.bindValue(":id", id);
    edgeQueryTo.exec();

    while(edgeQueryTo.next())
    {
      int to = edgeQueryTo.value("to_node_id").toInt();
      if(to != id && checkType(static_cast<nw::NodeType>(edgeQueryTo.value("to_node_type").toInt())) &&
         !node.edges.contains(to))
        node.edges.append(to);
    }

    SqlQuery edgeQueryFrom(db);
    edgeQueryFrom.prepare("select from_node_id, from_node_type from route_edge_radio "
                          "where to_node_id = :id");

    edgeQueryFrom.bindValue(":id", id);
    edgeQueryFrom.exec();

    while(edgeQueryFrom.next())
    {
      int from = edgeQueryFrom.value("from_node_id").toInt();
      if(from != id && checkType(static_cast<nw::NodeType>(edgeQueryFrom.value("from_node_type").toInt())) &&
         !node.edges.contains(from))
        node.edges.append(from);
    }

    nodes.insert(node.id, node);
    return node;
  }
  return Node();
}

void RouteNetwork::fillNode(const QSqlRecord& rec, nw::Node& node)
{
  node.id = rec.value("node_id").toInt();
  node.type = static_cast<nw::NodeType>(rec.value("type").toInt());
  node.range = rec.value("range").toInt();
  node.lonx = rec.value("lonx").toFloat();
  node.laty = rec.value("laty").toFloat();
}

bool RouteNetwork::checkType(nw::NodeType type)
{
  switch(type)
  {
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

void RouteNetwork::bindCoordRect(const atools::geo::Rect& rect, atools::sql::SqlQuery& query)
{
  query.bindValue(":leftx", rect.getWest());
  query.bindValue(":rightx", rect.getEast());
  query.bindValue(":bottomy", rect.getSouth());
  query.bindValue(":topy", rect.getNorth());
}
