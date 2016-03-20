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

#ifndef TYPES_H
#define TYPES_H

#include "geo/pos.h"
#include "geo/rect.h"
#include "geo/linestring.h"

#include <QColor>
#include <QString>

namespace maptypes {

enum MapObjectType
{
  NONE = 0x0000,
  AIRPORT = 0x0001,
  AIRPORT_HARD = 0x0002,
  AIRPORT_SOFT = 0x0004,
  AIRPORT_EMPTY = 0x0008,
  AIRPORT_ALL = AIRPORT | AIRPORT_HARD | AIRPORT_SOFT | AIRPORT_EMPTY,
  VOR = 0x0010,
  NDB = 0x0020,
  ILS = 0x0040,
  MARKER = 0x0080,
  WAYPOINT = 0x0100,
  ALL_NAV = VOR | NDB | WAYPOINT,
  ALL = 0xffff
};

Q_DECLARE_FLAGS(MapObjectTypes, MapObjectType);
Q_DECLARE_OPERATORS_FOR_FLAGS(maptypes::MapObjectTypes);

enum MapAirportFlag
{
  AP_NONE = 0x0000,
  AP_SCENERY = 0x0001,
  AP_ADDON = 0x0002,
  AP_LIGHT = 0x0004,
  AP_TOWER = 0x0008,
  AP_ILS = 0x0010,
  AP_APPR = 0x0020,
  AP_MIL = 0x0040,
  AP_CLOSED = 0x0080,
  AP_FUEL = 0x0100,
  AP_HARD = 0x0200,
  AP_SOFT = 0x0400,
  AP_WATER = 0x0800,
  AP_HELIPORT = 0x1000,
  AP_ALL = 0xffff
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
  atools::geo::Pos position, towerCoords;
  atools::geo::Rect bounding;

  bool hard() const;
  bool soft() const;
  bool softOnly() const;
  bool water() const;
  bool waterOnly() const;
  bool isHeliport() const;
  bool noRunways() const;
  bool scenery() const;
  bool tower() const;
  bool addon() const;

  bool isVisible(maptypes::MapObjectTypes objectTypes) const;

};

struct MapRunway
{
  int length, heading, width, primOffset, secOffset;
  QString surface, primName, secName, edgeLight;
  bool secClosed, primClosed;
  atools::geo::Pos position, primary, secondary;

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
  atools::geo::Pos position;
  QString type, name;
  int number, radius, heading;
  bool jetway;
};

struct MapHelipad
{
  QString surface, type;
  atools::geo::Pos position;
  int length, width, heading;
  bool closed;
};

struct MapWaypoint
{
  int id;
  float magvar;
  QString ident, region, type, apIdent;
  atools::geo::Pos position;
};

struct MapVor
{
  int id;
  QString ident, region, type, name, apIdent;
  float magvar;
  int frequency, range;
  bool dmeOnly, hasDme;
  atools::geo::Pos position;
};

struct MapNdb
{
  int id;
  QString ident, region, type, name, apIdent;
  float magvar;
  int frequency, range;
  atools::geo::Pos position;
};

struct MapMarker
{
  int id;
  QString type;
  int heading;
  atools::geo::Pos position;
};

struct MapIls
{
  int id;
  QString ident, name;
  float magvar, slope, heading, width;
  int frequency, range;
  bool dme;
  atools::geo::Pos position, pos1, pos2, posmid;
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

  bool needsDelete = false;

  void  deleteAllObjects();

};

struct RangeMarker
{
  QString text;
  MapObjectTypes type;
  QVector<int> ranges;
  atools::geo::Pos position;
};

struct DistanceMarker
{
  QString text;
  QColor color;
  atools::geo::Pos from, position;
  bool rhumbLine, hasMagvar;
  float magvar;
  maptypes::MapObjectTypes type;
};

QString navTypeName(const QString& type);
QString navName(const QString& type);

QString surfaceName(const QString& surface);
QString parkingGateName(const QString& gate);
QString parkingRampName(const QString& ramp);

} // namespace types

#endif // TYPES_H
