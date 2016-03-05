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
#include "geo/rect.h"

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

class MapLayer;

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
  FUEL = 0x0100,
  HARD = 0x0200,
  SOFT = 0x0400,
  WATER = 0x0800
};

struct MapAirport
{
  int id;
  QString ident, name;
  int longestRunwayLength = 0, longestRunwayHeading = 0;
  int altitude = 0;
  int flags = 0;
  float magvar = 0;

  bool valid = false;
  int towerFrequency = 0, atisFrequency = 0, awosFrequency = 0, asosFrequency = 0, unicomFrequency = 0;
  atools::geo::Pos coords;
  atools::geo::Rect bounding;

  bool isSet(MapAirportFlags flag) const
  {
    return (flags & flag) == flag;
  }

  bool hard() const
  {
    return isSet(HARD);
  }

  bool softOnly() const
  {
    return !isSet(HARD) && isSet(SOFT);
  }

  bool waterOnly() const
  {
    return !isSet(HARD) && !isSet(SOFT) && isSet(WATER);
  }

};

struct MapRunway
{
  int length, heading, width;
  QString surface;
  atools::geo::Pos center, primary, secondary;
};

class MapQuery
{
public:
  MapQuery(atools::sql::SqlDatabase *sqlDb);

  void getAirports(const Marble::GeoDataLatLonAltBox& rect, const MapLayer *mapLayer,
                   QList<MapAirport>& airports);

  void getRunwaysForOverview(int airportId, QList<MapRunway>& runways);
  void getRunways(int airportId, QList<MapRunway>& runways);

private:
  void getAirports(const Marble::GeoDataLatLonAltBox& rect, QList<MapAirport>& airports);
  void getAirportsMedium(const Marble::GeoDataLatLonAltBox& rect, QList<MapAirport>& ap);
  void getAirportsLarge(const Marble::GeoDataLatLonAltBox& rect, QList<MapAirport>& ap);

  int flag(const atools::sql::SqlQuery& query, const QString& field, MapAirportFlags flag);

  atools::sql::SqlDatabase *db;

  int getFlags(const atools::sql::SqlQuery& query);
  MapAirport getMapAirport(const atools::sql::SqlQuery& query);

  void bindCoordinateRect(const Marble::GeoDataLatLonAltBox& rect, atools::sql::SqlQuery& query);

};

#endif // MAPQUERY_H
