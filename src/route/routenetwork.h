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

#ifndef LITTLENAVMAP_ROUTENETWORK_H
#define LITTLENAVMAP_ROUTENETWORK_H

#include "common/maptypes.h"
#include "geo/calculations.h"

#include <QElapsedTimer>
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

enum Mode
{
  ROUTE_NONE = 0x00,
  ROUTE_RADIONAV = 0x01,
  ROUTE_VICTOR = 0x02,
  ROUTE_JET = 0x04
};

Q_DECLARE_FLAGS(Modes, Mode);
Q_DECLARE_OPERATORS_FOR_FLAGS(nw::Modes);

enum Type
{
  NONE = 0,
  VOR = 1,
  VORDME = 2,
  DME = 3,
  NDB = 4,
  WAYPOINT_VICTOR = 5,
  WAYPOINT_JET = 6,
  WAYPOINT_BOTH = 7,
  START = 10,
  DESTINATION = 11
};

enum EdgeType
{
  AIRWAY_NONE = 0,
  AIRWAY_VICTOR = 5,
  AIRWAY_JET = 6,
  AIRWAY_BOTH = 7
};

struct Edge
{
  Edge()
    : toNodeId(-1), distanceMeter(0), minAltFt(0), airwayId(-1), type(nw::AIRWAY_NONE)
  {
  }

  Edge(int to, int distance)
    : toNodeId(to), distanceMeter(distance), minAltFt(0), airwayId(-1), type(nw::AIRWAY_NONE)
  {
  }

  int toNodeId, distanceMeter, minAltFt, airwayId;
  nw::EdgeType type;

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

struct Node
{
  Node()
    : id(-1), range(0), type(nw::NONE), type2(nw::NONE)
  {
  }

  Node(int nodeId, nw::Type nodeType, nw::Type nodeType2,
       const atools::geo::Pos& position, int nodeRange = 0)
    : id(nodeId), range(nodeRange), pos(position), type(nodeType), type2(nodeType2)
  {
  }

  int id = -1;
  int range;
  QVector<Edge> edges;
  atools::geo::Pos pos;
  nw::Type type, type2;

  bool operator==(const nw::Node& other) const
  {
    return id == other.id && type == other.type && type2 == other.type2;
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
Q_DECLARE_TYPEINFO(nw::Edge, Q_PRIMITIVE_TYPE);

class RouteNetwork
{
public:
  RouteNetwork(atools::sql::SqlDatabase *sqlDb, const QString& nodeTableName,
               const QString& edgeTableName, const QStringList& nodeExtraColumns,
               const QStringList& edgeExtraColumns);
  virtual ~RouteNetwork();

  void getNavIdAndTypeForNode(int nodeId, int& navId, nw::Type& type);

  void initQueries();
  void deInitQueries();

  void getNeighbours(const nw::Node& from, QVector<nw::Node>& neighbours, QVector<nw::Edge>& edges);
  void addStartAndDestinationNodes(const atools::geo::Pos& from, const atools::geo::Pos& to);

  void clear();

  nw::Node getStartNode() const;
  nw::Node getDestinationNode() const;

  nw::Node getNode(int id);

  int getNumberOfNodesDatabase();
  int getNumberOfNodesCache() const;

  bool isAirwayRouting() const
  {
    return airwayRouting;
  }

  void setMode(nw::Modes routeMode);

protected:
  nw::Node getNodeByNavId(int id, nw::Type type);

  nw::Node fetchNode(int id);
  nw::Node fetchNode(float lonx, float laty, bool loadSuccessors, int id);

  void addDestNodeEdges(nw::Node& node);
  void cleanDestNodeEdges();

  void bindCoordRect(const atools::geo::Rect& rect, atools::sql::SqlQuery *query);
  bool checkType(nw::Type type);
  nw::Node createNode(const atools::sql::SqlRecord& rec);
  nw::Edge createEdge(const atools::sql::SqlRecord& rec, int toNodeId);

  void updateNodeIndexes(const atools::sql::SqlRecord& rec);
  void updateEdgeIndexes(const atools::sql::SqlRecord& rec);

  static Q_DECL_CONSTEXPR int NODE_SEARCH_RADIUS = atools::geo::nmToMeter(200);
  const int START_NODE_ID = -10;
  const int DESTINATION_NODE_ID = -20;
  int numNodesDb = -1;

  atools::sql::SqlQuery *nodeByNavIdQuery = nullptr, *nodeNavIdAndTypeQuery = nullptr,
  *nearestNodesQuery = nullptr, *nodeByIdQuery = nullptr, *edgeToQuery = nullptr,
  *edgeFromQuery = nullptr;

  atools::geo::Rect startNodeRect, destinationNodeRect;
  atools::geo::Pos startPos, destinationPos;
  QSet<int> destinationNodePredecessors;

  atools::sql::SqlDatabase *db;
  nw::Modes mode;
  QHash<int, nw::Node> nodes;
  QElapsedTimer timer;

  QString nodeTable, edgeTable;
  QStringList nodeExtraCols, edgeExtraCols;

  bool nodeIndexesCreated = false;
  int nodeIdIndex = -1, nodeTypeIndex = -1, nodeRangeIndex = -1, nodeLonXIndex = -1, nodeLatYIndex = -1;

  bool edgeIndexesCreated = false;
  int edgeTypeIndex = -1, edgeMinAltIndex = -1, edgeAirwayIdIndex = -1,
      edgeDistanceIndex = -1;

  bool airwayRouting;

};

#endif // LITTLENAVMAP_ROUTENETWORK_H
