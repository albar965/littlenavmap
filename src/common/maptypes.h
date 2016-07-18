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

#ifndef LITTLENAVMAP_TYPES_H
#define LITTLENAVMAP_TYPES_H

#include "geo/pos.h"
#include "geo/rect.h"
#include "geo/linestring.h"

#include <QColor>
#include <QString>

namespace maptypes {

enum MapObjectType
{
  NONE = 0x00000,
  AIRPORT = 0x00001,
  AIRPORT_HARD = 0x00002,
  AIRPORT_SOFT = 0x00004,
  AIRPORT_EMPTY = 0x00008,
  AIRPORT_ADDON = 0x00010,
  AIRPORT_ALL = AIRPORT | AIRPORT_HARD | AIRPORT_SOFT | AIRPORT_EMPTY | AIRPORT_ADDON,
  VOR = 0x00020,
  NDB = 0x00040,
  ILS = 0x00080,
  MARKER = 0x00100,
  WAYPOINT = 0x00200,
  AIRWAY = 0x00400,
  AIRWAYV = 0x00800,
  AIRWAYJ = 0x01000,
  ROUTE = 0x02000,
  AIRCRAFT = 0x04000,
  AIRCRAFT_TRACK = 0x08000,
  USER = 0x10000,
  PARKING = 0x20000,
  INVALID = 0x40000,
  ALL_NAV = VOR | NDB | WAYPOINT,
  ALL = 0xffff
};

Q_DECLARE_FLAGS(MapObjectTypes, MapObjectType);
Q_DECLARE_OPERATORS_FOR_FLAGS(maptypes::MapObjectTypes);

struct MapObjectRef
{
  int id;
  maptypes::MapObjectTypes type;

  bool operator==(const maptypes::MapObjectRef& other) const;
  bool operator!=(const maptypes::MapObjectRef& other) const;

};

int qHash(const maptypes::MapObjectRef& type);

typedef QVector<MapObjectRef> MapObjectRefList;

/* Convert nav_search type */
maptypes::MapObjectTypes navTypeToMapObjectType(const QString& navType);

enum MapAirportFlag
{
  AP_NONE = 0x000000,
  AP_ADDON = 0x000001,
  AP_LIGHT = 0x000002,
  AP_TOWER = 0x000004,
  AP_ILS = 0x000008,
  AP_APPR = 0x000010,
  AP_MIL = 0x000020,
  AP_CLOSED = 0x000040,
  AP_AVGAS = 0x000080,
  AP_JETFUEL = 0x000100,
  AP_HARD = 0x000200,
  AP_SOFT = 0x000400,
  AP_WATER = 0x000800,
  AP_HELIPAD = 0x001000,
  AP_APRON = 0x002000,
  AP_TAXIWAY = 0x004000,
  AP_TOWER_OBJ = 0x008000,
  AP_PARKING = 0x010000,
  AP_ALS = 0x020000,
  AP_VASI = 0x040000,
  AP_FENCE = 0x080000,
  AP_RW_CLOSED = 0x100000,
  AP_COMPLETE = 0x200000, // Struct completely loaded?
  AP_ALL = 0xfffff
};

Q_DECLARE_FLAGS(MapAirportFlags, MapAirportFlag);
Q_DECLARE_OPERATORS_FOR_FLAGS(MapAirportFlags);

bool isHardSurface(const QString& surface);
bool isWaterSurface(const QString& surface);
bool isSoftSurface(const QString& surface);

struct MapAirport
{
  QString ident, name;
  int id;
  int longestRunwayLength = 0, longestRunwayHeading = 0;
  MapAirportFlags flags = AP_NONE;
  float magvar = 0;

  int towerFrequency = 0, atisFrequency = 0, awosFrequency = 0, asosFrequency = 0, unicomFrequency = 0;
  atools::geo::Pos position, towerCoords;
  atools::geo::Rect bounding;
  int routeIndex = -1;

  bool closed() const;
  bool hard() const;
  bool soft() const;
  bool water() const;
  bool helipad() const;
  bool softOnly() const;
  bool waterOnly() const;
  bool helipadOnly() const;
  bool noRunways() const;
  bool tower() const;
  bool addon() const;
  bool anyFuel() const;
  bool complete() const;
  bool towerObject() const;
  bool apron() const;
  bool taxiway() const;
  bool parking() const;
  bool empty() const;
  bool als() const;
  bool vasi() const;
  bool fence() const;
  bool closedRunways() const;

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
  int length;
  float heading;
  int width, primOffset, secOffset, primBlastPad, secBlastPad, primOverrun, secOverrun;
  atools::geo::Pos position, primary, secondary;
  bool primClosed, secClosed;

  bool isHard() const
  {
    return isHardSurface(surface);
  }

  bool isWater() const
  {
    return isWaterSurface(surface);
  }

  bool isSoft() const
  {
    return isSoftSurface(surface);
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
  bool drawSurface;
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
  bool drawSurface;
  int getId() const
  {
    return -1;
  }

};

struct MapParking
{
  QString type, name, airlineCodes;
  int id, airportId;
  atools::geo::Pos position;
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

struct MapStart
{
  QString type, runwayName;
  int id, airportId;
  atools::geo::Pos position;
  int heading, helipadNumber;
  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return id;
  }

};

struct MapHelipad
{
  QString surface, type;
  atools::geo::Pos position;
  int length, width, heading;
  int routeIndex = -1;
  bool closed, transparent;

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
  int id;
  float magvar;
  int frequency, range;
  atools::geo::Pos position;
  int routeIndex = -1;
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
  int id;
  float magvar;
  int frequency, range;
  atools::geo::Pos position;
  int routeIndex = -1;
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
  int routeIndex = -1;
  bool hasVictor = false, hasJet = false;
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
  int routeIndex = -1;
  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return id;
  }

};

enum MapAirwayType
{
  NO_AIRWAY,
  VICTOR,
  JET,
  BOTH
};

struct MapAirway
{
  QString name;
  maptypes::MapAirwayType type;
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
  int id;
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
  QList<MapAirport> airports;
  QSet<int> airportIds;

  QList<MapAirport> towers;
  QList<MapParking> parkings;
  QList<MapHelipad> helipads;

  QList<MapWaypoint> waypoints;
  QSet<int> waypointIds;

  QList<MapVor> vors;
  QSet<int> vorIds;

  QList<MapNdb> ndbs;
  QSet<int> ndbIds;

  QList<MapMarker> markers;
  QList<MapIls> ils;

  QList<MapAirway> airways;

  QList<MapUserpoint> userPoints;
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

QDataStream& operator>>(QDataStream& dataStream, maptypes::RangeMarker& obj);
QDataStream& operator<<(QDataStream& dataStream, const maptypes::RangeMarker& obj);

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

QDataStream& operator>>(QDataStream& dataStream, maptypes::DistanceMarker& obj);
QDataStream& operator<<(QDataStream& dataStream, const maptypes::DistanceMarker& obj);

QString navTypeName(const QString& type);
QString navName(const QString& type);

QString surfaceName(const QString& surface);
int surfaceQuality(const QString& surface);
QString parkingGateName(const QString& gate);
QString parkingRampName(const QString& ramp);
QString parkingTypeName(const QString& type);
QString parkingName(const QString& name);
QString parkingNameNumberType(const maptypes::MapParking& parking);
QString parkingNameForFlightplan(const MapParking& parking);
QString airwayTypeToShortString(maptypes::MapAirwayType type);
QString airwayTypeToString(maptypes::MapAirwayType type);
MapAirwayType  airwayTypeFromString(const QString& typeStr);
QString comTypeName(const QString& type);

QString airportText(const maptypes::MapAirport& airport);
QString vorText(const maptypes::MapVor& vor);
QString vorType(const maptypes::MapVor& vor);
QString ndbText(const maptypes::MapNdb& ndb);
QString waypointText(const maptypes::MapWaypoint& waypoint);
QString userpointText(const maptypes::MapUserpoint& userpoint);
QString magvarText(float magvar);

} // namespace types

Q_DECLARE_TYPEINFO(maptypes::MapObjectRef, Q_PRIMITIVE_TYPE);

Q_DECLARE_TYPEINFO(maptypes::MapAirport, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapRunway, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapApron, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapTaxiPath, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapParking, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapStart, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapHelipad, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapVor, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapNdb, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapWaypoint, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapAirway, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapMarker, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapIls, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapUserpoint, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(maptypes::MapSearchResult, Q_MOVABLE_TYPE);

Q_DECLARE_TYPEINFO(maptypes::RangeMarker, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(maptypes::RangeMarker);
Q_DECLARE_TYPEINFO(maptypes::DistanceMarker, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(maptypes::DistanceMarker);

#endif // LITTLENAVMAP_TYPES_H
