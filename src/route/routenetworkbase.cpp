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

RouteNetworkBase::RouteNetworkBase(atools::sql::SqlDatabase *sqlDb, const QString& nodeTableName,
                                   const QString& edgeTableName, const QStringList& nodeExtraColumns,
                                   const QStringList& edgeExtraColumns)
  : db(sqlDb), nodeTable(nodeTableName), edgeTable(edgeTableName), nodeExtraCols(nodeExtraColumns),
    edgeExtraCols(edgeExtraColumns)
{
  nodes.reserve(60000);
  destinationNodePredecessors.reserve(1000);
  initQueries();
}

RouteNetworkBase::~RouteNetworkBase()
{
  deInitQueries();
}

int RouteNetworkBase::getNumberOfNodesDatabase()
{
  if(numNodesDb == -1)
    numNodesDb = atools::sql::SqlUtil(db).rowCount(nodeTable);
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
                                     QVector<Edge>& edges)
{
  for(const Edge& e : from.edges)
  {
    // TODO single query fetch is expensive
    neighbours.append(fetchNode(e.toNodeId));
    edges.append(e);
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

void RouteNetworkBase::cleanDestNodeEdges()
{
  // Remove all references to destination node
  for(int i : destinationNodePredecessors)
  {
    if(nodes.contains(i))
    {
      if(nodes[i].edges.removeAll(Edge(DESTINATION_NODE_ID)) == 0)
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

nw::Node RouteNetworkBase::getNodeByNavId(int id, nw::Type type)
{
  nodeByNavIdQuery->bindValue(":id", id);
  nodeByNavIdQuery->bindValue(":type", type);
  nodeByNavIdQuery->exec();

  if(nodeByNavIdQuery->next())
    return fetchNode(nodeByNavIdQuery->value("node_id").toInt());

  return Node();
}

void RouteNetworkBase::getNavIdAndTypeForNode(int nodeId, int& navId, nw::Type& type)
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
    nodeNavIdAndTypeQuery->bindValue(":id", nodeId);
    nodeNavIdAndTypeQuery->exec();

    if(nodeNavIdAndTypeQuery->next())
    {
      navId = nodeNavIdAndTypeQuery->value("nav_id").toInt();
      type = static_cast<nw::Type>(nodeNavIdAndTypeQuery->value("type").toInt());
    }
    else
    {
      navId = -1;
      type = nw::NONE;
    }
  }
}

nw::Node RouteNetworkBase::fetchNode(float lonx, float laty, bool loadSuccessors, int id)
{
  const int MAX_RANGE = atools::geo::nmToMeter(200);

  nodes.remove(id);

  Node node;
  node.id = id;
  node.range = 0;

  if(id == START_NODE_ID)
    node.type = START;
  else if(id == DESTINATION_NODE_ID)
    node.type = DESTINATION;
  else
    node.type = NONE;

  node.pos.setLonX(lonx);
  node.pos.setLatY(laty);

  if(loadSuccessors)
  {
    Rect queryRect(Pos(lonx, laty), MAX_RANGE);
    QSet<Edge> tempEdges;
    tempEdges.reserve(1000);

    for(const Rect& rect : queryRect.splitAtAntiMeridian())
    {
      bindCoordRect(rect, nearestNodesQuery);
      nearestNodesQuery->exec();
      while(nearestNodesQuery->next())
      {
        int nodeId = nearestNodesQuery->value("node_id").toInt();
        if(checkType(static_cast<nw::Type>(nearestNodesQuery->value("type").toInt())))
        {
          // TODO build edge add minAltFt and type
          Pos otherPos(nearestNodesQuery->value("lonx").toFloat(), nearestNodesQuery->value("laty").toFloat());
          tempEdges.insert(Edge(nodeId, static_cast<int>(node.pos.distanceMeterTo(otherPos) + 0.5f)));
        }
      }
    }
    node.edges = tempEdges.values().toVector();
    addDestNodeEdges(node);
  }

  nodes.insert(node.id, node);

  return node;
}

nw::Node RouteNetworkBase::fetchNode(int id)
{
  if(nodes.contains(id))
    return nodes.value(id);

  nodeByIdQuery->bindValue(":id", id);
  nodeByIdQuery->exec();

  if(nodeByIdQuery->next())
  {
    nw::Node node = createNode(nodeByIdQuery->record());
    node.id = id;

    QSet<Edge> tempEdges;
    tempEdges.reserve(1000);

    edgeToQuery->bindValue(":id", id);
    edgeToQuery->exec();

    while(edgeToQuery->next())
    {
      int nodeId = edgeToQuery->value("to_node_id").toInt();
      if(nodeId != id && checkType(static_cast<nw::Type>(edgeToQuery->value("to_node_type").toInt())))
        tempEdges.insert(createEdge(edgeToQuery->record(), nodeId));
    }

    edgeFromQuery->bindValue(":id", id);
    edgeFromQuery->exec();

    while(edgeFromQuery->next())
    {
      int nodeId = edgeFromQuery->value("from_node_id").toInt();
      if(nodeId != id && checkType(static_cast<nw::Type>(edgeFromQuery->value("from_node_type").toInt())))
        tempEdges.insert(createEdge(edgeFromQuery->record(), nodeId));
    }

    node.edges = tempEdges.values().toVector();
    addDestNodeEdges(node);

    nodes.insert(node.id, node);
    return node;
  }
  return Node();
}

void RouteNetworkBase::initQueries()
{
  QString nodeCols = nodeExtraCols.join(",");
  if(!nodeExtraCols.isEmpty())
    nodeCols.append(", ");

  QString edgeCols = edgeExtraCols.join(",");
  if(!edgeExtraCols.isEmpty())
    edgeCols.append(", ");

  nodeByNavIdQuery = new SqlQuery(db);
  nodeByNavIdQuery->prepare("select node_id from " + nodeTable + " where nav_id = :id and type = :type");

  nodeNavIdAndTypeQuery = new SqlQuery(db);
  nodeNavIdAndTypeQuery->prepare("select nav_id, type from " + nodeTable + " where node_id = :id");

  nearestNodesQuery = new SqlQuery(db);
  nearestNodesQuery->prepare(
    "select node_id, type, lonx, laty from " + nodeTable +
    " where lonx between :leftx and :rightx and laty between :bottomy and :topy");

  nodeByIdQuery = new SqlQuery(db);
  nodeByIdQuery->prepare(
    "select " + nodeCols + " type, lonx, laty from " + nodeTable + " where node_id = :id");

  edgeToQuery = new SqlQuery(db);
  edgeToQuery->prepare(
    "select " + edgeCols + "to_node_id, to_node_type from " + edgeTable +
    " where from_node_id = :id");

  edgeFromQuery = new SqlQuery(db);
  edgeFromQuery->prepare(
    "select " + edgeCols + " from_node_id, from_node_type from " + edgeTable +
    " where to_node_id = :id");
}

void RouteNetworkBase::deInitQueries()
{
  delete nodeByNavIdQuery;
  nodeByNavIdQuery = nullptr;

  delete nodeNavIdAndTypeQuery;
  nodeNavIdAndTypeQuery = nullptr;

  delete nearestNodesQuery;
  nearestNodesQuery = nullptr;

  delete nodeByIdQuery;
  nodeByIdQuery = nullptr;

  delete edgeToQuery;
  edgeToQuery = nullptr;

  delete edgeFromQuery;
  edgeFromQuery = nullptr;
}

nw::Node RouteNetworkBase::createNode(const QSqlRecord& rec)
{
  Node node;
  node.id = rec.value("node_id").toInt();
  node.type = static_cast<nw::Type>(rec.value("type").toInt());
  node.range = rec.value("range").toInt();
  node.pos.setLonX(rec.value("lonx").toFloat());
  node.pos.setLatY(rec.value("laty").toFloat());
  return node;
}

nw::Edge RouteNetworkBase::createEdge(const QSqlRecord& rec, int toNodeId)
{
  // TODO expensive
  Edge edge;
  edge.toNodeId = toNodeId;
  edge.type = static_cast<nw::Type>(rec.value("type").toInt());
  edge.minAltFt = rec.value("minimum_altitude").toInt();
  edge.airwayId = rec.value("airway_id").toInt();
  edge.distanceMeter = rec.value("distance").toInt();
  return edge;
}

bool RouteNetworkBase::checkType(nw::Type type)
{
  switch(type)
  {
    case nw::WAYPOINT_VICTOR:
      return mode & ROUTE_VICTOR;

    case nw::WAYPOINT_JET:
      return mode & ROUTE_JET;

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
