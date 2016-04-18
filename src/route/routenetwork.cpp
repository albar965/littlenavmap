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

void RouteNetwork::getNeighbours(const nw::Node& from, QList<nw::Node>& neighbours)
{
  for(int id : from.edges)
    neighbours.append(fetchNode(id));
}

nw::Node RouteNetwork::getNodeByNavId(int id, nw::NodeType type)
{
  SqlQuery nodeQuery(db);
  nodeQuery.prepare("select node_id from route_node where nav_id = :id and type = :type");
  nodeQuery.bindValue(":id", id);
  nodeQuery.bindValue(":type", type);
  nodeQuery.exec();

  if(nodeQuery.next())
    return fetchNode(nodeQuery.value("node_id").toInt());

  return Node();
}

int RouteNetwork::getNavIdForNode(const Node& node)
{
  SqlQuery nodeQuery(db);
  nodeQuery.prepare("select nav_id from route_node where node_id = :id");
  nodeQuery.bindValue(":id", node.id);
  nodeQuery.exec();

  if(nodeQuery.next())
    return nodeQuery.value("nav_id").toInt();

  return -1;
}

nw::Node RouteNetwork::getNodeById(int id)
{
  return fetchNode(id);
}

nw::Node RouteNetwork::addArtificialNode(const atools::geo::Pos& pos)
{
  return fetchNode(pos.getLonX(), pos.getLatY());
}

nw::Node RouteNetwork::fetchNode(float lonx, float laty)
{
  SqlQuery nearestStmt(db);
  nearestStmt.prepare(
    "select node_id from route_node "
    "where lonx between :leftx and :rightx and laty between :bottomy and :topy and type in (:types)");

  Node node;
  node.id = curArtificialNodeId--;
  node.range = -1;
  node.type = ARTIFICIAL;
  node.lonx = lonx;
  node.laty = laty;

  int maxRange = atools::geo::nmToMeter(500);
  Rect queryRect(Pos(lonx, laty), maxRange);
  nearestStmt.bindValue(":types", QVariantList({3}));

  for(const Rect& rect : queryRect.splitAtAntiMeridian())
  {
    bindCoordRect(rect, nearestStmt);
    nearestStmt.exec();
    while(nearestStmt.next())
    {
      int id = nearestStmt.value("node_id").toInt();
      if(!node.edges.contains(id))
        node.edges.append(id);
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
  nodeQuery.prepare("select type, range, lonx, laty from route_node where node_id = :id");
  nodeQuery.bindValue(":id", id);
  nodeQuery.exec();

  if(nodeQuery.next())
  {
    nw::Node node;
    fillNode(nodeQuery.record(), node);
    node.id = id;

    SqlQuery edgeQueryTo(db);
    edgeQueryTo.prepare("select to_node_id, to_node_type "
                        "from route_edge "
                        "where from_node_id = :id and to_node_type in (:type1, :type2, :type3, :type4)");

    edgeQueryTo.bindValue(":id", id);
    bindTypes(edgeQueryTo);
    edgeQueryTo.exec();
    qDebug() << edgeQueryTo.executedQuery();

    while(edgeQueryTo.next())
    {
      qDebug() << "to_node_type" << edgeQueryTo.value("to_node_type").toInt();

      int to = edgeQueryTo.value("to_node_id").toInt();
      if(to != id && !node.edges.contains(to))
        node.edges.append(to);
    }

    SqlQuery edgeQueryFrom(db);
    edgeQueryFrom.prepare("select from_node_id, from_node_type "
                          "from route_edge "
                          "where to_node_id = :id and from_node_type in (:type1, :type2, :type3, :type4)");

    edgeQueryFrom.bindValue(":id", id);
    bindTypes(edgeQueryFrom);
    edgeQueryFrom.exec();
    qDebug() << edgeQueryFrom.executedQuery();

    while(edgeQueryFrom.next())
    {
      qDebug() << "from_node_type" << edgeQueryFrom.value("from_node_type").toInt();

      int from = edgeQueryFrom.value("from_node_id").toInt();
      if(from != id && !node.edges.contains(from))
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

void RouteNetwork::bindTypes(atools::sql::SqlQuery& query)
{
  nw::NodeType defType = nw::NONE;

  if(mode & ROUTE_VOR)
    defType = nw::VOR;
  else if(mode & ROUTE_VORDME)
    defType = nw::VORDME;
  else if(mode & ROUTE_DME)
    defType = nw::DME;
  else if(mode & ROUTE_NDB)
    defType = nw::NDB;

  if(mode & ROUTE_VOR)
    query.bindValue(":type1", nw::VOR);
  else
    query.bindValue(":type1", defType);

  if(mode & ROUTE_VORDME)
    query.bindValue(":type2", nw::VORDME);
  else
    query.bindValue(":type2", defType);

  if(mode & ROUTE_DME)
    query.bindValue(":type3", nw::DME);
  else
    query.bindValue(":type3", defType);

  if(mode & ROUTE_NDB)
    query.bindValue(":type4", nw::NDB);
  else
    query.bindValue(":type4", defType);
}

void RouteNetwork::bindCoordRect(const atools::geo::Rect& rect, atools::sql::SqlQuery& query)
{
  query.bindValue(":leftx", rect.getWest());
  query.bindValue(":rightx", rect.getEast());
  query.bindValue(":bottomy", rect.getSouth());
  query.bindValue(":topy", rect.getNorth());
}
