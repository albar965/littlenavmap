/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#include "sql/sqldatabase.h"
#include "sql/sqlquery.h"
#include "sql/sqlrecord.h"
#include "sql/sqlutil.h"

#include "geo/pos.h"
#include "geo/rect.h"

#include <QElapsedTimer>

using atools::sql::SqlDatabase;
using atools::sql::SqlQuery;
using atools::sql::SqlRecord;
using atools::geo::Pos;
using atools::geo::Rect;

using namespace nw;

RouteNetwork::RouteNetwork(atools::sql::SqlDatabase *sqlDb, const QString& nodeTableName,
                           const QString& edgeTableName, const QStringList& nodeExtraColumns,
                           const QStringList& edgeExtraColumns)
  : db(sqlDb), nodeTable(nodeTableName), edgeTable(edgeTableName), nodeExtraCols(nodeExtraColumns),
  edgeExtraCols(edgeExtraColumns)
{
  nodeCache.reserve(60000);
  destinationNodePredecessors.reserve(1000);
  airwayRouting = mode & nw::ROUTE_JET || mode & nw::ROUTE_VICTOR;
  initQueries();
}

RouteNetwork::~RouteNetwork()
{
  deInitQueries();
}

int RouteNetwork::getNumberOfNodesDatabase()
{
  if(numNodesDb == -1)
    numNodesDb = atools::sql::SqlUtil(db).rowCount(nodeTable);
  return numNodesDb;
}

int RouteNetwork::getNumberOfNodesCache() const
{
  return nodeCache.size();
}

void RouteNetwork::setMode(nw::Modes routeMode)
{
  mode = routeMode;
  airwayRouting = mode & nw::ROUTE_JET || mode & nw::ROUTE_VICTOR;
}

void RouteNetwork::clearStartAndDestinationNodes()
{
  departurePos = atools::geo::EMPTY_POS;
  destinationPos = atools::geo::EMPTY_POS;
  nodeCache.clear();
  destinationNodePredecessors.clear();
  numNodesDb = -1;
  nodeIndexesCreated = false;
  edgeIndexesCreated = false;
  nodeCache.reserve(60000);
  destinationNodePredecessors.reserve(1000);
}

void RouteNetwork::getNeighbours(const nw::Node& from, QVector<nw::Node>& neighbours,
                                 QVector<Edge>& edges)
{
  for(const Edge& e : from.edges)
  {
    bool add = false;

    // Handle airways differently to keep cache for low and high alt routes together
    if(e.type == AIRWAY_BOTH)
      add = mode & ROUTE_JET || mode & ROUTE_VICTOR;
    else if(e.type == AIRWAY_JET)
      add = mode & ROUTE_JET;
    else if(e.type == AIRWAY_VICTOR)
      add = mode & ROUTE_VICTOR;
    else
      add = true;

    if(add)
    {
      // Add nodes and edges only if they match airway mode
      neighbours.append(fetchNode(e.toNodeId));
      edges.append(e);
    }
  }
}

void RouteNetwork::addDepartureAndDestinationNodes(const atools::geo::Pos& from, const atools::geo::Pos& to)
{
  qDebug() << "adding start and  destination to network";

  if(departurePos == from && destinationPos == to)
    return;

  if(destinationPos != to)
  {
    // Remove all references to destination node
    cleanDestNodeEdges();

    // Add destination first so it can be added to start successors
    destinationPos = to;

    destinationNodeRect = Rect(to, NODE_SEARCH_RADIUS_METER);

    // Will use the bounding rectangle to add any neighbor nodes to dest
    fetchNode(to.getLonX(), to.getLatY(), false, DESTINATION_NODE_ID);

    for(int id : nodeCache.keys())
      // Fill destination node predecessor index
      addDestNodeEdges(nodeCache[id]);
  }

  if(departurePos != from)
  {
    departurePos = from;
    fetchNode(from.getLonX(), from.getLatY(), true, DEPARTURE_NODE_ID);
  }
  qDebug() << "adding start and  destination to network done";
}

nw::Node RouteNetwork::getDepartureNode() const
{
  return nodeCache.value(DEPARTURE_NODE_ID);
}

nw::Node RouteNetwork::getDestinationNode() const
{
  return nodeCache.value(DESTINATION_NODE_ID);
}

nw::Node RouteNetwork::getNode(int id)
{
  if(id == -1)
    return nw::Node();
  else
    return fetchNode(id);
}

/* Remove all references to the destination node from the cache */
void RouteNetwork::cleanDestNodeEdges()
{
  // Remove all references to destination node
  for(int i : destinationNodePredecessors)
  {
    if(nodeCache.contains(i))
    {
      // node is in cache
      // Remove all edges from the node that point to the destination node
      QVector<Edge>& edges = nodeCache[i].edges;
      QVector<Edge>::iterator it = std::remove_if(edges.begin(), edges.end(),
                                                  [ = ](const Edge& e) -> bool
      {
        return e.toNodeId == DESTINATION_NODE_ID;
      });

      if(it == edges.end())
        qWarning() << "No node destination predecessors deleted for node" << nodeCache.value(i).id;
      else
        edges.erase(it, edges.end());
    }
    else
      qWarning() << "No node destination found" << nodeCache.value(i).id;
  }

  // Clear predecessor cache
  destinationNodePredecessors.clear();
}

/* Add a destination node virtual edge to the node if it is inside the destination bounding rectangle */
void RouteNetwork::addDestNodeEdges(nw::Node& node)
{
  if(destinationNodeRect.contains(node.pos) && !destinationNodePredecessors.contains(node.id) &&
     node.id != DESTINATION_NODE_ID)
  {
    // Node is not dest and not indexed yet - create virtual edge
    nw::Edge edge(DESTINATION_NODE_ID, static_cast<int>(node.pos.distanceMeterTo(destinationPos)));

    // Near destination - add as successor
    node.edges.append(edge);
    // Remember in cache for later cleanup
    destinationNodePredecessors.insert(node.id);
  }
}

/* Get a node by navaid id (waypoint_id, vor_id, ...) */
nw::Node RouteNetwork::fetchNodeByNavId(int id, nw::NodeType type)
{
  nodeByNavIdQuery->bindValue(":id", id);
  nodeByNavIdQuery->bindValue(":type", type);
  nodeByNavIdQuery->exec();

  nw::Node node;

  if(nodeByNavIdQuery->next())
    // Fetch node into the cache
    node = fetchNode(nodeByNavIdQuery->value("node_id").toInt());
  else
  {
    if(type == nw::WAYPOINT_JET || type == nw::WAYPOINT_VICTOR)
    {
      // Not found and is an airway - look for waypoints
      nodeByNavIdQuery->bindValue(":type", nw::WAYPOINT_BOTH);
      nodeByNavIdQuery->exec();
      if(nodeByNavIdQuery->next())
        node = fetchNode(nodeByNavIdQuery->value("node_id").toInt());
    }
  }

  nodeByNavIdQuery->finish();

  return node;
}

void RouteNetwork::getNavIdAndTypeForNode(int nodeId, int& navId, nw::NodeType& type)
{
  if(nodeId == DEPARTURE_NODE_ID)
  {
    type = DEPARTURE;
    navId = -1; // No database id available
  }
  else if(nodeId == DESTINATION_NODE_ID)
  {
    type = DESTINATION;
    navId = -1; // No database id available
  }
  else
  {
    nodeNavIdAndTypeQuery->bindValue(":id", nodeId);
    nodeNavIdAndTypeQuery->exec();

    if(nodeNavIdAndTypeQuery->next())
    {
      navId = nodeNavIdAndTypeQuery->value("nav_id").toInt();

      int typeVal = nodeNavIdAndTypeQuery->value("type").toInt();

      if(airwayRouting)
        // This is an airway network which has the type in the upper four bits
        type = static_cast<nw::NodeType>(typeVal >> 4);
      else
        type = static_cast<nw::NodeType>(typeVal);
    }
    else
    {
      navId = -1;
      type = nw::NONE;
    }
    nodeNavIdAndTypeQuery->finish();
  }
}

/* Create a virtual node at the given coordinates with the given id */
nw::Node RouteNetwork::fetchNode(float lonx, float laty, bool loadSuccessors, int id)
{
  nodeCache.remove(id);

  Node node;
  node.id = id;
  node.range = 0;

  if(id == DEPARTURE_NODE_ID)
    node.type = DEPARTURE;
  else if(id == DESTINATION_NODE_ID)
    node.type = DESTINATION;
  else
    node.type = NONE;

  node.pos.setLonX(lonx);
  node.pos.setLatY(laty);

  if(loadSuccessors)
  {
    // Load all successor nodes within the query rectangle
    Rect queryRect(Pos(lonx, laty), NODE_SEARCH_RADIUS_METER);

    // Use a set for de-duplication
    QSet<Edge> tempEdges;
    tempEdges.reserve(1000);

    for(const Rect& rect : queryRect.splitAtAntiMeridian())
    {
      bindCoordRect(rect, nearestNodesQuery);
      nearestNodesQuery->exec();
      while(nearestNodesQuery->next())
      {
        int nodeId = nearestNodesQuery->value("node_id").toInt();
        if(testType(static_cast<nw::NodeType>(nearestNodesQuery->value("type").toInt())))
        {
          Pos otherPos(nearestNodesQuery->value("lonx").toFloat(), nearestNodesQuery->value("laty").toFloat());
          tempEdges.insert(Edge(nodeId, static_cast<int>(node.pos.distanceMeterTo(otherPos))));
        }
      }
    }
    node.edges = tempEdges.values().toVector();

    // Add edges to destination node if there are any
    addDestNodeEdges(node);
  }

  nodeCache.insert(node.id, node);

  return node;
}

/* Get the node either from cache of from the database. The node will include all edges. */
nw::Node RouteNetwork::fetchNode(int id)
{
  if(nodeCache.contains(id))
    return nodeCache.value(id);

  nodeByIdQuery->bindValue(":id", id);
  nodeByIdQuery->exec();
  nw::Node node;

  if(nodeByIdQuery->next())
  {
    node = createNode(nodeByIdQuery->record());
    node.id = id;

    QSet<Edge> tempEdges;
    tempEdges.reserve(1000);

    // Add ingoing edges
    edgeToQuery->bindValue(":id", id);
    edgeToQuery->exec();

    while(edgeToQuery->next())
    {
      int nodeId = edgeToQuery->value("to_node_id").toInt();
      if(nodeId != id && testType(static_cast<nw::NodeType>(edgeToQuery->value("to_node_type").toInt())))
        tempEdges.insert(createEdge(edgeToQuery->record(), nodeId, false /* reverseDirection */));
    }

    // Add outgoing edges
    edgeFromQuery->bindValue(":id", id);
    edgeFromQuery->exec();

    while(edgeFromQuery->next())
    {
      int nodeId = edgeFromQuery->value("from_node_id").toInt();
      if(nodeId != id && testType(static_cast<nw::NodeType>(edgeFromQuery->value("from_node_type").toInt())))
        tempEdges.insert(createEdge(edgeFromQuery->record(), nodeId, true /* reverseDirection */));
    }

    node.edges = tempEdges.values().toVector();
    addDestNodeEdges(node);

    nodeCache.insert(node.id, node);
  }
  nodeByIdQuery->finish();
  return node;
}

void RouteNetwork::initQueries()
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
    "select " + edgeCols + " to_node_id, to_node_type from " + edgeTable +
    " where from_node_id = :id");

  edgeFromQuery = new SqlQuery(db);
  edgeFromQuery->prepare(
    "select " + edgeCols + " from_node_id, from_node_type from " + edgeTable +
    " where to_node_id = :id");
}

void RouteNetwork::deInitQueries()
{
  clearStartAndDestinationNodes();

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

/* Create node from SQL record */
nw::Node RouteNetwork::createNode(const SqlRecord& rec)
{
  // Update indexes into SQL record
  updateNodeIndexes(rec);
  Node node;

  if(airwayRouting)
  {
    node.type = static_cast<nw::NodeType>(rec.valueInt(nodeTypeIndex) >> 4);
    node.subtype = static_cast<nw::NodeType>(rec.valueInt(nodeTypeIndex) & 0x0f);
  }
  else
    node.type = static_cast<nw::NodeType>(rec.valueInt(nodeTypeIndex));

  if(nodeRangeIndex != -1)
    // Add range if part of the extra columns
    node.range = rec.valueInt(nodeRangeIndex);
  node.pos.setLonX(rec.valueFloat(nodeLonXIndex));
  node.pos.setLatY(rec.valueFloat(nodeLatYIndex));
  return node;
}

/* Update node index caches to avoid string lookups in SqlRecord */
void RouteNetwork::updateNodeIndexes(const SqlRecord& rec)
{
  if(!nodeIndexesCreated)
  {
    nodeTypeIndex = rec.indexOf("type");
    if(rec.contains("range"))
      nodeRangeIndex = rec.indexOf("range");
    else
      nodeRangeIndex = -1;
    nodeLonXIndex = rec.indexOf("lonx");
    nodeLatYIndex = rec.indexOf("laty");
    nodeIndexesCreated = true;
  }
}

/* Create edge from SQL record */
nw::Edge RouteNetwork::createEdge(const atools::sql::SqlRecord& rec, int toNodeId, bool reverseDirection)
{
  updateEdgeIndexes(rec);
  Edge edge;
  edge.toNodeId = toNodeId;

  if(edgeTypeIndex != -1)
    edge.type = static_cast<nw::EdgeType>(rec.valueInt(edgeTypeIndex));

  if(edgeDirectionIndex != -1)
  {
    edge.direction = static_cast<nw::EdgeDirection>(rec.valueInt(edgeDirectionIndex));
    if(reverseDirection && edge.direction != BOTH)
    {
      if(edge.direction == FORWARD)
        edge.direction = BACKWARD;
      else if(edge.direction == BACKWARD)
        edge.direction = FORWARD;
    }
  }

  if(edgeMinAltIndex != -1)
  {
    int minAlt = rec.valueInt(edgeMinAltIndex);
    if(minAlt > 0)
      edge.minAltFt = minAlt;
  }

  if(edgeMaxAltIndex != -1)
  {
    int maxAlt = rec.valueInt(edgeMaxAltIndex);
    if(maxAlt > 0)
      edge.maxAltFt = maxAlt;
    // otherwise leave max value
  }
  if(edgeAirwayIdIndex != -1)
    edge.airwayId = rec.valueInt(edgeAirwayIdIndex);

  if(edgeAirwayNameIndex != -1)
    edge.airwayName = rec.valueStr(edgeAirwayNameIndex);

  if(edgeDistanceIndex != -1)
    edge.lengthMeter = rec.valueInt(edgeDistanceIndex);
  return edge;
}

/* Update edge index caches to avoid string lookups in SqlRecord */
void RouteNetwork::updateEdgeIndexes(const atools::sql::SqlRecord& rec)
{
  if(!edgeIndexesCreated)
  {
    if(rec.contains("type"))
      edgeTypeIndex = rec.indexOf("type");
    else
      edgeTypeIndex = -1;

    if(rec.contains("direction"))
      edgeDirectionIndex = rec.indexOf("direction");
    else
      edgeDirectionIndex = -1;

    if(rec.contains("minimum_altitude"))
      edgeMinAltIndex = rec.indexOf("minimum_altitude");
    else
      edgeMinAltIndex = -1;

    if(rec.contains("maximum_altitude"))
      edgeMaxAltIndex = rec.indexOf("maximum_altitude");
    else
      edgeMaxAltIndex = -1;

    if(rec.contains("airway_name"))
      edgeAirwayNameIndex = rec.indexOf("airway_name");
    else
      edgeAirwayNameIndex = -1;

    if(rec.contains("airway_id"))
      edgeAirwayIdIndex = rec.indexOf("airway_id");
    else
      edgeAirwayIdIndex = -1;

    if(rec.contains("distance"))
      edgeDistanceIndex = rec.indexOf("distance");
    else
      edgeDistanceIndex = -1;

    edgeIndexesCreated = true;
  }
}

/* Check if the node type is part of the network and usable for the current mode */
bool RouteNetwork::testType(nw::NodeType type)
{
  nw::NodeType tmp = type;
  if(airwayRouting)
    tmp = static_cast<nw::NodeType>(static_cast<int>(type) >> 4);

  switch(tmp)
  {
    case nw::WAYPOINT_VICTOR:
    case nw::WAYPOINT_JET:
    case nw::WAYPOINT_BOTH:
      return mode & ROUTE_JET || mode & ROUTE_VICTOR;

    case nw::NDB:
    case nw::VOR:
    case nw::VORDME:
      return mode & ROUTE_RADIONAV;

    case nw::DEPARTURE:
    case nw::DESTINATION:
      return true;

    case nw::NONE:
      break;
  }

  return false;
}

void RouteNetwork::bindCoordRect(const atools::geo::Rect& rect, atools::sql::SqlQuery *query)
{
  query->bindValue(":leftx", rect.getWest());
  query->bindValue(":rightx", rect.getEast());
  query->bindValue(":bottomy", rect.getSouth());
  query->bindValue(":topy", rect.getNorth());
}
