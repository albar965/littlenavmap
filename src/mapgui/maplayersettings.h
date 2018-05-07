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

#ifndef LITTLENAVMAP_MAPLAYERSETTINGS_H
#define LITTLENAVMAP_MAPLAYERSETTINGS_H

#include "mapgui/maplayer.h"

#include <QList>

/*
 * A list of map layers that defines what is painted at what zoom distance
 */
class MapLayerSettings
{
public:
  MapLayerSettings();

  /* Add a map layer. Call finishAppend when done. */
  MapLayerSettings& append(const MapLayer& layer);

  /* Call when done appending layers. Sorts all layers by zoom distance. */
  void finishAppend();

  static Q_DECL_CONSTEXPR int MAP_DEFAULT_DETAIL_FACTOR = 10;
  static Q_DECL_CONSTEXPR int MAP_MAX_DETAIL_FACTOR = 15;
  static Q_DECL_CONSTEXPR int MAP_MIN_DETAIL_FACTOR = 5;

  /* Get a layer for current zoom distance and detail factor */
  const MapLayer *getLayer(float distance, int detailFactor = MAP_DEFAULT_DETAIL_FACTOR) const;

private:
  friend QDebug operator<<(QDebug out, const MapLayerSettings& record);

  bool compare(const MapLayer& layer, float distance) const;

  QList<MapLayer> layers;
};

#endif // LITTLENAVMAP_MAPLAYERSETTINGS_H
