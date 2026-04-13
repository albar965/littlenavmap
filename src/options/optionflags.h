/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#ifndef LITTLENAVMAP_OPTIONFLAGS_H
#define LITTLENAVMAP_OPTIONFLAGS_H

#include "util/flags.h"

/*
 * All these flags are not saved but restored from action and button states.
 */
namespace opts {
enum Flag : quint64
{
  NO_FLAGS = 0,

  /* Reload KML files on startup.
   * ui->checkBoxOptionsStartupLoadKml  */
  STARTUP_LOAD_KML = 1ULL << 0,

  /* Reload all map settings on startup.
   * ui->checkBoxOptionsStartupLoadMapSettings */
  STARTUP_LOAD_MAP_SETTINGS = 1ULL << 1,

  /* Reload route on startup.
   * ui->checkBoxOptionsStartupLoadRoute */
  STARTUP_LOAD_ROUTE = 1ULL << 2,

  /* Show home on starup.
   * ui->radioButtonOptionsStartupShowHome */
  STARTUP_SHOW_HOME = 1ULL << 3,

  /* Show last position on startup.
   * ui->radioButtonOptionsStartupShowLast */
  STARTUP_SHOW_LAST = 1ULL << 4,

  /* Show last position on startup.
   * ui->radioButtonOptionsStartupShowFlightplan */
  STARTUP_SHOW_ROUTE = 1ULL << 5,

  /* Center KML after loading.
   * ui->checkBoxOptionsGuiCenterKml */
  GUI_CENTER_KML = 1ULL << 6,

  /* Center flight plan after loading.
   * ui->checkBoxOptionsGuiCenterRoute */
  GUI_CENTER_ROUTE = 1ULL << 7,

  /* Treat empty airports special.
   * ui->checkBoxOptionsMapEmptyAirports */
  MAP_EMPTY_AIRPORTS = 1ULL << 8,

  /* East/west rule for flight plan calculation.
   * ui->checkBoxOptionsRouteEastWestRule */
  ROUTE_ALTITUDE_RULE = 1ULL << 9,

  /* ui->checkBoxOptionsOnlineRemoveShadow */
  ONLINE_REMOVE_SHADOW = 1ULL << 10,

  /* ui->checkBoxOptionsMapAiAircraftHideGround */
  MAP_AI_HIDE_GROUND = 1ULL << 11,

  /* No box mode when moving map.
   * ui->checkBoxOptionsSimUpdatesConstant */
  SIM_UPDATE_MAP_CONSTANTLY = 1ULL << 12,

  /* Center flight plan after loading.
   * ui->checkBoxOptionsGuiAvoidOverwrite */
  GUI_AVOID_OVERWRITE_FLIGHTPLAN = 1ULL << 13,

  /* radioButtonCacheUseOnlineElevation */
  CACHE_USE_ONLINE_ELEVATION = 1ULL << 14,

  /* radioButtonCacheUseOnlineElevation */
  CACHE_USE_OFFLINE_ELEVATION = 1ULL << 15,

  /* checkBoxOptionsStartupLoadInfoContent */
  STARTUP_LOAD_INFO = 1ULL << 17,

  /* checkBoxOptionsStartupLoadSearch */
  STARTUP_LOAD_SEARCH = 1ULL << 18,

  /* checkBoxOptionsStartupLoadTrail */
  STARTUP_LOAD_TRAIL = 1ULL << 19,

  /* checkBoxOptionsGuiOverrideLocale */
  GUI_OVERRIDE_LOCALE = 1ULL << 21,

  /* checkBoxOptionsDisplayTrailGradient */
  MAP_TRAIL_GRADIENT = 1ULL << 22,

  /* checkBoxOptionsFreetype */
  GUI_FREETYPE_FONT_ENGINE = 1ULL << 23,

  /* Reload aircraft performance on startup.
   * ui->checkBoxOptionsStartupLoadperf */
  STARTUP_LOAD_PERF = 1ULL << 24,

  /* Reload window layout on startup.
   * ui->checkBoxOptionsStartupLoadLayout */
  STARTUP_LOAD_LAYOUT = 1ULL << 25,

  /* Reload window layout on startup.
   * ui->checkBoxOptionsStartupShowSplash */
  STARTUP_SHOW_SPLASH = 1ULL << 26,

  /* Reverse wheel.
   * ui->checkBoxOptionsGuiWheel */
  GUI_REVERSE_WHEEL = 1ULL << 27,

  /* checkBoxOptionsGuiTooltipsAll */
  ENABLE_TOOLTIPS_ALL = 1ULL << 28,

  /* checkBoxOptionsGuiTooltipsMenu */
  ENABLE_TOOLTIPS_MENU = 1ULL << 29,

  /* ui->checkBoxOptionsMapAirspaceNoMultZ */
  MAP_AIRSPACE_NO_MULT_Z = 1ULL << 30,

  /* ui->checkBoxOptionsGuiAddDeparture */
  GUI_ADD_DEPARTURE = 1ULL << 31,

  /* checkBoxOptionsGuiTooltipsLink */
  ENABLE_TOOLTIPS_LINK = 1ULL << 32

};

ATOOLS_DECLARE_FLAGS_64(Flags, Flag)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(opts::Flags)

/* Map detail level during scrolling or zooming */
enum MapScrollDetail
{
  DETAIL_HIGH,
  DETAIL_NORMAL,
  DETAIL_LOW
};

/* Navigation mode */
enum MapNavigation
{
  MAP_NAV_CLICK_DRAG_MOVE,
  MAP_NAV_CLICK_CENTER,
  MAP_NAV_TOUCHSCREEN
};

/* Speed of simualator aircraft updates */
enum SimUpdateRate
{
  FAST,
  MEDIUM,
  LOW
};

/* Altitude rule for rounding up flight plan crusie altitude */
enum AltitudeRule
{
  EAST_WEST,
  NORTH_SOUTH, /* in Italy, France and Portugal, for example, southbound traffic uses odd flight levels */
  SOUTH_NORTH /* in New Zealand, southbound traffic uses even flight levels */
};

/* comboBoxOptionsUnitDistance */
enum UnitDist
{
  DIST_NM,
  DIST_KM,
  DIST_MILES
};

/* comboBoxOptionsUnitShortDistance */
enum UnitShortDist
{
  DIST_SHORT_FT,
  DIST_SHORT_METER
};

/* comboBoxOptionsUnitAlt */
enum UnitAlt
{
  ALT_FT,
  ALT_METER
};

/* comboBoxOptionsUnitSpeed */
enum UnitSpeed
{
  SPEED_KTS,
  SPEED_KMH,
  SPEED_MPH
};

/* comboBoxOptionsUnitVertSpeed */
enum UnitVertSpeed
{
  VERT_SPEED_FPM,
  VERT_SPEED_MS
};

/* comboBoxOptionsUnitCoords */
enum UnitCoords
{
  COORDS_DMS, /* Degree, minute and seconds */
  COORDS_DEC, /* Decimal degree */
  COORDS_DM, /* Degree and minutes */
  COORDS_LATY_LONX, /* lat/lon with sign */
  COORDS_LONX_LATY, /* lon/lat with sign - need to be swapped internally */
  COORDS_DECIMAL_GOOGLE, /* Degrees and decimal minutes (DMM): 41 24.2028, 2 10.4418 for direct input in Google maps */

  /* Used for bounds checking in case new options were added */
  COORDS_MIN = COORDS_DMS,
  COORDS_MAX = COORDS_DECIMAL_GOOGLE
};

/* comboBoxOptionsUnitVertFuel */
enum UnitFuelAndWeight
{
  FUEL_WEIGHT_GAL_LBS,
  FUEL_WEIGHT_LITER_KG
};

/* comboBoxOptionsDisplayTrailType */
enum DisplayTrailType
{
  TRAIL_TYPE_DASHED,
  TRAIL_TYPE_DOTTED,
  TRAIL_TYPE_SOLID
};

/* comboBoxOptionsDisplayTrailGradient */
enum DisplayTrailGradientType
{
  TRAIL_GRADIENT_COLOR_YELLOW_BLUE,
  TRAIL_GRADIENT_COLOR_RAINBOW,
  TRAIL_GRADIENT_BLACKWHITE
};

/* comboBoxOptionsStartupUpdateRate - how often to check for updates */
enum UpdateRate
{
  DAILY,
  WEEKLY,
  NEVER
};

/* comboBoxOptionsStartupUpdateChannels - what updates to check for */
enum UpdateChannels
{
  STABLE,
  STABLE_BETA,
  STABLE_BETA_DEVELOP
};

/* radioButtonOptionsOnlineIvao - online network to use */
enum OnlineNetwork
{
  ONLINE_NONE,
  ONLINE_VATSIM,
  ONLINE_IVAO,
  ONLINE_PILOTEDGE,
  ONLINE_CUSTOM_STATUS,
  ONLINE_CUSTOM
};

/* comboBoxOptionsOnlineFormat whazzup.txt file format */
enum OnlineFormat
{
  ONLINE_FORMAT_VATSIM,
  ONLINE_FORMAT_IVAO,
  ONLINE_FORMAT_VATSIM_JSON,
  ONLINE_FORMAT_IVAO_JSON
};

/* Override flags for circle radii for online centers, towers and more.
 * Online provided values are used if available and flag is set, i.e. checkbox is clicked. */
enum DisplayOnlineFlag : quint32
{
  DISPLAY_ONLINE_NONE = 0,

  /* ui->spinBoxDisplayOnlineClearance */
  DISPLAY_ONLINE_CLEARANCE = 1 << 0,

  /* ui->spinBoxDisplayOnlineArea      */
  DISPLAY_ONLINE_AREA = 1 << 1,

  /* ui->spinBoxDisplayOnlineApproach  */
  DISPLAY_ONLINE_APPROACH = 1 << 2,

  /* ui->spinBoxDisplayOnlineDeparture */
  DISPLAY_ONLINE_DEPARTURE = 1 << 3,

  /* ui->spinBoxDisplayOnlineFir       */
  DISPLAY_ONLINE_FIR = 1 << 4,

  /* ui->spinBoxDisplayOnlineObserver  */
  DISPLAY_ONLINE_OBSERVER = 1 << 5,

  /* ui->spinBoxDisplayOnlineGround    */
  DISPLAY_ONLINE_GROUND = 1 << 6,

  /* ui->spinBoxDisplayOnlineTower     */
  DISPLAY_ONLINE_TOWER = 1 << 7
};

ATOOLS_DECLARE_FLAGS_32(DisplayOnlineFlags, DisplayOnlineFlag)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(opts::DisplayOnlineFlags)

} // namespace opts

namespace opts2 {
/* Extension from flags to avoid overflow. Not saved. */
enum Flag2 : quint64
{
  NO_FLAGS2 = 0,

  /* Treat empty airports special.
   * ui->checkBoxOptionsMapEmptyAirports3D */
  MAP_EMPTY_AIRPORTS_3D = 1ULL << 0,

  /* checkBoxOptionsMapFlightplanHighlightActive */
  MAP_ROUTE_HIGHLIGHT_ACTIVE = 1ULL << 1,

  /* ui->checkBoxOptionsMapAirportText */
  MAP_AIRPORT_TEXT_BACKGROUND = 1ULL << 2,

  /* ui->checkBoxOptionsMapNavaidText */
  MAP_NAVAID_TEXT_BACKGROUND = 1ULL << 3,

  /* ui->checkBoxOptionsMapFlightplanText */
  MAP_ROUTE_TEXT_BACKGROUND = 1ULL << 4,

  /* ui->checkBoxOptionsSimHighlightActiveTable */
  ROUTE_HIGHLIGHT_ACTIVE_TABLE = 1ULL << 5,

  /* ui->checkBoxOptionsMapFlightplanDimPassed */
  MAP_ROUTE_DIM_PASSED = 1ULL << 6,

  /* ui->checkBoxOptionsSimDoNotFollowOnScroll */
  ROUTE_NO_FOLLOW_ON_MOVE = 1ULL << 7,

  /* ui->checkBoxOptionsSimCenterLeg */
  ROUTE_AUTOZOOM = 1ULL << 8,

  /* checkBoxOptionsGuiToolbarSize */
  OVERRIDE_TOOLBAR_SIZE = 1ULL << 9,

  /* ui->checkBoxOptionsSimCenterLegTable */
  ROUTE_CENTER_ACTIVE_LEG = 1ULL << 10,

  /* checkBoxOptionsMapZoomAvoidBlurred */
  MAP_AVOID_BLURRED_MAP = 1ULL << 11,

  /* checkBoxOptionsMapUndock */
  MAP_ALLOW_UNDOCK = 1ULL << 12,

  /* checkBoxOptionsGuiHighDpi - DISABLED in Qt 6 since always on */
  // HIGH_DPI_DISPLAY_SUPPORT = 1ULL << 13,

  /* checkBoxDisplayOnlineNameLookup */
  ONLINE_AIRSPACE_BY_NAME = 1ULL << 14,

  /* checkBoxDisplayOnlineFileLookup */
  ONLINE_AIRSPACE_BY_FILE = 1ULL << 15,

  /* checkBoxOptionsMapHighlightTransparent */
  MAP_HIGHLIGHT_TRANSPARENT = 1ULL << 16,

  /* checkBoxOptionsGuiRaiseWindows */
  RAISE_WINDOWS = 1ULL << 17,

  /* checkBoxOptionsUnitFuelOther */
  UNIT_FUEL_SHOW_OTHER = 1ULL << 18,

  /* checkBoxOptionsUnitTrueCourse */
  UNIT_TRUE_COURSE = 1ULL << 19,

  /* ui->checkBoxOptionsSimClearSelection */
  ROUTE_CLEAR_SELECTION = 1ULL << 20,

  /* checkBoxOptionsGuiRaiseDockWindows */
  RAISE_DOCK_WINDOWS = 1ULL << 21,

  /* checkBoxOptionsGuiRaiseMainWindow */
  RAISE_MAIN_WINDOW = 1ULL << 22,

  /* ui->checkBoxOptionsMapAirwayText */
  MAP_AIRWAY_TEXT_BACKGROUND = 1ULL << 23,

  /* checkBoxOptionsMapUserpointText */
  MAP_USERPOINT_TEXT_BACKGROUND = 1ULL << 24,

  /* checkBoxOptionsSimZoomOnTakeoff */
  ROUTE_ZOOM_TAKEOFF = 1ULL << 25,

  /* checkBoxOptionsMapUserAircraftText */
  MAP_USER_TEXT_BACKGROUND = 1ULL << 26,

  /* checkBoxOptionsMapAiAircraftText */
  MAP_AI_TEXT_BACKGROUND = 1ULL << 27,

  /* checkBoxOptionsMapAirportAddon */
  MAP_AIRPORT_HIGHLIGHT_ADDON = 1ULL << 28,

  /* checkBoxOptionsSimZoomOnLanding */
  ROUTE_ZOOM_LANDING = 1ULL << 29,

  /* checkBoxOptionsMapFlightplanTransparent */
  MAP_ROUTE_TRANSPARENT = 1ULL << 30,

  /* checkBoxOptionsWebScale */
  MAP_WEB_USE_UI_SCALE = 1ULL << 31,

  /* checkBoxOptionsUnitEnhancedAccuracy */
  UNIT_ENHANCED_ACCURACY = 1ULL << 32

};

ATOOLS_DECLARE_FLAGS_64(Flags2, Flag2)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(opts2::Flags2)

} // namespace opts2

namespace optsw {

enum FlagWeather : quint32
{
  NO_WEATHER_FLAGS = 0,

  /* Show ASN weather in info panel.
   * ui->checkBoxOptionsWeatherInfoAsn */
  WEATHER_INFO_ACTIVESKY = 1ULL << 0,

  /* Show NOAA weather in info panel.
   * ui->checkBoxOptionsWeatherInfoNoaa */
  WEATHER_INFO_NOAA = 1ULL << 1,

  /* Show Vatsim weather in info panel.
   * ui->checkBoxOptionsWeatherInfoVatsim */
  WEATHER_INFO_VATSIM = 1ULL << 2,

  /* Show FSX/P3D or X-Plane weather in info panel.
   * ui->checkBoxOptionsWeatherInfoFs */
  WEATHER_INFO_FS = 1ULL << 3,

  /* Show IVAO weather in info panel.
   * ui->checkBoxOptionsWeatherInfoIvao*/
  WEATHER_INFO_IVAO = 1ULL << 4,

  /* Show ASN weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipAsn */
  WEATHER_TOOLTIP_ACTIVESKY = 1ULL << 5,

  /* Show NOAA weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipNoaa */
  WEATHER_TOOLTIP_NOAA = 1ULL << 6,

  /* Show Vatsim weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipVatsim */
  WEATHER_TOOLTIP_VATSIM = 1ULL << 7,

  /* Show FSX/P3D or X-Plane weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipFs */
  WEATHER_TOOLTIP_FS = 1ULL << 8,

  /* Show IVAO weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipIvao*/
  WEATHER_TOOLTIP_IVAO = 1ULL << 9,

  WEATHER_INFO_ALL = WEATHER_INFO_ACTIVESKY | WEATHER_INFO_NOAA | WEATHER_INFO_VATSIM | WEATHER_INFO_FS |
                     WEATHER_INFO_IVAO,

  WEATHER_TOOLTIP_ALL = WEATHER_TOOLTIP_ACTIVESKY | WEATHER_TOOLTIP_NOAA | WEATHER_TOOLTIP_VATSIM | WEATHER_TOOLTIP_FS |
                        WEATHER_TOOLTIP_IVAO
};

ATOOLS_DECLARE_FLAGS_32(FlagsWeather, optsw::FlagWeather)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(optsw::FlagsWeather)
} // namespace opts2

namespace optsac {
/* Changing these option values will also change the saved values thus invalidating user settings */
enum DisplayOptionUserAircraft : quint32
{
  ITEM_USER_AIRCRAFT_NONE = 0,
  ITEM_USER_AIRCRAFT_REGISTRATION = 1 << 1,
  ITEM_USER_AIRCRAFT_TYPE = 1 << 2,
  ITEM_USER_AIRCRAFT_AIRLINE = 1 << 3,
  ITEM_USER_AIRCRAFT_FLIGHT_NUMBER = 1 << 4,
  ITEM_USER_AIRCRAFT_TRANSPONDER_CODE = 1 << 5,
  ITEM_USER_AIRCRAFT_IAS = 1 << 6,
  ITEM_USER_AIRCRAFT_GS = 1 << 7,
  ITEM_USER_AIRCRAFT_CLIMB_SINK = 1 << 8,
  ITEM_USER_AIRCRAFT_HEADING = 1 << 9,
  ITEM_USER_AIRCRAFT_ALTITUDE = 1 << 10,
  ITEM_USER_AIRCRAFT_INDICATED_ALTITUDE = 1 << 11,
  ITEM_USER_AIRCRAFT_WIND = 1 << 12,
  ITEM_USER_AIRCRAFT_TRACK_LINE = 1 << 13,
  ITEM_USER_AIRCRAFT_WIND_POINTER = 1 << 14,
  ITEM_USER_AIRCRAFT_TAS = 1 << 15,
  ITEM_USER_AIRCRAFT_COORDINATES = 1 << 16,
  ITEM_USER_AIRCRAFT_ICE = 1 << 17,
  ITEM_USER_AIRCRAFT_ALT_ABOVE_GROUND = 1 << 18
};

ATOOLS_DECLARE_FLAGS_32(DisplayOptionsUserAircraft, optsac::DisplayOptionUserAircraft)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(optsac::DisplayOptionsUserAircraft)

enum DisplayOptionAiAircraft : quint32
{
  ITEM_AI_AIRCRAFT_NONE = 0,
  ITEM_AI_AIRCRAFT_DEP_DEST = 1 << 1,
  ITEM_AI_AIRCRAFT_REGISTRATION = 1 << 2,
  ITEM_AI_AIRCRAFT_TYPE = 1 << 3,
  ITEM_AI_AIRCRAFT_AIRLINE = 1 << 4,
  ITEM_AI_AIRCRAFT_FLIGHT_NUMBER = 1 << 5,
  ITEM_AI_AIRCRAFT_TRANSPONDER_CODE = 1 << 6,
  ITEM_AI_AIRCRAFT_IAS = 1 << 7,
  ITEM_AI_AIRCRAFT_GS = 1 << 8,
  ITEM_AI_AIRCRAFT_CLIMB_SINK = 1 << 9,
  ITEM_AI_AIRCRAFT_HEADING = 1 << 10,
  ITEM_AI_AIRCRAFT_ALTITUDE = 1 << 11,
  ITEM_AI_AIRCRAFT_TAS = 1 << 12,
  ITEM_AI_AIRCRAFT_COORDINATES = 1 << 13,
  ITEM_AI_AIRCRAFT_DIST_BEARING_FROM_USER = 1 << 14,
  ITEM_AI_AIRCRAFT_INDICATED_ALTITUDE = 1 << 15,
  ITEM_AI_AIRCRAFT_OBJECT_ID = 1 << 16
};

ATOOLS_DECLARE_FLAGS_32(DisplayOptionsAiAircraft, optsac::DisplayOptionAiAircraft)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(optsac::DisplayOptionsAiAircraft)
}

namespace optsd {

/* Changing these option values will also change the saved values thus invalidating user settings */
enum DisplayOptionAirport : quint32
{
  AIRPORT_NONE = 0,

  ITEM_AIRPORT_NAME = 1 << 1,
  ITEM_AIRPORT_TOWER = 1 << 2,
  ITEM_AIRPORT_ATIS = 1 << 3,
  ITEM_AIRPORT_RUNWAY = 1 << 4,

  ITEM_AIRPORT_DETAIL_RUNWAY = 1 << 5,
  ITEM_AIRPORT_DETAIL_TAXI = 1 << 6,
  ITEM_AIRPORT_DETAIL_TAXI_LINE = 1 << 10,
  ITEM_AIRPORT_DETAIL_TAXI_NAME = 1 << 11,
  ITEM_AIRPORT_DETAIL_APRON = 1 << 7,
  ITEM_AIRPORT_DETAIL_PARKING = 1 << 8,
  ITEM_AIRPORT_DETAIL_BOUNDARY = 1 << 9
};

ATOOLS_DECLARE_FLAGS_32(DisplayOptionsAirport, optsd::DisplayOptionAirport)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(optsd::DisplayOptionsAirport)

/* On-screen navigation aids */
enum DisplayOptionNavAid : quint32
{
  NAVAIDS_NONE = 0,
  NAVAIDS_CENTER_CROSS = 1 << 1, /* White center cross on black background */
  NAVAIDS_TOUCHSCREEN_AREAS = 1 << 2, /* White center marks showing touch areas on black background */
  NAVAIDS_TOUCHSCREEN_REGIONS = 1 << 3, /* Dim gray highlighted touch areas  */
  NAVAIDS_TOUCHSCREEN_ICONS = 1 << 4 /* Icons */
};

ATOOLS_DECLARE_FLAGS_32(DisplayOptionsNavAid, optsd::DisplayOptionNavAid)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(optsd::DisplayOptionsNavAid)

/* Airspace labels */
enum DisplayOptionAirspace : quint32
{
  AIRSPACE_NONE = 0,
  AIRSPACE_NAME = 1 << 1, /* Airspace name */
  AIRSPACE_RESTRICTIVE_NAME = 1 << 2, /* Restrictive name */
  AIRSPACE_TYPE = 1 << 3, /* Type */
  AIRSPACE_ALTITUDE = 1 << 4, /* Altitude restriction */
  AIRSPACE_COM = 1 << 5 /* COM frequencies */
};

ATOOLS_DECLARE_FLAGS_32(DisplayOptionsAirspace, optsd::DisplayOptionAirspace)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(optsd::DisplayOptionsAirspace)

/* Measurement lines */
enum DisplayOptionMeasurement : quint32
{
  MEASUREMENT_NONE = 0,
  MEASUREMENT_TRUE = 1 << 0,
  MEASUREMENT_MAG = 1 << 1,
  MEASUREMENT_DIST = 1 << 2,
  MEASUREMENT_LABEL = 1 << 3,
  MEASUREMENT_RADIAL = 1 << 4
};

ATOOLS_DECLARE_FLAGS_32(DisplayOptionsMeasurement, optsd::DisplayOptionMeasurement)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(optsd::DisplayOptionsMeasurement)

enum DisplayOptionRose : quint32
{
  ROSE_NONE = 0,
  ROSE_RANGE_RINGS = 1 << 0,
  ROSE_DEGREE_MARKS = 1 << 1,
  ROSE_DEGREE_LABELS = 1 << 2,
  ROSE_HEADING_LINE = 1 << 3,
  ROSE_TRACK_LINE = 1 << 4,
  ROSE_TRACK_LABEL = 1 << 5,
  ROSE_CRAB_ANGLE = 1 << 6,
  ROSE_NEXT_WAYPOINT = 1 << 7,
  ROSE_DIR_LABELS = 1 << 8,
  ROSE_TRUE_HEADING = 1 << 9
};

ATOOLS_DECLARE_FLAGS_32(DisplayOptionsRose, optsd::DisplayOptionRose)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(optsd::DisplayOptionsRose)

enum DisplayOptionRoute : quint32
{
  ROUTE_NONE = 0,
  ROUTE_DISTANCE = 1 << 0,
  ROUTE_MAG_COURSE = 1 << 1,
  ROUTE_TRUE_COURSE = 1 << 2,
  ROUTE_INITIAL_FINAL_MAG_COURSE = 1 << 3,
  ROUTE_INITIAL_FINAL_TRUE_COURSE = 1 << 4,
  ROUTE_AIRWAY = 1 << 5
};

ATOOLS_DECLARE_FLAGS_32(DisplayOptionsRoute, optsd::DisplayOptionRoute)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(optsd::DisplayOptionsRoute)

enum DisplayTooltipOption : quint32
{
  TOOLTIP_NONE = 0,
  TOOLTIP_AIRPORT = 1 << 1,
  TOOLTIP_NAVAID = 1 << 2,
  TOOLTIP_AIRSPACE = 1 << 3,
  TOOLTIP_WIND = 1 << 4,
  TOOLTIP_AIRCRAFT_AI = 1 << 5,
  TOOLTIP_AIRCRAFT_USER = 1 << 6,
  TOOLTIP_VERBOSE = 1 << 7,
  TOOLTIP_MARKS = 1 << 8,
  TOOLTIP_AIRCRAFT_TRAIL = 1 << 9,
  TOOLTIP_DISTBRG_USER = 1 << 10,
  TOOLTIP_DISTBRG_ROUTE = 1 << 11
};

ATOOLS_DECLARE_FLAGS_32(DisplayTooltipOptions, optsd::DisplayTooltipOption)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(optsd::DisplayTooltipOptions)

enum DisplayClickOption : quint32
{
  CLICK_NONE = 0,
  CLICK_AIRPORT = 1 << 1,
  CLICK_NAVAID = 1 << 2,
  CLICK_AIRSPACE = 1 << 3,
  CLICK_AIRPORT_PROC = 1 << 4,
  CLICK_AIRCRAFT_AI = 1 << 5,
  CLICK_AIRCRAFT_USER = 1 << 6,
  CLICK_FLIGHTPLAN = 1 << 7
};

ATOOLS_DECLARE_FLAGS_32(DisplayClickOptions, optsd::DisplayClickOption)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(optsd::DisplayClickOptions)

} // namespace optsd

Q_DECLARE_TYPEINFO(opts::Flags, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(opts2::Flags2, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(opts::DisplayOnlineFlags, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(optsw::FlagsWeather, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(optsac::DisplayOptionsUserAircraft, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(optsac::DisplayOptionsAiAircraft, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(optsd::DisplayOptionsAirport, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(optsd::DisplayOptionsNavAid, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(optsd::DisplayOptionsAirspace, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(optsd::DisplayOptionsMeasurement, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(optsd::DisplayOptionsRose, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(optsd::DisplayOptionsRoute, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(optsd::DisplayTooltipOptions, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(optsd::DisplayClickOptions, Q_PRIMITIVE_TYPE);

#endif // LITTLENAVMAP_OPTIONFLAGS_H
