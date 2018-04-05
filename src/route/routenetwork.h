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

#ifndef LITTLENAVMAP_ROUTENETWORK_H
#define LITTLENAVMAP_ROUTENETWORK_H

#include "common/maptypes.h"
#include "geo/calculations.h"

#include <QHash>
#include <QVector>

namespace  atools {
namespace sql {
class SqlDatabase;
class SqlQuery;
class SqlRecord;
}
namespace geo {
class Pos;
class Rect;
}
}

namespace nw {

/* Network mode. Changes some internal behavior of the network. */
enum Mode
{
  ROUTE_NONE = 0x00,
  ROUTE_RADIONAV = 0x01, /* VOR/NDB to VOR/NDB */
  ROUTE_VICTOR = 0x02, /* Low airways  */
  ROUTE_JET = 0x04 /* High airways */
};

Q_DECLARE_FLAGS(Modes, Mode);
Q_DECLARE_OPERATORS_FOR_FLAGS(nw::Modes);

/* Type and subtype of a node */
enum NodeType
{
  NONE = 0,
  VOR = 1, /* Type or subtype for an airway waypoint */
  VORDME = 2, /* Type or subtype for an airway waypoint */
  // DME = 3, DME and TACAN are not part of the network
  NDB = 4, /* Type or subtype for an airway waypoint */
  WAYPOINT_VICTOR = 5, /* Airway waypoint */
  WAYPOINT_JET = 6, /* Airway waypoint */
  WAYPOINT_BOTH = 7, /* Airway waypoint */
  DEPARTURE = 10, /* User defined departure virtual node */
  DESTINATION = 11 /* User defined destination virtual node */
};

/* Edge type for airway routing */
enum EdgeType
{
  AIRWAY_NONE = 0,
  AIRWAY_VICTOR = 5,
  AIRWAY_JET = 6,
  AIRWAY_BOTH = 7
};

enum EdgeDirection
{
  /* 0 = both, 1 = forward only (from -> to), 2 = backward only (to -> from) */
  BOTH = 0,
  FORWARD = 1,
  BACKWARD = 2
};

/* Network edge that connects two nodes. Is loaded from the database. */
struct Edge
{
  static constexpr int MIN_ALTITUDE = 0;
  static constexpr int MAX_ALTITUDE = std::numeric_limits<int>::max();

  Edge()
    : toNodeId(-1), lengthMeter(0), minAltFt(MIN_ALTITUDE), maxAltFt(MAX_ALTITUDE), airwayId(-1),
    type(nw::AIRWAY_NONE), direction(nw::BOTH)
  {
  }

  Edge(int to, int distance)
    : toNodeId(to), lengthMeter(distance), minAltFt(MIN_ALTITUDE), maxAltFt(MAX_ALTITUDE), airwayId(-1),
    type(nw::AIRWAY_NONE), direction(nw::BOTH)
  {
  }

  int toNodeId /* database "node_id" */, lengthMeter, minAltFt, maxAltFt, airwayId;
  nw::EdgeType type;
  nw::EdgeDirection direction;
  QString airwayName;

  bool operator==(const nw::Edge& other) const
  {
    // Need compare both since edges are added for both directions
    return toNodeId == other.toNodeId && type == other.type;
  }

  bool operator!=(const nw::Edge& other) const
  {
    return !operator==(other);
  }

};

inline int qHash(const nw::Edge& edge)
{
  return edge.toNodeId ^ edge.type;
}

/* Network node. VOR, NDB, waypoint or user defined departure/destination */
struct Node
{
  Node()
    : id(-1), range(0), type(nw::NONE), subtype(nw::NONE)
  {
  }

  Node(int nodeId, nw::NodeType nodeType, nw::NodeType nodeType2,
       const atools::geo::Pos& position, int nodeRange = 0)
    : id(nodeId), range(nodeRange), pos(position), type(nodeType), subtype(nodeType2)
  {
  }

  int id = -1; /* Database id ("node_id") */
  int range; /* Range for a radio navaid or 0 if not applicable */
  QVector<Edge> edges; /* Attached edges leading to adjacent nodes */
  atools::geo::Pos pos;

  nw::NodeType type /* VOR, NDB, ..., WAYPOINT_VICTOR, ... */,
               subtype /* VOR, VORDME, NDB, ... for airway network if type is one of WAYPOINT_* */;

  bool operator==(const nw::Node& other) const
  {
    return id == other.id && type == other.type && subtype == other.subtype;
  }

  bool operator!=(const nw::Node& other) const
  {
    return !operator==(other);
  }

};

inline int qHash(const nw::Node& node)
{
  return node.id;
}

}

Q_DECLARE_TYPEINFO(nw::Node, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(nw::Edge, Q_MOVABLE_TYPE);

/*
 * Routing network that loads and caches nodes and edges from the database.
 * Allows to resolve relations between objects and walk through the network.
 */
class RouteNetwork
{
public:
  /*
   * Create network object and provide the needed tables. Tables need to have a certain layout.
   * @param sqlDb Database to use
   * @param nodeTableName Where nodes are loaded from
   * @param edgeTableName Where edges are loaded from
   * @param nodeExtraColumns Extra columns that are loaded with the nodes
   * @param edgeExtraColumns Extra columns that are loaded with the edges
   */
  RouteNetwork(atools::sql::SqlDatabase *sqlDb, const QString& nodeTableName,
               const QString& edgeTableName, const QStringList& nodeExtraColumns,
               const QStringList& edgeExtraColumns);
  virtual ~RouteNetwork();

  /* Get the navaid id and type for the given network node id. */
  void getNavIdAndTypeForNode(int nodeId, int& navId, nw::NodeType& type);

  /* Set up and prepare all queries */
  void initQueries();

  /* Disconnect queries from database and remove departure and destination nodes */
  void deInitQueries();

  /* Get all adjacent nodes and attached edges for the given node */
  void getNeighbours(const nw::Node& from, QVector<nw::Node>& neighbours, QVector<nw::Edge>& edges);

  /* Integrate departure and destination positions into the network as virtual nodes/edges */
  void addDepartureAndDestinationNodes(const atools::geo::Pos& from, const atools::geo::Pos& to);

  /* Get the virtual departure node that was added using addDepartureAndDestinationNodes */
  nw::Node getDepartureNode() const;

  /* Get the virtual destination node that was added using addDepartureAndDestinationNodes */
  nw::Node getDestinationNode() const;

  /* Get a node by routing network node id. If id is -1 an invalid node with id -1 is returned */
  nw::Node getNode(int id);

  /* Number of nodes in the database */
  int getNumberOfNodesDatabase();

  /* Number of nodes in the memory cache */
  int getNumberOfNodesCache() const;

  /* true if mode is either ROUTE_VICTOR, ROUTE_JET  or both flags */
  bool isAirwayRouting() const
  {
    return airwayRouting;
  }

  /* Sets the route mode. This will change some internal behavior like checking subtypes and more */
  void setMode(nw::Modes routeMode);

private:
  void clearStartAndDestinationNodes();

  nw::Node fetchNodeByNavId(int id, nw::NodeType type);
  nw::Node fetchNode(int id);
  nw::Node fetchNode(float lonx, float laty, bool loadSuccessors, int id);

  void addDestNodeEdges(nw::Node& node);
  void cleanDestNodeEdges();

  void bindCoordRect(const atools::geo::Rect& rect, atools::sql::SqlQuery *query);
  bool testType(nw::NodeType type);
  nw::Node createNode(const atools::sql::SqlRecord& rec);
  nw::Edge createEdge(const atools::sql::SqlRecord& rec, int toNodeId, bool reverseDirection);

  void updateNodeIndexes(const atools::sql::SqlRecord& rec);
  void updateEdgeIndexes(const atools::sql::SqlRecord& rec);

  /* Search radius for nodes around departure and destination position */
  static Q_DECL_CONSTEXPR int NODE_SEARCH_RADIUS_METER = atools::geo::nmToMeter(200);

  /* Departure virtual node id */
  const int DEPARTURE_NODE_ID = -10;
  /* Destination virtual node id */
  const int DESTINATION_NODE_ID = -20;

  /* Cache the number of nodes in the database */
  int numNodesDb = -1;

  atools::sql::SqlQuery *nodeByNavIdQuery = nullptr, *nodeNavIdAndTypeQuery = nullptr,
                        *nearestNodesQuery = nullptr, *nodeByIdQuery = nullptr, *edgeToQuery = nullptr,
                        *edgeFromQuery = nullptr;

  /* Bounding rectangle around destination used to find virtual successor edges */
  atools::geo::Rect destinationNodeRect;
  atools::geo::Pos departurePos, destinationPos;

  /* Collected destination predecessor node ids */
  QSet<int> destinationNodePredecessors;

  atools::sql::SqlDatabase *db;
  nw::Modes mode;

  /* Cache for nodes (also containing edges) for the whole network. Filled on demand. */
  QHash<int, nw::Node> nodeCache;

  /* Database tables and extra columns */
  QString nodeTable, edgeTable;
  QStringList nodeExtraCols, edgeExtraCols;

  /* Index caches to avoid string lookups in SqlRecord */
  bool nodeIndexesCreated = false;
  int nodeTypeIndex = -1, nodeRangeIndex = -1, nodeLonXIndex = -1, nodeLatYIndex = -1;
  bool edgeIndexesCreated = false;
  int edgeTypeIndex = -1, edgeAirwayNameIndex = -1, edgeDirectionIndex = -1, edgeMinAltIndex = -1, edgeMaxAltIndex = -1,
      edgeAirwayIdIndex = -1, edgeDistanceIndex = -1;

  bool airwayRouting;
};

#endif // LITTLENAVMAP_ROUTENETWORK_H
