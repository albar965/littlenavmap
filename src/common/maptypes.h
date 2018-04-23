/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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
bool navTypeTacan(const QString& navType);
bool navTypeVortac(const QString& navType);

/* Check surface attributes */
bool isHardSurface(const QString& surface);
bool isWaterSurface(const QString& surface);
bool isSoftSurface(const QString& surface);

/* Airport type not including runways (have to queried separately) */
struct MapAirport
{
  QString ident, /* ICAO ident*/ name, region;
  int id; /* Database id airport.airport_id */
  int longestRunwayLength = 0, longestRunwayHeading = 0, transitionAltitude = 0;
  int rating = -1;
  map::MapAirportFlags flags = AP_NONE;
  float magvar = 0; /* Magnetic variance - positive is east, negative is west */
  bool navdata, /* true if source is third party nav database, false if source is simulator data */
       xplane; /* true if data source is X-Plane */

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
  QString surface, shoulder, primaryName, secondaryName, edgeLight;
  int length, primaryEndId, secondaryEndId;
  float heading;
  int width,
      primaryOffset, secondaryOffset, /* part of the runway length */
      primaryBlastPad, secondaryBlastPad, primaryOverrun, secondaryOverrun; /* not part of the runway length */
  atools::geo::Pos position, primaryPosition, secondaryPosition;

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
  bool navdata; /* true if source is third party nav database, false if source is simulator data */

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
  /* FSX/P3D simple geometry */
  atools::geo::LineString vertices;

  /* X-Plane complex geometry including curves and holes */
  atools::fs::common::XpGeo geometry;

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
  int number, /* -1 for X-Plane style free names. Otherwise FSX/P3D number */
      radius, heading;
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
  QString surface, type, runwayName;
  atools::geo::Pos position;
  int id, startId, airportId, length, width, heading, start;
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
    return id;
  }

};

/* VOR station */
struct MapVor
{
  QString ident, region, type /* HIGH, LOW, TERMINAL */, name /*, airportIdent*/;
  int id; /* database id vor.vor_id*/
  float magvar;
  int frequency /* MHz * 1000 */, range /* nm */;
  QString channel;
  atools::geo::Pos position;
  int routeIndex = -1; /* Filled by the get nearest methods for building the context menu */
  bool dmeOnly, hasDme, tacan, vortac;

  QString getFrequencyOrChannel() const
  {
    if(frequency > 0)
      return QString::number(frequency);
    else
      return channel;
  }

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
struct MapUserpointRoute
{
  QString name;
  int id; /* Sequence number as it was added to the flight plan */
  float magvar;
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

/* User defined waypoint from the user database */
struct MapUserpoint
{
  QString name, ident, region, type, description, tags;
  int id;
  atools::geo::Pos position;
  bool temp = false;

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

enum MapAirwayDirection
{
  /* 0 = both, 1 = forward only (from -> to), 2 = backward only (to -> from) */
  DIR_BOTH = 0,
  DIR_FORWARD = 1,
  DIR_BACKWARD = 2
};

/* Airway segment */
struct MapAirway
{
  QString name;
  map::MapAirwayType type;
  int id, fromWaypointId, toWaypointId /* all database ids */;
  MapAirwayDirection direction;
  int minAltitude, maxAltitude /* feet */,
      sequence /* segment sequence in airway */,
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
  QString type, ident;
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
  QString ident, name, region;
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

/* Airspace boundary */
struct MapAirspace
{
  int id;
  int minAltitude, maxAltitude;
  QString name, /* Airspace name or callsign for online ATC */
          comName, comType, minAltitudeType, maxAltitudeType;
  QVector<int> comFrequencies;
  map::MapAirspaceTypes type;
  bool online = false;

  atools::geo::Rect bounding;

  atools::geo::Pos getPosition() const
  {
    return bounding.getCenter();
  }

  bool isValid() const
  {
    return bounding.isValid();
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
  QList<MapUserpointRoute> userPointsRoute;

  /* User defined waypoints */
  QList<MapUserpoint> userpoints;
  QSet<int> userpointIds; /* Ids used to deduplicate */

  QList<atools::fs::sc::SimConnectAircraft> aiAircraft;
  atools::fs::sc::SimConnectUserAircraft userAircraft;

  QList<atools::fs::sc::SimConnectAircraft> onlineAircraft;
  QSet<int> onlineAircraftIds; /* Ids used to deduplicate */

  bool isEmpty(const map::MapObjectTypes& types) const;

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

QDebug operator<<(QDebug out, const map::MapSearchResult& record);

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

  bool isRhumbLine;

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

/* Database type strings to GUI strings and map objects to display strings */
QString navTypeName(const QString& type);
const QString& navTypeNameVor(const QString& type);
const QString& navTypeNameVorLong(const QString& type);
const QString& navTypeNameNdb(const QString& type);
const QString& navTypeNameWaypoint(const QString& type);

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

/* Compare runway numbers fuzzy */
bool runwayAlmostEqual(const QString& name1, const QString& name2);

/* Parking name from PLN to database name */
const QString& parkingDatabaseName(const QString& name);

/* Get short name for a parking spot */
QString parkingShortName(const QString& name);

/* Parking description as needed in the PLN files */
QString parkingNameForFlightplan(const MapParking& parking);

const QString& airspaceTypeToString(map::MapAirspaceTypes type);
const QString& airspaceFlagToString(map::MapAirspaceFlags type);
const QString& airspaceRemark(map::MapAirspaceTypes type);
int airspaceDrawingOrder(map::MapAirspaceTypes type);

map::MapAirspaceTypes airspaceTypeFromDatabase(const QString& type);
const QString& airspaceTypeToDatabase(map::MapAirspaceTypes type);

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
QString userpointRouteText(const map::MapUserpointRoute& userpoint);
QString userpointText(const MapUserpoint& userpoint);
QString airwayText(const map::MapAirway& airway);

/* Altitude text for airways */
QString airwayAltText(const MapAirway& airway);

/* Short for map display */
QString airwayAltTextShort(const MapAirway& airway);

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

Q_DECLARE_TYPEINFO(map::RangeMarker, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(map::RangeMarker);
Q_DECLARE_TYPEINFO(map::DistanceMarker, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(map::DistanceMarker);

#endif // LITTLENAVMAP_MAPTYPES_H
