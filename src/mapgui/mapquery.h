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
#include "common/maptypes.h"
#include "maplayer.h"

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

class CoordinateConverter;

enum MapAirportFlag
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
  HELIPORT = 0x1000,
  ALL = 0xffff
};

Q_DECLARE_FLAGS(MapAirportFlags, MapAirportFlag);
Q_DECLARE_OPERATORS_FOR_FLAGS(MapAirportFlags);

struct MapAirport
{
  int id;
  QString ident, name;
  int longestRunwayLength = 0, longestRunwayHeading = 0;
  int altitude = 0;
  MapAirportFlags flags = 0;
  float magvar = 0;

  bool valid = false;
  int towerFrequency = 0, atisFrequency = 0, awosFrequency = 0, asosFrequency = 0, unicomFrequency = 0;
  atools::geo::Pos pos, towerCoords;
  atools::geo::Rect bounding;

  bool hard() const
  {
    return flags.testFlag(HARD);
  }

  bool soft() const
  {
    return flags.testFlag(SOFT);
  }

  bool softOnly() const
  {
    return !flags.testFlag(HARD) && flags.testFlag(SOFT);
  }

  bool water() const
  {
    return flags.testFlag(WATER);
  }

  bool waterOnly() const
  {
    return !flags.testFlag(HARD) && !flags.testFlag(SOFT) && flags.testFlag(WATER);
  }

  bool isHeliport() const
  {
    return !flags.testFlag(HARD) && !flags.testFlag(SOFT) &&
           !flags.testFlag(WATER) && flags.testFlag(HELIPORT);
  }

};

struct MapRunway
{
  int length, heading, width, primOffset, secOffset;
  QString surface, primName, secName, edgeLight;
  bool secClosed, primClosed;
  atools::geo::Pos center, primary, secondary;

  bool isHard() const
  {
    return surface == "CONCRETE" || surface == "ASPHALT" || surface == "BITUMINOUS" || surface == "TARMAC";
  }

  bool isWater() const
  {
    return surface == "WATER";
  }

  bool isSoft() const
  {
    return !isWater() && !isHard();
  }

};

struct MapApron
{
  atools::geo::LineString vertices;
  QString surface;
};

struct MapTaxiPath
{
  atools::geo::Pos start, end;
  QString surface, name;
  int width;
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

struct MapWaypoint
{
  int id;
  QString ident, region, type;
  atools::geo::Pos pos;
};

struct MapVor
{
  int id;
  QString ident, type, name;
  float magvar;
  int frequency, range;
  bool dmeOnly, hasDme;
  atools::geo::Pos pos;
};

struct MapNdb
{
  int id;
  QString ident, type, name;
  float magvar;
  int frequency, range;
  atools::geo::Pos pos;
};

struct MapMarker
{
  int id;
  QString type;
  int heading;
  atools::geo::Pos pos;
};

struct MapIls
{
  int id;
  QString ident, name;
  float magvar, slope, heading, width;
  int frequency, range;
  bool dme;
  atools::geo::Pos pos, pos1, pos2, posmid;
  atools::geo::Rect bounding;
};

struct MapSearchResult
{
  QList<const MapAirport *> airports;
  QList<const MapAirport *> towers;
  QList<const MapParking *> parkings;
  QList<const MapHelipad *> helipads;

  QList<const MapWaypoint *> waypoints;
  QList<const MapVor *> vors;
  QList<const MapNdb *> ndbs;
  QList<const MapMarker *> markers;
  QList<const MapIls *> ils;
};

class MapQuery
{
public:
  MapQuery(atools::sql::SqlDatabase *sqlDb);
  virtual ~MapQuery();

  /* Result is only valid until the next paint is called */
  void getNearestObjects(const CoordinateConverter& conv, const MapLayer *mapLayer,
                         maptypes::ObjectTypes types,
                         int xs, int ys, int screenDistance,
                         MapSearchResult& result);

  const QList<MapAirport> *getAirports(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                       bool lazy);

  const QList<MapWaypoint> *getWaypoints(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                         bool lazy);

  const QList<MapVor> *getVors(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                               bool lazy);

  const QList<MapNdb> *getNdbs(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                               bool lazy);

  const QList<MapMarker> *getMarkers(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                     bool lazy);

  const QList<MapIls> *getIls(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                              bool lazy);

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
  struct SimpleCache
  {
    bool handleCache(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, bool lazy);

    Marble::GeoDataLatLonBox curRect;
    const MapLayer *curMapLayer = nullptr;
    QList<TYPE> list;
  };

  atools::sql::SqlDatabase *db;

  // Marble::GeoDataLatLonBox curRect;
  // const MapLayer *curMapLayer;

  SimpleCache<MapAirport> airportCache;
  SimpleCache<MapWaypoint> waypointCache;
  SimpleCache<MapVor> vorCache;
  SimpleCache<MapNdb> ndbCache;
  SimpleCache<MapMarker> markerCache;
  SimpleCache<MapIls> ilsCache;

  QCache<int, QList<MapRunway> > runwayCache;
  QCache<int, QList<MapRunway> > runwayOverwiewCache;
  QCache<int, QList<MapApron> > apronCache;
  QCache<int, QList<MapTaxiPath> > taxipathCache;
  QCache<int, QList<MapParking> > parkingCache;
  QCache<int, QList<MapHelipad> > helipadCache;

  atools::sql::SqlQuery *airportQuery = nullptr, *airportMediumQuery = nullptr, *airportLargeQuery = nullptr,
  *runwayOverviewQuery = nullptr, *apronQuery = nullptr, *parkingQuery = nullptr, *helipadQuery = nullptr,
  *taxiparthQuery = nullptr, *runwaysQuery = nullptr,
  *waypointsQuery = nullptr, *vorsQuery = nullptr, *ndbsQuery = nullptr, *markersQuery = nullptr,
  *ilsQuery = nullptr;

  const QList<MapAirport> *fetchAirports(const Marble::GeoDataLatLonBox& rect, atools::sql::SqlQuery *query,
                                         bool lazy);

  MapAirportFlags flag(const atools::sql::SqlQuery *query, const QString& field, MapAirportFlags flag);

  MapAirportFlags getFlags(const atools::sql::SqlQuery *query);
  MapAirport fillMapAirport(const atools::sql::SqlQuery *query);

  void bindCoordinateRect(const Marble::GeoDataLatLonBox& rect, atools::sql::SqlQuery *query);

  QList<Marble::GeoDataLatLonBox> splitAtAntiMeridian(const Marble::GeoDataLatLonBox& rect);

  static void inflateRect(Marble::GeoDataLatLonBox& rect, double degree);

  bool runwayCompare(const MapRunway& r1, const MapRunway& r2);

  const static double RECT_INFLATION_FACTOR;
  const static double RECT_INFLATION_ADD;
};

// ---------------------------------------------------------------------------------
template<typename TYPE>
bool MapQuery::SimpleCache<TYPE>::handleCache(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer,
                                              bool lazy)
{
  if(lazy)
    return false;

  Marble::GeoDataLatLonBox cur(curRect);
  MapQuery::inflateRect(cur, cur.width(Marble::GeoDataCoordinates::Degree) *
                        MapQuery::RECT_INFLATION_FACTOR + MapQuery::RECT_INFLATION_ADD);

  if(curRect.isEmpty() || !cur.contains(rect) || curMapLayer == nullptr ||
     !curMapLayer->hasSameQueryParameters(mapLayer))
  {
    list.clear();
    curRect = rect;
    curMapLayer = mapLayer;
    return true;
  }
  return false;
}

#endif // MAPQUERY_H
