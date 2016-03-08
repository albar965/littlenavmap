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
#include "geo/linestring.h"

#include <QCache>
#include <QList>

#include <marble/GeoDataCoordinates.h>
#include <marble/GeoDataLatLonBox.h>

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
class GeoDataLatLonBox;
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
  WATER = 0x0800,
  HELIPORT = 0x1000
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
  atools::geo::Pos coords, towerCoords;
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

  bool isHeliport() const
  {
    return !isSet(HARD) && !isSet(SOFT) && !isSet(WATER) && isSet(HELIPORT);
  }

};

struct MapRunway
{
  int length, heading, width, primOffset, secOffset;
  QString surface, primName, secName, edgeLight;
  bool secClosed, primClosed;
  atools::geo::Pos center, primary, secondary;
};

struct MapApron
{
  atools::geo::LineString vertices;
  QString surface;
  bool drawSurface;
};

struct MapTaxiPath
{
  atools::geo::Pos start, end;
  QString surface, name, startType, endType;
  int width;
  bool drawSurface;
};

struct MapParking
{
  atools::geo::Pos pos;
  QString type, name;
  int number, radius, heading;
  bool jetway;
};

struct MapHelipad
{
  QString surface, type;
  atools::geo::Pos pos;
  int length, width, heading;
  bool closed;
};

class MapQuery
{
public:
  MapQuery(atools::sql::SqlDatabase *sqlDb);
  virtual ~MapQuery();

  void getAirports(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                   QList<MapAirport>& airportList);

  const QList<MapRunway> *getRunwaysForOverview(int airportId);

  const QList<MapRunway> *getRunways(int airportId);

  const QList<MapApron> *getAprons(int airportId);

  const QList<MapTaxiPath> *getTaxiPaths(int airportId);

  const QList<MapParking> *getParking(int airportId);

  const QList<MapHelipad> *getHelipads(int airportId);

  void initQueries();
  void deInitQueries();

private:
  template<typename TYPE>
  bool handleCache(QMultiMap<int, TYPE>& cache, int id, QList<MapRunway>& runwayList);

  atools::sql::SqlDatabase *db;
  Marble::GeoDataLatLonBox curRect;
  const MapLayer *curMapLayer;

  QList<MapAirport> airports;

  QCache<int, QList<MapRunway> > runwayCache;
  QCache<int, QList<MapRunway> > runwayOverwiewCache;
  QCache<int, QList<MapApron> > apronCache;
  QCache<int, QList<MapTaxiPath> > taxipathCache;
  QCache<int, QList<MapParking> > parkingCache;
  QCache<int, QList<MapHelipad> > helipadCache;

  atools::sql::SqlQuery *airportQuery = nullptr, *airportMediumQuery = nullptr, *airportLargeQuery = nullptr,
  *runwayOverviewQuery = nullptr, *apronQuery = nullptr, *parkingQuery = nullptr, *helipadQuery = nullptr,
  *taxiparthQuery = nullptr, *runwaysQuery = nullptr;

  void fetchAirports(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                     QList<MapAirport>& airportList);
  void fetchAirportsMedium(const Marble::GeoDataLatLonBox& rect, QList<MapAirport>& airportList);
  void fetchAirportsLarge(const Marble::GeoDataLatLonBox& rect, QList<MapAirport>& airportList);

  int flag(const atools::sql::SqlQuery *query, const QString& field, MapAirportFlags flag);

  int getFlags(const atools::sql::SqlQuery *query);
  MapAirport fillMapAirport(const atools::sql::SqlQuery *query);

  void bindCoordinateRect(const Marble::GeoDataLatLonBox& rect, atools::sql::SqlQuery *query);

  QList<Marble::GeoDataLatLonBox> splitAtAntiMeridian(const Marble::GeoDataLatLonBox& rect);

  void inflateRect(Marble::GeoDataLatLonBox& rect, double degree);
};

#endif // MAPQUERY_H
