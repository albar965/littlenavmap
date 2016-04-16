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

#include <geo/pos.h>

using namespace atools::sql;
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

void RouteNetwork::getNeighbours(const nw::Node& from, QList<nw::Node *>& neighbours)
{

}

void RouteNetwork::getNeighboursFromArtificial(int idFrom, QList<nw::Node *>& neighbours)
{
  const Node *node = nodeIdIndex.value(-idFrom);
}

void RouteNetwork::addArtificialNode(const atools::geo::Pos& pos, int id)
{
  Node node;
  node.id = -id;
  node.range = -1;
  node.type = ARTIFICIAL;
  node.lonx = pos.getLonX();
  node.laty = pos.getLatY();

  nodes.append(node);
  nodeIdIndex.insert(id, &nodes.last());
}

void RouteNetwork::initQueries()
{

}

void RouteNetwork::deInitQueries()
{

}
