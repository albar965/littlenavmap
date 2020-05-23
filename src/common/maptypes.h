/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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
#include "common/mapflags.h"
#include "geo/linestring.h"
#include "fs/sc/simconnectuseraircraft.h"
#include "fs/weather/weathertypes.h"
#include "fs/common/xpgeometry.h"

#include <QColor>
#include <QString>

class OptionData;

namespace proc {

struct MapProcedurePoint;

}
/*
 * Maptypes are mostly filled from database tables and are used to pass airport, navaid and more information
 * around in the program. The types are kept primitive (no inheritance no vtable) for performance reasons.
 * Units are usually feet. Type string are as they appear in the database.
 */
namespace map {

/* Initialize all text that are translateable after loading the translation files */
void initTranslateableTexts();

// =====================================================================
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

// =====================================================================
/* Primitive id type combo that is hashable and comparable */
struct MapObjectRef
{
  MapObjectRef()
    : id(-1), objType(NONE)
  {

  }

  MapObjectRef(int idParam, map::MapObjectType typeParam)
    : id(idParam), objType(typeParam)
  {
  }

  MapObjectRef(int idParam, map::MapObjectTypes typeParam)
    : id(idParam), objType(static_cast<map::MapObjectType>(typeParam.operator unsigned int()))
  {
  }

  /* Database id or -1 if not applicable */
  int id;

  /* Use simple type information to avoid vtable and RTTI overhead. Avoid QFlags wrapper here. */
  map::MapObjectType objType;

  bool operator==(const map::MapObjectRef& other) const
  {
    return objType == other.objType && id == other.id;
  }

  bool operator!=(const map::MapObjectRef& other) const
  {
    return !operator==(other);
  }

};

QDebug operator<<(QDebug out, const map::MapObjectRef& ref);

inline uint qHash(const map::MapObjectRef& type)
{
  return static_cast<uint>(type.id) ^ static_cast<uint>(type.objType);
}

typedef QVector<MapObjectRef> MapObjectRefVector;

// =====================================================================
/* Extended reference type that also covers coordinates and name.
 *  Hashable and comparable.*/
struct MapObjectRefExt
  : public MapObjectRef
{
  MapObjectRefExt()
  {
  }

  MapObjectRefExt(int idParam, map::MapObjectType typeParam)
    : MapObjectRef(idParam, typeParam)
  {
  }

  MapObjectRefExt(int idParam, const atools::geo::Pos posParam, map::MapObjectType typeParam)
    : MapObjectRef(idParam, typeParam), position(posParam)
  {
  }

  MapObjectRefExt(int idParam, map::MapObjectType typeParam, const QString& nameParam)
    : MapObjectRef(idParam, typeParam), name(nameParam)
  {
  }

  MapObjectRefExt(int idParam, const atools::geo::Pos posParam, map::MapObjectType typeParam, const QString& nameParam)
    : MapObjectRef(idParam, typeParam), position(posParam), name(nameParam)
  {
  }

  /* Always valid for USERPOINTROUTE and always filled for all types in results of RouteStringReader. */
  atools::geo::Pos position;

  /* Original name or coordinate format string for user points */
  QString name;

  bool operator==(const map::MapObjectRefExt& other) const
  {
    return objType == map::USERPOINT || objType == map::USERPOINTROUTE ?
           position == other.position : MapObjectRef::operator==(other);
  }

  bool operator!=(const map::MapObjectRefExt& other) const
  {
    return !operator==(other);
  }

};

QDebug operator<<(QDebug out, const map::MapObjectRefExt& ref);

inline uint qHash(const map::MapObjectRefExt& type)
{
  return qHash(static_cast<map::MapObjectRef>(type)) ^ qHash(type.position);
}

typedef QVector<MapObjectRefExt> MapObjectRefExtVector;

// =====================================================================
/* Convert type from nav_search table to enum */
map::MapObjectTypes navTypeToMapObjectType(const QString& navType);
bool navTypeTacan(const QString& navType);
bool navTypeVortac(const QString& navType);

/* Check surface attributes */
bool isHardSurface(const QString& surface);
bool isWaterSurface(const QString& surface);
bool isSoftSurface(const QString& surface);

// =====================================================================
/* Base struct for all map objects covering id and position
 * Position is used to check for validity, i.e. not initialized objects
 * Object type can be NONE if no polymorphism is needed */
struct MapBase
{
  MapBase(map::MapObjectType type)
    : objType(type)
  {
  }

  int id;
  atools::geo::Pos position;

  /* Use simple type information to avoid vtable and RTTI overhead. Avoid QFlags wrapper here. */
  map::MapObjectType objType;

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

  map::MapObjectRef getRef() const
  {
    return map::MapObjectRef(id, objType);
  }

  map::MapObjectRefExt getRefExt() const
  {
    return map::MapObjectRefExt(id, position, objType);
  }

  map::MapObjectRefExt getRefExt(const QString& name) const
  {
    return map::MapObjectRefExt(id, position, objType, name);
  }

  template<typename TYPE>
  const TYPE *asType(map::MapObjectTypes type) const
  {
    if(objType == type)
      return static_cast<const TYPE *>(this);
    else
      return nullptr;
  }

  bool operator==(const map::MapBase& other) const
  {
    return id == other.id && objType == other.objType;
  }

  bool operator!=(const map::MapBase& other) const
  {
    return !operator==(other);
  }

  /* Set type using QFlags wrapper */
  void setType(map::MapObjectTypes type)
  {
    objType = static_cast<map::MapObjectType>(type.operator unsigned int());
  }

  /* Get type using QFlags wrapper */
  map::MapObjectTypes getType() const
  {
    return objType;
  }

};

QDebug operator<<(QDebug out, const map::MapBase& obj);

// =====================================================================
/* Airport type not including runways (have to queried separately) */
/* Database id airport.airport_id */
struct MapAirport
  : public MapBase
{
  MapAirport() : MapBase(map::AIRPORT)
  {
  }

  QString ident, /* Ident in simulator mostly ICAO */
           icao, /* Real ICAO ident */
           iata, /* IATA ident */
           name, /* Full name */
           region; /* Two letter region code */
  int longestRunwayLength = 0, longestRunwayHeading = 0, transitionAltitude = 0;
  int rating = -1;
  map::MapAirportFlags flags = AP_NONE;
  float magvar = 0; /* Magnetic variance - positive is east, negative is west */
  bool navdata, /* true if source is third party nav database, false if source is simulator data */
       xplane; /* true if data source is X-Plane */

  int towerFrequency = 0, atisFrequency = 0, awosFrequency = 0, asosFrequency = 0, unicomFrequency = 0;
  atools::geo::Pos towerCoords;
  atools::geo::Rect bounding;
  int routeIndex = -1;

  const QString& icaoIdent()
  {
    return !icao.isEmpty() ? icao : ident;
  }

  bool closed() const;
  bool hard() const;
  bool soft() const;
  bool water() const;
  bool lighted() const;
  bool helipad() const;
  bool softOnly() const;
  bool waterOnly() const;
  bool helipadOnly() const;
  bool noRunways() const;
  bool tower() const;
  bool addon() const;
  bool is3d() const;
  bool anyFuel() const;
  bool complete() const;
  bool towerObject() const;
  bool apron() const;
  bool taxiway() const;
  bool parking() const;
  bool als() const;
  bool vasi() const;
  bool fence() const;
  bool closedRunways() const;
  bool procedure() const;

  /* Check if airport should be drawn empty */
  bool emptyDraw() const;
  bool emptyDraw(const OptionData& od) const;

  /* Check if airport has any scenery elements */
  bool empty() const;

  /*
   * @param objectTypes Map display configuration flags
   * @return true if this airport is visible on map
   */
  bool isVisible(map::MapObjectTypes objectTypes) const;

};

// =====================================================================
/* Airport runway. All dimensions are feet */
/* Datbase id is runway.runway_id */
struct MapRunway
  : public MapBase
{
  MapRunway() : MapBase(map::NONE)
  {
  }

  QString surface, shoulder, primaryName, secondaryName, edgeLight;
  int length /* ft */, primaryEndId, secondaryEndId;
  float heading, patternAlt;
  int width,
      primaryOffset, secondaryOffset, /* part of the runway length */
      primaryBlastPad, secondaryBlastPad, primaryOverrun, secondaryOverrun; /* not part of the runway length all in ft */
  atools::geo::Pos primaryPosition, secondaryPosition;

  /* Used by AirportQuery::getRunways */
  int airportId;

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

  bool isLighted() const
  {
    return !edgeLight.isEmpty();
  }

  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

};

// =====================================================================
/* Airport runway end. All dimensions are feet */
/* Database id is runway_end.runway_end_id */
struct MapRunwayEnd
  : public MapBase
{
  MapRunwayEnd() : MapBase(map::RUNWAYEND)
  {
  }

  QString name, leftVasiType, rightVasiType, pattern;
  float heading, /* degree true */
        leftVasiPitch = 0.f, rightVasiPitch = 0.f;
  bool secondary;
  bool navdata; /* true if source is third party nav database, false if source is simulator data */
};

// =====================================================================
/* Apron including full geometry */
struct MapApron
  : public MapBase
{
  MapApron() : MapBase(map::NONE)
  {
  }

  /* FSX/P3D simple geometry */
  atools::geo::LineString vertices;

  /* X-Plane complex geometry including curves and holes */
  atools::fs::common::XpGeo geometry;

  QString surface;
  bool drawSurface;
};

// =====================================================================
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

// =====================================================================
/* Gate, GA ramp, cargo ramps, etc. */
/* database id parking.parking_id */
struct MapParking
  : public MapBase
{
  MapParking() : MapBase(map::PARKING)
  {
  }

  QString type, name, airlineCodes /* Comma separated list of airline codes */;
  int airportId /* database id airport.airport_id */;
  int number, /* -1 for X-Plane style free names. Otherwise FSX/P3D number */
      radius, heading;
  bool jetway;
};

// =====================================================================
/* Start position (runway, helipad or parking */
/* database id start.start_id */
struct MapStart
  : public MapBase
{
  MapStart() : MapBase(map::NONE)
  {
  }

  QString type /* RUNWAY, HELIPAD or WATER */, runwayName /* not empty if this is a runway start */;
  int airportId /* database id airport.airport_id */;
  int heading, helipadNumber /* -1 if not a helipad otherwise sequence number as it appeared in the BGL */;
};

// =====================================================================
/* Airport helipad */
struct MapHelipad
  : public MapBase
{
  MapHelipad() : MapBase(map::HELIPAD)
  {
  }

  QString surface, type, runwayName;
  int startId, airportId, length, width, heading, start;
  bool closed, transparent;
};

// =====================================================================
/* VOR station */
/* database id vor.vor_id */
struct MapVor
  : public MapBase
{
  MapVor() : MapBase(map::VOR)
  {
  }

  QString ident, region, type /* HIGH, LOW, TERMINAL */, name /*, airportIdent*/;
  float magvar;
  int frequency /* MHz * 1000 */, range /* nm */;
  QString channel;
  int routeIndex = -1; /* Filled by the get nearest methods for building the context menu */
  bool dmeOnly, hasDme, tacan, vortac;

  /* true if this is valid and a real VOR with calibration (VOR, VORDME or VORTAC) */
  bool isCalibratedVor() const
  {
    return isValid() && !tacan && !dmeOnly;
  }

  QString getFrequencyOrChannel() const
  {
    if(frequency > 0)
      return QString::number(frequency);
    else
      return channel;
  }

};

// =====================================================================
/* NDB station */
/* database id ndb.ndb_id */
struct MapNdb
  : public MapBase
{
  MapNdb() : MapBase(map::NDB)
  {
  }

  QString ident, region, type /* HH, H, COMPASS_POINT, etc. */, name /*, airportIdent*/;
  float magvar;
  int frequency /* kHz * 100 */, range /* nm */;
  int routeIndex = -1; /* Filled by the get nearest methods for building the context menu */
};

// =====================================================================
/* database waypoint.waypoint_id */
struct MapWaypoint
  : public MapBase
{
  MapWaypoint() : MapBase(map::WAYPOINT)
  {
  }

  float magvar;
  QString ident, region, type /* NAMED, UNAMED, etc. *//*, airportIdent*/;
  int routeIndex = -1; /* Filled by the get nearest methods for building the context menu */

  bool hasVictorAirways = false, hasJetAirways = false, hasTracks = false;
};

/* Waypoint or intersection */
struct MapAirwayWaypoint
{
  map::MapWaypoint waypoint;
  int airwayId, airwayFragmentId, seqNum;
};

// =====================================================================
/* User defined waypoint of a flight plan */
/* Id is Sequence number as it was added to the flight plan */
struct MapUserpointRoute
  : public MapBase
{
  MapUserpointRoute() : MapBase(map::USERPOINTROUTE)
  {
  }

  QString ident, region, name, comment;
  float magvar;
  int routeIndex = -1; /* Filled by the get nearest methods for building the context menu */
};

// =====================================================================
/* User defined waypoint from the user database */
struct MapUserpoint
  : public MapBase
{
  MapUserpoint() : MapBase(map::USERPOINT)
  {
  }

  QString name, ident, region, type, description, tags;
  bool temp = false;
};

// =====================================================================
/* Logbook entry */
struct MapLogbookEntry
  : public MapBase
{
  MapLogbookEntry() : MapBase(map::LOGBOOK)
  {
  }

  QString departureName, departureIdent, departureRunway,
           destinationName, destinationIdent, destinationRunway,
           description, simulator, aircraftType,
           aircraftRegistration, routeString, routeFile, perfFile;
  float distance, distanceGc;
  atools::geo::Pos departurePos, destinationPos;

  map::MapAirport departure, destination;

  atools::geo::LineString lineString() const
  {
    atools::geo::LineString l(departurePos, destinationPos);
    l.removeInvalid();
    return l;
  }

  atools::geo::Line line() const
  {
    if(departurePos.isValid() && destinationPos.isValid())
      return atools::geo::Line(departurePos, destinationPos);
    else if(departurePos.isValid())
      return atools::geo::Line(departurePos);
    else if(destinationPos.isValid())
      return atools::geo::Line(destinationPos);

    return atools::geo::Line();
  }

  atools::geo::Rect bounding() const
  {
    atools::geo::Rect rect(departurePos);
    rect.extend(destinationPos);
    return rect;
  }

  bool isDestAndDepartPosValid() const
  {
    return departurePos.isValid() && destinationPos.isValid();
  }

  bool isDestOrDepartPosValid() const
  {
    return departurePos.isValid() || destinationPos.isValid();
  }

};

// Airways =====================================================================
/* Airway and track type */
enum MapAirwayTrackType
{
  NO_AIRWAY,
  AIRWAY_VICTOR,
  AIRWAY_JET,
  AIRWAY_BOTH,
  TRACK_NAT,
  TRACK_PACOTS,
  TRACK_AUSOTS
};

enum MapAirwayDirection
{
  /* 0 = both, 1 = forward only (from -> to), 2 = backward only (to -> from) */
  DIR_BOTH = 0,
  DIR_FORWARD = 1,
  DIR_BACKWARD = 2
};

enum MapAirwayRouteType
{
  RT_NONE,
  RT_AIRLINE, /* A Airline Airway (Tailored Data) */
  RT_CONTROL, /* C Control (appears in DFD) */
  RT_DIRECT, /* D Direct Route */
  RT_HELICOPTER, /* H Helicopter Airways */
  RT_OFFICIAL, /* O Officially Designated Airways, except RNAV, Helicopter Airways (appears in DFD) */
  RT_RNAV, /* R RNAV Airways (appears in DFD) */
  RT_UNDESIGNATED, /* S Undesignated ATS Route */
  RT_TRACK /* S Undesignated ATS Route */
};

/* Airway segment or part of NAT, PACOTS or AUSOTS track */
struct MapAirway
  : public MapBase
{
  MapAirway() : MapBase(map::AIRWAY)
  {
  }

  bool isTrack() const
  {
    return routeType == RT_TRACK;
  }

  bool isAirway() const
  {
    return !isTrack();
  }

  QString name;
  map::MapAirwayTrackType type;
  map::MapAirwayRouteType routeType;
  int fromWaypointId, toWaypointId /* all database ids */,
      airwayId /* Mirrored from underlying airways for tracks */;
  MapAirwayDirection direction;
  int minAltitude, maxAltitude /* feet */,
      sequence /* segment sequence in airway */,
      fragment /* fragment number of disconnected airways with the same name */;
  QVector<quint16> altitudeLevelsEast, altitudeLevelsWest;
  atools::geo::Pos from, to;
  atools::geo::Rect bounding; /* pre calculated using from and to */

};

// =====================================================================
/* Marker beacon */
/* database id marker.marker_id */
struct MapMarker
  : public MapBase
{
  MapMarker() : MapBase(map::MARKER)
  {
  }

  QString type, ident;
  int heading;
};

// =====================================================================
/* ILS */
/* database id ils.ils_id */
struct MapIls
  : public MapBase
{
  MapIls() : MapBase(map::ILS)
  {
  }

  QString ident, name, region;
  float magvar, slope, heading, width;
  int frequency /* MHz * 1000 */, range /* nm */;

  /* MapBase pos is the origin (pointy end) */
  atools::geo::Pos pos1, /* Position 1 of the feather end */
                   pos2, /* Position 2 of the feather end */
                   posmid; /* Middle position of the feather end - depends on type ILS or LOC */
  atools::geo::Rect bounding;
  bool hasDme;

  atools::geo::LineString boundary() const;
  atools::geo::Line centerLine() const;

};

// =====================================================================
/* Airspace boundary */
struct MapAirspace
  : public MapBase
{
  MapAirspace() : MapBase(map::AIRSPACE)
  {
  }

  int minAltitude, maxAltitude;
  QString name, /* Airspace name or callsign for online ATC */
          comName, comType, minAltitudeType, maxAltitudeType,
          multipleCode /* A-Z if duplicates exist */,
          restrictiveDesignation /* Number or name to display together with type on the map only for restricted airspaces*/,
          restrictiveType /* Type of restricted airspace */,
          timeCode;
  /* timeCode: C active continuously, including holidays -  H active continuously, excluding holidays -
   *  N active not continuously - time not known - NULL active times announced by Notams
   *  U Unknown - do not display value */

  QVector<int> comFrequencies;
  map::MapAirspaceTypes type;
  map::MapAirspaceSources src;

  map::MapAirspaceId combinedId() const
  {
    return {id, src};
  }

  bool isOnline() const
  {
    return src & map::AIRSPACE_SRC_ONLINE;
  }

  bool isSim() const
  {
    return src & map::AIRSPACE_SRC_SIM;
  }

  bool isNav() const
  {
    return src & map::AIRSPACE_SRC_NAV;
  }

  bool isUser() const
  {
    return src & map::AIRSPACE_SRC_USER;
  }

  atools::geo::Rect bounding;
};

// =====================================================================
/* All information for complete traffic pattern structure */
/* Threshold position (end of final) and runway altitude MSL */
struct TrafficPattern
  : public MapBase
{
  TrafficPattern() : MapBase(map::NONE)
  {
  }

  QString airportIcao, runwayName;
  QColor color;
  bool turnRight,
       base45Degree /* calculate base turn from 45 deg after threshold */,
       showEntryExit /* Entry and exit indicators */;
  int runwayLength; /* ft Does not include displaced threshold */

  float downwindDistance, baseDistance; /* NM */
  float courseTrue; /* degree true final course*/
  float magvar;

  float magCourse() const;

};

QDataStream& operator>>(QDataStream& dataStream, map::TrafficPattern& obj);
QDataStream& operator<<(QDataStream& dataStream, const map::TrafficPattern& obj);

// =====================================================================
/* All information for a hold
 * position is hold reference position and altitude */
struct Hold
  : public MapBase
{
  Hold() : MapBase(map::NONE)
  {
  }

  QString navIdent; /* Only for display purposes */
  map::MapObjectTypes navType; /* AIRPORT, VOR, NDB or WAYPOINT*/
  bool vorDmeOnly, vorHasDme, vorTacan, vorVortac; /* VOR specific flags */
  QColor color;

  bool turnLeft; /* Standard is right */
  float minutes, speedKts; /* Used to calculate segment length - speed in knots */

  float courseTrue; /* degree true inbound course to fix */
  float magvar; /* Taken from environment or navaid */

  float magCourse() const;

  /* Distance in NM */
  float distance() const
  {
    return speedKts * minutes / 60.f;
  }

};

QDataStream& operator>>(QDataStream& dataStream, map::Hold& obj);
QDataStream& operator<<(QDataStream& dataStream, const map::Hold& obj);

// =====================================================================
/* Mixed search result for e.g. queries on a bounding rectangle for map display or for all get nearest methods */
struct MapSearchResult
{
  QList<MapAirport> airports;
  QSet<int> airportIds; /* Ids used to deduplicate when merging highlights and nearest */

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
  QList<MapUserpointRoute> userpointsRoute;

  /* User defined waypoints */
  QList<MapUserpoint> userpoints;
  QSet<int> userpointIds; /* Ids used to deduplicate */

  /* Logbook entries */
  QList<MapLogbookEntry> logbookEntries;

  QList<atools::fs::sc::SimConnectAircraft> aiAircraft;
  atools::fs::sc::SimConnectUserAircraft userAircraft;

  QList<atools::fs::sc::SimConnectAircraft> onlineAircraft;
  QSet<int> onlineAircraftIds; /* Ids used to deduplicate */

  atools::geo::Pos windPos;
  QList<map::Hold> holds;
  QList<map::TrafficPattern> trafficPatterns;

  QList<proc::MapProcedurePoint> procPoints;

  /* true if none of the types exists in this result */
  bool isEmpty(const map::MapObjectTypes& types = map::ALL) const
  {
    return size(types) == 0;
  }

  /* Number of map objects for the given types */
  int size(const map::MapObjectTypes& types = map::ALL) const;

  /* Get id and type from the result. Vector of types defines priority. true if something was found.
   * id is set to -1 if nothing was found. */
  bool getIdAndType(int& id, MapObjectTypes& type, const std::initializer_list<MapObjectTypes>& types) const;
  QString getIdent(const std::initializer_list<MapObjectTypes>& types) const;
  const atools::geo::Pos& getPosition(const std::initializer_list<MapObjectTypes>& types) const;

  /* Remove the given types only */
  void clear(const MapObjectTypes& types = map::ALL);

  void clearAllButFirst(const MapObjectTypes& types = map::ALL);

  /* Give online airspaces/centers priority */
  void moveOnlineAirspacesToFront();
  map::MapSearchResult moveOnlineAirspacesToFront() const;

  bool hasAirports() const
  {
    return !airports.isEmpty();
  }

  bool hasAirways() const
  {
    return !airways.isEmpty();
  }

  bool hasVor() const
  {
    return !vors.isEmpty();
  }

  bool hasNdb() const
  {
    return !ndbs.isEmpty();
  }

  bool hasUserpoints() const
  {
    return !userpoints.isEmpty();
  }

  bool hasUserpointsRoute() const
  {
    return !userpointsRoute.isEmpty();
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

  bool hasAirspaces() const
  {
    return !airspaces.isEmpty();
  }

  /* Special methods for the online and navdata airspaces which are stored mixed */
  bool hasSimNavUserAirspaces() const;
  bool hasOnlineAirspaces() const;
  void clearNavdataAirspaces();
  void clearOnlineAirspaces();
  MapAirspace *firstSimNavUserAirspace();
  MapAirspace *firstOnlineAirspace();
  int numSimNavUserAirspaces() const;
  int numOnlineAirspaces() const;

  QList<MapAirspace> getSimNavUserAirspaces() const;

  QList<MapAirspace> getOnlineAirspaces() const;

private:
  template<typename T>
  void clearAllButFirst(QList<T>& list);

};

QDebug operator<<(QDebug out, const map::MapSearchResult& record);

// =====================================================================
/* Mixed search result using inherited objects, Does not support aircraft objects */
/* Maintains only pointers to the original objects and creates a copy the MapSearchResult. */
struct MapSearchResultIndex
  : public QVector<const map::MapBase *>
{
  /* Add all result objects to list. Result and all objects are copied. */
  void addFromResult(const map::MapSearchResult& resultParm, const MapObjectTypes& types = map::ALL);

  /* Sort objects by distance to given position from closest to farthest */
  void sortByDistance(const atools::geo::Pos& pos, bool sortNearToFar);

  /* Remove all objects which are more far away  from pos than max distance */
  void removeByDistance(const atools::geo::Pos& pos, float maxDistanceNm);

  void clearAll()
  {
    clear();
    result.clear();
  }

  const map::MapSearchResult& getResult() const
  {
    return result;
  }

private:
  template<typename TYPE>
  void addAll(const QList<TYPE>& list)
  {
    for(const TYPE& obj : list)
      append(&obj);
  }

  map::MapSearchResult result;
};

// =====================================================================
/* Range rings marker. Can be converted to QVariant */
struct RangeMarker
{
  QString text; /* Text to display like VOR name and frequency */
  QVector<int> ranges; /* Range ring list (nm) */
  atools::geo::Pos center;
  MapObjectTypes type; /* VOR, NDB, AIRPORT, etc. */

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

// =====================================================================
/* Distance measurement line. Can be converted to QVariant */
struct DistanceMarker
{
  QString text; /* Text to display like VOR name and frequency */
  QColor color; /* Line color depends on origin (airport or navaid type */
  atools::geo::Pos from, to;
  float magvar;

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

// =====================================================================
/* Stores last METARs to avoid unneeded updates in widget */
struct WeatherContext
{
  atools::fs::weather::MetarResult fsMetar, ivaoMetar, noaaMetar;
  bool isAsDeparture = false, isAsDestination = false;
  QString asMetar, asType, vatsimMetar, ident;

  bool isEmpty() const
  {
    return fsMetar.isEmpty() && asMetar.isEmpty() && noaaMetar.isEmpty() && vatsimMetar.isEmpty() &&
           ivaoMetar.isEmpty();
  }

};

QDebug operator<<(QDebug out, const map::WeatherContext& record);

// =====================================================================================
/* Database type strings to GUI strings and map objects to display strings */
QString navTypeName(const QString& type);
const QString& navTypeNameVor(const QString& type);
const QString& navTypeNameVorLong(const QString& type);
const QString& navTypeNameNdb(const QString& type);
const QString& navTypeNameWaypoint(const QString& type);

QString ilsText(const map::MapIls& ils);
QString ilsType(const MapIls& ils);
QString ilsTextShort(const MapIls& ils);
QString ilsTextShort(QString ident, QString name, bool gs, bool dme);

QString edgeLights(const QString& type);
QString patternDirection(const QString& type);

const QString& navName(const QString& type);
const QString& surfaceName(const QString& surface);
const QString& parkingGateName(const QString& gate);
const QString& parkingRampName(const QString& ramp);
const QString& parkingTypeName(const QString& type);
const QString& parkingName(const QString& name);
QString parkingNameNumberType(const map::MapParking& parking);
QString startType(const map::MapStart& start);

/* Split runway name into parts and return true if name matches a runway number */
bool runwayNameSplit(const QString& name, int *number = nullptr, QString *designator = nullptr);
bool runwayNameSplit(const QString& name, QString *number = nullptr, QString *designator = nullptr);

/* Get the closes matching runway name from the list of airport runways or empty if none */
QString runwayBestFit(const QString& procRunwayName, const QStringList& airportRunwayNames);

/* Gives all variants of the runway (+1 and -1) plus the original one as the first in the list.
 *  Can deal with prefix "RW" and keeps it. */
QStringList runwayNameVariants(QString name);

/* Returns all variants of zero prefixed runways 09 vs 9
 *  Can deal with prefix "RW" and keeps it. */
QStringList runwayNameZeroPrefixVariants(QString name);

/* Gives all variants of the runway (+1 and -1) plus the original one as the first in the list for an
 * ARINC name like N32 or I19-Y */
QStringList arincNameNameVariants(const QString& name);

/* Compare runway numbers fuzzy by ignoring a deviation of one */
bool runwayAlmostEqual(const QString& name1, const QString& name2);

/* Compare runway numbers by ignoring leading zero */
bool runwayEqual(QString name1, QString name2);

/* Parking name from PLN to database name */
const QString& parkingDatabaseName(const QString& name);

/* Get short name for a parking spot */
QString parkingShortName(const QString& name);

/* Parking description as needed in the PLN files */
QString parkingNameForFlightplan(const MapParking& parking);

const QString& airspaceTypeToString(map::MapAirspaceTypes type);
const QString& airspaceFlagToString(map::MapAirspaceFlags type);
QString mapObjectTypeToString(MapObjectTypes type); /* For debugging purposes. */
const QString& airspaceRemark(map::MapAirspaceTypes type);
int airspaceDrawingOrder(map::MapAirspaceTypes type);
QString airspaceSourceText(map::MapAirspaceSources src);

map::MapAirspaceTypes airspaceTypeFromDatabase(const QString& type);
const QString& airspaceTypeToDatabase(map::MapAirspaceTypes type);

QString airwayTrackTypeToShortString(map::MapAirwayTrackType type); /* Also used when writing to db for tracks */
QString airwayTrackTypeToString(map::MapAirwayTrackType type);
QString airwayRouteTypeToString(map::MapAirwayRouteType type);
QString airwayRouteTypeToStringShort(map::MapAirwayRouteType type); /* Also used when writing to db for tracks */
map::MapAirwayTrackType  airwayTrackTypeFromString(const QString& typeStr);
map::MapAirwayRouteType  airwayRouteTypeFromString(const QString& typeStr);
QString comTypeName(const QString& type);

QString airportText(const map::MapAirport& airport, int elideName = 1000);
QString airportTextShort(const map::MapAirport& airport);
QString vorFullShortText(const map::MapVor& vor);
QString vorText(const map::MapVor& vor);
QString vorTextShort(const MapVor& vor);
QString vorType(const map::MapVor& vor);
QString vorType(bool dmeOnly, bool hasDme, bool tacan, bool vortac);
QString ndbFullShortText(const map::MapNdb& ndb);
QString ndbText(const map::MapNdb& ndb);
QString ndbTextShort(const MapNdb& ndb);
QString waypointText(const map::MapWaypoint& waypoint);
QString userpointRouteText(const map::MapUserpointRoute& userpoint);
QString userpointText(const MapUserpoint& userpoint);
QString logEntryText(const MapLogbookEntry& logEntry);
QString airwayText(const map::MapAirway& airway);

/* Altitude text for airways */
QString airwayAltText(const MapAirway& airway);

/* Short for map display */
QString airwayAltTextShort(const MapAirway& airway, bool addUnit = true, bool narrow = true);

QString magvarText(float magvar, bool shortText = false);

/* Get a number for surface quality to get the best runway. Higher numbers are better surface. */
int surfaceQuality(const QString& surface);

void updateUnits();

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
Q_DECLARE_TYPEINFO(map::MapUserpointRoute, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapSearchResult, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::PosCourse, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(map::MapAirspace, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(map::MapLogbookEntry, Q_MOVABLE_TYPE);

Q_DECLARE_TYPEINFO(map::RangeMarker, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(map::RangeMarker);
Q_DECLARE_TYPEINFO(map::DistanceMarker, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(map::DistanceMarker);
Q_DECLARE_TYPEINFO(map::TrafficPattern, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(map::TrafficPattern);
Q_DECLARE_TYPEINFO(map::Hold, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(map::Hold);

#endif // LITTLENAVMAP_MAPTYPES_H
