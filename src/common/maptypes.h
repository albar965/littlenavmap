/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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
#include "geo/line.h"
#include "fs/fspaths.h"
#include "geo/linestring.h"
#include "fs/sc/simconnectaircraft.h"
#include "fs/sc/simconnectuseraircraft.h"

#include <QColor>
#include <QString>

namespace proc {

struct MapProcedurePoint;

}
/*
 * Maptypes are mostly filled from database tables and are used to pass airport, navaid and more information
 * around in the program. The types are kept primitive (no inheritance no vtable) for performance reasons.
 * Units are usually feet. Type string are as they appear in the database.
 */
namespace map {

/* Value for invalid/not found distances */
Q_DECL_CONSTEXPR static float INVALID_COURSE_VALUE = std::numeric_limits<float>::max();
Q_DECL_CONSTEXPR static float INVALID_DISTANCE_VALUE = std::numeric_limits<float>::max();
Q_DECL_CONSTEXPR static int INVALID_INDEX_VALUE = std::numeric_limits<int>::max();

Q_DECL_CONSTEXPR static float INVALID_MAGVAR = 9999.f;

struct PosCourse
{
  PosCourse()
    : course(INVALID_COURSE_VALUE)
  {
  }

  explicit PosCourse(atools::geo::Pos posParam, float courseParam = INVALID_COURSE_VALUE)
    : pos(posParam), course(courseParam)
  {
  }

  atools::geo::Pos pos;
  float course;

  bool isCourseValid() const
  {
    return course < INVALID_COURSE_VALUE;
  }

  bool isValid() const
  {
    return pos.isValid();
  }

};

/* Type covering all objects that are passed around in the program. Also use to determine what should be drawn. */
enum MapObjectType
{
  NONE = 0,
  AIRPORT = 1 << 0,
  AIRPORT_HARD = 1 << 1,
  AIRPORT_SOFT = 1 << 2,
  AIRPORT_EMPTY = 1 << 3,
  AIRPORT_ADDON = 1 << 4,
  VOR = 1 << 5,
  NDB = 1 << 6,
  ILS = 1 << 7,
  MARKER = 1 << 8,
  WAYPOINT = 1 << 9,
  AIRWAY = 1 << 10,
  AIRWAYV = 1 << 11,
  AIRWAYJ = 1 << 12,
  FLIGHTPLAN = 1 << 13, /* Flight plan */
  AIRCRAFT = 1 << 14, /* Simulator aircraft */
  AIRCRAFT_AI = 1 << 15, /* AI or multiplayer Simulator aircraft */
  AIRCRAFT_TRACK = 1 << 16, /* Simulator aircraft track */
  USER = 1 << 17, /* Flight plan user waypoint */
  PARKING = 1 << 18,
  RUNWAYEND = 1 << 19,
  INVALID = 1 << 20, /* Flight plan waypoint not found in database */
  MISSED_APPROACH = 1 << 21, /* Only procedure type that can be hidden */
  PROCEDURE = 1 << 22, /* General procedure leg */
  AIRSPACE = 1 << 23, /* General airspace boundary */

  AIRPORT_ALL = AIRPORT | AIRPORT_HARD | AIRPORT_SOFT | AIRPORT_EMPTY | AIRPORT_ADDON,
  NAV_ALL = VOR | NDB | WAYPOINT,
  NAV_MAGVAR = AIRPORT | VOR | NDB | WAYPOINT, /* All objects that have a magvar assigned */

  ALL = 0xffffffff
};

Q_DECLARE_FLAGS(MapObjectTypes, MapObjectType);
Q_DECLARE_OPERATORS_FOR_FLAGS(map::MapObjectTypes);

QDebug operator<<(QDebug out, const map::MapObjectTypes& type);

/* Primitive id type combo that is hashable */
struct MapObjectRef
{
  int id;
  map::MapObjectTypes type;

  bool operator==(const map::MapObjectRef& other) const;
  bool operator!=(const map::MapObjectRef& other) const;

};

int qHash(const map::MapObjectRef& type);

typedef QVector<MapObjectRef> MapObjectRefList;

/* Convert type from nav_search table to enum */
map::MapObjectTypes navTypeToMapObjectType(const QString& navType);

/* Airport flags coverting most airport attributes and facilities. */
enum MapAirportFlag
{
  AP_NONE = 0,
  AP_ADDON = 1 << 0,
  AP_LIGHT = 1 << 1, /* Has at least one lighted runway */
  AP_TOWER = 1 << 2, /* Has a tower frequency */
  AP_ILS = 1 << 3, /* At least one runway end has ILS */
  AP_PROCEDURE = 1 << 4, /* At least one runway end has an approach */
  AP_MIL = 1 << 5,
  AP_CLOSED = 1 << 6, /* All runways are closed */
  AP_AVGAS = 1 << 7,
  AP_JETFUEL = 1 << 8,
  AP_HARD = 1 << 9, /* Has at least one hard runway */
  AP_SOFT = 1 << 10, /* Has at least one soft runway */
  AP_WATER = 1 << 11, /* Has at least one water runway */
  AP_HELIPAD = 1 << 12,
  AP_APRON = 1 << 13,
  AP_TAXIWAY = 1 << 14,
  AP_TOWER_OBJ = 1 << 15,
  AP_PARKING = 1 << 16,
  AP_ALS = 1 << 17, /* Has at least one runway with an approach lighting system */
  AP_VASI = 1 << 18, /* Has at least one runway with a VASI */
  AP_FENCE = 1 << 19,
  AP_RW_CLOSED = 1 << 20, /* Has at least one closed runway */
  AP_COMPLETE = 1 << 21, /* Struct completely loaded? */
  AP_ALL = 0xffffffff
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

  bool isValid() const
  {
    return position.isValid();
  }

  /*
   * @param objectTypes Map display configuration flags
   * @return true if this airport is visible on map
   */
  bool isVisible(map::MapObjectTypes objectTypes) const;

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
  int length, primaryEndId, secondaryEndId;
  float heading;
  int width,
      primaryOffset, secondaryOffset, /* part of the runway length */
      primaryBlastPad, secondaryBlastPad, primaryOverrun, secondaryOverrun; /* not part of the runway length */
  atools::geo::Pos position, primaryPosition, secondaryPosition;

  bool primaryClosed, secondaryClosed; /* true if ends have closed markings */

  bool isValid() const
  {
    return position.isValid();
  }

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

/* Airport runway end. All dimensions are feet */
struct MapRunwayEnd
{
  QString name;
  float heading;
  atools::geo::Pos position;
  bool secondary;

  bool isValid() const
  {
    return position.isValid();
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

  bool isValid() const
  {
    return !vertices.isEmpty();
  }

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

  bool isValid() const
  {
    return start.isValid();
  }

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

  bool isValid() const
  {
    return position.isValid();
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

/* Start position (runway, helipad or parking */
struct MapStart
{
  QString type /* RUNWAY, HELIPAD or WATER */, runwayName /* not empty if this is a runway start */;
  int id /* database id start.start_id */, airportId /* database id airport.airport_id */;
  atools::geo::Pos position;
  int heading, helipadNumber /* -1 if not a helipad otherwise sequence number as it appeared in the BGL */;

  bool isValid() const
  {
    return position.isValid();
  }

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
  int length, width, heading, start;
  bool closed, transparent;

  bool isValid() const
  {
    return position.isValid();
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

/* VOR station */
struct MapVor
{
  QString ident, region, type /* HIGH, LOW, TERMINAL */, name /*, airportIdent*/;
  int id; /* database id vor.vor_id*/
  float magvar;
  int frequency /* MHz * 1000 */, range /* nm */;
  atools::geo::Pos position;
  int routeIndex = -1; /* Filled by the get nearest methods for building the context menu */
  bool dmeOnly, hasDme;

  bool isValid() const
  {
    return position.isValid();
  }

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
  QString ident, region, type /* HH, H, COMPASS_POINT, etc. */, name /*, airportIdent*/;
  int id; /* database id ndb.ndb_id*/
  float magvar;
  int frequency /* kHz * 100 */, range /* nm */;
  atools::geo::Pos position;
  int routeIndex = -1; /* Filled by the get nearest methods for building the context menu */

  bool isValid() const
  {
    return position.isValid();
  }

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
  QString ident, region, type /* NAMED, UNAMED, etc. *//*, airportIdent*/;
  atools::geo::Pos position;
  int routeIndex = -1; /* Filled by the get nearest methods for building the context menu */

  bool hasVictorAirways = false, hasJetAirways = false;

  bool isValid() const
  {
    return position.isValid();
  }

  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return id;
  }

};

/* Waypoint or intersection */
struct MapAirwayWaypoint
{
  map::MapWaypoint waypoint;
  int airwayId, airwayFragmentId, seqNum;
};

/* User defined waypoint of a flight plan */
struct MapUserpoint
{
  QString name;
  int id; /* Sequence number as it was added to the flight plan */
  atools::geo::Pos position;
  int routeIndex = -1; /* Filled by the get nearest methods for building the context menu */

  bool isValid() const
  {
    return position.isValid();
  }

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
  map::MapAirwayType type;
  int id, fromWaypointId, toWaypointId; /* all database ids waypoint.waypoint_id */
  int minAltitude /* feet */, sequence /* segment sequence in airway */,
      fragment /* fragment number of disconnected airways with the same name */;
  atools::geo::Pos from, to;
  atools::geo::Rect bounding; /* pre calculated using from and to */

  bool isValid() const
  {
    return from.isValid();
  }

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

  bool isValid() const
  {
    return position.isValid();
  }

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

  bool isValid() const
  {
    return position.isValid();
  }

  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return id;
  }

};

enum MapAirspaceType
{
  AIRSPACE_NONE = 0,
  CENTER = 1 << 0,
  CLASS_A = 1 << 1, // ICAO airspace - controlled - no VFR
  CLASS_B = 1 << 2, // ICAO airspace - controlled
  CLASS_C = 1 << 3, // ICAO airspace - controlled
  CLASS_D = 1 << 4, // ICAO airspace - controlled
  CLASS_E = 1 << 5, // ICAO airspace - controlled
  CLASS_F = 1 << 6, // Open FIR - uncontrolled
  CLASS_G = 1 << 7, // Open FIR - uncontrolled
  TOWER = 1 << 8,
  CLEARANCE = 1 << 9,
  GROUND = 1 << 10,
  DEPARTURE = 1 << 11,
  APPROACH = 1 << 12,
  MOA = 1 << 13,
  RESTRICTED = 1 << 14,
  PROHIBITED = 1 << 15,
  WARNING = 1 << 16,
  ALERT = 1 << 17,
  DANGER = 1 << 18,
  NATIONAL_PARK = 1 << 19,
  MODEC = 1 << 20,
  RADAR = 1 << 21,
  TRAINING = 1 << 22,

  /* Special filter flags - not airspaces */
  AIRSPACE_BELOW_10000 = 1 << 23,
  AIRSPACE_BELOW_18000 = 1 << 24,
  AIRSPACE_ABOVE_10000 = 1 << 25,
  AIRSPACE_ABOVE_18000 = 1 << 26,
  AIRSPACE_ALL_ALTITUDE = 1 << 27,

  /* Action flags - not airspaces */
  AIRSPACE_ALL_ON = 1 << 28,
  AIRSPACE_ALL_OFF = 1 << 29,

  AIRSPACE_ICAO = CLASS_A | CLASS_B | CLASS_C | CLASS_D | CLASS_E,
  AIRSPACE_FIR = CLASS_F | CLASS_G,
  AIRSPACE_CENTER = CENTER,
  AIRSPACE_RESTRICTED = RESTRICTED | PROHIBITED | MOA | DANGER,
  AIRSPACE_SPECIAL = WARNING | ALERT | TRAINING,
  AIRSPACE_OTHER = TOWER | CLEARANCE | GROUND | DEPARTURE | APPROACH | MODEC | RADAR | NATIONAL_PARK,

  AIRSPACE_FOR_VFR = CLASS_B | CLASS_C | CLASS_D | CLASS_E | AIRSPACE_FIR,
  AIRSPACE_FOR_IFR = CLASS_A | CLASS_B | CLASS_C | CLASS_D | CLASS_E | CENTER,

  AIRSPACE_ALL = AIRSPACE_ICAO | AIRSPACE_FIR | AIRSPACE_CENTER | AIRSPACE_RESTRICTED | AIRSPACE_SPECIAL |
                 AIRSPACE_OTHER,

  // Default value on first start
  AIRSPACE_DEFAULT = AIRSPACE_ICAO | AIRSPACE_RESTRICTED | AIRSPACE_ALL_ALTITUDE
};

Q_DECL_CONSTEXPR int MAP_AIRSPACE_TYPE_BITS = 22;

Q_DECLARE_FLAGS(MapAirspaceTypes, MapAirspaceType);
Q_DECLARE_OPERATORS_FOR_FLAGS(map::MapAirspaceTypes);

/* Airspace boundary */
struct MapAirspace
{
  int id;
  int minAltitude, maxAltitude;
  QString name, comName, comType, minAltitudeType, maxAltitudeType;
  int comFrequency;
  map::MapAirspaceTypes type;

  atools::geo::Rect bounding;
  atools::geo::LineString lines;

  bool isValid() const
  {
    return bounding.isValid();
  }

};

/* Mixed search result for e.g. queries on a bounding rectangle for map display or for all get nearest methods */
struct MapSearchResult
{
  QList<MapAirport> airports;
  QSet<int> airportIds; /* Ids used to deduplicate */

  QList<MapRunwayEnd> runwayEnds;
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
  QList<MapAirspace> airspaces;

  /* User defined route points */
  QList<MapUserpoint> userPoints;

  QList<atools::fs::sc::SimConnectAircraft> aiAircraft;
  atools::fs::sc::SimConnectUserAircraft userAircraft;

  bool isEmpty(const map::MapObjectTypes& types) const;

  bool hasVor() const
  {
    return !vors.isEmpty();
  }

  bool hasNdb() const
  {
    return !ndbs.isEmpty();
  }

  bool hasIls() const
  {
    return !ils.isEmpty();
  }

  bool hasRunwayEnd() const
  {
    return !runwayEnds.isEmpty();
  }

  bool hasWaypoints() const
  {
    return !waypoints.isEmpty();
  }

};

/* Range rings marker. Can be converted to QVariant */
struct RangeMarker
{
  QString text; /* Text to display like VOR name and frequency */
  QVector<int> ranges; /* Range ring list (nm) */
  atools::geo::Pos center;
  MapObjectTypes type; /* VOR, NDB, AIRPORT, etc. - used to determine color */

  bool isValid() const
  {
    return center.isValid();
  }

  const atools::geo::Pos& getPosition() const
  {
    return center;
  }

};

QDataStream& operator>>(QDataStream& dataStream, map::RangeMarker& obj);
QDataStream& operator<<(QDataStream& dataStream, const map::RangeMarker& obj);

/* Distance measurement line. Can be converted to QVariant */
struct DistanceMarker
{
  QString text; /* Text to display like VOR name and frequency */
  QColor color; /* Line color depends on origin (airport or navaid type */
  atools::geo::Pos from, to;
  float magvar;

  bool isRhumbLine, hasMagvar /* If true use  degrees magnetic for display */;

  bool isValid() const
  {
    return from.isValid();
  }

  const atools::geo::Pos& getPosition() const
  {
    return to;
  }

};

QDataStream& operator>>(QDataStream& dataStream, map::DistanceMarker& obj);
QDataStream& operator<<(QDataStream& dataStream, const map::DistanceMarker& obj);

/* Stores last METARs to avoid unneeded updates in widget */
struct WeatherContext
{
  atools::fs::sc::MetarResult fsMetar;
  bool isAsDeparture = false, isAsDestination = false;
  QString asMetar, asType, vatsimMetar, noaaMetar, ident;

};

/* Database type strings to GUI strings and map objects to display strings */
QString navTypeName(const QString& type);
QString navTypeNameVor(const QString& type);
QString navTypeNameVorLong(const QString& type);
QString navTypeNameNdb(const QString& type);
QString navTypeNameWaypoint(const QString& type);

QString edgeLights(const QString& type);
QString patternDirection(const QString& type);

QString navName(const QString& type);
QString surfaceName(const QString& surface);
QString parkingGateName(const QString& gate);
QString parkingRampName(const QString& ramp);
QString parkingTypeName(const QString& type);
QString parkingName(const QString& name);
QString parkingNameNumberType(const map::MapParking& parking);
QString startType(const map::MapStart& start);

/* Parking name from PLN to database name */
QString parkingDatabaseName(const QString& name);

/* Get short name for a parking spot */
QString parkingShortName(const QString& name);

/* Parking description as needed in the PLN files */
QString parkingNameForFlightplan(const MapParking& parking);

QString airspaceTypeToString(map::MapAirspaceTypes type);
QString airspaceRemark(map::MapAirspaceTypes type);
int airspaceDrawingOrder(map::MapAirspaceTypes type);

map::MapAirspaceTypes airspaceTypeFromDatabase(const QString& type);
QString airspaceTypeToDatabase(map::MapAirspaceTypes type);

QString airwayTypeToShortString(map::MapAirwayType type);
QString airwayTypeToString(map::MapAirwayType type);
MapAirwayType  airwayTypeFromString(const QString& typeStr);
QString comTypeName(const QString& type);

QString airportText(const map::MapAirport& airport);
QString airportTextShort(const map::MapAirport& airport);
QString vorFullShortText(const map::MapVor& vor);
QString vorText(const map::MapVor& vor);
QString vorType(const map::MapVor& vor);
QString ndbFullShortText(const map::MapNdb& ndb);
QString ndbText(const map::MapNdb& ndb);
QString waypointText(const map::MapWaypoint& waypoint);
QString userpointText(const map::MapUserpoint& userpoint);
QString airwayText(const map::MapAirway& airway);
QString magvarText(float magvar);

/* Get a number for surface quality to get the best runway. Higher numbers are better surface. */
int surfaceQuality(const QString& surface);

} // namespace types

Q_DECLARE_TYPEINFO(map::MapObjectRef, Q_PRIMITIVE_TYPE);

Q_DECLARE_TYPEINFO(map::MapAirport, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapRunway, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapRunwayEnd, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapApron, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapTaxiPath, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapParking, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapStart, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapHelipad, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapVor, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapNdb, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapWaypoint, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapAirwayWaypoint, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapAirway, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapMarker, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapIls, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapUserpoint, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapSearchResult, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::PosCourse, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(map::MapAirspace, Q_MOVABLE_TYPE);

Q_DECLARE_TYPEINFO(map::RangeMarker, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(map::RangeMarker);
Q_DECLARE_TYPEINFO(map::DistanceMarker, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(map::DistanceMarker);

#endif // LITTLENAVMAP_MAPTYPES_H
