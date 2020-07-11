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

#ifndef LITTLENAVMAP_MAPFLAGS_H
#define LITTLENAVMAP_MAPFLAGS_H

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
Q_DECL_CONSTEXPR static float INVALID_DISTANCE_VALUE = std::numeric_limits<float>::max();
Q_DECL_CONSTEXPR static float INVALID_ALTITUDE_VALUE = std::numeric_limits<float>::max();
Q_DECL_CONSTEXPR static float INVALID_SPEED_VALUE = std::numeric_limits<float>::max();
Q_DECL_CONSTEXPR static float INVALID_TIME_VALUE = std::numeric_limits<float>::max();
Q_DECL_CONSTEXPR static float INVALID_WEIGHT_VALUE = std::numeric_limits<float>::max();
Q_DECL_CONSTEXPR static float INVALID_VOLUME_VALUE = std::numeric_limits<float>::max();
Q_DECL_CONSTEXPR static int INVALID_INDEX_VALUE = std::numeric_limits<int>::max();

Q_DECL_CONSTEXPR static float INVALID_MAGVAR = 9999.f;

Q_DECL_CONSTEXPR static float DEFAULT_ILS_WIDTH = 4.f;

/* minimum ground speed for fuel flow calculations and other */
Q_DECL_CONSTEXPR static float MIN_GROUND_SPEED = 30.f;

/* Do not draw barbs below this altitude */
Q_DECL_CONSTEXPR static float MIN_WIND_BARB_ALTITUDE = 4000.f;

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
  // 13
  AIRCRAFT = 1 << 14, /* Simulator user aircraft */
  AIRCRAFT_AI = 1 << 15, /* AI or multiplayer simulator aircraft */
  AIRCRAFT_AI_SHIP = 1 << 16, /* AI or multiplayer simulator ship */
  AIRCRAFT_TRACK = 1 << 17, /* Simulator aircraft track */
  USERPOINTROUTE = 1 << 18, /* Flight plan user waypoint */
  PARKING = 1 << 19,
  RUNWAYEND = 1 << 20,
  INVALID = 1 << 21, /* Flight plan waypoint not found in database */
  MISSED_APPROACH = 1 << 22, /* Only procedure type that can be hidden */
  PROCEDURE = 1 << 23, /* General procedure leg */
  AIRSPACE = 1 << 24, /* General airspace boundary, online or offline */
  HELIPAD = 1 << 25, /* Helipads on airports */
  // 26
  USERPOINT = 1 << 27, /* A user defined waypoint - not used to define if should be drawn or not */
  TRACK = 1 << 28, /* NAT, PACOTS or AUSOTS track */
  AIRCRAFT_ONLINE = 1 << 29, /* Online network client/aircraft */

  LOGBOOK = 1 << 30, /* Logbook entry */
  // LAST = 1 << 31,

  /* All online, AI and multiplayer aircraft */
  AIRCRAFT_ALL = AIRCRAFT | AIRCRAFT_AI | AIRCRAFT_AI_SHIP | AIRCRAFT_ONLINE,

  AIRWAY_ALL = AIRWAY | AIRWAYV | AIRWAYJ,

  AIRPORT_ALL = AIRPORT | AIRPORT_HARD | AIRPORT_SOFT | AIRPORT_EMPTY | AIRPORT_ADDON,

  /* All navaids */
  NAV_ALL = VOR | NDB | WAYPOINT,

  /* All objects that have a magvar assigned */
  NAV_MAGVAR = AIRPORT | VOR | NDB | WAYPOINT,

  ALL = 0xffffffff
};

Q_DECLARE_FLAGS(MapObjectTypes, MapObjectType);
Q_DECLARE_OPERATORS_FOR_FLAGS(map::MapObjectTypes);

QDebug operator<<(QDebug out, const map::MapObjectTypes& type);

/* Type that is used only for flags to determine what should be drawn. Not used in other contexts. */
enum MapObjectDisplayType
{
  DISPLAY_TYPE_NONE = 0,
  AIRPORT_WEATHER = 1 << 0, /* Airport weather icons */
  MINIMUM_ALTITUDE = 1 << 1, /* MORA (minimum off route altitude) */

  WIND_BARBS = 1 << 2, /* Wind barbs grid */
  WIND_BARBS_ROUTE = 1 << 3, /* Wind barbs at flight plan waypoints */

  LOGBOOK_DIRECT = 1 << 4, /* GC direct line in logbook entry highlight */
  LOGBOOK_ROUTE = 1 << 5, /* Route in logbook entry highlight */
  LOGBOOK_TRACK = 1 << 6, /* Track in logbook entry highlight */

  COMPASS_ROSE = 1 << 7, /* Compass rose */
  COMPASS_ROSE_ATTACH = 1 << 8, /* Attach to user aircraft */

  FLIGHTPLAN = 1 << 9, /* Flight plan */
  FLIGHTPLAN_TOC_TOD = 1 << 10, /* Top of climb and top of descent */

  LOGBOOK_ALL = LOGBOOK_DIRECT | LOGBOOK_ROUTE | LOGBOOK_TRACK
};

Q_DECLARE_FLAGS(MapObjectDisplayTypes, MapObjectDisplayType);
Q_DECLARE_OPERATORS_FOR_FLAGS(map::MapObjectDisplayTypes);

QDebug operator<<(QDebug out, const map::MapObjectDisplayTypes& type);

/* Query type for all getNearest and other functions. Covers all what is not included in MapObjectTypes */
enum MapObjectQueryType
{
  QUERY_NONE = 0,
  QUERY_PROC_POINTS = 1 << 0, /* Procedure points */
  QUERY_HOLDS = 1 << 1, /* Holds */
  QUERY_PATTERNS = 1 << 2, /* Traffic patterns */
  QUERY_PROCEDURES = 1 << 3, /* Procedures when querying route */
  QUERY_RANGEMARKER = 1 << 4 /* Range rings */
};

Q_DECLARE_FLAGS(MapObjectQueryTypes, MapObjectQueryType);
Q_DECLARE_OPERATORS_FOR_FLAGS(map::MapObjectQueryTypes);

/* ================================================================================== */
/* Covers all airspace types */
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
  GLIDERPROHIBITED = 1 << 23, // Not FSX/P3D
  WAVEWINDOW = 1 << 24, // Not FSX/P3D
  CAUTION = 1 << 25, // DFD

  ONLINE_OBSERVER = 1 << 26, // VATSIM or IVAO observer

  FIR = 1 << 27, // New FIR region instead of center
  UIR = 1 << 28, // New UIR region instead of center

  AIRSPACE_CLASS_ICAO = CLASS_A | CLASS_B | CLASS_C | CLASS_D | CLASS_E,
  AIRSPACE_CLASS_FG = CLASS_F | CLASS_G,
  AIRSPACE_FIR_UIR = FIR | UIR,
  AIRSPACE_CENTER = CENTER,
  AIRSPACE_RESTRICTED = RESTRICTED | PROHIBITED | GLIDERPROHIBITED | MOA | DANGER,
  AIRSPACE_SPECIAL = WARNING | ALERT | TRAINING | CAUTION,
  AIRSPACE_OTHER = TOWER | CLEARANCE | GROUND | DEPARTURE | APPROACH | MODEC | RADAR | NATIONAL_PARK | WAVEWINDOW |
                   ONLINE_OBSERVER,

  AIRSPACE_ALL = AIRSPACE_CLASS_ICAO | AIRSPACE_CLASS_FG | AIRSPACE_FIR_UIR | AIRSPACE_CENTER | AIRSPACE_RESTRICTED |
                 AIRSPACE_SPECIAL | AIRSPACE_OTHER,

  // Default value on first start
  AIRSPACE_DEFAULT = AIRSPACE_CLASS_ICAO | AIRSPACE_RESTRICTED
};

Q_DECLARE_FLAGS(MapAirspaceTypes, MapAirspaceType);
Q_DECLARE_OPERATORS_FOR_FLAGS(map::MapAirspaceTypes);

Q_DECL_CONSTEXPR int MAP_AIRSPACE_TYPE_BITS = 29;

/* Source database for airspace */
enum MapAirspaceSource
{
  AIRSPACE_SRC_NONE = 0,
  AIRSPACE_SRC_SIM = 1 << 0,
  AIRSPACE_SRC_NAV = 1 << 1,
  AIRSPACE_SRC_ONLINE = 1 << 2,
  AIRSPACE_SRC_USER = 1 << 3,

  AIRSPACE_SRC_ALL = 0xff,

  AIRSPACE_SRC_NOT_ONLINE = AIRSPACE_SRC_SIM | AIRSPACE_SRC_NAV | AIRSPACE_SRC_USER
};

Q_DECLARE_FLAGS(MapAirspaceSources, MapAirspaceSource);
Q_DECLARE_OPERATORS_FOR_FLAGS(map::MapAirspaceSources);

static const QVector<map::MapAirspaceSources> MAP_AIRSPACE_SRC_VALUES =
{AIRSPACE_SRC_SIM, AIRSPACE_SRC_NAV, AIRSPACE_SRC_ONLINE, AIRSPACE_SRC_USER};

static const QVector<map::MapAirspaceSources> MAP_AIRSPACE_SRC_NO_ONLINE_VALUES =
{AIRSPACE_SRC_SIM, AIRSPACE_SRC_NAV, AIRSPACE_SRC_USER};

/* Airspace filter flags */
enum MapAirspaceFlag
{
  AIRSPACE_FLAG_NONE = 0,

  /* Special filter flags - not airspaces */
  AIRSPACE_BELOW_10000 = 1 << 1,
  AIRSPACE_BELOW_18000 = 1 << 2,
  AIRSPACE_ABOVE_10000 = 1 << 3,
  AIRSPACE_ABOVE_18000 = 1 << 4,
  AIRSPACE_AT_FLIGHTPLAN = 1 << 5,
  AIRSPACE_ALL_ALTITUDE = 1 << 6,

  /* Action flags - not airspaces */
  AIRSPACE_ALL_ON = 1 << 7,
  AIRSPACE_ALL_OFF = 1 << 8,

  AIRSPACE_FLAG_DEFAULT = AIRSPACE_ALL_ALTITUDE
};

Q_DECLARE_FLAGS(MapAirspaceFlags, MapAirspaceFlag);
Q_DECLARE_OPERATORS_FOR_FLAGS(map::MapAirspaceFlags);

/* Combines all airspace types and flags into a serializable object */
struct MapAirspaceFilter
{
  MapAirspaceTypes types;
  MapAirspaceFlags flags;
};

QDataStream& operator>>(QDataStream& dataStream, map::MapAirspaceFilter& obj);
QDataStream& operator<<(QDataStream& dataStream, const map::MapAirspaceFilter& obj);

/* Combines id and source database (online, user, etc.) */
struct MapAirspaceId
{
  int id;
  MapAirspaceSources src;
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
/* Visible user features. */
enum MapMarkType
{
  MARK_NONE = 0,
  MARK_RANGE_RINGS = 1 << 0, /* All range rings */
  MARK_MEASUREMENT = 1 << 1, /* All measurement lines */
  MARK_HOLDS = 1 << 2, /* Holdings */
  MARK_PATTERNS = 1 << 3, /* Traffic patterns */
  MARK_ALL = MARK_RANGE_RINGS | MARK_MEASUREMENT | MARK_HOLDS | MARK_PATTERNS
};

Q_DECLARE_FLAGS(MapMarkTypes, MapMarkType);
Q_DECLARE_OPERATORS_FOR_FLAGS(map::MapMarkTypes);

/* ================================================================================== */
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
  AP_3D = 1 << 22, /* X-Plane 3D airport */
  AP_ALL = 0xffffffff
};

Q_DECLARE_FLAGS(MapAirportFlags, MapAirportFlag);
Q_DECLARE_OPERATORS_FOR_FLAGS(map::MapAirportFlags);

/* Index values of the map theme combo box */
enum MapThemeComboIndex
{
  OPENSTREETMAP,
  OPENTOPOMAP,
  STAMENTERRAIN,
  CARTOLIGHT,
  CARTODARK,
  SIMPLE,
  PLAIN,
  ATLAS,
  CUSTOM, /* Custom maps count from this index up */
  INVALID_THEME = -1
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
  WEATHER_SOURCE_IVAO
};

QString mapWeatherSourceString(map::MapWeatherSource source);

} // namespace map

namespace textflags {
/* Flags that determine what information is added to an icon */
enum TextFlag
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
  LOG_TEXT = 0x0200 /* Object is part of log entry - only for airports */
};

Q_DECLARE_FLAGS(TextFlags, TextFlag);
Q_DECLARE_OPERATORS_FOR_FLAGS(textflags::TextFlags);
}

namespace textatt {
/* Low level text attributes for custom text boxes */
enum TextAttribute
{
  NONE = 0x0000,
  BOLD = 0x0001,
  ITALIC = 0x0002,
  UNDERLINE = 0x0004,
  OVERLINE = 0x0008,
  RIGHT = 0x0010, /* Reference point is at the right of the text (left-aligned) */
  LEFT = 0x0020,
  CENTER = 0x0040,
  ROUTE_BG_COLOR = 0x0080, /* Use light yellow background for route objects */
  LOG_BG_COLOR = 0x0100 /* Use light blue text background for log */
};

Q_DECLARE_FLAGS(TextAttributes, TextAttribute);
Q_DECLARE_OPERATORS_FOR_FLAGS(TextAttributes);
}

Q_DECLARE_TYPEINFO(map::MapAirspaceFilter, Q_PRIMITIVE_TYPE);
Q_DECLARE_METATYPE(map::MapAirspaceFilter);

Q_DECLARE_TYPEINFO(map::MapAirspaceId, Q_PRIMITIVE_TYPE);
Q_DECLARE_METATYPE(map::MapAirspaceId);

#endif // LITTLENAVMAP_MAPFLAGS_H
