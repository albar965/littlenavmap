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

#include "mapgui/maplayersettings.h"

#include <algorithm>
#include <functional>

MapLayerSettings::MapLayerSettings()
{

}

MapLayerSettings& MapLayerSettings::append(const MapLayer& layer)
{
  layers.append(layer);
  return *this;
}

void MapLayerSettings::finishAppend()
{
  std::sort(layers.begin(), layers.end());
}

const MapLayer *MapLayerSettings::getLayer(float distance, int detailFactor) const
{
  using namespace std::placeholders;

  // Get the layer with the next lowest zoom distance
  QList<MapLayer>::const_iterator it = std::lower_bound(layers.begin(), layers.end(), distance,
                                                        std::bind(&MapLayerSettings::compare, this, _1, _2));

  // Adjust iterator for detail level changes
  if(detailFactor > MAP_DEFAULT_DETAIL_FACTOR)
    it -= detailFactor - MAP_DEFAULT_DETAIL_FACTOR;
  else if(detailFactor < MAP_DEFAULT_DETAIL_FACTOR)
    it += MAP_DEFAULT_DETAIL_FACTOR - detailFactor;

  if(it >= layers.end())
    return &layers.last();

  if(it < layers.begin())
    return &(*(layers.begin()));

  return &(*it);
}

bool MapLayerSettings::compare(const MapLayer& layer, float distance) const
{
  // The value returned indicates whether the first argument is considered to go before the second.
  return layer.getMaxRange() < distance;
}

QDebug operator<<(QDebug out, const MapLayerSettings& record)
{
  QDebugStateSaver saver(out);

  out.nospace().noquote() << "LayerSettings[" << record.layers << "]";

  return out;
}
