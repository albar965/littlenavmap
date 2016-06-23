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

#ifndef LITTLENAVMAP_MAPSCREENINDEX_H
#define LITTLENAVMAP_MAPSCREENINDEX_H

#include "route/routemapobjectlist.h"

namespace maptypes {
struct MapSearchResult;

}

namespace Marble {
class GeoDataLatLonAltBox;
}

class MapWidget;
class MapPaintLayer;

class MapScreenIndex
{
public:
  MapScreenIndex(MapWidget *parentWidget, MapQuery *mapQueryParam, MapPaintLayer *mapPaintLayer);
  ~MapScreenIndex();

  void getAllNearest(int xs, int ys, int screenDistance, maptypes::MapSearchResult& mapSearchResult);
  void getNearestAirways(int xs, int ys, int screenDistance, maptypes::MapSearchResult& result);
  void getNearestHighlights(int xs, int ys, int screenDistance, maptypes::MapSearchResult& mapobjects);

  int getNearestDistanceMarksIndex(int xs, int ys, int screenDistance);
  int getNearestRangeMarkIndex(int xs, int ys, int screenDistance);
  int getNearestRouteLegIndex(int xs, int ys, int screenDistance);
  int getNearestRoutePointIndex(int xs, int ys, int screenDistance);

  void updateRouteScreenGeometry();
  void updateAirwayScreenLines(const Marble::GeoDataLatLonAltBox& curBox);

  void saveState();
  void restoreState();

  RouteMapObjectList& getRouteHighlights();
  const RouteMapObjectList& getRouteHighlights() const;

  maptypes::MapSearchResult& getHighlights();
  const maptypes::MapSearchResult& getHighlights() const;

  QList<maptypes::RangeMarker>& getRangeMarks();

  const QList<maptypes::RangeMarker>& getRangeMarks() const;

  QList<maptypes::DistanceMarker>& getDistanceMarks();

  const QList<maptypes::DistanceMarker>& getDistanceMarks() const;

private:
  MapWidget *mapWidget;
  MapQuery *mapQuery;
  MapPaintLayer *paintLayer;

  maptypes::MapSearchResult highlights;
  RouteMapObjectList routeHighlights;
  QList<maptypes::RangeMarker> rangeMarks;
  QList<maptypes::DistanceMarker> distanceMarks;
  QList<std::pair<int, QLine> > routeLines;
  QList<std::pair<int, QLine> > airwayLines;
  QList<std::pair<int, QPoint> > routePoints;

};

#endif // LITTLENAVMAP_MAPSCREENINDEX_H
