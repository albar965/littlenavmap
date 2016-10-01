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

  /* Center KML after loading.
   * ui->checkBoxOptionsGuiCenterKml */
  GUI_CENTER_KML = 1 << 5,

  /* Center flight plan after loading.
   * ui->checkBoxOptionsGuiCenterRoute */
  GUI_CENTER_ROUTE = 1 << 6,

  /* Treat empty airports special.
   * ui->checkBoxOptionsMapEmptyAirports */
  MAP_EMPTY_AIRPORTS = 1 << 7,

  /* East/west rule for flight plan calculation.
   * ui->checkBoxOptionsRouteEastWestRule */
  ROUTE_EAST_WEST_RULE = 1 << 8,

  /* Start airway route at NDB.
   * ui->checkBoxOptionsRoutePreferNdb */
  ROUTE_PREFER_NDB = 1 << 9,

  /* Start airway route at VOR.
   * ui->checkBoxOptionsRoutePreferVor */
  ROUTE_PREFER_VOR = 1 << 10,

  /* Show ASN weather in info panel.
   * ui->checkBoxOptionsWeatherInfoAsn */
  WEATHER_INFO_ACTIVESKY = 1 << 11,

  /* Show NOAA weather in info panel.
   * ui->checkBoxOptionsWeatherInfoNoaa */
  WEATHER_INFO_NOAA = 1 << 12,

  /* Show Vatsim weather in info panel.
   * ui->checkBoxOptionsWeatherInfoVatsim */
  WEATHER_INFO_VATSIM = 1 << 13,

  /* Show ASN weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipAsn */
  WEATHER_TOOLTIP_ACTIVESKY = 1 << 14,

  /* Show NOAA weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipNoaa */
  WEATHER_TOOLTIP_NOAA = 1 << 15,

  /* Show Vatsim weather in tooltip.
   * ui->checkBoxOptionsWeatherTooltipVatsim */
  WEATHER_TOOLTIP_VATSIM = 1 << 16
};

Q_DECLARE_FLAGS(Flags, Flag);
Q_DECLARE_OPERATORS_FOR_FLAGS(opts::Flags);

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

  /* Default zoom distance for point objects */
  float getMapZoomShow() const
  {
    return mapZoomShow;
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
  float mapZoomShow = 1.5f;

  // ui->spinBoxOptionsRouteGroundBuffer
  int routeGroundBuffer = 1000;

  // Used in the singelton to check if data was already loaded
  bool valid = false;
};

#endif // LITTLENAVMAP_OPTIONDATA_H
