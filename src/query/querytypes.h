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

#ifndef LNM_QUERYTYPES_H
#define LNM_QUERYTYPES_H

#include <QList>

#include <functional>

#include <marble/GeoDataCoordinates.h>
#include <marble/GeoDataLatLonBox.h>

namespace atools {
namespace sql {
class SqlQuery;
}
}

class MapLayer;

namespace query {
void bindCoordinatePointInRect(const Marble::GeoDataLatLonBox& rect, atools::sql::SqlQuery *query,
                               const QString& prefix = QString());

QList<Marble::GeoDataLatLonBox> splitAtAntiMeridian(const Marble::GeoDataLatLonBox& rect, double factor,
                                                    double increment);

/* Inflate rect by width and height in degrees. If it crosses the poles or date line it will be limited */
void inflateQueryRect(Marble::GeoDataLatLonBox& rect, double factor, double increment);

}

/* Simple spatial cache that deals with objects in a bounding rectangle but does not run any queries to load data */
template<typename TYPE>
struct SimpleRectCache
{
  typedef std::function<bool (const MapLayer * curLayer, const MapLayer * mapLayer)> LayerCompareFunc;

  /*
   * @param rect bounding rectangle - all objects inside this rectangle are returned
   * @param mapLayer current map layer
   * @param lazy if true do not fetch new data but return the old potentially incomplete dataset
   * @return true after clearing the cache. The caller has to request new data
   */
  bool updateCache(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, double factor, double increment,
                   bool lazy,
                   LayerCompareFunc funcSameLayer);
  void clear();
  void validate(int queryMaxRows);

  Marble::GeoDataLatLonBox curRect;
  const MapLayer *curMapLayer = nullptr;
  QList<TYPE> list;

};

// ---------------------------------------------------------------------------------

template<typename TYPE>
bool SimpleRectCache<TYPE>::updateCache(const Marble::GeoDataLatLonBox& rect, const MapLayer *mapLayer, double factor,
                                        double increment, bool lazy, LayerCompareFunc funcSameLayer)
{
  if(lazy)
    // Nothing changed11
    return false;

  // Store bounding rectangle and inflate it
  Marble::GeoDataLatLonBox cur(curRect);
  query::inflateQueryRect(cur, factor, increment);

  if(curRect.isEmpty() || !cur.contains(rect) || !funcSameLayer(curMapLayer, mapLayer))
  {
    // Rectangle not covered by loaded data or new layer selected
    list.clear();
    curRect = rect;
    curMapLayer = mapLayer;
    return true;
  }
  return false;
}

template<typename TYPE>
void SimpleRectCache<TYPE>::validate(int queryMaxRows)
{
  if(list.size() >= queryMaxRows)
  {
    curRect.clear();
    curMapLayer = nullptr;
  }
}

template<typename TYPE>
void SimpleRectCache<TYPE>::clear()
{
  list.clear();
  curRect.clear();
  curMapLayer = nullptr;
}

#endif // LNM_QUERYTYPES_H
