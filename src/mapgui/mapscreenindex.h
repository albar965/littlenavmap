/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#include "fs/sc/simconnectdata.h"

#include "route/routemapobjectlist.h"

namespace maptypes {
struct MapSearchResult;

}

namespace Marble {
class GeoDataLatLonAltBox;
}

class MapWidget;
class MapPaintLayer;

/*
 * Keeps an indes of certain map objects like flight plan lines, airway lines in screen coordinates
 * to allow mouse over reaction.
 * Also maintains distance measurement lines and range rings.
 * All get nearest methods return objects sorted by distance
 */
class MapScreenIndex
{
public:
  MapScreenIndex(MapWidget *parentWidget, MapQuery *mapQueryParam, MapPaintLayer *mapPaintLayer);
  ~MapScreenIndex();

  /*
   * Finds all objects near the screen coordinates with maximal distance of maxDistance to xs/ys.
   * Gets airways, visible map objects like airports, navaids and lightlighted objects.
   * @param result Result ordered by distance to xs/ys
   * @param xs/ys Screen coordinates.
   * @param maxDistance maximum distance to xs/ys
   */
  void getAllNearest(int xs, int ys, int maxDistance, maptypes::MapSearchResult& result);

  /* Get nearest distance measurement line index (only the endpoint)
   * or -1 if nothing was found near the cursor position. Index points into the list of getDistanceMarks */
  int getNearestDistanceMarkIndex(int xs, int ys, int maxDistance);

  /* Get nearest range rings index (only the centerpoint)
   * or -1 if nothing was found near the cursor position. Index points into the list of getRangeMarks */
  int getNearestRangeMarkIndex(int xs, int ys, int maxDistance);

  /* Get index of nearest flight plan leg or -1 if nothing was found nearby or cursor is not along a leg. */
  int getNearestRouteLegIndex(int xs, int ys, int maxDistance);

  /* Get index of nearest flight plan waypoint or -1 if nothing was found nearby. */
  int getNearestRoutePointIndex(int xs, int ys, int maxDistance);

  /* Update geometry after a route or scroll or map change */
  void updateRouteScreenGeometry();
  void updateAirwayScreenGeometry(const Marble::GeoDataLatLonAltBox& curBox);

  /* Save and restore distance markers and range rings */
  void saveState();
  void restoreState();

  /* Get objects that are highlighted because of selected flight plan legs in the table */
  const QList<int>& getRouteHighlights() const
  {
    return routeHighlights;
  }

  void setRouteHighlights(const QList<int>& value)
  {
    routeHighlights = value;
  }

  /* Get objects that are highlighted because of selected rows in a search result table */
  maptypes::MapSearchResult& getSearchHighlights()
  {
    return highlights;
  }

  const maptypes::MapSearchResult& getSearchHighlights() const
  {
    return highlights;
  }

  void setApproachLegHighlights(const maptypes::MapApproachLeg *leg)
  {
    if(leg != nullptr)
      approachLegHighlights = *leg;
    else
      approachLegHighlights = maptypes::MapApproachLeg();
  }

  const maptypes::MapApproachLeg& getApproachLegHighlights() const
  {
    return approachLegHighlights;
  }

  /* Get range rings */
  QList<maptypes::RangeMarker>& getRangeMarks()
  {
    return rangeMarks;
  }

  const QList<maptypes::RangeMarker>& getRangeMarks() const
  {
    return rangeMarks;
  }

  /* Get distance measurment lines */
  QList<maptypes::DistanceMarker>& getDistanceMarks()
  {
    return distanceMarks;
  }

  const QList<maptypes::DistanceMarker>& getDistanceMarks() const
  {
    return distanceMarks;
  }

  const atools::fs::sc::SimConnectUserAircraft& getUserAircraft()
  {
    return simData.getUserAircraft();
  }

  const atools::fs::sc::SimConnectUserAircraft& getLastUserAircraft()
  {
    return lastSimData.getUserAircraft();
  }

  const QVector<atools::fs::sc::SimConnectAircraft>& getAiAircraft()
  {
    return simData.getAiAircraft();
  }

  void updateSimData(const atools::fs::sc::SimConnectData& data)
  {
    simData = data;
  }

  void updateLastSimData(const atools::fs::sc::SimConnectData& data)
  {
    lastSimData = data;
  }

  const maptypes::MapApproachLegList& getApproachHighlight() const
  {
    return approachHighlight;
  }

  maptypes::MapApproachLegList& getApproachHighlight()
  {
    return approachHighlight;
  }

  const maptypes::MapApproachLegList& getTransitionHighlight() const
  {
    return transitionHighlight;
  }

  maptypes::MapApproachLegList& getTransitionHighlight()
  {
    return transitionHighlight;
  }

private:
  void getNearestAirways(int xs, int ys, int maxDistance, maptypes::MapSearchResult& result);
  void getNearestHighlights(const maptypes::MapSearchResult& from, int xs, int ys, int maxDistance,
                            maptypes::MapSearchResult& result);

  atools::fs::sc::SimConnectData simData, lastSimData;
  MapWidget *mapWidget;
  MapQuery *mapQuery;
  MapPaintLayer *paintLayer;

  maptypes::MapSearchResult highlights;
  maptypes::MapApproachLeg approachLegHighlights;

  maptypes::MapApproachLegList approachHighlight, transitionHighlight;

  QList<int> routeHighlights;
  QList<maptypes::RangeMarker> rangeMarks;
  QList<maptypes::DistanceMarker> distanceMarks;
  QList<std::pair<int, QLine> > routeLines;
  QList<std::pair<int, QLine> > airwayLines;
  QList<std::pair<int, QPoint> > routePoints;

};

#endif // LITTLENAVMAP_MAPSCREENINDEX_H
