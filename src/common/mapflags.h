/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#ifndef LITTLENAVMAP_MAPFLAGS_H
#define LITTLENAVMAP_MAPFLAGS_H

#include "util/flags.h"

#include <QVector>
#include <QObject>

/*
 * Maptypes are mostly filled from database tables and are used to pass airport, navaid and more information
 * around in the program. The types are kept primitive (no inheritance no vtable) for performance reasons.
 * Units are usually feet. Type string are as they appear in the database.
 */
namespace map {

/* Value for invalid/not found distances */
Q_DECL_CONSTEXPR static float INVALID_COURSE_VALUE = std::numeric_limits<float>::max();
Q_DECL_CONSTEXPR static float INVALID_HEADING_VALUE = INVALID_COURSE_VALUE;
Q_DECL_CONSTEXPR static float INVALID_ANGLE_VALUE = INVALID_COURSE_VALUE;
Q_DECL_CONSTEXPR static float INVALID_DISTANCE_VALUE = std::numeric_limits<float>::max();
Q_DECL_CONSTEXPR static float INVALID_ALTITUDE_VALUE = std::numeric_limits<float>::max();
Q_DECL_CONSTEXPR static float INVALID_SPEED_VALUE = std::numeric_limits<float>::max();
Q_DECL_CONSTEXPR static float INVALID_TIME_VALUE = std::numeric_limits<float>::max();
Q_DECL_CONSTEXPR static float INVALID_WEIGHT_VALUE = std::numeric_limits<float>::max();
Q_DECL_CONSTEXPR static float INVALID_VOLUME_VALUE = std::numeric_limits<float>::max();
Q_DECL_CONSTEXPR static float INVALID_LON_LAT_VALUE = std::numeric_limits<float>::max();
Q_DECL_CONSTEXPR static float INVALID_METAR_VALUE = std::numeric_limits<float>::max(); // Same as in metarparser.h
Q_DECL_CONSTEXPR static int INVALID_INDEX_VALUE = std::numeric_limits<int>::max();

Q_DECL_CONSTEXPR static float INVALID_MAGVAR = 9999.f;

Q_DECL_CONSTEXPR static float DEFAULT_ILS_WIDTH_DEG = 4.f;
Q_DECL_CONSTEXPR static float DEFAULT_GLS_RNP_WIDTH_DEG = 8.f;

/* minimum ground speed for fuel flow calculations and other */
Q_DECL_CONSTEXPR static float MIN_GROUND_SPEED = 30.f;

/* Do not draw barbs below this altitude */
Q_DECL_CONSTEXPR static float MIN_WIND_BARB_ALTITUDE = 4000.f;

/* Maximum number of objects to query and show on map */
Q_DECL_CONSTEXPR static int MAX_MAP_OBJECTS = 12000;

/* 64 bit Type covering all objects that are passed around in the program.
 * Partially used to determine what should be drawn.
 * These types are used in map::MapBase::objType.
 * Can be serialized to QVariant.
 * This state is not saved but filled by the saved state of the various actions at runtime. */
/* *INDENT-OFF* */
enum MapType : quint64
{
  NONE =             0,
  AIRPORT =          1ULL << 0, /* Master switch for airport display */
  RUNWAY =           1ULL << 1, /* Stores runways for queries */
  // 1 << 2, UNUSED
  // 1 << 3,
  // 1 << 4,
  VOR =              1ULL << 5, /* Also: DME, VORDME, VORTAC and TACAN */
  NDB =              1ULL << 6,
  ILS =              1ULL << 7, /* Type also covers GLS approaches */
  MARKER =           1ULL << 8,
  WAYPOINT =         1ULL << 9,
  AIRWAY =           1ULL << 10,
  AIRWAYV =          1ULL << 11,
  AIRWAYJ =          1ULL << 12,
  USER_FEATURE =     1ULL << 13,
  AIRCRAFT =         1ULL << 14, /* Simulator user aircraft */
  AIRCRAFT_AI =      1ULL << 15, /* AI or multiplayer simulator aircraft */
  AIRCRAFT_AI_SHIP = 1ULL << 16, /* AI or multiplayer simulator ship */
  AIRPORT_MSA =      1ULL << 17, /* Minimum safe altitude for airports and navaids - small icon */
  USERPOINTROUTE =   1ULL << 18, /* Flight plan user waypoint */
  PARKING =          1ULL << 19,
  START =            1ULL << 50,
  RUNWAYEND =        1ULL << 20,
  INVALID =          1ULL << 21, /* Flight plan waypoint not found in database */
  MISSED_APPROACH =  1ULL << 22, /* Only procedure type that can be hidden */
  PROCEDURE =        1ULL << 23, /* General procedure leg */
  AIRSPACE =         1ULL << 24, /* General airspace boundary, online or offline */
  HELIPAD =          1ULL << 25, /* Helipads on airports */
  HOLDING =          1ULL << 26, /* Enroute holds and user holds. User holds are enabled by MapMarkType below */
  USERPOINT =        1ULL << 27, /* A user defined waypoint - not used to define if should be drawn or not */
  TRACK =            1ULL << 28, /* NAT, PACOTS or AUSOTS track */
  AIRCRAFT_ONLINE =  1ULL << 29, /* Online network client/aircraft */
  LOGBOOK =          1ULL << 30, /* Logbook entry */

  /* Need to use constant expressions for long values above 32 bit */

  /* Mark state is saved in the MapMarkHandler ================================ */
  MARK_RANGE =           1ULL << 31, /* 1 << 31 All range rings */
  MARK_DISTANCE =        1ULL << 32, /* All measurement lines */
  MARK_HOLDING =         1ULL << 33, /* Holdings */
  MARK_PATTERNS =        1ULL << 34, /* Traffic patterns */
  MARK_MSA =             1ULL << 35, /* Airport MSA placed/highlighted by user */

  /* All marks */
  MARK_ALL = MARK_RANGE | MARK_DISTANCE | MARK_HOLDING | MARK_PATTERNS | MARK_MSA,

  /* Airport display flags ================================  */
  AIRPORT_HARD =         1ULL << 36, /* Display flag for airports having at least one hard runway */
  AIRPORT_SOFT =         1ULL << 37, /* Display flag for airports having only soft runways */
  AIRPORT_WATER =        1ULL << 38, /* Display flag for water only airports */
  AIRPORT_HELIPAD =      1ULL << 39, /* Display flag for helipad only airports */
  AIRPORT_EMPTY =        1ULL << 40, /* Filter flag for empty airports */
  // 1ULL << 41 / 0x0000'0200'0000'0000 FREE
  AIRPORT_UNLIGHTED =    1ULL << 42, /* Filter flag. Show airports having no lighting */
  AIRPORT_NO_PROCS =     1ULL << 43, /* Filter flag. Show airports without approach procedure */
  AIRPORT_CLOSED =       1ULL << 44, /* Filter flag. Show closed airports */
  AIRPORT_MILITARY =     1ULL << 46, /* Filter flag. Show military airports */

  /* Procedure flags ================================  */
  PROCEDURE_POINT =      1ULL << 45, /* Type flag for map base and context menu */

  AIRCRAFT_TRAIL =       1ULL << 47, /* Simulator aircraft track.  */

  /* Add-on airport flags ================================  */
  AIRPORT_ADDON_ZOOM =        1ULL << 48, /* Add-on airports override zoom distance but but not filter */
  AIRPORT_ADDON_ZOOM_FILTER = 1ULL << 49, /* Add-on airports override zoom distance and filters */


  // NEXT = 0x0001'0000'0000'0000 / 1ULL << 51 after START

  /* =============================================================================================== */
  /* Pure visibiliy flags. Nothing is shown if not at least one of these is set */
  AIRPORT_ALL_VISIBLE = AIRPORT_HARD | AIRPORT_SOFT | AIRPORT_WATER | AIRPORT_HELIPAD,

  /* Any add-on override */
  AIRPORT_ADDON_ANY = AIRPORT_ADDON_ZOOM | AIRPORT_ADDON_ZOOM_FILTER,

  /* All available filters in drop down button */
  AIRPORT_FILTER_ALL = AIRPORT_ALL_VISIBLE | AIRPORT_EMPTY | AIRPORT_UNLIGHTED | AIRPORT_NO_PROCS |
                       AIRPORT_CLOSED | AIRPORT_MILITARY | AIRPORT_ADDON_ANY,

  /* Visible and filter flags */
  AIRPORT_ALL = AIRPORT_ALL_VISIBLE | AIRPORT | AIRPORT_EMPTY | AIRPORT_UNLIGHTED | AIRPORT_NO_PROCS | AIRPORT_CLOSED | AIRPORT_MILITARY,

/* Used to mask airport values */
  AIRPORT_ALL_MASK = AIRPORT_ALL | AIRPORT_ADDON_ANY,

  /* Default value on first start */
  AIRPORT_DEFAULT = AIRPORT_ALL | AIRPORT_ADDON_ZOOM_FILTER,

  /* All online, AI and multiplayer aircraft ========================================= */
  AIRCRAFT_ALL = AIRCRAFT | AIRCRAFT_AI | AIRCRAFT_AI_SHIP | AIRCRAFT_ONLINE,

  /* All airways ==================================== */
  AIRWAY_ALL = AIRWAY | AIRWAYV | AIRWAYJ,

  /* All navaids ======================================== */
  NAV_ALL = VOR | NDB | WAYPOINT,

  /* All flight plan related types ======================================== */
  NAV_FLIGHTPLAN = NAV_ALL | AIRPORT | PROCEDURE_POINT | USERPOINTROUTE,

  /* All objects that have a magvar assigned */
  NAV_MAGVAR = AIRPORT | VOR | NDB | WAYPOINT,

  ALL = 0xffffffffffffffff
};
/* *INDENT-ON* */

ATOOLS_DECLARE_FLAGS_64(MapTypes, MapType)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(map::MapTypes)

QDebug operator<<(QDebug out, const map::MapType& type);
QDebug operator<<(QDebug out, const map::MapTypes& type);

/* Type that is used only for flags to determine what should be drawn.
 * Rarely used in other contexts and not as types in map::MapBase. */
enum MapDisplayType : quint32
{
  DISPLAY_TYPE_NONE = 0,
  AIRPORT_WEATHER = 1 << 0, /* Airport weather icons */
  MORA = 1 << 1, /* MORA (minimum off route altitude) */

  WIND_BARBS = 1 << 2, /* Wind barbs grid */
  WIND_BARBS_ROUTE = 1 << 3, /* Wind barbs at flight plan waypoints */

  LOGBOOK_DIRECT = 1 << 4, /* GC direct line in logbook entry highlight */
  LOGBOOK_ROUTE = 1 << 5, /* Route in logbook entry highlight */
  LOGBOOK_TRACK = 1 << 6, /* Track in logbook entry highlight */

  COMPASS_ROSE = 1 << 7, /* Compass rose */
  COMPASS_ROSE_ATTACH = 1 << 8, /* Attach to user aircraft */

  FLIGHTPLAN = 1 << 9, /* Flight plan */
  FLIGHTPLAN_TOC_TOD = 1 << 10, /* Top of climb and top of descent */
  FLIGHTPLAN_ALTERNATE = 1 << 21, /* Alternate airport and lines in flightplan */

  GLS = 1 << 13, /* RNV approach, GLS approache or GBAS path - only display flag. Object is stored with type ILS. */

  AIRCRAFT_ENDURANCE = 1 << 18, /* Range ring for current aircraft endurance. */

  AIRCRAFT_SELECTED_ALT_RANGE = 1 << 19, /* Altitude range for selected autopilot altitude ("green banana"). */
  AIRCRAFT_TURN_PATH = 1 << 20, /* Turn path at aircraft */

  LOGBOOK_ALL = LOGBOOK_DIRECT | LOGBOOK_ROUTE | LOGBOOK_TRACK
};

ATOOLS_DECLARE_FLAGS_32(MapDisplayTypes, map::MapDisplayType)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(map::MapDisplayTypes)

QDebug operator<<(QDebug out, const map::MapDisplayTypes& type);

/* Query type for all getNearest and other functions. Covers all what is not included in MapObjectTypes */
enum MapObjectQueryType : quint32
{
  QUERY_NONE = 0,
  QUERY_PROC_POINTS = 1 << 0, /* Procedure points */
  QUERY_PROC_MISSED_POINTS = 1 << 1, /* Missed procedure points */
  QUERY_MARK_HOLDINGS = 1 << 2, /* User defined holdings */
  QUERY_MARK_PATTERNS = 1 << 3, /* Traffic patterns */
  QUERY_PROCEDURES = 1 << 4, /* Procedure navaids when querying route */
  QUERY_PROCEDURES_MISSED = 1 << 5, /* Missed procedure navaids when querying route */
  QUERY_MARK_RANGE = 1 << 6, /* Range rings */
  QUERY_MARK_MSA = 1 << 7, /* MSA sectors */
  QUERY_MARK_DISTANCE = 1 << 8, /* Measurement lines */
  QUERY_PREVIEW_PROC_POINTS = 1 << 9, /* Points from procedure preview */
  QUERY_PROC_RECOMMENDED = 1 << 10, /* Recommended navaids from procedures */
  QUERY_ALTERNATE = 1 << 11, /* Alternate airports in flight plan */
  QUERY_AIRCRAFT_TRAIL = 1 << 12, /* Aircraft trail */
  QUERY_AIRCRAFT_TRAIL_LOG = 1 << 13, /* Aircraft trail */

  /* All user creatable/placeable features */
  QUERY_MARK = QUERY_MARK_DISTANCE | QUERY_MARK_HOLDINGS | QUERY_MARK_PATTERNS | QUERY_MARK_RANGE | QUERY_MARK_MSA,
};

ATOOLS_DECLARE_FLAGS_32(MapObjectQueryTypes, map::MapObjectQueryType)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(map::MapObjectQueryTypes)

/* ================================================================================== */
/* Ident queries for airport defines which ident columns should be used in the lookup */
enum DistanceMarkerFlag : quint32
{
  DIST_MARK_NONE = 0,
  DIST_MARK_RADIAL = 1 << 0, /* Draw radial */
  DIST_MARK_MAGVAR = 1 << 1 /* Has calibrated declination */
};

ATOOLS_DECLARE_FLAGS_32(DistanceMarkerFlags, map::DistanceMarkerFlag)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(map::DistanceMarkerFlags)

/* ================================================================================== */
/* Ident queries for airport defines which ident columns should be used in the lookup */
enum AirportQueryFlag : quint32
{
  AP_QUERY_NONE = 0,
  AP_QUERY_IDENT = 1 << 0,
  AP_QUERY_ICAO = 1 << 1,
  AP_QUERY_IATA = 1 << 2,
  AP_QUERY_FAA = 1 << 3,
  AP_QUERY_LOCAL = 1 << 4,

  AP_QUERY_ALL = AP_QUERY_IDENT | AP_QUERY_ICAO | AP_QUERY_IATA | AP_QUERY_FAA | AP_QUERY_LOCAL,

  /* All but ident */
  AP_QUERY_OFFICIAL = AP_QUERY_ICAO | AP_QUERY_IATA | AP_QUERY_FAA | AP_QUERY_LOCAL
};

ATOOLS_DECLARE_FLAGS_32(AirportQueryFlags, map::AirportQueryFlag)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(map::AirportQueryFlags)

/* ================================================================================== */
/* Covers all airspace types. Value save to config. */
enum MapAirspaceType : quint64
{
  AIRSPACE_NONE = 0,
  CENTER = 1ULL << 0,
  CLASS_A = 1ULL << 1, // ICAO airspace - controlled - no VFR
  CLASS_B = 1ULL << 2, // ICAO airspace - controlled
  CLASS_C = 1ULL << 3, // ICAO airspace - controlled
  CLASS_D = 1ULL << 4, // ICAO airspace - controlled
  CLASS_E = 1ULL << 5, // ICAO airspace - controlled
  CLASS_F = 1ULL << 6, // Open FIR - uncontrolled
  CLASS_G = 1ULL << 7, // Open FIR - uncontrolled
  TOWER = 1ULL << 8,
  CLEARANCE = 1ULL << 9,
  GROUND = 1ULL << 10,
  DEPARTURE = 1ULL << 11,
  APPROACH = 1ULL << 12,
  MOA = 1ULL << 13,
  RESTRICTED = 1ULL << 14,
  PROHIBITED = 1ULL << 15,
  WARNING = 1ULL << 16,
  ALERT = 1ULL << 17,
  DANGER = 1ULL << 18,
  NATIONAL_PARK = 1ULL << 19,
  MODEC = 1ULL << 20,
  RADAR = 1ULL << 21,
  TRAINING = 1ULL << 22,
  GLIDERPROHIBITED = 1ULL << 23, // Not FSX/P3D
  WAVEWINDOW = 1ULL << 24, // Not FSX/P3D
  CAUTION = 1ULL << 25, // DFD

  ONLINE_OBSERVER = 1ULL << 26, // VATSIM or IVAO observer

  FIR = 1ULL << 27, // New FIR region instead of center
  UIR = 1ULL << 28, // New UIR region instead of center

  GCA = 1ULL << 29, // New general control area combining several unknown types
  MCTR = 1ULL << 30, // Military Control Zone (MCTR)
  TRSA = 1ULL << 31, // Terminal Radar Service Area (TRSA)

  // >>> Update MAP_AIRSPACE_TYPE_BITS below too <<<

  AIRSPACE_CLASS_ICAO = CLASS_A | CLASS_B | CLASS_C | CLASS_D | CLASS_E,
  AIRSPACE_CLASS_FG = CLASS_F | CLASS_G,
  AIRSPACE_FIR_UIR = FIR | UIR,
  AIRSPACE_CENTER = CENTER,
  AIRSPACE_RESTRICTED = RESTRICTED | PROHIBITED | GLIDERPROHIBITED | MOA | DANGER,
  AIRSPACE_SPECIAL = WARNING | ALERT | TRAINING | CAUTION,
  AIRSPACE_OTHER = TOWER | CLEARANCE | GROUND | DEPARTURE | APPROACH | MODEC | RADAR | GCA | MCTR | TRSA |
                   NATIONAL_PARK | WAVEWINDOW | ONLINE_OBSERVER,

  AIRSPACE_ALL = AIRSPACE_CLASS_ICAO | AIRSPACE_CLASS_FG | AIRSPACE_FIR_UIR | AIRSPACE_CENTER | AIRSPACE_RESTRICTED |
                 AIRSPACE_SPECIAL | AIRSPACE_OTHER,

  // Default value on first start
  AIRSPACE_DEFAULT = AIRSPACE_CLASS_ICAO | AIRSPACE_RESTRICTED
};

ATOOLS_DECLARE_FLAGS_64(MapAirspaceTypes, MapAirspaceType)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(map::MapAirspaceTypes)

/* for(int i = 0; i <= map::MAP_AIRSPACE_TYPE_BITS; i++)
 *   map::MapAirspaceTypes type(1 << i); */
Q_DECL_CONSTEXPR int MAP_AIRSPACE_TYPE_BITS = 31;

/* Source database for airspace */
enum MapAirspaceSource : quint32
{
  AIRSPACE_SRC_NONE = 0,
  AIRSPACE_SRC_SIM = 1 << 0,
  AIRSPACE_SRC_NAV = 1 << 1,
  AIRSPACE_SRC_ONLINE = 1 << 2,
  AIRSPACE_SRC_USER = 1 << 3,

  AIRSPACE_SRC_ALL = 0xff,

  AIRSPACE_SRC_NOT_ONLINE = AIRSPACE_SRC_SIM | AIRSPACE_SRC_NAV | AIRSPACE_SRC_USER
};

ATOOLS_DECLARE_FLAGS_32(MapAirspaceSources, map::MapAirspaceSource)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(map::MapAirspaceSources)

QDebug operator<<(QDebug out, const map::MapAirspaceSource& type);
QDebug operator<<(QDebug out, const map::MapAirspaceSources& type);

extern const QVector<map::MapAirspaceSource> MAP_AIRSPACE_SRC_VALUES;
extern const QVector<map::MapAirspaceSource> MAP_AIRSPACE_SRC_NO_ONLINE_VALUES;

/* Airspace filter flags */
enum MapAirspaceFlag : quint32
{
  AIRSPACE_ALTITUDE_FLAG_NONE = 0, /* Indicates that filter is used only for type */

  /* Special filter flags - not airspace types */
  AIRSPACE_ALTITUDE_ALL = 1 << 0,
  AIRSPACE_ALTITUDE_FLIGHTPLAN = 1 << 1,
  AIRSPACE_ALTITUDE_SET = 1 << 2, /* Use values MapAirspaceFilter::minAltitudeFt and MapAirspaceFilter::maxAltitudeFt */

  /* Action flags - not airspace or altitude filter types */
  AIRSPACE_ALL_ON = 1 << 3,
  AIRSPACE_ALL_OFF = 1 << 4,

  AIRSPACE_NO_MULTIPLE_Z = 1 << 5,

  AIRSPACE_FLAG_DEFAULT = AIRSPACE_ALTITUDE_ALL
};

ATOOLS_DECLARE_FLAGS_32(MapAirspaceFlags, map::MapAirspaceFlag)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(map::MapAirspaceFlags)

/* Combines all airspace types and flags for display into a serializable object
 * Serialized in MapWidget::saveState() and aggregated by MapPaintLayer::airspaceTypes */
struct MapAirspaceFilter
{
  const static int MIN_AIRSPACE_ALT = 0;
  const static int MAX_AIRSPACE_ALT = 60000; /* The max value is treated as unlimited in AirspaceQuery::getAirspaces() */

  /* Construct default values also used for inital start */
  MapAirspaceFilter()
    : types(AIRSPACE_DEFAULT), flags(AIRSPACE_FLAG_DEFAULT), minAltitudeFt(MIN_AIRSPACE_ALT), maxAltitudeFt(MAX_AIRSPACE_ALT)
  {
  }

  MapAirspaceFilter(const MapAirspaceType& typesParam, const MapAirspaceFlag& flagsParam, int minAltitudeFtParam, int maxAltitudeFtParam)
    : types(typesParam), flags(flagsParam), minAltitudeFt(minAltitudeFtParam), maxAltitudeFt(maxAltitudeFtParam)
  {
  }

  bool operator==(const map::MapAirspaceFilter& other) const
  {
    return types == other.types && flags == other.flags && minAltitudeFt == other.minAltitudeFt && maxAltitudeFt == other.maxAltitudeFt;
  }

  bool operator!=(const map::MapAirspaceFilter& other) const
  {
    return !operator==(other);
  }

  MapAirspaceTypes types;
  MapAirspaceFlags flags;
  int minAltitudeFt, maxAltitudeFt;
};

QDebug operator<<(QDebug out, const map::MapAirspaceFilter& type);
QDebug operator<<(QDebug out, const map::MapAirspaceFlags& type);
QDebug operator<<(QDebug out, const map::MapAirspaceTypes& type);

QDataStream& operator>>(QDataStream& dataStream, map::MapAirspaceFilter& obj);
QDataStream& operator<<(QDataStream& dataStream, const map::MapAirspaceFilter& obj);

/* Combines id and source database (online, user, etc.) */
struct MapAirspaceId
{
  int id;
  MapAirspaceSource src;
};

inline uint qHash(const map::MapAirspaceId& id)
{
  return static_cast<unsigned int>(id.id) ^ id.src;
}

inline bool operator==(const map::MapAirspaceId& id1, const map::MapAirspaceId& id2)
{
  return id1.id == id2.id && id1.src == id2.src;
}

/* ================================================================================== */
/* Airport flags coverting most airport attributes and facilities. */
enum MapAirportFlag : quint32
{
  AP_NONE = 0,
  AP_ADDON = 1 << 0,
  AP_LIGHT = 1 << 1, /* Has at least one lighted runway */
  AP_TOWER = 1 << 2, /* Has a tower frequency */
  AP_ILS = 1 << 3, /* At least one runway end has ILS */
  AP_PROCEDURE = 1 << 4, /* At least one runway end has a procedure in the sim data */
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
  AP_RW_CLOSED = 1 << 20, /* Has at least one closed runway */
  AP_COMPLETE = 1 << 21, /* Struct completely loaded? */
  AP_3D = 1 << 22, /* X-Plane 3D airport */
  AP_ALL = 0xffffffff
};

ATOOLS_DECLARE_FLAGS_32(MapAirportFlags, map::MapAirportFlag)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(map::MapAirportFlags)

/* X-Plane airport type. Matches values in apt.dat */
enum MapAirportType
{
  AP_TYPE_NONE = -1,
  AP_TYPE_LAND = 1,
  AP_TYPE_SEAPLANE = 16,
  AP_TYPE_HELIPORT = 17,
};

/* X-Plane airport type. Matches values in apt.dat */
enum MapWaypointArtificial
{
  WAYPOINT_ARTIFICIAL_NONE = 0, /* Normal waypoint */
  WAYPOINT_ARTIFICIAL_AIRWAYS = 1, /* Dummy waypoint created for airways or airways and procedures */
  WAYPOINT_ARTIFICIAL_PROCEDURES = 2 /* Dummy waypoint created for procedures only */
};

/* Sun shading sub menu actions.
 * Values are saved in settings do not change */
enum MapSunShading
{
  SUN_SHADING_SIMULATOR_TIME,
  SUN_SHADING_REAL_TIME,
  SUN_SHADING_USER_TIME
};

/* Weather source for map icons menu actions.
 * Values are saved in settings do not change */
enum MapWeatherSource
{
  WEATHER_SOURCE_SIMULATOR,
  WEATHER_SOURCE_ACTIVE_SKY,
  WEATHER_SOURCE_NOAA,
  WEATHER_SOURCE_VATSIM,
  WEATHER_SOURCE_IVAO,
  WEATHER_SOURCE_DISABLED
};

QString mapWeatherSourceString(map::MapWeatherSource source);
QString mapWeatherSourceStringShort(map::MapWeatherSource source);

QDebug operator<<(QDebug out, const map::MapWeatherSource& type);

} // namespace map

namespace textflags {
/* Flags that determine what information is added to an icon */
enum TextFlag : quint32
{
  NONE = 0x0000,
  IDENT = 0x0001, /* Draw airport or navaid ICAO ident */
  TYPE = 0x0002, /* Draw navaid type (HIGH, MEDIUM, TERMINAL, HH, H, etc.) */
  FREQ = 0x0004, /* Draw navaid frequency */
  NAME = 0x0008,
  MORSE = 0x0010, /* Draw navaid morse code */
  INFO = 0x0020, /* Additional airport information like tower frequency, etc. */
  ROUTE_TEXT = 0x0040, /* Object is part of route */
  ABS_POS = 0x0080, /* Use absolute text positioning */
  NO_BACKGROUND = 0x0100, /* No background */
  LOG_TEXT = 0x0200, /* Object is part of log entry - only for airports */
  ELLIPSE_IDENT = 0x0400 /* Add allipse to first text (ident) and ignore additional texts if additonal are not empty */
};

ATOOLS_DECLARE_FLAGS_32(TextFlags, textflags::TextFlag)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(textflags::TextFlags)
}

namespace textatt {

/* Low level text attributes for custom text boxes */
enum TextAttribute : quint32
{
  NONE = 0x0000,

  /* Font attributes */
  BOLD = 0x0001,
  ITALIC = 0x0002,
  UNDERLINE = 0x0004,
  OVERLINE = 0x0008,
  STRIKEOUT = 0x0010,

  /* Text placement */
  LEFT = 0x0020, /* Reference point is at the right of the text (right-aligned) to place text at the LEFT of an icon */
  RIGHT = 0x0040, /* Reference point is at the left of the text (left-aligned) to place text at the RIGHT of an icon */
  CENTER = 0x0080,

  /* Vertical alignment */
  BELOW = 0x1000, /* Reference point at top to place text BELOW an icon */
  ABOVE = 0x2000, /* Reference point at bottom to place text ABOVE an icon */

  /* Color attributes */
  ROUTE_BG_COLOR = 0x0100, /* Use light yellow background for route objects */
  LOG_BG_COLOR = 0x0200, /* Use light blue text background for log */
  WARNING_COLOR = 0x0400, /* Orange warning text */
  ERROR_COLOR = 0x0800, /* White on red error text */

  NO_ROUND_RECT = 0x4000, /* No rounded background rect */

  /* Automatic text placement to octants for flight plan labels */
  PLACE_ABOVE = ABOVE | CENTER,
  PLACE_ABOVE_RIGHT = ABOVE | RIGHT,
  PLACE_RIGHT = RIGHT,
  PLACE_BELOW_RIGHT = BELOW | RIGHT,
  PLACE_BELOW = BELOW | CENTER,
  PLACE_BELOW_LEFT = BELOW | LEFT,
  PLACE_LEFT = LEFT,
  PLACE_ABOVE_LEFT = ABOVE | LEFT,

  /* Horizontal placement flags */
  PLACE_ALL_HORIZ = LEFT | RIGHT,
  /* Vertical placement flags */
  PLACE_ALL_VERT = ABOVE | BELOW,

  /* All placement flags */
  PLACE_ALL = LEFT | RIGHT | CENTER | BELOW | ABOVE,
};

ATOOLS_DECLARE_FLAGS_32(TextAttributes, textatt::TextAttribute)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(textatt::TextAttributes)
}

Q_DECLARE_TYPEINFO(map::MapAirspaceFilter, Q_PRIMITIVE_TYPE);
Q_DECLARE_METATYPE(map::MapAirspaceFilter)

Q_DECLARE_TYPEINFO(map::MapAirspaceId, Q_PRIMITIVE_TYPE);
Q_DECLARE_METATYPE(map::MapAirspaceId)

Q_DECLARE_TYPEINFO(map::MapTypes, Q_PRIMITIVE_TYPE);
Q_DECLARE_METATYPE(map::MapTypes)

#endif // LITTLENAVMAP_MAPFLAGS_H
