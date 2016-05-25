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

#ifndef ROUTENETWORK_H
#define ROUTENETWORK_H

#include <QElapsedTimer>
#include <QHash>
#include <QVector>

#include <common/maptypes.h>

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
  ROUTE_VOR = 0x01,
  ROUTE_VORDME = 0x02,
  ROUTE_DME = 0x04,
  ROUTE_NDB = 0x08,
  ROUTE_VICTOR = 0x10,
  ROUTE_JET = 0x20
};

Q_DECLARE_FLAGS(Modes, Mode);
Q_DECLARE_OPERATORS_FOR_FLAGS(nw::Modes);

enum Type
{
  VOR = 0,
  VORDME = 1,
  DME = 2,
  NDB = 3,
  WAYPOINT_VICTOR = 4,
  WAYPOINT_JET = 5,
  WAYPOINT_BOTH = 6,
  START = 10,
  DESTINATION = 11,
  NONE = 20
};

struct Edge
{
  Edge()
    : toNodeId(-1), distanceMeter(0), minAltFt(0), airwayId(-1), type(nw::NONE)
  {
  }

  Edge(int to, int distance)
    : toNodeId(to), distanceMeter(distance), minAltFt(0), airwayId(-1), type(nw::NONE)
  {

  }

  int toNodeId, distanceMeter, minAltFt, airwayId;
  nw::Type type;

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
    : id(-1), range(0), type(nw::NONE)
  {
  }

  Node(int nodeId, nw::Type nodeType, const atools::geo::Pos& position, int nodeRange = 0)
    : id(nodeId), range(nodeRange), pos(position), type(nodeType)
  {

  }

  int id = -1;
  int range;
  QVector<Edge> edges;
  atools::geo::Pos pos;
  nw::Type type;

  bool operator==(const nw::Node& other) const
  {
    return id == other.id && type == other.type;
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

  void setMode(nw::Modes mode);

  nw::Node getNodeById(int id)
  {
    return fetchNode(id);
  }

  nw::Node getNodeByNavId(int id, nw::Type type);
  void getNavIdAndTypeForNode(int nodeId, int& navId, nw::Type& type);

  void initQueries();
  void deInitQueries();
  nw::Node fetchNode(int id);
  nw::Node fetchNode(float lonx, float laty, bool loadSuccessors, int id);

  atools::sql::SqlQuery *nodeByNavIdQuery = nullptr, *nodeNavIdAndTypeQuery = nullptr,
  *nearestNodesQuery = nullptr, *nodeByIdQuery = nullptr, *edgeToQuery = nullptr,
  *edgeFromQuery = nullptr;

  void getNeighbours(const nw::Node& from, QVector<nw::Node>& neighbours, QVector<nw::Edge>& edges);
  void addStartAndDestinationNodes(const atools::geo::Pos& from, const atools::geo::Pos& to);

  void clear();

  nw::Node getStartNode() const;
  nw::Node getDestinationNode() const;

  int getNumberOfNodesDatabase();
  int getNumberOfNodesCache() const;

  bool isAirwayRouting() const
  {
    return mode & nw::ROUTE_JET || mode & nw::ROUTE_VICTOR;
  }

#ifdef DEBUG_ROUTE
  void printNodeDebug(const nw::Node& node);
  void printEdgeDebug(const nw::Edge& edge);

#endif

protected:
  void addDestNodeEdges(nw::Node& node);
  void cleanDestNodeEdges();

  void bindCoordRect(const atools::geo::Rect& rect, atools::sql::SqlQuery *query);
  bool checkType(nw::Type type);
  nw::Node createNode(const atools::sql::SqlRecord& rec);
  nw::Edge createEdge(const atools::sql::SqlRecord& rec, int toNodeId);

  void updateNodeIndexes(const atools::sql::SqlRecord& rec);
  void updateEdgeIndexes(const atools::sql::SqlRecord& rec);

  const int START_NODE_ID = -10;
  const int DESTINATION_NODE_ID = -20;
  int numNodesDb = -1;

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

};

#endif // ROUTENETWORK_H
