/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

#include "mappainter/mappainternav.h"

#include "common/symbolpainter.h"
#include "common/mapcolors.h"
#include "common/unit.h"
#include "mapgui/mapwidget.h"
#include "common/textplacement.h"
#include "util/paintercontextsaver.h"
#include "mapgui/maplayer.h"
#include "query/mapquery.h"
#include "query/airwaytrackquery.h"
#include "query/waypointtrackquery.h"

#include <QElapsedTimer>

#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

using namespace Marble;
using namespace atools::geo;
using namespace map;

MapPainterNav::MapPainterNav(MapPaintWidget *mapWidget, MapScale *mapScale)
  : MapPainter(mapWidget, mapScale)
{
}

MapPainterNav::~MapPainterNav()
{
}

void MapPainterNav::render(PaintContext *context)
{
  const GeoDataLatLonAltBox& curBox = context->viewport->viewLatLonAltBox();

  atools::util::PainterContextSaver saver(context->painter);

  // Airways -------------------------------------------------
  bool drawAirway = context->mapLayer->isAirway() &&
                    (context->objectTypes.testFlag(map::AIRWAYJ) ||
                     context->objectTypes.testFlag(map::AIRWAYV));

  context->szFont(context->textSizeNavaid);

  if(drawAirway && !context->isOverflow())
  {
    // Draw airway lines
    QList<MapAirway> airways;
    airwayQuery->getAirways(airways, curBox, context->mapLayer, context->lazyUpdate);
    paintAirways(context, &airways, context->drawFast);
  }

  // Tracks -------------------------------------------------
  bool drawTrack = context->mapLayer->isTrack() && context->objectTypes.testFlag(map::TRACK);
  if(drawTrack && !context->isOverflow())
  {
    // Draw track lines
    QList<MapAirway> tracks;
    airwayQuery->getTracks(tracks, curBox, context->mapLayer, context->lazyUpdate);
    paintAirways(context, &tracks, context->drawFast);
  }

  // Waypoints -------------------------------------------------
  bool drawWaypoint = context->mapLayer->isWaypoint() && context->objectTypes.testFlag(map::WAYPOINT);
  if((drawWaypoint || drawAirway || drawTrack) && !context->isOverflow())
  {
    // If airways are drawn we also have to go through waypoints
    QList<MapWaypoint> waypoints;
    waypointQuery->getWaypoints(waypoints, curBox, context->mapLayer, context->lazyUpdate);
    if(!waypoints.isEmpty())
      paintWaypoints(context, &waypoints, drawWaypoint);
  }

  // VOR -------------------------------------------------
  if(context->mapLayer->isVor() && context->objectTypes.testFlag(map::VOR) && !context->isOverflow())
  {
    const QList<MapVor> *vors = mapQuery->getVors(curBox, context->mapLayer, context->lazyUpdate);
    if(vors != nullptr)
      paintVors(context, vors, context->drawFast);
  }

  // NDB -------------------------------------------------
  if(context->mapLayer->isNdb() && context->objectTypes.testFlag(map::NDB) && !context->isOverflow())
  {
    const QList<MapNdb> *ndbs = mapQuery->getNdbs(curBox, context->mapLayer, context->lazyUpdate);
    if(ndbs != nullptr)
      paintNdbs(context, ndbs, context->drawFast);
  }

  // Marker -------------------------------------------------
  if(context->mapLayer->isMarker() && context->objectTypes.testFlag(map::ILS) && !context->isOverflow())
  {
    const QList<MapMarker> *markers = mapQuery->getMarkers(curBox, context->mapLayer, context->lazyUpdate);
    if(markers != nullptr)
      paintMarkers(context, markers, context->drawFast);
  }
}

/* Draw airways and texts */
void MapPainterNav::paintAirways(PaintContext *context, const QList<MapAirway> *airways, bool fast)
{
  QFontMetrics metrics = context->painter->fontMetrics();

  // Keep text placement information for each airway line which can cover multiple texts/airways
  struct Place
  {
    Place()
    {
    }

    Place(const QString& text, int initialIndex, bool reversed)
      : texts(
    {
      text
    }), airwayIndexByText({initialIndex}), positionReversed({reversed})
    {
    }

    QStringList texts; // Prepared airway texts
    QVector<int> airwayIndexByText; // Index into "airways" for each text
    QVector<bool> positionReversed; // Line is reversed for text
  };

  // Used to combine texts of different airway lines with the same coordinates into one text.
  // Key is line coordinates as text (avoid floating point compare) and
  // value is index into texts and airwayIndex
  QHash<QString, int> lines;
  // Contains combined text for overlapping airway lines and points to index or airway in airway list
  QVector<Place> textlist;
  QPolygonF arrow = buildArrow(static_cast<float>(mapcolors::airwayBothPen.widthF() * 2.5));
  Marble::GeoPainter *painter = context->painter;

  for(int i = 0; i < airways->size(); i++)
  {
    const MapAirway& airway = airways->at(i);
    bool isTrack = airway.isTrack();
    bool isAirway = airway.isAirway();

    bool ident = (isAirway && context->mapLayer->isAirwayIdent()) || (isTrack && context->mapLayer->isTrackIdent());
    bool info = (isAirway && context->mapLayer->isAirwayInfo()) || (isTrack && context->mapLayer->isTrackInfo());

    if(airway.type == map::AIRWAY_JET && !context->objectTypes.testFlag(map::AIRWAYJ))
      continue;
    if(airway.type == map::AIRWAY_VICTOR && !context->objectTypes.testFlag(map::AIRWAYV))
      continue;
    if(isTrack && !context->objectTypes.testFlag(map::TRACK))
      continue;

    painter->setPen(mapcolors::penForAirwayTrack(airway));
    painter->setBrush(painter->pen().color());

    // Get start and end point of airway segment in screen coordinates
    int x1, y1, x2, y2;
    bool visible1 = wToS(airway.from, x1, y1);
    bool visible2 = wToS(airway.to, x2, y2);

    if(!visible1 && !visible2)
    {
      // Check bounding rect for visibility
      const Rect& bnd = airway.bounding;
      Marble::GeoDataLatLonBox airwaybox(bnd.getNorth(), bnd.getSouth(), bnd.getEast(), bnd.getWest(),
                                         Marble::GeoDataCoordinates::Degree);
      visible1 = airwaybox.intersects(context->viewport->viewLatLonAltBox());
    }

    // Draw line if both points are visible or line intersects screen coordinates
    if(visible1 || visible2)
    {
      if(context->objCount())
        return;

      drawLine(painter, Line(airway.from, airway.to));

      if(!fast)
      {
        if(airway.direction != map::DIR_BOTH && !ident)
        {
          Line arrLine = airway.direction != map::DIR_FORWARD ?
                         Line(airway.from, airway.to) : Line(airway.to, airway.from);
          paintArrowAlongLine(painter, arrLine, arrow, 0.5f);
        }

        // Build text index
        QString text;
        if(ident)
          text += airway.name;

        if(info)
        {
          text += QString(tr(" / "));

          if(isTrack)
            text += map::airwayTrackTypeToString(airway.type);
          else
            text += map::airwayTrackTypeToShortString(airway.type);

          QString altTxt = map::airwayAltTextShort(airway);

          if(!altTxt.isEmpty())
            text += QString(tr(" / ")) + altTxt;
        }

        if(!text.isEmpty())
        {
          QString firstCoordStr = airway.from.toString(3, false /*altitude*/);
          QString toCoordStr = airway.to.toString(3, false /*altitude*/);

          // Create string key for index by using the coordinates
          QString lineTextKey = firstCoordStr + "|" + toCoordStr;

          bool reversed = false;

          // Does it already exist in the map?
          int index = lines.value(lineTextKey, -1);
          if(index == -1)
          {
            // Try with reversed coordinates
            index = lines.value(toCoordStr + "|" + firstCoordStr, -1);
            reversed = index != -1;
          }

          // if(reversed)
          // qDebug() << text << reversed;

          if(index != -1)
          {
            // Index already found - add the new text to the present one
            textlist[index].texts.append(text);
            textlist[index].airwayIndexByText.append(i);
            textlist[index].positionReversed.append(reversed);
          }
          else if(!text.isEmpty())
          {
            // Neither with forward nor reversed coordinates found - insert a new entry
            textlist.append(Place(text, i, reversed));
            lines.insert(lineTextKey, textlist.size() - 1);
          }
        }
      }
    }
  }

  TextPlacement textPlacement(painter, this);

  // Draw texts ----------------------------------------
  if(!textlist.isEmpty())
  {
    int i = 0;
    painter->setPen(mapcolors::airwayTextColor);
    for(Place& place: textlist)
    {
      const MapAirway& airway = airways->at(place.airwayIndexByText.first());
      int xt = -1, yt = -1;
      float textBearing;

      // First find text position with incomplete text
      QString text = place.texts.join(tr(", "));
      if(textPlacement.findTextPos(airway.from, airway.to, metrics.width(text), metrics.height() * 2,
                                   xt, yt, &textBearing))
      {
        // Prepend arrows to all texts
        for(int j = 0; j < place.texts.size(); ++j)
        {
          const map::MapAirway& aw = airways->at(place.airwayIndexByText.at(j));
          QString& txt = place.texts[j];

          if(aw.direction != map::DIR_BOTH)
            // Turn arrow depending on text angle, direction and depending if text segment is reversed compared to first
            txt.prepend(((textBearing > 180.f) ^
                         place.positionReversed.at(j) ^
                         (aw.direction == map::DIR_FORWARD)) ? tr("◄ ") : tr("► "));
        }
        text = place.texts.join(tr(", "));

        painter->translate(xt, yt);
        painter->rotate(textBearing > 180.f ? textBearing + 90.f : textBearing - 90.f);
        painter->drawText(-painter->fontMetrics().width(text) / 2,
                          painter->fontMetrics().ascent(), text);
        painter->resetTransform();
      }
      i++;
    }
  }
}

/* Draw waypoints. If airways are enabled corresponding waypoints are drawn too */
void MapPainterNav::paintWaypoints(PaintContext *context, const QList<MapWaypoint> *waypoints, bool drawWaypoint)
{
  bool drawAirwayV = context->mapLayer->isAirwayWaypoint() && context->objectTypes.testFlag(map::AIRWAYV);
  bool drawAirwayJ = context->mapLayer->isAirwayWaypoint() && context->objectTypes.testFlag(map::AIRWAYJ);
  bool drawTrack = context->mapLayer->isTrackWaypoint() && context->objectTypes.testFlag(map::TRACK);

  bool fill = context->flags2 & opts2::MAP_NAVAID_TEXT_BACKGROUND;

  for(const MapWaypoint& waypoint : *waypoints)
  {
    // If waypoints are off, airways are on and waypoint has no airways skip it
    if(!(drawWaypoint || (drawAirwayV && waypoint.hasVictorAirways) || (drawAirwayJ && waypoint.hasJetAirways) ||
         (drawTrack && waypoint.hasTracks)))
      continue;

    if(context->routeIdMap.contains(waypoint.getRef()))
      continue;

    int x, y;
    bool visible = wToS(waypoint.position, x, y);

    if(visible)
    {
      if(context->objCount())
        return;

      int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getWaypointSymbolSize());
      symbolPainter->drawWaypointSymbol(context->painter, QColor(), x, y, size, false);

      // If airways are drawn force display of the respecive waypoints
      if(context->mapLayer->isWaypointName() || // Draw all waypoint names or ...
         (context->mapLayer->isAirwayIdent() && // Draw names for specific airway waypoints
          ((drawAirwayV && waypoint.hasVictorAirways) || (drawAirwayJ && waypoint.hasJetAirways))) ||
         (context->mapLayer->isTrackIdent() && // Draw names for specific airway waypoints
          (drawTrack && waypoint.hasTracks)))
        symbolPainter->drawWaypointText(context->painter, waypoint, x, y, textflags::IDENT, size, fill);
    }
  }
}

void MapPainterNav::paintVors(PaintContext *context, const QList<MapVor> *vors, bool drawFast)
{
  bool fill = context->flags2 & opts2::MAP_NAVAID_TEXT_BACKGROUND;

  for(const MapVor& vor : *vors)
  {
    if(context->routeIdMap.contains(vor.getRef()))
      continue;

    int x, y;
    bool visible = wToS(vor.position, x, y);

    if(visible)
    {
      if(context->objCount())
        return;

      int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getVorSymbolSize());
      symbolPainter->drawVorSymbol(context->painter, vor, x, y,
                                   size, false, drawFast,
                                   context->mapLayerEffective->isVorLarge() ? size * 5 : 0);

      textflags::TextFlags flags;

      if(context->mapLayer->isVorInfo())
        flags = textflags::IDENT | textflags::TYPE | textflags::FREQ;
      else if(context->mapLayer->isVorIdent())
        flags = textflags::IDENT;

      symbolPainter->drawVorText(context->painter, vor, x, y, flags, size, fill);
    }
  }
}

void MapPainterNav::paintNdbs(PaintContext *context, const QList<MapNdb> *ndbs, bool drawFast)
{
  bool fill = context->flags2 & opts2::MAP_NAVAID_TEXT_BACKGROUND;

  for(const MapNdb& ndb : *ndbs)
  {
    if(context->routeIdMap.contains(ndb.getRef()))
      continue;

    int x, y;
    bool visible = wToS(ndb.position, x, y);

    if(visible)
    {
      if(context->objCount())
        return;

      int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getNdbSymbolSize());
      symbolPainter->drawNdbSymbol(context->painter, x, y, size, false, drawFast);

      textflags::TextFlags flags;

      if(context->mapLayer->isNdbInfo())
        flags = textflags::IDENT | textflags::TYPE | textflags::FREQ;
      else if(context->mapLayer->isNdbIdent())
        flags = textflags::IDENT;

      symbolPainter->drawNdbText(context->painter, ndb, x, y, flags, size, fill);
    }
  }
}

void MapPainterNav::paintMarkers(PaintContext *context, const QList<MapMarker> *markers, bool drawFast)
{
  int transparency = context->flags2 & opts2::MAP_NAVAID_TEXT_BACKGROUND ? 255 : 0;

  for(const MapMarker& marker : *markers)
  {
    int x, y;
    bool visible = wToS(marker.position, x, y);

    if(visible)
    {
      if(context->objCount())
        return;

      int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getMarkerSymbolSize());
      symbolPainter->drawMarkerSymbol(context->painter, marker, x, y, size, drawFast);

      if(context->mapLayer->isMarkerInfo())
      {
        QString type = marker.type.toLower();
        type[0] = type.at(0).toUpper();
        x -= size / 2 + 2;
        symbolPainter->textBox(context->painter, {type}, mapcolors::markerSymbolColor, x, y,
                               textatt::BOLD | textatt::RIGHT, transparency);
      }
    }
  }
}
