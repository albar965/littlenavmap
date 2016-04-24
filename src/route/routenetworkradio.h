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

#ifndef ROUTENETWORKRADIO_H
#define ROUTENETWORKRADIO_H

#include "routenetworkbase.h"

#include <QHash>
#include <QVector>

#include <common/maptypes.h>

class QSqlRecord;

namespace  atools {
namespace sql {
class SqlDatabase;
class SqlQuery;
}
namespace geo {
class Pos;
class Rect;
}
}

class RouteNetworkRadio :
  public RouteNetworkBase
{
public:
  RouteNetworkRadio(atools::sql::SqlDatabase *sqlDb);
  virtual ~RouteNetworkRadio();

  virtual nw::Node getNodeByNavId(int id, nw::NodeType type);
  virtual void getNavIdAndTypeForNode(int nodeId, int& navId, nw::NodeType& type);

  virtual void initQueries();
  virtual void deInitQueries();

private:
  virtual nw::Node fetchNode(int id);
  virtual nw::Node fetchNode(float lonx, float laty, bool loadSuccessors, int id);

  atools::sql::SqlQuery *nodeByNavIdQuery = nullptr, *nodeNavIdAndTypeQuery = nullptr,
  *nearestNodesQuery = nullptr, *nodeByIdQuery = nullptr, *edgeToQuery = nullptr,
  *edgeFromQuery = nullptr;

};

#endif // ROUTENETWORKRADIO_H
