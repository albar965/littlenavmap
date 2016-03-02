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

#ifndef MAPQUERY_H
#define MAPQUERY_H

#include "geo/pos.h"

#include <QList>

#include <marble/GeoDataCoordinates.h>

namespace atools {
namespace geo {
class Rect;
}
namespace sql {
class SqlDatabase;
class SqlQuery;
}
}

namespace Marble {
class GeoDataLatLonAltBox;
}

enum MapAirportFlags
{
  NONE = 0x0000,
  SCENERY = 0x0001,
  ADDON = 0x0002,
  LIGHT = 0x0004,
  TOWER = 0x0008,
  ILS = 0x0010,
  APPR = 0x0020,
  MIL = 0x0040,
  CLOSED = 0x0080,
  AVGAS = 0x0100,
  JETFUEL = 0x0200,
  HARD = 0x0400,
  SOFT = 0x0800,
  WATER = 0x1000
};

struct MapAirport
{
  int id;
  QString ident, name;
  int longestRunwayLength;
  int longestRunwayHeading;
  int flags;
  Marble::GeoDataCoordinates coords;

};

class MapQuery
{
public:
  MapQuery(atools::sql::SqlDatabase *sqlDb);

  void getAirports(const Marble::GeoDataLatLonAltBox& rect, QList<MapAirport>& ap);

private:
  int flag(const atools::sql::SqlQuery& query, const QString& field, MapAirportFlags flag);

  atools::sql::SqlDatabase *db;

};

#endif // MAPQUERY_H
