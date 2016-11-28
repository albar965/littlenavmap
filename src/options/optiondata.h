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

#ifndef LITTLENAVMAP_OPTIONDATA_H
#define LITTLENAVMAP_OPTIONDATA_H

#include <QColor>
#include <QFlags>
#include <QVector>

namespace opts {
enum Flag
{
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
  ROUTE_EAST_WEST_RULE = 1 << 9,

  /* Start airway route at NDB.
   * ui->checkBoxOptionsRoutePreferNdb */
  ROUTE_PREFER_NDB = 1 << 10,

  /* Start airway route at VOR.
   * ui->checkBoxOptionsRoutePreferVor */
  ROUTE_PREFER_VOR = 1 << 11,

  /* Show ASN weather in info panel.
   * ui->checkBoxOptionsWeatherInfoAsn */
  WEATHER_INFO_ACTIVESKY = 1 << 12,

  /* Show NOAA weather in info panel.
   * ui->checkBoxOptionsWeatherInfoNoaa */
  WEATHER_INFO_NOAA = 1 << 13,

  /* Show Vatsim weather in info panel.
   * ui->checkBoxOptionsWeatherInfoVatsim */
  WEATHER_INFO_VATSIM = 1 << 14,

  /* Show Vatsim weather in info panel.
  * ui->checkBoxOptionsWeatherInfoFs */
  WEATHER_INFO_FS = 1 << 15,

  /* Show ASN weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipAsn */
  WEATHER_TOOLTIP_ACTIVESKY = 1 << 16,

  /* Show NOAA weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipNoaa */
  WEATHER_TOOLTIP_NOAA = 1 << 17,

  /* Show Vatsim weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipVatsim */
  WEATHER_TOOLTIP_VATSIM = 1 << 18,

  /* Show Vatsim weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipFs */
  WEATHER_TOOLTIP_FS = 1 << 19,

  /* No box mode when moving map.
   * ui->checkBoxOptionsSimUpdatesConstant */
  SIM_UPDATE_MAP_CONSTANTLY = 1 << 20,

  /* Center flight plan after loading.
   * ui->checkBoxOptionsGuiAvoidOverwrite */
  GUI_AVOID_OVERWRITE_FLIGHTPLAN = 1 << 21

};

Q_DECLARE_FLAGS(Flags, Flag);
Q_DECLARE_OPERATORS_FOR_FLAGS(opts::Flags);

/* Changing these option values will also change the saved values thus invalidatin user settings */
enum DisplayOption
{
  ITEM_NONE = 0,

  ITEM_AIRPORT_NAME = 1 << 1,
  ITEM_AIRPORT_TOWER = 1 << 2,
  ITEM_AIRPORT_ATIS = 1 << 3,
  ITEM_AIRPORT_RUNWAY = 1 << 4,
  ITEM_AIRPORT_WIND_POINTER = 1 << 5, // TODO

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
  ITEM_USER_AIRCRAFT_TRACK_LINE = 1 << 18, // TODO
  ITEM_USER_AIRCRAFT_WIND_POINTER = 1 << 19, // TODO

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
Q_DECLARE_OPERATORS_FOR_FLAGS(opts::DisplayOptions);

/* Map detail level during scrolling or zooming */
enum MapScrollDetail
{
  FULL,
  NORMAL,
  NONE
};

/* Speed of simualator aircraft updates */
enum SimUpdateRate
{
  FAST,
  MEDIUM,
  LOW
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
  COORDS_DMS,
  COORDS_DEC
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

}

/*
 * Contains global options that are provided using a singelton pattern.
 * All default values are defined in the widgets in the options.ui file.
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

  opts::UnitFuelAndWeight getUnitFuelWeight() const
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

  const QString& getWeatherNoaaUrl() const
  {
    return weatherNoaaUrl;
  }

  const QString& getWeatherVatsimUrl() const
  {
    return weatherVatsimUrl;
  }

  /* List of directories that excludes paths from being recognized as add-ons. Only for scenery database loading. */
  const QStringList& getDatabaseAddonExclude() const
  {
    return databaseAddonExclude;
  }

  /* List of directories that are excluded from scenery database loading */
  const QStringList& getDatabaseExclude() const
  {
    return databaseExclude;
  }

  opts::MapScrollDetail getMapScrollDetail() const
  {
    return mapScrollDetail;
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

  const QColor& getTrailColor() const
  {
    return trailColor;
  }

  const opts::DisplayOptions& getDisplayOptions() const
  {
    return displayOptions;
  }

  int getDisplayThicknessRangeDistance() const
  {
    return displayThicknessRangeDistance;
  }

  float getRouteTodRule() const
  {
    return routeTodRule;
  }

private:
  friend class OptionsDialog;

  OptionData();
  static OptionData& instanceInternal();

  // Singleton instance
  static OptionData *optionData;

  opts::Flags flags =
    opts::STARTUP_LOAD_KML |
    opts::STARTUP_LOAD_MAP_SETTINGS |
    opts::STARTUP_LOAD_ROUTE |
    // opts::STARTUP_SHOW_HOME |
    opts::STARTUP_SHOW_LAST |
    opts::GUI_CENTER_KML |
    opts::GUI_CENTER_ROUTE |
    opts::MAP_EMPTY_AIRPORTS |
    // opts::ROUTE_PREFER_NDB |
    // opts::ROUTE_PREFER_VOR |
    opts::ROUTE_EAST_WEST_RULE |
    opts::WEATHER_INFO_ACTIVESKY |
    opts::WEATHER_INFO_NOAA |
    opts::WEATHER_INFO_VATSIM |
    opts::WEATHER_TOOLTIP_ACTIVESKY |
    opts::WEATHER_TOOLTIP_NOAA
    // opts::WEATHER_TOOLTIP_VATSIM
  ;

  // ui->lineEditOptionsMapRangeRings
  QVector<int> mapRangeRings = QVector<int>({50, 100, 200, 500});

  // ui->lineEditOptionsWeatherAsnPath
  QString weatherActiveSkyPath,
          weatherNoaaUrl = "http://tgftp.nws.noaa.gov/data/observations/metar/stations/%1.TXT",
          weatherVatsimUrl = "http://metar.vatsim.net/metar.php?id=%1";

  // ui->listWidgetOptionsDatabaseAddon
  QStringList databaseAddonExclude;

  // ui->listWidgetOptionsDatabaseExclude
  QStringList databaseExclude;

  // ui->radioButtonOptionsMapScrollFull
  // ui->radioButtonOptionsMapScrollNone
  // ui->radioButtonOptionsMapScrollNormal
  opts::MapScrollDetail mapScrollDetail = opts::NORMAL;

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

  // ui->spinBoxOptionsGuiSimInfoText
  int guiInfoSimSize = 100;

  // ui->spinBoxOptionsGuiRouteText
  int guiRouteTableTextSize = 100;

  // ui->spinBoxOptionsGuiSearchText
  int guiSearchTableTextSize = 100;

  // ui->spinBoxOptionsMapClickRect
  int mapClickSensitivity = 16;

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

  // ui->doubleSpinBoxOptionsRouteTodRuleSuffix
  float routeTodRule = 3.f;

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

  // spinBoxOptionsDisplayTextSizeAircraftAi
  int displayTextSizeAircraftAi = 100;

  // spinBoxOptionsDisplayThicknessFlightplan
  int displayThicknessFlightplan = 100;

  // spinBoxOptionsDisplaySymbolSizeAirport
  int displaySymbolSizeAirport = 100;

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

  QColor flightplanColor, trailColor;

  // comboBoxOptionsDisplayTrailType
  opts::DisplayTrailType displayTrailType = opts::DASHED;

  opts::DisplayOptions displayOptions = opts::ITEM_NONE;

  // Used in the singelton to check if data was already loaded
  bool valid = false;

};

#endif // LITTLENAVMAP_OPTIONDATA_H
