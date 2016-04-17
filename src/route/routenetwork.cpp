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

nw::Node RouteNetwork::addArtificialNode(const atools::geo::Pos& pos)
{
  return fetchNode(pos.getLonX(), pos.getLatY());
}

nw::Node RouteNetwork::fetchNode(float lonx, float laty)
{
  SqlQuery nearestStmt(db);
  nearestStmt.prepare(
    "select node_id from route_node "
    "where lonx between :leftx and :rightx and laty between :bottomy and :topy and type < 4");

  int maxRange = atools::geo::nmToMeter(500);
  Rect queryRect(Pos(lonx, laty), maxRange);

  Node node;
  node.id = curArtificialNodeId--;
  node.range = -1;
  node.type = ARTIFICIAL;
  node.lonx = lonx;
  node.laty = laty;

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

    SqlQuery edgeQuery(db);
    edgeQuery.prepare("select from_node_id, to_node_id "
                      "from route_edge where from_node_id = :id or to_node_id = :id and type < 4");
    edgeQuery.bindValue(":id", id);
    edgeQuery.exec();

    while(edgeQuery.next())
    {
      int from = edgeQuery.value("from_node_id").toInt();
      if(from != id && !node.edges.contains(from))
        node.edges.append(from);

      int to = edgeQuery.value("to_node_id").toInt();
      if(to != id && !node.edges.contains(to))
        node.edges.append(to);
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

void RouteNetwork::bindCoordRect(const atools::geo::Rect& rect, atools::sql::SqlQuery& query)
{
  query.bindValue(":leftx", rect.getWest());
  query.bindValue(":rightx", rect.getEast());
  query.bindValue(":bottomy", rect.getSouth());
  query.bindValue(":topy", rect.getNorth());
}
