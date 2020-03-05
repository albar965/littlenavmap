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

#ifndef LITTLENAVMAP_OPTIONDATA_H
#define LITTLENAVMAP_OPTIONDATA_H

#include <QColor>
#include <QFlags>
#include <QVector>

namespace opts {
enum Flag
{
  NO_FLAGS = 0,

  /* Reload KML files on startup.
   * ui->checkBoxOptionsStartupLoadKml  */
  STARTUP_LOAD_KML = 1 << 0,

  /* Reload all map settings on startup.
   * ui->checkBoxOptionsStartupLoadMapSettings */
  STARTUP_LOAD_MAP_SETTINGS = 1 << 1,

  /* Reload route on startup.
   * ui->checkBoxOptionsStartupLoadRoute */
  STARTUP_LOAD_ROUTE = 1 << 2,

  /* Show home on starup.
   * ui->radioButtonOptionsStartupShowHome */
  STARTUP_SHOW_HOME = 1 << 3,

  /* Show last position on startup.
   * ui->radioButtonOptionsStartupShowLast */
  STARTUP_SHOW_LAST = 1 << 4,

  /* Show last position on startup.
   * ui->radioButtonOptionsStartupShowFlightplan */
  STARTUP_SHOW_ROUTE = 1 << 5,

  /* Center KML after loading.
   * ui->checkBoxOptionsGuiCenterKml */
  GUI_CENTER_KML = 1 << 6,

  /* Center flight plan after loading.
   * ui->checkBoxOptionsGuiCenterRoute */
  GUI_CENTER_ROUTE = 1 << 7,

  /* Treat empty airports special.
   * ui->checkBoxOptionsMapEmptyAirports */
  MAP_EMPTY_AIRPORTS = 1 << 8,

  /* East/west rule for flight plan calculation.
   * ui->checkBoxOptionsRouteEastWestRule */
  ROUTE_ALTITUDE_RULE = 1 << 9,

  /* Start airway route at NDB.
   * ui->checkBoxOptionsRoutePreferNdb */
  ROUTE_PREFER_NDB = 1 << 10,

  /* Start airway route at VOR.
   * ui->checkBoxOptionsRoutePreferVor */
  ROUTE_PREFER_VOR = 1 << 11,

  /* No box mode when moving map.
   * ui->checkBoxOptionsSimUpdatesConstant */
  SIM_UPDATE_MAP_CONSTANTLY = 1 << 12,

  /* Center flight plan after loading.
   * ui->checkBoxOptionsGuiAvoidOverwrite */
  GUI_AVOID_OVERWRITE_FLIGHTPLAN = 1 << 13,

  /* radioButtonCacheUseOnlineElevation */
  CACHE_USE_ONLINE_ELEVATION = 1 << 14,

  /* radioButtonCacheUseOnlineElevation */
  CACHE_USE_OFFLINE_ELEVATION = 1 << 15,

  /* checkBoxOptionsShowTod*/
  FLIGHT_PLAN_SHOW_TOD = 1 << 16,

  /* checkBoxOptionsStartupLoadInfoContent */
  STARTUP_LOAD_INFO = 1 << 17,

  /* checkBoxOptionsStartupLoadSearch */
  STARTUP_LOAD_SEARCH = 1 << 18,

  /* checkBoxOptionsStartupLoadTrail */
  STARTUP_LOAD_TRAIL = 1 << 19,

  /* checkBoxOptionsGuiOverrideLanguage */
  GUI_OVERRIDE_LANGUAGE = 1 << 20,

  /* checkBoxOptionsGuiOverrideLocale */
  GUI_OVERRIDE_LOCALE = 1 << 21,

  /* checkBoxOptionsRouteExportUserWpt */
  ROUTE_GARMIN_USER_WPT = 1 << 22,

  /* checkBoxOptionsRouteDeclination */
  ROUTE_IGNORE_VOR_DECLINATION = 1 << 23
};

Q_DECLARE_FLAGS(Flags, Flag);
Q_DECLARE_OPERATORS_FOR_FLAGS(opts::Flags);

/* Map detail level during scrolling or zooming */
enum MapScrollDetail
{
  FULL,
  HIGHER,
  NORMAL,
  NONE
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
  COORDS_LONX_LATY /* lon/lat with sign */
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
  DASHED,
  DOTTED,
  SOLID
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

/* comboBoxOptionsStartupUpdateChannels - what updates to check for */
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
  ONLINE_FORMAT_IVAO
};

} // namespace opts

namespace opts2 {
/* Extension from flags to avoid overflow */
enum Flag2
{
  NO_FLAGS2 = 0,

  /* Treat empty airports special.
   * ui->checkBoxOptionsMapEmptyAirports3D */
  MAP_EMPTY_AIRPORTS_3D = 1 << 0,

  /* Save PLN using short names
   * ui->checkBoxOptionsRouteShortName */
  ROUTE_SAVE_SHORT_NAME = 1 << 1,

  /* ui->checkBoxOptionsMapAirportText */
  MAP_AIRPORT_TEXT_BACKGROUND = 1 << 2,

  /* ui->checkBoxOptionsMapNavaidText */
  MAP_NAVAID_TEXT_BACKGROUND = 1 << 3,

  /* ui->checkBoxOptionsMapFlightplanText */
  MAP_ROUTE_TEXT_BACKGROUND = 1 << 4,

  /* ui->checkBoxOptionsMapAirportBoundary */
  MAP_AIRPORT_BOUNDARY = 1 << 5,

  /* ui->checkBoxOptionsMapFlightplanDimPassed */
  MAP_ROUTE_DIM_PASSED = 1 << 6,

  /* ui->checkBoxOptionsSimDoNotFollowOnScroll */
  ROUTE_NO_FOLLOW_ON_MOVE = 1 << 7,

  /* ui->checkBoxOptionsSimCenterLeg */
  ROUTE_AUTOZOOM = 1 << 8,

  /* ui->checkBoxOptionsMapAirportDiagram */
  MAP_AIRPORT_DIAGRAM = 1 << 9,

  /* ui->checkBoxOptionsSimCenterLegTable */
  ROUTE_CENTER_ACTIVE_LEG = 1 << 10,

  /* checkBoxOptionsMapZoomAvoidBlurred */
  MAP_AVOID_BLURRED_MAP = 1 << 11,

  /* checkBoxOptionsMapUndock */
  MAP_ALLOW_UNDOCK = 1 << 12,

  /* checkBoxOptionsGuiHighDpi */
  HIGH_DPI_DISPLAY_SUPPORT = 1 << 13,

  /* checkBoxDisplayOnlineNameLookup */
  ONLINE_AIRSPACE_BY_NAME = 1 << 14,

  /* checkBoxDisplayOnlineFilenameLookup */
  ONLINE_AIRSPACE_BY_FILE = 1 << 15,

  /* checkBoxOptionsGuiProposeFilename */
  PROPOSE_FILENAME = 1 << 16,

  /* checkBoxOptionsGuiRaiseWindows */
  RAISE_WINDOWS = 1 << 17,

  /* checkBoxOptionsUnitFuelOther */
  UNIT_FUEL_SHOW_OTHER = 1 << 18,

  /* checkBoxOptionsUnitTrueCourse */
  UNIT_TRUE_COURSE = 1 << 19
};

Q_DECLARE_FLAGS(Flags2, Flag2);
Q_DECLARE_OPERATORS_FOR_FLAGS(opts2::Flags2);

} // namespace opts2

namespace optsw {

enum FlagWeather
{
  NO_WEATHER_FLAGS = 0,

  /* Show ASN weather in info panel.
   * ui->checkBoxOptionsWeatherInfoAsn */
  WEATHER_INFO_ACTIVESKY = 1 << 0,

  /* Show NOAA weather in info panel.
   * ui->checkBoxOptionsWeatherInfoNoaa */
  WEATHER_INFO_NOAA = 1 << 1,

  /* Show Vatsim weather in info panel.
   * ui->checkBoxOptionsWeatherInfoVatsim */
  WEATHER_INFO_VATSIM = 1 << 2,

  /* Show FSX/P3D or X-Plane weather in info panel.
   * ui->checkBoxOptionsWeatherInfoFs */
  WEATHER_INFO_FS = 1 << 3,

  /* Show IVAO weather in info panel.
   * ui->checkBoxOptionsWeatherInfoIvao*/
  WEATHER_INFO_IVAO = 1 << 4,

  /* Show ASN weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipAsn */
  WEATHER_TOOLTIP_ACTIVESKY = 1 << 5,

  /* Show NOAA weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipNoaa */
  WEATHER_TOOLTIP_NOAA = 1 << 6,

  /* Show Vatsim weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipVatsim */
  WEATHER_TOOLTIP_VATSIM = 1 << 7,

  /* Show FSX/P3D or X-Plane weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipFs */
  WEATHER_TOOLTIP_FS = 1 << 8,

  /* Show IVAO weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipIvao*/
  WEATHER_TOOLTIP_IVAO = 1 << 9,

  WEATHER_INFO_ALL = WEATHER_INFO_ACTIVESKY | WEATHER_INFO_NOAA | WEATHER_INFO_VATSIM | WEATHER_INFO_FS |
                     WEATHER_INFO_IVAO,

  WEATHER_TOOLTIP_ALL = WEATHER_TOOLTIP_ACTIVESKY | WEATHER_TOOLTIP_NOAA | WEATHER_TOOLTIP_VATSIM | WEATHER_TOOLTIP_FS |
                        WEATHER_TOOLTIP_IVAO
};

Q_DECLARE_FLAGS(FlagsWeather, FlagWeather);
Q_DECLARE_OPERATORS_FOR_FLAGS(optsw::FlagsWeather);
} // namespace opts2

namespace optsd {
/* Changing these option values will also change the saved values thus invalidating user settings */
enum DisplayOption
{
  ITEM_NONE = 0,

  ITEM_AIRPORT_NAME = 1 << 1,
  ITEM_AIRPORT_TOWER = 1 << 2,
  ITEM_AIRPORT_ATIS = 1 << 3,
  ITEM_AIRPORT_RUNWAY = 1 << 4,
  // ITEM_AIRPORT_WIND_POINTER = 1 << 5,

  ITEM_USER_AIRCRAFT_REGISTRATION = 1 << 8,
  ITEM_USER_AIRCRAFT_TYPE = 1 << 9,
  ITEM_USER_AIRCRAFT_AIRLINE = 1 << 10,
  ITEM_USER_AIRCRAFT_FLIGHT_NUMBER = 1 << 11,
  ITEM_USER_AIRCRAFT_IAS = 1 << 12,
  ITEM_USER_AIRCRAFT_GS = 1 << 13,
  ITEM_USER_AIRCRAFT_CLIMB_SINK = 1 << 14,
  ITEM_USER_AIRCRAFT_HEADING = 1 << 15,
  ITEM_USER_AIRCRAFT_ALTITUDE = 1 << 16,
  ITEM_USER_AIRCRAFT_WIND = 1 << 17,
  ITEM_USER_AIRCRAFT_TRACK_LINE = 1 << 18,
  ITEM_USER_AIRCRAFT_WIND_POINTER = 1 << 19,

  ITEM_AI_AIRCRAFT_DEP_DEST = 1 << 21,
  ITEM_AI_AIRCRAFT_REGISTRATION = 1 << 22,
  ITEM_AI_AIRCRAFT_TYPE = 1 << 23,
  ITEM_AI_AIRCRAFT_AIRLINE = 1 << 24,
  ITEM_AI_AIRCRAFT_FLIGHT_NUMBER = 1 << 25,
  ITEM_AI_AIRCRAFT_IAS = 1 << 26,
  ITEM_AI_AIRCRAFT_GS = 1 << 27,
  ITEM_AI_AIRCRAFT_CLIMB_SINK = 1 << 28,
  ITEM_AI_AIRCRAFT_HEADING = 1 << 29,
  ITEM_AI_AIRCRAFT_ALTITUDE = 1 << 30
};

Q_DECLARE_FLAGS(DisplayOptions, DisplayOption);
Q_DECLARE_OPERATORS_FOR_FLAGS(optsd::DisplayOptions);

/* On-screen navigation aids */
enum DisplayOptionNavAid
{
  NAVAIDS_NONE = 0,
  NAVAIDS_CENTER_CROSS = 1 << 1, /* White center cross on black background */
  NAVAIDS_TOUCHSCREEN_AREAS = 1 << 2, /* White center marks showing touch areas on black background */
  NAVAIDS_TOUCHSCREEN_REGIONS = 1 << 3, /* Dim gray highlighted touch areas  */
  NAVAIDS_TOUCHSCREEN_ICONS = 1 << 4 /* Icons */
};

Q_DECLARE_FLAGS(DisplayOptionsNavAid, DisplayOptionNavAid);
Q_DECLARE_OPERATORS_FOR_FLAGS(optsd::DisplayOptionsNavAid);

enum DisplayOptionRose
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
  ROSE_DIR_LABLES = 1 << 8
};

Q_DECLARE_FLAGS(DisplayOptionsRose, DisplayOptionRose);
Q_DECLARE_OPERATORS_FOR_FLAGS(optsd::DisplayOptionsRose);

enum DisplayOptionRoute
{
  ROUTE_NONE = 0,
  ROUTE_DISTANCE = 1 << 0,
  ROUTE_MAG_COURSE_RHUMB = 1 << 1,
  ROUTE_TRUE_COURSE_RHUMB = 1 << 2,
  ROUTE_MAG_COURSE_GC = 1 << 3,
  ROUTE_TRUE_COURSE_GC = 1 << 4
};

Q_DECLARE_FLAGS(DisplayOptionsRoute, DisplayOptionRoute);
Q_DECLARE_OPERATORS_FOR_FLAGS(optsd::DisplayOptionsRoute);

enum DisplayTooltipOption
{
  TOOLTIP_NONE = 0,
  TOOLTIP_AIRPORT = 1 << 1,
  TOOLTIP_NAVAID = 1 << 2,
  TOOLTIP_AIRSPACE = 1 << 3,
  TOOLTIP_WIND = 1 << 4
};

Q_DECLARE_FLAGS(DisplayTooltipOptions, DisplayTooltipOption);
Q_DECLARE_OPERATORS_FOR_FLAGS(optsd::DisplayTooltipOptions);

enum DisplayClickOption
{
  CLICK_NONE = 0,
  CLICK_AIRPORT = 1 << 1,
  CLICK_NAVAID = 1 << 2,
  CLICK_AIRSPACE = 1 << 3
};

Q_DECLARE_FLAGS(DisplayClickOptions, DisplayClickOption);
Q_DECLARE_OPERATORS_FOR_FLAGS(optsd::DisplayClickOptions);

} // namespace optsd

/*
 * Contains global options that are provided using a singelton pattern.
 * All default values are defined in the widgets in the options.ui file.
 *
 * Values applied by the reset function in the options dialog are defined in this structure.
 *
 * This class will be populated by the OptionsDialog which loads widget data from the settings
 * and transfers this data into this class.
 */
class OptionData
{
public:
  /* Get a the global options instance. Not thread safe.
   * OptionsDialog.restoreState() has to be called before getting an instance */
  static const OptionData& instance();

  ~OptionData();

  /* Get option flags */
  opts::Flags getFlags() const
  {
    return flags;
  }

  opts2::Flags2 getFlags2() const
  {
    return flags2;
  }

  opts::UnitDist getUnitDist() const
  {
    return unitDist;
  }

  opts::UnitShortDist getUnitShortDist() const
  {
    return unitShortDist;
  }

  opts::UnitAlt getUnitAlt() const
  {
    return unitAlt;
  }

  opts::UnitSpeed getUnitSpeed() const
  {
    return unitSpeed;
  }

  opts::UnitVertSpeed getUnitVertSpeed() const
  {
    return unitVertSpeed;
  }

  opts::UnitCoords getUnitCoords() const
  {
    return unitCoords;
  }

  opts::UnitFuelAndWeight getUnitFuelAndWeight() const
  {
    return unitFuelWeight;
  }

  /* Vector of (red) range ring distances in nautical miles */
  const QVector<int>& getMapRangeRings() const
  {
    return mapRangeRings;
  }

  /* ASN path that overrides the default */
  const QString& getWeatherActiveSkyPath() const
  {
    return weatherActiveSkyPath;
  }

  /* X-Plane path that overrides the default */
  const QString& getWeatherXplanePath() const
  {
    return weatherXplanePath;
  }

  const QString& getWeatherNoaaUrl() const
  {
    return weatherNoaaUrl;
  }

  const QString& getWeatherVatsimUrl() const
  {
    return weatherVatsimUrl;
  }

  const QString& getWeatherIvaoUrl() const
  {
    return weatherIvaoUrl;
  }

  /* List of directories that excludes paths from being recognized as add-ons. Only for scenery database loading. */
  const QStringList& getDatabaseAddonExclude() const
  {
    return databaseAddonExclude;
  }

  /* List of directories and files that are excluded from scenery database loading */
  const QStringList& getDatabaseExclude() const
  {
    return databaseExclude;
  }

  opts::MapScrollDetail getMapScrollDetail() const
  {
    return mapScrollDetail;
  }

  opts::MapNavigation getMapNavigation() const
  {
    return mapNavigation;
  }

  opts::SimUpdateRate getSimUpdateRate() const
  {
    return simUpdateRate;
  }

  /* Disk cache size for OSM, OTM and elevation map data */
  unsigned int getCacheSizeDiskMb() const
  {
    return static_cast<unsigned int>(cacheSizeDisk);
  }

  /* RAM cache size for OSM, OTM and elevation map data */
  unsigned int getCacheSizeMemoryMb() const
  {
    return static_cast<unsigned int>(cacheSizeMemory);
  }

  /* Info panel text size in percent */
  int getGuiInfoTextSize() const
  {
    return guiInfoTextSize;
  }

  /* Aircraft performance report in flight plan dock */
  int getGuiPerfReportTextSize() const
  {
    return guiPerfReportTextSize;
  }

  /* User aircraft panel text size in percent */
  int getGuiInfoSimSize() const
  {
    return guiInfoSimSize;
  }

  /* Route table view text size in percent */
  int getGuiRouteTableTextSize() const
  {
    return guiRouteTableTextSize;
  }

  /* Search result table view text size in percent */
  int getGuiSearchTableTextSize() const
  {
    return guiSearchTableTextSize;
  }

  /* Map click recognition radius in pixel */
  int getMapClickSensitivity() const
  {
    return mapClickSensitivity;
  }

  /* Map tooltip recognition radius in pixel */
  int getMapTooltipSensitivity() const
  {
    return mapTooltipSensitivity;
  }

  /* Map symbol size in percent */
  int getMapSymbolSize() const
  {
    return mapSymbolSize;
  }

  /* Map text size in percent */
  int getMapTextSize() const
  {
    return mapTextSize;
  }

  /* Ground buffer in feet for red line in profile view */
  int getRouteGroundBuffer() const
  {
    return routeGroundBuffer;
  }

  /* Bounding box for aircraft updates in percent */
  int getSimUpdateBox() const
  {
    return simUpdateBox;
  }

  /* Default zoom distance for point objects */
  float getMapZoomShowClick() const
  {
    return mapZoomShowClick;
  }

  /* Default zoom distance for point objects */
  float getMapZoomShowMenu() const
  {
    return mapZoomShowMenu;
  }

  int getDisplayTextSizeAircraftAi() const
  {
    return displayTextSizeAircraftAi;
  }

  int getDisplayThicknessFlightplan() const
  {
    return displayThicknessFlightplan;
  }

  int getDisplaySymbolSizeAirport() const
  {
    return displaySymbolSizeAirport;
  }

  int getDisplaySymbolSizeAirportWeather() const
  {
    return displaySymbolSizeAirportWeather;
  }

  int getDisplaySymbolSizeWindBarbs() const
  {
    return displaySymbolSizeWindBarbs;
  }

  int getDisplaySymbolSizeAircraftAi() const
  {
    return displaySymbolSizeAircraftAi;
  }

  int getDisplayTextSizeFlightplan() const
  {
    return displayTextSizeFlightplan;
  }

  int getDisplayTextSizeAircraftUser() const
  {
    return displayTextSizeAircraftUser;
  }

  int getDisplaySymbolSizeAircraftUser() const
  {
    return displaySymbolSizeAircraftUser;
  }

  int getDisplayTextSizeAirport() const
  {
    return displayTextSizeAirport;
  }

  int getDisplayThicknessTrail() const
  {
    return displayThicknessTrail;
  }

  opts::DisplayTrailType getDisplayTrailType() const
  {
    return displayTrailType;
  }

  int getDisplayTextSizeNavaid() const
  {
    return displayTextSizeNavaid;
  }

  int getDisplaySymbolSizeNavaid() const
  {
    return displaySymbolSizeNavaid;
  }

  const QColor& getFlightplanColor() const
  {
    return flightplanColor;
  }

  const QColor& getFlightplanProcedureColor() const
  {
    return flightplanProcedureColor;
  }

  const QColor& getFlightplanActiveSegmentColor() const
  {
    return flightplanActiveColor;
  }

  const QColor& getFlightplanPassedSegmentColor() const
  {
    return flightplanPassedColor;
  }

  const QColor& getTrailColor() const
  {
    return trailColor;
  }

  const optsd::DisplayOptions& getDisplayOptions() const
  {
    return displayOptions;
  }

  const optsd::DisplayOptionsRose& getDisplayOptionsRose() const
  {
    return displayOptionsRose;
  }

  const optsd::DisplayOptionsNavAid& getDisplayOptionsNavAid() const
  {
    return displayOptionsNavAid;
  }

  const optsd::DisplayOptionsRoute& getDisplayOptionsRoute() const
  {
    return displayOptionsRoute;
  }

  optsd::DisplayTooltipOptions getDisplayTooltipOptions() const
  {
    return displayTooltipOptions;
  }

  optsd::DisplayClickOptions getDisplayClickOptions() const
  {
    return displayClickOptions;
  }

  int getDisplayThicknessRangeDistance() const
  {
    return displayThicknessRangeDistance;
  }

  int getDisplayThicknessCompassRose() const
  {
    return displayThicknessCompassRose;
  }

  int getDisplaySunShadingDimFactor() const
  {
    return displaySunShadingDimFactor;
  }

  int getGuiStyleMapDimming() const
  {
    return guiStyleMapDimming;
  }

  const QString& getOfflineElevationPath() const
  {
    return cacheOfflineElevationPath;
  }

  opts::AltitudeRule getAltitudeRuleType() const
  {
    return altitudeRuleType;
  }

  opts::UpdateRate getUpdateRate() const
  {
    return updateRate;
  }

  opts::UpdateChannels getUpdateChannels() const
  {
    return updateChannels;
  }

  int getAircraftTrackMaxPoints() const
  {
    return aircraftTrackMaxPoints;
  }

  int getSimNoFollowAircraftOnScrollSeconds() const
  {
    return simNoFollowAircraftOnScroll;
  }

  opts::OnlineNetwork getOnlineNetwork() const
  {
    return onlineNetwork;
  }

  opts::OnlineFormat getOnlineFormat() const;

  /* URL to "status.txt" or empty if not applicable */
  QString getOnlineStatusUrl() const;

  /* URL to "whazzup.txt" or empty if not applicable */
  QString getOnlineWhazzupUrl() const;

  /* Reload files or automatic or not applicable if -1 */
  int getOnlineReloadTimeSeconds() const
  {
    return onlineReloadSeconds;
  }

  /* Value from networks.cfg or -1 if auto */
  int getOnlineReloadTimeSecondsConfig() const
  {
    return onlineReloadSecondsConfig;
  }

  int getDisplayTextSizeRangeDistance() const
  {
    return displayTextSizeRangeDistance;
  }

  int getDisplayTextSizeCompassRose() const
  {
    return displayTextSizeCompassRose;
  }

  /* Online center diameter below. -1 if network value should be used */
  int getDisplayOnlineClearance() const
  {
    return displayOnlineClearance;
  }

  int getDisplayOnlineArea() const
  {
    return displayOnlineArea;
  }

  int getDisplayOnlineApproach() const
  {
    return displayOnlineApproach;
  }

  int getDisplayOnlineDeparture() const
  {
    return displayOnlineDeparture;
  }

  int getDisplayOnlineFir() const
  {
    return displayOnlineFir;
  }

  int getDisplayOnlineObserver() const
  {
    return displayOnlineObserver;
  }

  int getDisplayOnlineGround() const
  {
    return displayOnlineGround;
  }

  int getDisplayOnlineTower() const
  {
    return displayOnlineTower;
  }

  QString getWebDocumentRoot() const
  {
    return webDocumentRoot;
  }

  int getWebPort() const
  {
    return webPort;
  }

  bool isWebEncrypted() const
  {
    return webEncrypted;
  }

  const QString& getWeatherXplaneWind() const
  {
    return weatherXplaneWind;
  }

  const QString& getWeatherNoaaWindBaseUrl() const
  {
    return weatherNoaaWindBaseUrl;
  }

  const QString& getCacheUserAirspacePath() const
  {
    return cacheUserAirspacePath;
  }

  const QString& getCacheUserAirspaceExtensions() const
  {
    return cacheUserAirspaceExtensions;
  }

  int getDisplayTransparencyMora() const
  {
    return displayTransparencyMora;
  }

  int getDisplayTextSizeMora() const
  {
    return displayTextSizeMora;
  }

  int getMapNavTouchArea() const
  {
    return mapNavTouchArea;
  }

  optsw::FlagsWeather getFlagsWeather() const
  {
    return flagsWeather;
  }

private:
  friend class OptionsDialog;

  OptionData();
  static OptionData& instanceInternal();

  // Singleton instance
  static OptionData *optionData;

  // Defines the defaults used for reset
  opts::Flags flags = opts::STARTUP_LOAD_KML | opts::STARTUP_LOAD_MAP_SETTINGS | opts::STARTUP_LOAD_ROUTE |
                      opts::STARTUP_SHOW_LAST | opts::GUI_CENTER_KML | opts::GUI_CENTER_ROUTE |
                      opts::GUI_AVOID_OVERWRITE_FLIGHTPLAN | opts::MAP_EMPTY_AIRPORTS | opts::ROUTE_ALTITUDE_RULE |
                      opts::FLIGHT_PLAN_SHOW_TOD | opts::CACHE_USE_ONLINE_ELEVATION |
                      opts::STARTUP_LOAD_INFO | opts::STARTUP_LOAD_SEARCH | opts::STARTUP_LOAD_TRAIL;

  // Defines the defaults used for reset
  optsw::FlagsWeather flagsWeather =
    optsw::WEATHER_INFO_FS |
    optsw::WEATHER_INFO_ACTIVESKY |
    optsw::WEATHER_INFO_NOAA |
    optsw::WEATHER_TOOLTIP_FS |
    optsw::WEATHER_TOOLTIP_ACTIVESKY |
    optsw::WEATHER_TOOLTIP_NOAA;

  opts2::Flags2 flags2 = opts2::MAP_AIRPORT_TEXT_BACKGROUND | opts2::MAP_ROUTE_TEXT_BACKGROUND |
                         opts2::MAP_ROUTE_DIM_PASSED | opts2::MAP_AIRPORT_DIAGRAM |
                         opts2::MAP_ALLOW_UNDOCK | opts2::MAP_AVOID_BLURRED_MAP | opts2::ONLINE_AIRSPACE_BY_FILE |
                         opts2::ONLINE_AIRSPACE_BY_NAME | opts2::RAISE_WINDOWS | opts2::MAP_EMPTY_AIRPORTS_3D |
                         opts2::HIGH_DPI_DISPLAY_SUPPORT | opts2::ROUTE_CENTER_ACTIVE_LEG |
                         opts2::ROUTE_CENTER_ACTIVE_LEG | opts2::ROUTE_AUTOZOOM | opts2::ROUTE_NO_FOLLOW_ON_MOVE;

  // ui->lineEditOptionsMapRangeRings
  QVector<int> mapRangeRings = QVector<int>({50, 100, 200, 500});

  QString weatherActiveSkyPath, // ui->lineEditOptionsWeatherAsnPath
          weatherXplanePath, // lineEditOptionsWeatherXplanePath
          weatherNoaaUrl = "https://tgftp.nws.noaa.gov/data/observations/metar/cycles/%1Z.TXT",
          weatherVatsimUrl = "https://metar.vatsim.net/metar.php?id=%1",
          weatherIvaoUrl = "http://wx.ivao.aero/metar.php";

  QString cacheOfflineElevationPath, cacheUserAirspacePath, cacheUserAirspaceExtensions = "*.txt";

  // ui->listWidgetOptionsDatabaseAddon
  QStringList databaseAddonExclude;

  // ui->listWidgetOptionsDatabaseExclude
  QStringList databaseExclude;

  opts::MapScrollDetail mapScrollDetail = opts::HIGHER;
  opts::MapNavigation mapNavigation = opts::MAP_NAV_CLICK_DRAG_MOVE;

  // ui->radioButtonOptionsMapSimUpdateFast
  // ui->radioButtonOptionsMapSimUpdateLow
  // ui->radioButtonOptionsMapSimUpdateMedium
  opts::SimUpdateRate simUpdateRate = opts::MEDIUM;

  // ui->spinBoxOptionsMapSimUpdateBox
  int simUpdateBox = 50;

  // ui->spinBoxOptionsCacheDiskSize
  int cacheSizeDisk = 2000;

  // ui->spinBoxOptionsCacheMemorySize
  int cacheSizeMemory = 1000;

  // ui->spinBoxOptionsGuiInfoText
  int guiInfoTextSize = 100;

  // ui->spinBoxOptionsGuiAircraftPerf
  int guiPerfReportTextSize = 100;

  // ui->spinBoxOptionsGuiSimInfoText
  int guiInfoSimSize = 100;

  // ui->spinBoxOptionsGuiRouteText
  int guiRouteTableTextSize = 100;

  // ui->spinBoxOptionsGuiSearchText
  int guiSearchTableTextSize = 100;

  // ui->spinBoxOptionsGuiThemeMapDimming
  int guiStyleMapDimming = 50;

  // ui->spinBoxOptionsMapClickRect
  int mapClickSensitivity = 10;

  // ui->spinBoxOptionsMapTooltipRect
  int mapTooltipSensitivity = 10;

  // ui->spinBoxOptionsMapSymbolSize
  int mapSymbolSize = 100;

  // ui->spinBoxOptionsMapTextSize
  int mapTextSize = 100;

  // ui->doubleSpinBoxOptionsMapZoomShowMap
  float mapZoomShowClick = 1.5f;

  // ui->doubleSpinBoxOptionsMapZoomShowMapMenu
  float mapZoomShowMenu = 1.5f;

  // ui->spinBoxOptionsRouteGroundBuffer
  int routeGroundBuffer = 1000;

  // comboBoxOptionsUnitDistance
  opts::UnitDist unitDist = opts::DIST_NM;

  // comboBoxOptionsUnitShortDistance
  opts::UnitShortDist unitShortDist = opts::DIST_SHORT_FT;

  // comboBoxOptionsUnitAlt
  opts::UnitAlt unitAlt = opts::ALT_FT;

  // comboBoxOptionsUnitSpeed
  opts::UnitSpeed unitSpeed = opts::SPEED_KTS;

  // comboBoxOptionsUnitVertSpeed
  opts::UnitVertSpeed unitVertSpeed = opts::VERT_SPEED_FPM;

  // comboBoxOptionsUnitCoords
  opts::UnitCoords unitCoords = opts::COORDS_DMS;

  // comboBoxOptionsUnitVertFuel
  opts::UnitFuelAndWeight unitFuelWeight = opts::FUEL_WEIGHT_GAL_LBS;

  // comboBoxOptionsRouteAltitudeRuleType
  opts::AltitudeRule altitudeRuleType = opts::EAST_WEST;

  // spinBoxOptionsDisplayTextSizeAircraftAi
  int displayTextSizeAircraftAi = 100;

  // spinBoxOptionsDisplayThicknessFlightplan
  int displayThicknessFlightplan = 100;

  // spinBoxOptionsDisplaySymbolSizeAirport
  int displaySymbolSizeAirport = 100;

  // spinBoxOptionsDisplaySymbolSizeAirportWeather
  int displaySymbolSizeAirportWeather = 100;

  // spinBoxOptionsDisplaySymbolSizeWindBarbs
  int displaySymbolSizeWindBarbs = 100;

  // spinBoxOptionsDisplaySymbolSizeAircraftAi
  int displaySymbolSizeAircraftAi = 100;

  // spinBoxOptionsDisplayTextSizeNavaid
  int displayTextSizeNavaid = 100;

  // spinBoxOptionsDisplaySymbolSizeNavaid
  int displaySymbolSizeNavaid = 100;

  // spinBoxOptionsDisplayTextSizeFlightplan
  int displayTextSizeFlightplan = 100;

  // spinBoxOptionsDisplayTextSizeAircraftUser
  int displayTextSizeAircraftUser = 100;

  // spinBoxOptionsDisplaySymbolSizeAircraftUser
  int displaySymbolSizeAircraftUser = 100;

  // spinBoxOptionsDisplayTextSizeAirport
  int displayTextSizeAirport = 100;

  // spinBoxOptionsDisplayThicknessTrail
  int displayThicknessTrail = 100;

  // spinBoxOptionsDisplayThicknessRangeDistance
  int displayThicknessRangeDistance = 100;

  // spinBoxOptionsDisplayThicknessCompassRose
  int displayThicknessCompassRose = 100;

  // spinBoxOptionsDisplaySunShadeDarkness
  int displaySunShadingDimFactor = 40;

  // spinBoxSimMaxTrackPoints
  int aircraftTrackMaxPoints = 20000;

  // spinBoxSimDoNotFollowOnScrollTime
  int simNoFollowAircraftOnScroll = 10;

  // spinBoxOptionsDisplayTextSizeRangeDistance
  int displayTextSizeRangeDistance = 100;

  // spinBoxOptionsDisplayTextSizeCompassRose
  int displayTextSizeCompassRose = 100;

  // spinBoxDisplayOnlineClearance
  int displayOnlineClearance = -1;

  // spinBoxDisplayOnlineArea
  int displayOnlineArea = 200;

  // spinBoxDisplayOnlineApproach
  int displayOnlineApproach = 40;

  // spinBoxDisplayOnlineDeparture
  int displayOnlineDeparture = -1;

  // spinBoxDisplayOnlineFir
  int displayOnlineFir = 200;

  // spinBoxDisplayOnlineObserver
  int displayOnlineObserver = -1;

  // spinBoxDisplayOnlineGround
  int displayOnlineGround = 10;

  // spinBoxDisplayOnlineTower
  int displayOnlineTower = 20;

  // spinBoxOptionsDisplayTransparencyMora
  int displayTransparencyMora = 50;

  // spinBoxOptionsDisplayTransparencyMora
  int displayTextSizeMora = 100;

  // spinBoxOptionsMapNavTouchscreenArea
  int mapNavTouchArea = 10;

  QColor flightplanColor = QColor(Qt::yellow), flightplanProcedureColor = QColor(255, 150, 0),
         flightplanActiveColor = QColor(Qt::magenta), flightplanPassedColor = QColor(Qt::gray),
         trailColor = QColor(Qt::black);

  // comboBoxOptionsDisplayTrailType
  opts::DisplayTrailType displayTrailType = opts::DASHED;

  /* Default values are set by widget states - these are needed for the reset button */
  optsd::DisplayOptions displayOptions =
    optsd::ITEM_AIRPORT_NAME | optsd::ITEM_AIRPORT_TOWER | optsd::ITEM_AIRPORT_ATIS |
    optsd::ITEM_AIRPORT_RUNWAY |
    optsd::ITEM_USER_AIRCRAFT_GS | optsd::ITEM_USER_AIRCRAFT_ALTITUDE |
    optsd::ITEM_USER_AIRCRAFT_WIND | optsd::ITEM_USER_AIRCRAFT_TRACK_LINE |
    optsd::ITEM_USER_AIRCRAFT_WIND_POINTER |
    optsd::ITEM_AI_AIRCRAFT_REGISTRATION | optsd::ITEM_AI_AIRCRAFT_TYPE |
    optsd::ITEM_AI_AIRCRAFT_AIRLINE | optsd::ITEM_AI_AIRCRAFT_GS |
    optsd::ITEM_AI_AIRCRAFT_ALTITUDE | optsd::ITEM_AI_AIRCRAFT_DEP_DEST;

  optsd::DisplayOptionsRose displayOptionsRose =
    optsd::ROSE_RANGE_RINGS | optsd::ROSE_DEGREE_MARKS | optsd::ROSE_DEGREE_LABELS | optsd::ROSE_HEADING_LINE |
    optsd::ROSE_TRACK_LINE | optsd::ROSE_TRACK_LABEL | optsd::ROSE_CRAB_ANGLE | optsd::ROSE_NEXT_WAYPOINT |
    optsd::ROSE_DIR_LABLES;

  optsd::DisplayOptionsNavAid displayOptionsNavAid = optsd::NAVAIDS_NONE;

  optsd::DisplayOptionsRoute displayOptionsRoute = optsd::ROUTE_DISTANCE | optsd::ROUTE_MAG_COURSE_RHUMB;

  optsd::DisplayTooltipOptions displayTooltipOptions = optsd::TOOLTIP_AIRPORT | optsd::TOOLTIP_AIRSPACE |
                                                       optsd::TOOLTIP_NAVAID | optsd::TOOLTIP_WIND;
  optsd::DisplayClickOptions displayClickOptions = optsd::CLICK_AIRPORT | optsd::CLICK_AIRSPACE | optsd::CLICK_NAVAID;

  opts::UpdateRate updateRate = opts::DAILY;
  opts::UpdateChannels updateChannels = opts::STABLE;

  // Used in the singelton to check if data was already loaded
  bool valid = false;

  /* Online network configuration ========================================= */
  opts::OnlineNetwork onlineNetwork = opts::ONLINE_NONE;
  opts::OnlineFormat onlineFormat = opts::ONLINE_FORMAT_VATSIM;
  QString onlineStatusUrl, onlineWhazzupUrl;

  /* Values loaded from networks.cfg */
  int onlineReloadSeconds = 180, onlineReloadSecondsConfig = 180;
  QString onlineVatsimStatusUrl;
  QString onlineIvaoStatusUrl;
  QString onlinePilotEdgeStatusUrl;

  /* Webserver values */
  QString webDocumentRoot;
  int webPort = 8965;
  /* true for HTTPS / SSL */
  bool webEncrypted = false;

  /* X-Plane GRIB file or NOAA GRIB base URL */
  QString weatherXplaneWind, weatherNoaaWindBaseUrl = "https://nomads.ncep.noaa.gov/cgi-bin/filter_gfs_1p00.pl";
};

#endif // LITTLENAVMAP_OPTIONDATA_H
