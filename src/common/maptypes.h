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

#ifndef LITTLENAVMAP_MAPTYPES_H
#define LITTLENAVMAP_MAPTYPES_H

#include "geo/pos.h"
#include "geo/rect.h"
#include "geo/linestring.h"

#include <QColor>
#include <QString>

/*
 * Maptypes are mostly filled from database tables and are used to pass airport, navaid and more information
 * around in the program. The types are kept primitive (no inheritance no vtable) for performance reasons.
 * Units are usually feet. Type string are as they appear in the database.
 */
namespace maptypes {

/* Type covering all objects that are passed around in the program. Also use to determine what should be drawn. */
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
  ROUTE = 0x02000, /* Flight plan */
  AIRCRAFT = 0x04000, /* Simulator aircraft */
  AIRCRAFT_TRACK = 0x08000, /* Simulator aircraft track */
  USER = 0x10000, /* Flight plan user waypoint */
  PARKING = 0x20000,
  INVALID = 0x40000, /* Flight plan waypoint not found in database */
  ALL_NAV = VOR | NDB | WAYPOINT,
  ALL = 0xffff
};

Q_DECLARE_FLAGS(MapObjectTypes, MapObjectType);
Q_DECLARE_OPERATORS_FOR_FLAGS(maptypes::MapObjectTypes);

/* Primitive id type combo that is hashable */
struct MapObjectRef
{
  int id;
  maptypes::MapObjectTypes type;

  bool operator==(const maptypes::MapObjectRef& other) const;
  bool operator!=(const maptypes::MapObjectRef& other) const;

};

int qHash(const maptypes::MapObjectRef& type);

typedef QVector<MapObjectRef> MapObjectRefList;

/* Convert type from nav_search table to enum */
maptypes::MapObjectTypes navTypeToMapObjectType(const QString& navType);

/* Airport flags coverting most airport attributes and facilities. */
enum MapAirportFlag
{
  AP_NONE = 0x000000,
  AP_ADDON = 0x000001,
  AP_LIGHT = 0x000002, /* Has at least one lighted runway */
  AP_TOWER = 0x000004, /* Has a tower frequency */
  AP_ILS = 0x000008, /* At least one runway end has ILS */
  AP_APPR = 0x000010, /* At least one runway end has an approach */
  AP_MIL = 0x000020,
  AP_CLOSED = 0x000040, /* All runways are closed */
  AP_AVGAS = 0x000080,
  AP_JETFUEL = 0x000100,
  AP_HARD = 0x000200, /* Has at least one hard runway */
  AP_SOFT = 0x000400, /* Has at least one soft runway */
  AP_WATER = 0x000800, /* Has at least one water runway */
  AP_HELIPAD = 0x001000,
  AP_APRON = 0x002000,
  AP_TAXIWAY = 0x004000,
  AP_TOWER_OBJ = 0x008000,
  AP_PARKING = 0x010000,
  AP_ALS = 0x020000, /* Has at least one runway with an approach lighting system */
  AP_VASI = 0x040000, /* Has at least one runway with a VASI */
  AP_FENCE = 0x080000,
  AP_RW_CLOSED = 0x100000, /* Has at least one closed runway */
  AP_COMPLETE = 0x200000, /* Struct completely loaded? */
  AP_ALL = 0xfffff
};

Q_DECLARE_FLAGS(MapAirportFlags, MapAirportFlag);
Q_DECLARE_OPERATORS_FOR_FLAGS(MapAirportFlags);

/* Check surface attributes */
bool isHardSurface(const QString& surface);
bool isWaterSurface(const QString& surface);
bool isSoftSurface(const QString& surface);

/* Airport type not including runways (have to queried separately) */
struct MapAirport
{
  QString ident, /* ICAO ident*/ name;
  int id; /* Database id airport.airport_id */
  int longestRunwayLength = 0, longestRunwayHeading = 0;
  MapAirportFlags flags = AP_NONE;
  float magvar = 0; /* Magnetic variance - positive is east, negative is west */

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

  /*
   * @param objectTypes Map display configuration flags
   * @return true if this airport is visible on map
   */
  bool isVisible(maptypes::MapObjectTypes objectTypes) const;

  /* Used by template functions */
  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  /* Used by template functions */
  int getId() const
  {
    return id;
  }

};

/* Airport runway. All dimensions are feet */
struct MapRunway
{
  QString surface, primaryName, secondaryName, edgeLight;
  int length;
  float heading;
  int width,
      primaryOffset, secondaryOffset, /* part of the runway length */
      primaryBlastPad, secondaryBlastPad, primaryOverrun, secondaryOverrun; /* not part of the runway length */
  atools::geo::Pos position, primaryPosition, secondaryPosition;

  bool primaryClosed, secondaryClosed; /* true if ends have closed markings */

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

/* Apron including full geometry */
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

/* Taxiway segment */
struct MapTaxiPath
{
  atools::geo::Pos start, end;
  QString surface, name;
  int width; /* feet */
  bool drawSurface, closed;

  int getId() const
  {
    return -1;
  }

};

/* Gate, GA ramp, cargo ramps, etc. */
struct MapParking
{
  QString type, name, airlineCodes /* Comma separated list of airline codes */;
  int id /* database id parking.parking_id */, airportId /* database id airport.airport_id */;
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

/* Start position (runway, helipad or parking */
struct MapStart
{
  QString type /* RUNWAY, HELIPAD or WATER */, runwayName /* not empty if this is a runway start */;
  int id /* database id start.start_id */, airportId /* database id airport.airport_id */;
  atools::geo::Pos position;
  int heading, helipadNumber /* -1 if not a helipad otherwise sequence number as it appeared in the BGL */;

  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return id;
  }

};

/* Airport helipad */
struct MapHelipad
{
  QString surface, type;
  atools::geo::Pos position;
  int length, width, heading;
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

/* VOR station */
struct MapVor
{
  QString ident, region, type /* HIGH, LOW, TERMINAL */, name, airportIdent;
  int id; /* database id vor.vor_id*/
  float magvar;
  int frequency /* MHz * 1000 */, range /* nm */;
  atools::geo::Pos position;
  int routeIndex = -1; /* Filled by the get nearest methods for building the context menu */
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

/* NDB station */
struct MapNdb
{
  QString ident, region, type /* HH, H, COMPASS_POINT, etc. */, name, airportIdent;
  int id; /* database id ndb.ndb_id*/
  float magvar;
  int frequency /* kHz * 100 */, range /* nm */;
  atools::geo::Pos position;
  int routeIndex = -1; /* Filled by the get nearest methods for building the context menu */

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
  int id; /* database waypoint.waypoint_id */
  float magvar;
  QString ident, region, type /* NAMED, UNAMED, etc. */, airportIdent;
  atools::geo::Pos position;
  int routeIndex = -1; /* Filled by the get nearest methods for building the context menu */

  bool hasVictorAirways = false, hasJetAirways = false;

  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return id;
  }

};

/* User defined waypoint of a flight plan */
struct MapUserpoint
{
  QString name;
  int id; /* Sequence number as it was added to the flight plan */
  atools::geo::Pos position;
  int routeIndex = -1; /* Filled by the get nearest methods for building the context menu */

  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return id;
  }

};

/* Airway type */
enum MapAirwayType
{
  NO_AIRWAY,
  VICTOR,
  JET,
  BOTH
};

/* Airway segment */
struct MapAirway
{
  QString name;
  maptypes::MapAirwayType type;
  int id, fromWaypointId, toWaypointId; /* all database ids waypoint.waypoint_id */
  int minAltitude /* feet */, sequence /* segment sequence in airway */,
      fragment /* fragment number of disconnected airways with the same name */;
  atools::geo::Pos from, to;
  atools::geo::Rect bounding; /* pre calculated using from and to */

  atools::geo::Pos getPosition() const
  {
    return bounding.getCenter();
  }

  int getId() const
  {
    return id;
  }

};

/* Marker beacon */
struct MapMarker
{
  QString type;
  int id; /* database id marker.marker_id */
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

/* ILS */
struct MapIls
{
  QString ident, name;
  int id; /* database id ils.ils_id */
  float magvar, slope, heading, width;
  int frequency /* MHz * 1000 */, range /* nm */;
  atools::geo::Pos position, pos1, pos2, posmid; /* drawing positions for the feather */
  atools::geo::Rect bounding;
  bool hasDme;

  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return id;
  }

};

/* Mixed search result for e.g. queries on a bounding rectangle for map display or for all get nearest methods */
struct MapSearchResult
{
  QList<MapAirport> airports;
  QSet<int> airportIds; /* Ids used to deduplicate */

  QList<MapAirport> towers;
  QList<MapParking> parkings;
  QList<MapHelipad> helipads;

  QList<MapWaypoint> waypoints;
  QSet<int> waypointIds; /* Ids used to deduplicate */

  QList<MapVor> vors;
  QSet<int> vorIds; /* Ids used to deduplicate */

  QList<MapNdb> ndbs;
  QSet<int> ndbIds; /* Ids used to deduplicate */

  QList<MapMarker> markers;
  QList<MapIls> ils;

  QList<MapAirway> airways;

  QList<MapUserpoint> userPoints;
};

/* Range rings marker. Can be converted to QVariant */
struct RangeMarker
{
  QString text; /* Text to display like VOR name and frequency */
  QVector<int> ranges; /* Range ring list (nm) */
  atools::geo::Pos center;
  MapObjectTypes type; /* VOR, NDB, AIRPORT, etc. - used to determine color */

  const atools::geo::Pos& getPosition() const
  {
    return center;
  }

};

QDataStream& operator>>(QDataStream& dataStream, maptypes::RangeMarker& obj);
QDataStream& operator<<(QDataStream& dataStream, const maptypes::RangeMarker& obj);

/* Distance measurement line. Can be converted to QVariant */
struct DistanceMarker
{
  QString text; /* Text to display like VOR name and frequency */
  QColor color; /* Line color depends on origin (airport or navaid type */
  atools::geo::Pos from, to;
  float magvar;

  bool isRhumbLine, hasMagvar /* If true use  degrees magnetic for display */;

  const atools::geo::Pos& getPosition() const
  {
    return to;
  }

};

QDataStream& operator>>(QDataStream& dataStream, maptypes::DistanceMarker& obj);
QDataStream& operator<<(QDataStream& dataStream, const maptypes::DistanceMarker& obj);

/* Database type strings to GUI strings and map objects to display strings */
QString navTypeName(const QString& type);
QString navName(const QString& type);
QString surfaceName(const QString& surface);
QString parkingGateName(const QString& gate);
QString parkingRampName(const QString& ramp);
QString parkingTypeName(const QString& type);
QString parkingName(const QString& name);
QString parkingNameNumberType(const maptypes::MapParking& parking);

/* Get short name for a parking spot */
QString parkingShortName(const QString& name);

/* Parking description as needed in the PLN files */
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
QString airwayText(const maptypes::MapAirway& airway);
QString magvarText(float magvar);

/* Get a number for surface quality to get the best runway. Higher numbers are better surface. */
int surfaceQuality(const QString& surface);

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

#endif // LITTLENAVMAP_MAPTYPES_H
