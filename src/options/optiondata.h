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
  // ui->checkBoxOptionsStartupLoadKml
  STARTUP_LOAD_KML = 1 << 0,

  // ui->checkBoxOptionsStartupLoadMapSettings
  STARTUP_LOAD_MAP_SETTINGS = 1 << 1,

  // ui->checkBoxOptionsStartupLoadRoute
  STARTUP_LOAD_ROUTE = 1 << 2,

  // ui->radioButtonOptionsStartupShowHome
  STARTUP_SHOW_HOME = 1 << 3,

  // ui->radioButtonOptionsStartupShowLast
  STARTUP_SHOW_LAST = 1 << 4,

  // ui->checkBoxOptionsGuiCenterKml
  GUI_CENTER_KML = 1 << 5,

  // ui->checkBoxOptionsGuiCenterRoute
  GUI_CENTER_ROUTE = 1 << 6,

  // ui->checkBoxOptionsMapEmptyAirports
  MAP_EMPTY_AIRPORTS = 1 << 7,

  // ui->checkBoxOptionsRouteEastWestRule
  ROUTE_EAST_WEST_RULE = 1 << 8,

  // ui->checkBoxOptionsRoutePreferNdb
  ROUTE_PREFER_NDB = 1 << 9,

  // ui->checkBoxOptionsRoutePreferVor
  ROUTE_PREFER_VOR = 1 << 10,

  // ui->checkBoxOptionsWeatherInfoAsn
  WEATHER_INFO_ASN = 1 << 11,

  // ui->checkBoxOptionsWeatherInfoNoaa
  WEATHER_INFO_NOAA = 1 << 12,

  // ui->checkBoxOptionsWeatherInfoVatsim
  WEATHER_INFO_VATSIM = 1 << 13,

  // ui->checkBoxOptionsWeatherTooltipAsn
  WEATHER_TOOLTIP_ASN = 1 << 14,

  // ui->checkBoxOptionsWeatherTooltipNoaa
  WEATHER_TOOLTIP_NOAA = 1 << 15,

  // ui->checkBoxOptionsWeatherTooltipVatsim
  WEATHER_TOOLTIP_VATSIM = 1 << 16
};

Q_DECLARE_FLAGS(Flags, Flag);
Q_DECLARE_OPERATORS_FOR_FLAGS(opts::Flags);

enum MapScrollDetail
{
  FULL,
  NORMAL,
  NONE
};

enum SimUpdateRate
{
  FAST,
  MEDIUM,
  LOW
};

}

/* All default values are defined in this class */
class OptionData
{
public:
  static const OptionData& instance();

  ~OptionData();

  opts::Flags getFlags() const
  {
    return flags;
  }

  QVector<int> getMapRangeRings() const
  {
    return mapRangeRings;
  }

  QString getWeatherActiveSkyPath() const
  {
    return weatherActiveSkyPath;
  }

  QStringList getDatabaseAddonExclude() const
  {
    return databaseAddonExclude;
  }

  QStringList getDatabaseExclude() const
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

  unsigned int getCacheSizeDiskMb() const
  {
    return static_cast<unsigned int>(cacheSizeDisk);
  }

  unsigned int getCacheSizeMemoryMb() const
  {
    return static_cast<unsigned int>(cacheSizeMemory);
  }

  int getGuiInfoTextSize() const
  {
    return guiInfoTextSize;
  }

  int getGuiRouteTableTextSize() const
  {
    return guiRouteTableTextSize;
  }

  int getGuiSearchTableTextSize() const
  {
    return guiSearchTableTextSize;
  }

  int getGuiInfoSimSize() const
  {
    return guiInfoSimSize;
  }

  int getMapClickSensitivity() const
  {
    return mapClickSensitivity;
  }

  int getMapTooltipSensitivity() const
  {
    return mapTooltipSensitivity;
  }

  int getMapSymbolSize() const
  {
    return mapSymbolSize;
  }

  int getMapTextSize() const
  {
    return mapTextSize;
  }

  float getMapZoomShow() const
  {
    return mapZoomShow;
  }

  int getRouteGroundBuffer() const
  {
    return routeGroundBuffer;
  }

  int getSimUpdateBox() const
  {
    return simUpdateBox;
  }

private:
  friend class OptionsDialog;

  OptionData();
  static OptionData& instanceInternal();

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
    opts::ROUTE_PREFER_NDB |
    opts::ROUTE_PREFER_VOR |
    opts::ROUTE_EAST_WEST_RULE |
    opts::WEATHER_INFO_ASN |
    opts::WEATHER_INFO_NOAA |
    opts::WEATHER_INFO_VATSIM |
    opts::WEATHER_TOOLTIP_ASN |
    opts::WEATHER_TOOLTIP_NOAA
    // opts::WEATHER_TOOLTIP_VATSIM
  ;

  // ui->lineEditOptionsMapRangeRings
  QVector<int> mapRangeRings = QVector<int>({50, 100, 200, 500});

  // ui->lineEditOptionsWeatherAsnPath
  QString weatherActiveSkyPath;

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
  int mapSymbolSize = 100; // TODO

  // ui->spinBoxOptionsMapTextSize
  int mapTextSize = 100; // TODO

  // ui->doubleSpinBoxOptionsMapZoomShowMap
  float mapZoomShow = 1.5f; // TODO

  // ui->spinBoxOptionsRouteGroundBuffer
  int routeGroundBuffer = 1000;

  bool valid = false;
};

#endif // LITTLENAVMAP_OPTIONDATA_H
