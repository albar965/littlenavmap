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

#ifndef LAYERSETTINGS_H
#define LAYERSETTINGS_H

#include "mapgui/maplayer.h"

#include <QList>

const int MAP_DEFAULT_DETAIL_FACTOR = 10;
const int MAP_MAX_DETAIL_FACTOR = 15;
const int MAP_MIN_DETAIL_FACTOR = 5;

class MapLayerSettings
{
public:
  MapLayerSettings();

  MapLayerSettings& append(const MapLayer& layer);
  void finishAppend();

  const MapLayer *getLayer(float distance, int detailFactor = MAP_DEFAULT_DETAIL_FACTOR) const;

private:
  friend QDebug operator<<(QDebug out, const MapLayerSettings& record);

  bool compare(const MapLayer& ml, float distance) const;

  QList<MapLayer> layers;
};

#endif // LAYERSETTINGS_H
