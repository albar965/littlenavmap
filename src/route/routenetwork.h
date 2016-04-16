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

#include <QHash>
#include <QList>
#include <QVector>

namespace  atools {
namespace sql {
class SqlDatabase;
class SqlQuery;
}
namespace geo {
class Pos;
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

enum NodeType
{
  VOR,
  VORDME,
  DME,
  NDB,
  WAYPOINT,
  ARTIFICIAL
};

struct Node;

struct Node
{
  int id;
  int range;
  float lonx, laty;
  QVector<Node *> edges;
  NodeType type;
};

}

Q_DECLARE_TYPEINFO(nw::Node, Q_MOVABLE_TYPE);

class RouteNetwork
{
public:
  RouteNetwork(atools::sql::SqlDatabase *sqlDb);
  virtual ~RouteNetwork();

  void setMode(nw::Modes mode);

  void getNeighbours(const nw::Node& from, QList<nw::Node *>& neighbours);
  void getNeighboursFromArtificial(int idFrom, QList<nw::Node *>& neighbours);
  void addArtificialNode(const atools::geo::Pos& pos, int id);

  void initQueries();
  void deInitQueries();

private:
  atools::sql::SqlDatabase *db;
  nw::Modes mode;
  QVector<nw::Node> nodes;
  QHash<int, nw::Node *> nodeIdIndex;
};

#endif // ROUTENETWORK_H
