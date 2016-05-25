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

#include "routenetwork.h"

#include <QHash>
#include <QVector>

#include <common/maptypes.h>

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
  public RouteNetwork
{
public:
  RouteNetworkRadio(atools::sql::SqlDatabase *sqlDb);
  virtual ~RouteNetworkRadio();



};

#endif // ROUTENETWORKRADIO_H
