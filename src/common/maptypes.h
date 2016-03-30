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
  AIRWAY = 0x0200,
  AIRWAYV = 0x0400,
  AIRWAYJ = 0x0800,
  ROUTE = 0x1000,
  USER = 0x2000,
  INVALID = 0x4000,
  ALL_NAV = VOR | NDB | WAYPOINT,
  ALL = 0xffff
};

Q_DECLARE_FLAGS(MapObjectTypes, MapObjectType);
Q_DECLARE_OPERATORS_FOR_FLAGS(maptypes::MapObjectTypes);

/* Convert nav_search type */
maptypes::MapObjectTypes navTypeToMapObjectType(const QString& navType);

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
  AP_AVGAS = 0x0100,
  AP_JETFUEL = 0x0200,
  AP_HARD = 0x0400,
  AP_SOFT = 0x0800,
  AP_WATER = 0x1000,
  AP_HELIPAD = 0x2000,
  AP_COMPLETE = 0x4000, // Struct completely loaded?
  AP_ALL = 0xffff
};

Q_DECLARE_FLAGS(MapAirportFlags, MapAirportFlag);
Q_DECLARE_OPERATORS_FOR_FLAGS(MapAirportFlags);

struct MapAirport
{
  QString ident, name;
  int id;
  int longestRunwayLength = 0, longestRunwayHeading = 0;
  int altitude = 0;
  MapAirportFlags flags = AP_NONE;
  float magvar = 0;

  int towerFrequency = 0, atisFrequency = 0, awosFrequency = 0, asosFrequency = 0, unicomFrequency = 0;
  atools::geo::Pos position, towerCoords;
  atools::geo::Rect bounding;

  bool valid = false;

  bool hard() const;
  bool soft() const;
  bool water() const;
  bool helipad() const;
  bool softOnly() const;
  bool waterOnly() const;
  bool helipadOnly() const;
  bool noRunways() const;
  bool scenery() const;
  bool tower() const;
  bool addon() const;
  bool anyFuel() const;
  bool complete() const;

  bool isVisible(maptypes::MapObjectTypes objectTypes) const;

  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return id;
  }

};

struct MapRunway
{
  QString surface, primName, secName, edgeLight;
  int length, heading, width, primOffset, secOffset;
  atools::geo::Pos position, primary, secondary;
  bool secClosed, primClosed;

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

  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return -1;
  }

};

struct MapApron
{
  atools::geo::LineString vertices;
  QString surface;
  int getId() const
  {
    return -1;
  }

};

struct MapTaxiPath
{
  atools::geo::Pos start, end;
  QString surface, name;
  int width;
  int getId() const
  {
    return -1;
  }

};

struct MapParking
{
  atools::geo::Pos position;
  QString type, name;
  int number, radius, heading;
  bool jetway;
  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return -1;
  }

};

struct MapHelipad
{
  QString surface, type;
  atools::geo::Pos position;
  int length, width, heading;
  bool closed;

  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return -1;
  }

};

struct MapVor
{
  QString ident, region, type, name, apIdent;
  int id, altitude;
  float magvar;
  int frequency, range;
  atools::geo::Pos position;
  bool dmeOnly, hasDme;

  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return id;
  }

};

struct MapNdb
{
  QString ident, region, type, name, apIdent;
  int id, altitude;
  float magvar;
  int frequency, range;
  atools::geo::Pos position;
  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return id;
  }

};

struct MapWaypoint
{
  int id;
  float magvar;
  QString ident, region, type, apIdent;
  atools::geo::Pos position;
  bool hasRoute = false;
  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return id;
  }

};

struct MapUserpoint
{
  QString name;
  int id;
  atools::geo::Pos position;
  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return id;
  }

};

struct MapAirway
{
  QString name, type;
  int id, fromWpId, toWpId;
  int minalt, sequence, fragment;
  atools::geo::Pos from, to;
  atools::geo::Rect bounding;

  atools::geo::Pos getPosition() const
  {
    return bounding.getCenter();
  }

  int getId() const
  {
    return id;
  }

};

struct MapMarker
{
  QString type;
  int id;
  int heading;
  atools::geo::Pos position;

  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return id;
  }

};

struct MapIls
{
  QString ident, name;
  int id, altitude;
  float magvar, slope, heading, width;
  int frequency, range;
  atools::geo::Pos position, pos1, pos2, posmid;
  atools::geo::Rect bounding;
  bool dme;

  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return id;
  }

};

struct MapSearchResult
{
  QList<const MapAirport *> airports;
  QSet<int> airportIds;

  QList<const MapAirport *> towers;
  QList<const MapParking *> parkings;
  QList<const MapHelipad *> helipads;

  QList<const MapWaypoint *> waypoints;
  QSet<int> waypointIds;

  QList<const MapVor *> vors;
  QSet<int> vorIds;

  QList<const MapNdb *> ndbs;
  QSet<int> ndbIds;

  QList<const MapMarker *> markers;
  QList<const MapIls *> ils;

  QList<const MapAirway *> airways;

  QList<MapUserpoint> userPoints;

  bool needsDelete = false;

  void  deleteAllObjects();

};

struct RangeMarker
{
  QString text;
  QVector<int> ranges;
  atools::geo::Pos center;
  MapObjectTypes type;

  const atools::geo::Pos& getPosition() const
  {
    return center;
  }

};

struct DistanceMarker
{
  QString text;
  QColor color;
  atools::geo::Pos from, to;
  float magvar;

  maptypes::MapObjectTypes type;
  bool rhumbLine, hasMagvar;

  const atools::geo::Pos& getPosition() const
  {
    return to;
  }

};

QString navTypeName(const QString& type);
QString navName(const QString& type);

QString surfaceName(const QString& surface);
QString parkingGateName(const QString& gate);
QString parkingRampName(const QString& ramp);
QString parkingTypeName(const QString& ramp);
QString parkingName(const QString& ramp);

} // namespace types

Q_DECLARE_TYPEINFO(maptypes::MapAirport, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapRunway, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapApron, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapTaxiPath, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapParking, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapHelipad, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapVor, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapNdb, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapWaypoint, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapAirway, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapMarker, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapIls, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::RangeMarker, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::DistanceMarker, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapUserpoint, Q_MOVABLE_TYPE);

#endif // TYPES_H
