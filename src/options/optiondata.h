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

#ifndef LITTLENAVMAP_OPTIONDATA_H
#define LITTLENAVMAP_OPTIONDATA_H

#include "options/optionflags.h"

#include <QColor>
#include <QMap>

class QSize;
class QFont;

/*
 * Contains global options that are provided using a singelton pattern.
 * All default values are defined in the widgets in the options.ui file.
 *
 * Values applied by the reset function in the options dialog are defined in this structure.
 *
 * Values applied on first startup are defined by action state in .ui file.
 *
 * This class will be populated by the OptionsDialog which loads widget data from the settings
 * and transfers this data into this class.
 */
class OptionData
{
public:
  /* Define default operators for comparison */
  bool operator==(const OptionData&) const = default;
  bool operator!=(const OptionData&) const = default;

  /* Get a the global options instance. Not thread safe.
   * OptionsDialog.restoreState() has to be called before getting an instance */
  static const OptionData& instance();

  /* Get locale name like "en_US" or "de" for user interface language. Empty on first start.
   * This uses the settings directly and does not need an OptionData instance.
   * Can be overridden by command line. */
  static QString getLanguageFromConfigFile();

  /* Write directly to settings file */
  static void saveLanguageToConfigFile(const QString& language);

  /* Get option flags */
  const opts::Flags getFlags() const
  {
    return flags;
  }

  const opts2::Flags2 getFlags2() const
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

  /* ASN path that overrides the default */
  const QString& getWeatherActiveSkyPath() const
  {
    return weatherActiveSkyPath;
  }

  /* X-Plane 11 path to METAR.rwx that overrides the default */
  const QString& getWeatherXplane11Path() const
  {
    return weatherXplane11Path;
  }

  /* X-Plane 12 path to METAR and wind files that overrides the default "X-Plane 12/Output/real weather" */
  const QString& getWeatherXplane12Path() const
  {
    return weatherXplane12Path;
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

  /* Folders that are included in scanning */
  const QStringList& getDatabaseInclude() const
  {
    return databaseInclude;
  }

  /* List of directories and files that are excluded from scenery database loading */
  const QStringList& getDatabaseExclude() const
  {
    return databaseExclude;
  }

  /* List of directories that excludes paths from being recognized as add-ons. Only for scenery database loading. */
  const QStringList& getDatabaseAddonExclude() const
  {
    return databaseExcludeAddon;
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
  int getCacheSizeMemoryMapMb() const
  {
    return cacheSizeMemoryMap;
  }

  /* GLOBE elevation tiles cache */
  int getCacheSizeMemoryProfileMb() const
  {
    return cacheSizeMemoryProfile;
  }

  /* Buffer radius for secondary elevation polygon showing surrounding. In local user units (NM, mi, KM) */
  float getProfileBuffer() const
  {
    return profileBuffer;
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

  /* Route remarks input field text size in percent */
  int getGuiRouteRemarksTextSize() const
  {
    return guiRouteRemarksTextSize;
  }

  /* Route header label text size in percent */
  int getGuiRouteInfoTextSize() const
  {
    return guiRouteInfoTextSize;
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

  /* Default zoom distance for point objects using double click */
  float getMapZoomShowClick() const
  {
    return mapZoomShowClick;
  }

  /* Default zoom distance for point objects selected from menu or information window */
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

  int getDisplaySymbolSizeFlightplan() const
  {
    return displaySymbolSizeFlightplan;
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

  int getDisplayTextSizeAirportRunway() const
  {
    return displayTextSizeAirportRunway;
  }

  int getDisplayTextSizeAirportTaxiway() const
  {
    return displayTextSizeAirportTaxiway;
  }

  int getDisplayThicknessTrail() const
  {
    return displayThicknessTrail;
  }

  opts::DisplayTrailType getDisplayTrailType() const
  {
    return displayTrailType;
  }

  opts::DisplayTrailGradientType getDisplayTrailGradientType() const
  {
    return displayTrailGradientType;
  }

  int getDisplayTextSizeNavaid() const
  {
    return displayTextSizeNavaid;
  }

  int getDisplaySymbolSizeNavaid() const
  {
    return displaySymbolSizeNavaid;
  }

  int getDisplayTextSizeAirway() const
  {
    return displayTextSizeAirway;
  }

  int getDisplayThicknessAirway() const
  {
    return displayThicknessAirway;
  }

  const QColor& getFlightplanColor() const
  {
    return flightplanColor;
  }

  const QColor& getFlightplanOutlineColor() const
  {
    return flightplanOutlineColor;
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

  const QColor& getMeasurementColor() const
  {
    return measurementColor;
  }

  const optsd::DisplayOptionsAirport& getDisplayOptionsAirport() const
  {
    return displayOptionsAirport;
  }

  const optsd::DisplayOptionsRose& getDisplayOptionsRose() const
  {
    return displayOptionsRose;
  }

  const optsd::DisplayOptionsMeasurement& getDisplayOptionsMeasurement() const
  {
    return displayOptionsMeasurement;
  }

  const optsd::DisplayOptionsNavAid& getDisplayOptionsNavAid() const
  {
    return displayOptionsNavAid;
  }

  const optsd::DisplayOptionsAirspace& getDisplayOptionsAirspace() const
  {
    return displayOptionsAirspace;
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

  int getDisplayThicknessUserFeature() const
  {
    return displayThicknessUserFeature;
  }

  int getDisplayThicknessMeasurement() const
  {
    return displayThicknessMeasurement;
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

  const QString& getFlightplanPattern() const
  {
    return flightplanPattern;
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

  int getAircraftTrailMaxPoints() const
  {
    return aircraftTrailMaxPoints;
  }

  int getSimNoFollowAircraftScrollSeconds() const
  {
    return simNoFollowOnScrollTime;
  }

  /* In local user units (NM, mi, KM) */
  float getSimZoomOnLandingDistance() const
  {
    return simZoomOnLandingDist;
  }

  /* In local user units (NM, mi, KM) */
  float getSimZoomOnTakeoffDistance() const
  {
    return simZoomOnTakeoffDist;
  }

  int getSimCleanupTableTime() const
  {
    return simCleanupTableTime;
  }

  opts::OnlineNetwork getOnlineNetwork() const
  {
    return onlineNetwork;
  }

  /* Get data format for selected online service. whazzup.txt, JSON, etc. */
  opts::OnlineFormat getOnlineFormat() const;

  /* URL to "status.txt" or empty if not applicable */
  QString getOnlineStatusUrl() const;

  /* URL to "transceivers.json" or empty if not applicable. Only for VATSIM JSON format 3 */
  QString getOnlineTransceiverUrl() const;

  /* URL to "whazzup.txt" or empty if not applicable */
  QString getOnlineWhazzupUrl() const;

  int getDisplayTextSizeUserFeature() const
  {
    return displayTextSizeUserFeature;
  }

  int getDisplayTextSizeMeasurement() const
  {
    return displayTextSizeMeasurement;
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

  const opts::DisplayOnlineFlags& getDisplayOnlineFlags() const
  {
    return displayOnlineFlags;
  }

  const QString& getWebDocumentRoot() const
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

  int getDisplayTransparencyMora() const
  {
    return displayTransparencyMora;
  }

  int getDisplayTextSizeMora() const
  {
    return displayTextSizeMora;
  }

  int getDisplayTransparencyAirportMsa() const
  {
    return displayTransparencyAirportMsa;
  }

  int getDisplayTextSizeAirportMsa() const
  {
    return displayTextSizeAirportMsa;
  }

  int getMapNavTouchArea() const
  {
    return mapNavTouchArea;
  }

  optsw::FlagsWeather getFlagsWeather() const
  {
    return flagsWeather;
  }

  /* Get selected font for map. Falls back to GUI font and then back to system font. */
  const QFont getMapFont() const;
  const QFont getGuiFont() const;

  /* User set online refresh rate in seconds for custom configurations or stock networks in seconds
   * or -1 for auto value fetched from whazzup or JSON */
  int getOnlineReload(opts::OnlineNetwork network) const;

  /* Seconds */
  int getOnlineVatsimTransceiverReload() const
  {
    return onlineVatsimTransceiverReload;
  }

  const optsac::DisplayOptionsUserAircraft& getDisplayOptionsUserAircraft() const
  {
    return displayOptionsUserAircraft;
  }

  const optsac::DisplayOptionsAiAircraft& getDisplayOptionsAiAircraft() const
  {
    return displayOptionsAiAircraft;
  }

  int getDisplayTransparencyFlightplan() const
  {
    return displayTransparencyFlightplan;
  }

  int getDisplayThicknessFlightplanProfile() const
  {
    return displayThicknessFlightplanProfile;
  }

  int getDisplayTextSizeFlightplanProfile() const
  {
    return displayTextSizeFlightplanProfile;
  }

  int getDisplayTextSizeUserpoint() const
  {
    return displayTextSizeUserpoint;
  }

  int getDisplaySymbolSizeUserpoint() const
  {
    return displaySymbolSizeUserpoint;
  }

  int getDisplaySymbolSizeHighlight() const
  {
    return displaySymbolSizeHighlight;
  }

  int getDisplayMapHighlightTransparent() const
  {
    return displayMapHighlightTransparent;
  }

  int getSimUpdateBoxCenterLegZoom() const
  {
    return simUpdateBoxCenterLegZoom;
  }

  const QSize getGuiToolbarSize() const;

  const QColor& getHighlightFlightplanColor() const
  {
    return highlightFlightplanColor;
  }

  const QColor& getHighlightSearchColor() const
  {
    return highlightSearchColor;
  }

  const QColor& getHighlightProfileColor() const
  {
    return highlightProfileColor;
  }

  const QString& getCacheMapThemeDir() const
  {
    return cacheMapThemeDir;
  }

  int getDisplayThicknessAirspace() const
  {
    return displayThicknessAirspace;
  }

  int getDisplayTransparencyAirspace() const
  {
    return displayTransparencyAirspace;
  }

  int getDisplayTextSizeAirspace() const
  {
    return displayTextSizeAirspace;
  }

  int getDisplayScaleAll() const
  {
    return displayScaleAll;
  }

  int getDisplayScaleAllWeb() const
  {
    return displayScaleAllWeb;
  }

  const QMap<QString, QString>& getMapThemeKeys() const
  {
    return mapThemeKeys;
  }

private:
  friend class OptionsDialog;

  OptionData();
  static OptionData& instanceInternal();

  // Singleton instance
  static OptionData *optionData;

  // Defines the defaults used for reset
  opts::Flags flags = opts::STARTUP_LOAD_KML | opts::STARTUP_LOAD_MAP_SETTINGS | opts::STARTUP_LOAD_ROUTE | opts::STARTUP_SHOW_LAST |
                      opts::GUI_CENTER_KML | opts::GUI_CENTER_ROUTE | opts::ROUTE_ALTITUDE_RULE | opts::CACHE_USE_ONLINE_ELEVATION |
                      opts::STARTUP_LOAD_INFO | opts::STARTUP_LOAD_SEARCH | opts::STARTUP_LOAD_TRAIL | opts::STARTUP_SHOW_SPLASH |
                      opts::ONLINE_REMOVE_SHADOW | opts::ENABLE_TOOLTIPS_ALL | opts::ENABLE_TOOLTIPS_LINK | opts::STARTUP_LOAD_PERF |
                      opts::GUI_AVOID_OVERWRITE_FLIGHTPLAN | opts::MAP_AIRSPACE_NO_MULT_Z | opts::GUI_FREETYPE_FONT_ENGINE |
                      opts::MAP_TRAIL_GRADIENT;

  // Defines the defaults used for reset
  optsw::FlagsWeather flagsWeather = optsw::WEATHER_INFO_FS | optsw::WEATHER_INFO_ACTIVESKY | optsw::WEATHER_INFO_NOAA |
                                     optsw::WEATHER_TOOLTIP_FS | optsw::WEATHER_TOOLTIP_ACTIVESKY | optsw::WEATHER_TOOLTIP_NOAA;

  opts2::Flags2 flags2 = opts2::MAP_AIRPORT_TEXT_BACKGROUND | opts2::MAP_AIRPORT_HIGHLIGHT_ADDON |
                         opts2::MAP_ROUTE_TEXT_BACKGROUND | opts2::MAP_USER_TEXT_BACKGROUND | opts2::ROUTE_HIGHLIGHT_ACTIVE_TABLE |
                         opts2::MAP_AI_TEXT_BACKGROUND | opts2::MAP_ROUTE_DIM_PASSED | opts2::MAP_AVOID_BLURRED_MAP |
                         opts2::ONLINE_AIRSPACE_BY_FILE | opts2::ONLINE_AIRSPACE_BY_NAME | opts2::RAISE_WINDOWS |
                         opts2::ROUTE_CENTER_ACTIVE_LEG | opts2::ROUTE_NO_FOLLOW_ON_MOVE | opts2::MAP_ROUTE_HIGHLIGHT_ACTIVE |
                         opts2::MAP_ROUTE_TRANSPARENT | opts2::MAP_HIGHLIGHT_TRANSPARENT | opts2::MAP_WEB_USE_UI_SCALE;

  opts::DisplayOnlineFlags displayOnlineFlags = opts::DISPLAY_ONLINE_OBSERVER | opts::DISPLAY_ONLINE_CLEARANCE |
                                                opts::DISPLAY_ONLINE_DEPARTURE;

  QString weatherActiveSkyPath, // ui->lineEditOptionsWeatherAsnPath
          weatherXplane11Path, // ui->lineEditOptionsWeatherXplanePath
          weatherXplane12Path; // ui->lineEditOptionsWeatherXplane12Path

  /* Default weather URLs used in reset */
  const static QString WEATHER_NOAA_DEFAULT_URL;
  const static QString WEATHER_VATSIM_DEFAULT_URL;
  const static QString WEATHER_IVAO_DEFAULT_URL;
  const static QString WEATHER_NOAA_WIND_BASE_DEFAULT_URL;

  /* Current weather URLs */
  QString weatherNoaaUrl = WEATHER_NOAA_DEFAULT_URL,
          weatherVatsimUrl = WEATHER_VATSIM_DEFAULT_URL,
          weatherIvaoUrl = WEATHER_IVAO_DEFAULT_URL,
          weatherNoaaWindBaseUrl = WEATHER_NOAA_WIND_BASE_DEFAULT_URL;

  /* X-Plane GRIB file or NOAA GRIB base URL */
  QString weatherXplaneWind;

  QString cacheOfflineElevationPath;
  QString cacheMapThemeDir;

  // Initialized by widget
  QString flightplanPattern;

  // ui->tableWidgetOptionsDatabaseInclude
  QStringList databaseInclude;

  // ui->tableWidgetOptionsDatabaseExclude
  QStringList databaseExclude;

  // ui->tableWidgetOptionsDatabaseExcludeAddon
  QStringList databaseExcludeAddon;

  opts::MapScrollDetail mapScrollDetail = opts::DETAIL_NORMAL;
  opts::MapNavigation mapNavigation = opts::MAP_NAV_CLICK_DRAG_MOVE;

  // ui->radioButtonOptionsMapSimUpdateFast
  // ui->radioButtonOptionsMapSimUpdateLow
  // ui->radioButtonOptionsMapSimUpdateMedium
  opts::SimUpdateRate simUpdateRate = opts::MEDIUM;

  // ui->spinBoxOptionsMapSimUpdateBox
  int simUpdateBox = 50;

  // ui->spinBoxOptionsSimCenterLegZoom
  int simUpdateBoxCenterLegZoom = 100;

  // ui->spinBoxOptionsCacheDiskSize
  int cacheSizeDisk = 2000;

  // ui->spinBoxOptionsCacheMemorySize
  int cacheSizeMemoryMap = 1000;

  // ui->spinBoxOptionsCacheMemoryProfile
  int cacheSizeMemoryProfile = 1000;

  // ui->doubleSpinBoxOptionsProfileBuffer
  float profileBuffer = 5.f;

  // ui->spinBoxOptionsGuiInfoText
  int guiInfoTextSize = 100;

  // ui->spinBoxOptionsGuiAircraftPerf
  int guiPerfReportTextSize = 100;

  // ui->spinBoxOptionsGuiSimInfoText
  int guiInfoSimSize = 100;

  // ui->spinBoxOptionsGuiRouteText
  int guiRouteTableTextSize = 100;

  // ui->spinBoxOptionsGuiRouteRemarksText
  int guiRouteRemarksTextSize = 100;

  // ui->spinBoxOptionsGuiRouteInfoText
  int guiRouteInfoTextSize = 100;

  // ui->spinBoxOptionsGuiSearchText
  int guiSearchTableTextSize = 100;

  // ui->spinBoxOptionsGuiToolbarSize
  int guiToolbarSize = 24;

  // ui->spinBoxOptionsGuiThemeMapDimming
  int guiStyleMapDimming = 80;

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

  QString guiLanguage;

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

  // spinBoxOptionsDisplayThicknessFlightplanProfile
  int displayThicknessFlightplanProfile = 100;

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

  // spinBoxOptionsDisplayTextSizeUserpoint
  int displayTextSizeUserpoint = 100;

  // spinBoxOptionsDisplaySymbolSizeNavaid
  int displaySymbolSizeNavaid = 100;

  // spinBoxOptionsDisplaySymbolSizeUserpoint
  int displaySymbolSizeUserpoint = 100;

  // spinBoxOptionsDisplaySymbolSizeHighlight
  int displaySymbolSizeHighlight = 100;

  // spinBoxOptionsDisplayTextSizeAirway
  int displayTextSizeAirway = 100;

  // spinBoxOptionsDisplayThicknessAirway
  int displayThicknessAirway = 100;

  // spinBoxOptionsDisplayTextSizeFlightplan
  int displayTextSizeFlightplan = 100;

  // spinBoxOptionsDisplaySymbolSizeFlightplan
  int displaySymbolSizeFlightplan = 100;

  // spinBoxOptionsDisplayTextSizeFlightplanProfile
  int displayTextSizeFlightplanProfile = 100;

  // spinBoxOptionsDisplayTransparencyFlightplan
  int displayTransparencyFlightplan = 50;

  // spinBoxOptionsDisplayTextSizeAircraftUser
  int displayTextSizeAircraftUser = 100;

  // spinBoxOptionsDisplaySymbolSizeAircraftUser
  int displaySymbolSizeAircraftUser = 100;

  // spinBoxOptionsDisplayTextSizeAirport
  int displayTextSizeAirport = 100;

  // spinBoxOptionsDisplayTextSizeAirportRunway
  int displayTextSizeAirportRunway = 100;

  // spinBoxOptionsDisplayTextSizeAirportTaxiway
  int displayTextSizeAirportTaxiway = 100;

  // spinBoxOptionsDisplayThicknessTrail
  int displayThicknessTrail = 100;

  // spinBoxOptionsDisplayThicknessUserFeature
  int displayThicknessUserFeature = 100;

  // spinBoxOptionsDisplayThicknessMeasurement
  int displayThicknessMeasurement = 100;

  // spinBoxOptionsDisplayThicknessCompassRose
  int displayThicknessCompassRose = 100;

  // spinBoxOptionsDisplaySunShadeDarkness
  int displaySunShadingDimFactor = 40;

  // spinBoxSimMaxTrackPoints
  int aircraftTrailMaxPoints = 20000;

  // spinBoxSimDoNotFollowOnScrollTime
  int simNoFollowOnScrollTime = 10;

  // doubleSpinBoxOptionsSimZoomOnLanding,
  float simZoomOnLandingDist = 0.1f;

  // spinBoxOptionsSimZoomOnTakeoff,
  float simZoomOnTakeoffDist = 10.f;

  // spinBoxOptionsSimCleanupTableTime,
  int simCleanupTableTime = 10;

  // spinBoxOptionsDisplayTextSizeUserFeature
  int displayTextSizeUserFeature = 100;

  // spinBoxOptionsDisplayTextSizeMeasurement
  int displayTextSizeMeasurement = 100;

  // spinBoxOptionsDisplayTextSizeCompassRose
  int displayTextSizeCompassRose = 100;

  // spinBoxOptionsMapHighlightTransparent
  int displayMapHighlightTransparent = 30;

  // spinBoxDisplayOnlineClearance
  int displayOnlineClearance = 20;

  // spinBoxDisplayOnlineArea
  int displayOnlineArea = 200;

  // spinBoxDisplayOnlineApproach
  int displayOnlineApproach = 40;

  // spinBoxDisplayOnlineDeparture
  int displayOnlineDeparture = 20;

  // spinBoxDisplayOnlineFir
  int displayOnlineFir = 200;

  // spinBoxDisplayOnlineObserver
  int displayOnlineObserver = 20;

  // spinBoxDisplayOnlineGround
  int displayOnlineGround = 10;

  // spinBoxDisplayOnlineTower
  int displayOnlineTower = 20;

  // spinBoxOptionsDisplayTransparencyMora
  int displayTransparencyMora = 50;

  // spinBoxOptionsDisplayTransparencyMora
  int displayTextSizeMora = 100;

  // spinBoxOptionsDisplayTransparencyAirportMsa
  int displayTransparencyAirportMsa = 50;

  // spinBoxOptionsDisplayTextSizeAirportMsa
  int displayTextSizeAirportMsa = 100;

  // spinBoxOptionsMapNavTouchscreenArea
  int mapNavTouchArea = 10;

  // spinBoxOptionsDisplayThicknessAirspace
  int displayThicknessAirspace = 100;

  // spinBoxOptionsDisplayTransparencyAirspace
  int displayTransparencyAirspace = 80;

  // spinBoxOptionsDisplayTextSizeAirspace
  int displayTextSizeAirspace = 100;

  // spinBoxOptionsDisplayScaleAll,
  int displayScaleAll = 100;

  // spinBoxOptionsDisplayScaleAllWeb,
  int displayScaleAllWeb = 100;

  // Default values
  QColor flightplanColor = QColor(Qt::red),
         flightplanOutlineColor = QColor(Qt::black), flightplanProcedureColor = QColor(QLatin1String("#aa0000")),
         flightplanActiveColor = QColor(Qt::magenta), flightplanPassedColor = QColor(QLatin1String("#a0a0a4")),
         trailColor = QColor(Qt::black), measurementColor = QColor(Qt::black),
         highlightFlightplanColor = QColor(Qt::green), highlightSearchColor = QColor(Qt::yellow), highlightProfileColor = QColor(Qt::cyan);

  // comboBoxOptionsDisplayTrailType
  opts::DisplayTrailType displayTrailType = opts::TRAIL_TYPE_DASHED;

  // comboBoxOptionsDisplayTrailGradient
  opts::DisplayTrailGradientType displayTrailGradientType = opts::TRAIL_GRADIENT_COLOR_YELLOW_BLUE;

  /* Default values are set by widget states - these are needed for the reset button */
  optsac::DisplayOptionsUserAircraft displayOptionsUserAircraft =
    optsac::ITEM_USER_AIRCRAFT_GS | optsac::ITEM_USER_AIRCRAFT_ALTITUDE | optsac::ITEM_USER_AIRCRAFT_WIND |
    optsac::ITEM_USER_AIRCRAFT_TRACK_LINE | optsac::ITEM_USER_AIRCRAFT_WIND_POINTER;

  optsac::DisplayOptionsAiAircraft displayOptionsAiAircraft =
    optsac::ITEM_AI_AIRCRAFT_REGISTRATION | optsac::ITEM_AI_AIRCRAFT_TYPE | optsac::ITEM_AI_AIRCRAFT_AIRLINE |
    optsac::ITEM_AI_AIRCRAFT_GS | optsac::ITEM_AI_AIRCRAFT_ALTITUDE;

  optsd::DisplayOptionsAirport displayOptionsAirport =
    optsd::ITEM_AIRPORT_NAME | optsd::ITEM_AIRPORT_TOWER | optsd::ITEM_AIRPORT_ATIS | optsd::ITEM_AIRPORT_RUNWAY |
    optsd::ITEM_AIRPORT_DETAIL_RUNWAY | optsd::ITEM_AIRPORT_DETAIL_TAXI |
    optsd::ITEM_AIRPORT_DETAIL_TAXI | optsd::ITEM_AIRPORT_DETAIL_TAXI_LINE | optsd::ITEM_AIRPORT_DETAIL_TAXI_NAME |
    optsd::ITEM_AIRPORT_DETAIL_APRON | optsd::ITEM_AIRPORT_DETAIL_PARKING;

  optsd::DisplayOptionsRose displayOptionsRose =
    optsd::ROSE_RANGE_RINGS | optsd::ROSE_DEGREE_MARKS | optsd::ROSE_DEGREE_LABELS | optsd::ROSE_HEADING_LINE | optsd::ROSE_TRACK_LINE |
    optsd::ROSE_TRACK_LABEL | optsd::ROSE_CRAB_ANGLE | optsd::ROSE_NEXT_WAYPOINT | optsd::ROSE_DIR_LABELS;

  optsd::DisplayOptionsMeasurement displayOptionsMeasurement = optsd::MEASUREMENT_MAG | optsd::MEASUREMENT_TRUE |
                                                               optsd::MEASUREMENT_DIST | optsd::MEASUREMENT_LABEL;

  optsd::DisplayOptionsNavAid displayOptionsNavAid = optsd::NAVAIDS_NONE;
  optsd::DisplayOptionsAirspace displayOptionsAirspace = optsd::AIRSPACE_RESTRICTIVE_NAME | optsd::AIRSPACE_TYPE |
                                                         optsd::AIRSPACE_ALTITUDE | optsd::AIRSPACE_COM;

  optsd::DisplayOptionsRoute displayOptionsRoute = optsd::ROUTE_DISTANCE | optsd::ROUTE_INITIAL_FINAL_MAG_COURSE;

  optsd::DisplayTooltipOptions displayTooltipOptions = optsd::TOOLTIP_AIRCRAFT_USER | optsd::TOOLTIP_AIRCRAFT_AI |
                                                       optsd::TOOLTIP_AIRPORT | optsd::TOOLTIP_AIRSPACE |
                                                       optsd::TOOLTIP_NAVAID | optsd::TOOLTIP_WIND | optsd::TOOLTIP_VERBOSE |
                                                       optsd::TOOLTIP_MARKS | optsd::TOOLTIP_AIRCRAFT_TRAIL |
                                                       optsd::TOOLTIP_DISTBRG_ROUTE | optsd::TOOLTIP_DISTBRG_USER;

  optsd::DisplayClickOptions displayClickOptions = optsd::CLICK_AIRCRAFT_USER | optsd::CLICK_AIRCRAFT_AI |
                                                   optsd::CLICK_AIRPORT | optsd::CLICK_AIRSPACE | optsd::CLICK_NAVAID;

  opts::UpdateRate updateRate = opts::DAILY;
  opts::UpdateChannels updateChannels = opts::STABLE;

  // Used in the singelton to check if data was already loaded
  bool valid = false;

  /* Online network configuration ========================================= */
  opts::OnlineNetwork onlineNetwork = opts::ONLINE_NONE;
  opts::OnlineFormat onlineFormat = opts::ONLINE_FORMAT_VATSIM;
  QString onlineStatusUrl, onlineWhazzupUrl;

  /* Values loaded from networks.cfg */
  int onlineCustomReload = 180,
      onlineVatsimReload = 180,
      onlineVatsimTransceiverReload = 180,
      onlinePilotEdgeReload = 180,
      onlineIvaoReload = 15;

  QString onlineVatsimStatusUrl, onlineVatsimTransceiverUrl;
  QString onlineIvaoWhazzupUrl;
  QString onlinePilotEdgeStatusUrl;

  /* Webserver values */
  QString webDocumentRoot;
  int webPort = 8965;

  /* true for HTTPS / SSL */
  bool webEncrypted = false;

  QString guiFont, mapFont;

  /* API keys or tokens extracted from DGML files. Saved and loaded in MapThemeHandler class. */
  QMap<QString, QString> mapThemeKeys;
};

#endif // LITTLENAVMAP_OPTIONDATA_H
