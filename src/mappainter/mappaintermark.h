/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#ifndef LITTLENAVMAP_MAPPAINTERMARK_H
#define LITTLENAVMAP_MAPPAINTERMARK_H

#include "mappainter/mappainter.h"

namespace Marble {
class GeoDataLineString;
}

namespace map {
struct MapAirspace;
struct MapAirway;
struct MapLogbookEntry;
struct DistanceMarker;
}

class MapWidget;

/*
 * Paint all marks, distance measure lines, range rings, selected object highlights
 * and magnetic pole indications.
 */
class MapPainterMark :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainterMark)

public:
  MapPainterMark(MapPaintWidget *mapWidgetParam, MapScale *mapScale, PaintContext *paintContext);
  virtual ~MapPainterMark() override;

  virtual void render() override;

private:
  /* Draw black yellow cross for search distance marker */
  void paintMark();

  /* Paint the center of the home position */
  void paintHome();

  /* Draw rings around objects that are selected on the search or flight plan tables */
  void paintHighlights();

  /* Draw all rang rings. This includes the red rings and the radio navaid ranges. */
  void paintRangeMarks();

  /* Draw great circle line distance measurement lines */
  void paintDistanceMarks();

  /* Traffic patterns */
  void paintPatternMarks();

  /* Draw a compass rose for the user aircraft with tick marks. */
  void paintCompassRose();

  /* Draw a dotted line around aircraft to indicate endurance */
  void paintEndurance();

  /* Draw a "green banana" where aircraft reaches autopilot altitude */
  void paintSelectedAltitudeRange();

  /* Draw drag operations in progress */
  void paintUserpointDrag();
  void paintRouteDrag();

  /* Draw hightlighted airspaces */
  void paintAirspace(const map::MapAirspace& airspace);

  /* Draw highligthted airways */
  void paintAirwayList(const QList<map::MapAirway>& airwayList);
  void paintAirwayTextList(const QList<map::MapAirway>& airwayList);

  /* Logbook highlights */
  void paintLogEntries(const QList<map::MapLogbookEntry>& entries);

  QStringList distanceMarkText(const map::DistanceMarker& marker, bool drawFast) const;

  QString magTrueSuffix, magSuffix, trueSuffix;
};

#endif // LITTLENAVMAP_MAPPAINTERMARK_H
