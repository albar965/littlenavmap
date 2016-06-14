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

#include "routenetworkairway.h"

#include "sql/sqldatabase.h"
#include "sql/sqlquery.h"
#include "sql/sqlutil.h"

#include "geo/calculations.h"
#include "geo/pos.h"
#include "geo/rect.h"

#include <QElapsedTimer>

using atools::sql::SqlDatabase;
using atools::sql::SqlQuery;
using atools::geo::Pos;
using atools::geo::Rect;

using namespace nw;

RouteNetworkAirway::RouteNetworkAirway(atools::sql::SqlDatabase *sqlDb)
  : RouteNetwork(sqlDb, "route_node_airway", "route_edge_airway", {}, {"type", "minimum_altitude", "airway_id"})
{
}

RouteNetworkAirway::~RouteNetworkAirway()
{
}
