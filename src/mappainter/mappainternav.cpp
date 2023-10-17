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

#include "mappainter/mappainternav.h"

#include "common/symbolpainter.h"
#include "common/mapcolors.h"
#include "common/textplacement.h"
#include "util/paintercontextsaver.h"
#include "mapgui/maplayer.h"
#include "query/mapquery.h"
#include "query/airwaytrackquery.h"
#include "query/waypointtrackquery.h"
#include "common/maptools.h"

#include <QElapsedTimer>
#include <QStringBuilder>

#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

using namespace Marble;
using namespace atools::geo;
using namespace map;

MapPainterNav::MapPainterNav(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainter(mapWidget, mapScale, paintContext)
{
}

MapPainterNav::~MapPainterNav()
{
}

void MapPainterNav::render()
{
  const GeoDataLatLonAltBox& curBox = context->viewport->viewLatLonAltBox();

  atools::util::PainterContextSaver saver(context->painter);

  // Airways -------------------------------------------------
  bool drawAirway = context->mapLayer->isAirway() &&
                    (context->objectTypes.testFlag(map::AIRWAYJ) || context->objectTypes.testFlag(map::AIRWAYV));

  context->szFont(context->textSizeAirway);

  if(drawAirway && !context->isObjectOverflow())
  {
    // Draw airway lines
    context->startTimer("Airway fetch");
    QList<MapAirway> airways;
    airwayQuery->getAirways(airways, curBox, context->mapLayer, context->lazyUpdate);
    context->endTimer("Airway fetch");

    paintAirways(&airways, context->drawFast, false /* track */);
  }

  // Tracks -------------------------------------------------
  bool drawTrack = context->mapLayer->isTrack() && context->objectTypes.testFlag(map::TRACK);
  if(drawTrack && !context->isObjectOverflow())
  {
    // Draw track lines
    context->startTimer("Track fetch");
    QList<MapAirway> tracks;
    airwayQuery->getTracks(tracks, curBox, context->mapLayer, context->lazyUpdate);
    context->endTimer("Track fetch");

    paintAirways(&tracks, context->drawFast, true /* track */);
  }

  context->szFont(context->textSizeNavaid);

  // Waypoints on airways and tracks -------------------------------------------------
  bool overflow = false;
  bool drawAirwayWpV = context->mapLayer->isAirwayWaypoint() && context->objectTypes.testFlag(map::AIRWAYV);
  bool drawAirwayWpJ = context->mapLayer->isAirwayWaypoint() && context->objectTypes.testFlag(map::AIRWAYJ);
  bool drawNormalWp = context->mapLayer->isWaypoint() && context->objectTypes.testFlag(map::WAYPOINT);
  bool drawTrackWp = context->mapLayer->isTrackWaypoint() && context->objectTypes.testFlag(map::TRACK);

  // Merge and disambiguate all navaids and airway related navaids into hashes
  QHash<int, MapWaypoint> allWaypoints;
  QHash<int, MapVor> allVor;
  QHash<int, MapNdb> allNdb;
  if((drawAirwayWpV || drawAirwayWpJ || drawTrackWp) && !context->isObjectOverflow())
  {
    context->startTimer("Waypoint fetch");
    // If airways are drawn we also have to go through waypoints
    QList<MapWaypoint> waypoints;
    waypointQuery->getWaypointsAirway(waypoints, curBox, context->mapLayer, context->lazyUpdate, overflow);
    context->setQueryOverflow(overflow);
    context->endTimer("Waypoint fetch");

    context->startTimer("Waypoint resolve");
    // Resolve all artificial waypoints to the respective radio navaids and also filter by airway/track type
    // Do not copy flight plan waypoints - these are drawn in MapPainterRoute
    mapQuery->resolveWaypointNavaids(waypoints, allWaypoints, allVor, allNdb, false /* flightplan */,
                                     drawNormalWp, drawAirwayWpV, drawAirwayWpJ, drawTrackWp);
    context->endTimer("Waypoint resolve");
  }

  // Waypoints -------------------------------------------------
  context->startTimer("Waypoint draw");
  if(drawNormalWp && !context->isObjectOverflow())
  {
    QList<MapWaypoint> waypoints;
    waypointQuery->getWaypoints(waypoints, curBox, context->mapLayer, context->lazyUpdate, overflow);
    context->setQueryOverflow(overflow);
    maptools::insert(allWaypoints, waypoints);
  }
  paintWaypoints(allWaypoints);
  context->endTimer("Waypoint draw");

  // VOR -------------------------------------------------
  context->startTimer("VOR");
  if(context->mapLayer->isVor() && context->objectTypes.testFlag(map::VOR) && !context->isObjectOverflow())
  {
    const QList<MapVor> *vors = mapQuery->getVors(curBox, context->mapLayer, context->lazyUpdate, overflow);
    context->setQueryOverflow(overflow);
    if(vors != nullptr)
      maptools::insert(allVor, *vors);
  }
  paintVors(allVor, context->drawFast);
  context->endTimer("VOR");

  // NDB -------------------------------------------------
  context->startTimer("NDB");
  if(context->mapLayer->isNdb() && context->objectTypes.testFlag(map::NDB) && !context->isObjectOverflow())
  {
    const QList<MapNdb> *ndbs = mapQuery->getNdbs(curBox, context->mapLayer, context->lazyUpdate, overflow);
    context->setQueryOverflow(overflow);
    if(ndbs != nullptr)
      maptools::insert(allNdb, *ndbs);
  }
  paintNdbs(allNdb, context->drawFast);
  context->endTimer("NDB");

  // Marker -------------------------------------------------
  context->startTimer("Marker");
  if(context->mapLayer->isMarker() && context->objectTypes.testFlag(map::MARKER) && !context->isObjectOverflow())
  {
    const QList<MapMarker> *markers = mapQuery->getMarkers(curBox, context->mapLayer, context->lazyUpdate, overflow);
    context->setQueryOverflow(overflow);

    if(markers != nullptr)
      paintMarkers(markers, context->drawFast);
  }
  context->endTimer("Marker");

  // Holding -------------------------------------------------
  context->startTimer("Hold");
  if(context->mapLayer->isHolding() && context->objectTypes.testFlag(map::HOLDING) && !context->isObjectOverflow())
  {
    const QList<MapHolding> *holds = mapQuery->getHoldings(curBox, context->mapLayer, context->lazyUpdate, overflow);
    context->setQueryOverflow(overflow);

    if(holds != nullptr)
      paintHoldingMarks(*holds, false /* user */, context->drawFast);
  }
  context->endTimer("Hold");
}

/* Draw airways and texts */
void MapPainterNav::paintAirways(const QList<map::MapAirway> *airways, bool fast, bool track)
{
#ifdef Q_OS_WIN
  bool rotateText = context->viewContext == Marble::Still;
#else
  bool rotateText = true;
#endif

  // Keep text placement information for each airway line which can cover multiple texts/airways
  struct Place
  {
    Place(const QString& text, int initialIndex, bool reversed)
      : texts(text), airwayIndexByText({initialIndex}), positionReversed({reversed})
    {
    }

    QStringList texts; // Prepared airway texts
    QVector<int> airwayIndexByText; // Index into "airways" for each text
    QVector<bool> positionReversed; // Line is reversed for text
  };

  bool fill = context->flags2 & opts2::MAP_AIRWAY_TEXT_BACKGROUND;
  float linewidthAirway = context->szF(context->thicknessAirway, 1.f);
  float linewidthTrack = context->szF(context->thicknessAirway, 2.f);

  // Used to combine texts of different airway lines with the same coordinates into one text.
  // Key is line coordinates as text (avoid floating point compare) and
  // value is index into texts and airwayIndex
  QHash<QString, int> lines;
  // Contains combined text for overlapping airway lines and points to index or airway in airway list
  QVector<Place> textlist;
  QPolygonF arrowAirway = buildArrow(static_cast<float>(linewidthAirway * 2.5));
  QPolygonF arrowTrack = buildArrow(static_cast<float>(linewidthTrack * 2.5));
  Marble::GeoPainter *painter = context->painter;

  context->startTimer(track ? "Track draw" : "Airway draw");
  for(int i = 0; i < airways->size(); i++)
  {
    const MapAirway& airway = airways->at(i);
    bool isTrack = airway.isTrack();

    if((airway.type == map::AIRWAY_JET && !context->objectTypes.testFlag(map::AIRWAYJ)) ||
       (airway.type == map::AIRWAY_VICTOR && !context->objectTypes.testFlag(map::AIRWAYV)) ||
       (isTrack && !context->objectTypes.testFlag(map::TRACK)))
      continue;

    bool ident = (!isTrack && context->mapLayer->isAirwayIdent()) || (isTrack && context->mapLayer->isTrackIdent());
    bool info = (!isTrack && context->mapLayer->isAirwayInfo()) || (isTrack && context->mapLayer->isTrackInfo());

    painter->setPen(QPen(mapcolors::colorForAirwayOrTrack(airway), isTrack ? linewidthTrack : linewidthAirway));
    painter->setBrush(painter->pen().color());

    // Get start and end point of airway segment in screen coordinates
    int x1, y1, x2, y2;
    bool visible1 = wToS(airway.from, x1, y1);
    bool visible2 = wToS(airway.to, x2, y2);

    if(!visible1 && !visible2)
      // Check bounding rect for visibility
      visible1 = context->viewportRect.overlaps(airway.bounding);

    // Draw line if both points are visible or line intersects screen coordinates
    if(visible1 || visible2)
    {
      if(context->objCount())
        return;

      drawLine(painter, Line(airway.from, airway.to));

      if(!fast)
      {
        if(airway.direction != map::DIR_BOTH && !ident && context->mapLayer->isAirwayDetails())
        {
          Line arrLine = airway.direction != map::DIR_FORWARD ? Line(airway.from, airway.to) : Line(airway.to, airway.from);
          paintArrowAlongLine(painter, arrLine, isTrack ? arrowTrack : arrowAirway, 0.5f);
        }

        // Build text index
        QString text;
        if(ident)
          text.append(airway.name);

        if(info && rotateText)
        {
          text.append(tr(" / "));

          if(isTrack)
            text.append(map::airwayTrackTypeToString(airway.type));
          else
            text.append(map::airwayTrackTypeToShortString(airway.type));

          QString altTxt = map::airwayAltTextShort(airway);

          if(!altTxt.isEmpty())
            text.append(tr(" / ") % altTxt);
        }

        if(!text.isEmpty())
        {
          QString firstCoordStr = airway.from.toString(3, false /*altitude*/);
          QString toCoordStr = airway.to.toString(3, false /*altitude*/);

          // Create string key for index by using the coordinates
          QString lineTextKey = firstCoordStr % "|" % toCoordStr;

          bool reversed = false;

          // Does it already exist in the map?
          int index = lines.value(lineTextKey, -1);
          if(index == -1)
          {
            // Try with reversed coordinates
            index = lines.value(toCoordStr % "|" % firstCoordStr, -1);
            reversed = index != -1;
          }

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
  context->endTimer(track ? "Track draw" : "Airway draw");

  context->startTimer(track ? "Track draw text" : "Airway draw text");
  // Draw texts ----------------------------------------
  TextPlacement textPlacement(painter, this, context->screenRect);
  if(!textlist.isEmpty())
  {
    painter->setPen(mapcolors::airwayTextColor);
    if(fill)
    {
      painter->setBackgroundMode(Qt::OpaqueMode);
      painter->setBrush(mapcolors::textBoxColor);
      painter->setBackground(mapcolors::textBoxColor);
    }

    QFontMetricsF metrics(context->painter->font());
    for(Place& place : textlist)
    {
      const MapAirway& airway = airways->at(place.airwayIndexByText.constFirst());
      float xt = -1.f, yt = -1.f, textBearing;

      // First find text position with incomplete text
      // Add space at start and end to avoid letters touching the background rectangle border
      QString text = " " % place.texts.join(tr(", ")) % " ";
      Line line(airway.from, airway.to);
      if(textPlacement.findTextPos(line, line.lengthMeter(),
                                   static_cast<float>(metrics.horizontalAdvance(text)),
                                   static_cast<float>(metrics.height()), 20, xt, yt, &textBearing))
      {
        // Prepend arrows to all texts
        for(int j = 0; j < place.texts.size(); ++j)
        {
          const map::MapAirway& aw = airways->at(place.airwayIndexByText.at(j));
          QString& txt = place.texts[j];

          if(aw.direction != map::DIR_BOTH && rotateText)
            // Turn arrow depending on text angle, direction and depending if text segment is reversed compared to first
            // Omit arrow if no rotation
            txt.prepend(((textBearing < 180.f) ^ place.positionReversed.at(j) ^ (aw.direction == map::DIR_FORWARD)) ? tr("◄ ") : tr("► "));
          else
            // Elide for not rotated texts
            txt = atools::elideTextShort(txt, 20);
        }

        // Add space at start and end to avoid letters touching the background rectangle border
        text = " " % place.texts.join(tr(", ")) % " ";

        painter->translate(xt, yt);
        if(rotateText)
          painter->rotate(textBearing > 180.f ? textBearing + 90.f : textBearing - 90.f);
        painter->drawText(QPointF(-metrics.horizontalAdvance(text) / 2.,
                                  -metrics.descent() - linewidthAirway), text);
        painter->resetTransform();
      }
    }
  }
  context->endTimer(track ? "Track draw text" : "Airway draw text");
}

/* Draw waypoints. If airways are enabled corresponding waypoints are drawn too */
void MapPainterNav::paintWaypoints(const QHash<int, map::MapWaypoint>& waypoints)
{
  bool drawAirwayV = context->mapLayer->isAirwayWaypoint() && context->objectTypes.testFlag(map::AIRWAYV);
  bool drawAirwayJ = context->mapLayer->isAirwayWaypoint() && context->objectTypes.testFlag(map::AIRWAYJ);
  bool drawTrack = context->mapLayer->isTrackWaypoint() && context->objectTypes.testFlag(map::TRACK);

  bool fill = context->flags2 & opts2::MAP_NAVAID_TEXT_BACKGROUND;

  // Use margins for text placed on the right side of the object to avoid disappearing at the left screen border
  const static QMargins MARGINS(50, 10, 10, 10);

  for(const MapWaypoint& waypoint : waypoints)
  {
    if(context->routeProcIdMap.contains(waypoint.getRef()) || context->routeProcIdMapRec.contains(waypoint.getRef()))
      continue;

    float x, y;
    if(wToSBuf(waypoint.position, x, y, MARGINS))
    {
      if(context->objCount())
        return;

      float size = context->szF(context->symbolSizeNavaid, context->mapLayer->getWaypointSymbolSize());

      // Use minimum size for airway waypoints if respective airways are shown
      if((waypoint.hasJetAirways && drawAirwayJ) || (waypoint.hasVictorAirways && drawAirwayV) || (waypoint.hasTracks && drawTrack))
        size = std::max(5.f, size);

      symbolPainter->drawWaypointSymbol(context->painter, QColor(), x, y, size, false);

      // If airways are drawn force display of the respecive waypoints
      if(context->mapLayer->isWaypointName() || // Draw all waypoint names or ...
         (context->mapLayer->isAirwayIdent() && // Draw names for specific airway waypoints
          ((drawAirwayV && waypoint.hasVictorAirways) || (drawAirwayJ && waypoint.hasJetAirways))) ||
         (context->mapLayer->isTrackInfo() && // Draw names for specific airway waypoints
          (drawTrack && waypoint.hasTracks)))
        symbolPainter->drawWaypointText(context->painter, waypoint, x, y, textflags::IDENT, size, fill);
    }
  }
}

void MapPainterNav::paintVors(const QHash<int, map::MapVor>& vors, bool drawFast)
{
  bool fill = context->flags2 & opts2::MAP_NAVAID_TEXT_BACKGROUND;
  float size = context->szF(context->symbolSizeNavaid, context->mapLayer->getVorSymbolSize());
  float sizeLarge = context->szF(context->symbolSizeNavaid, context->mapLayer->getVorSymbolSizeLarge());

  // Use margins for text placed on the left side of the object to avoid disappearing at the right screen border
  // Also consider VOR size
  int margin = static_cast<int>(std::max(sizeLarge, size));
  QMargins margins(margin, margin, std::max(margin, 50), margin);

  for(const MapVor& vor : vors)
  {
    if(context->routeProcIdMap.contains(vor.getRef()) || context->routeProcIdMapRec.contains(vor.getRef()))
      continue;

    float x, y;
    if(wToSBuf(vor.position, x, y, margins))
    {
      if(context->objCount())
        return;

      symbolPainter->drawVorSymbol(context->painter, vor, x, y, size, sizeLarge, false /* routeFill */, drawFast);

      textflags::TextFlags flags;

      if(context->mapLayer->isVorInfo())
        flags = textflags::IDENT | textflags::TYPE | textflags::FREQ;
      else if(context->mapLayer->isVorIdent())
        flags = textflags::IDENT;

      symbolPainter->drawVorText(context->painter, vor, x, y, flags, size, fill);
    }
  }
}

void MapPainterNav::paintNdbs(const QHash<int, map::MapNdb>& ndbs, bool drawFast)
{
  bool fill = context->flags2 & opts2::MAP_NAVAID_TEXT_BACKGROUND;

  float size = context->szF(context->symbolSizeNavaid, context->mapLayer->getNdbSymbolSize());

  // Use margins for text placed on the bottom of the object to avoid disappearing at the top screen border
  int sizeInt = static_cast<int>(size);
  QMargins margins(sizeInt, std::max(sizeInt, 50), sizeInt, sizeInt);

  for(const MapNdb& ndb : ndbs)
  {
    if(context->routeProcIdMap.contains(ndb.getRef()) || context->routeProcIdMapRec.contains(ndb.getRef()))
      continue;

    float x, y;
    if(wToSBuf(ndb.position, x, y, margins))
    {
      if(context->objCount())
        return;

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

void MapPainterNav::paintMarkers(const QList<map::MapMarker> *markers, bool drawFast)
{
  int transparency = context->flags2 & opts2::MAP_NAVAID_TEXT_BACKGROUND ? 255 : 0;

  float size = context->szF(context->symbolSizeNavaid, context->mapLayer->getMarkerSymbolSize());
  int sizeInt = static_cast<int>(size);
  QMargins margins(sizeInt, sizeInt, sizeInt, sizeInt);

  for(const MapMarker& marker : *markers)
  {
    float x, y;
    bool visible = wToSBuf(marker.position, x, y, margins);

    if(visible)
    {
      if(context->objCount())
        return;

      symbolPainter->drawMarkerSymbol(context->painter, marker, x, y, size, drawFast);

      if(context->mapLayer->isMarkerInfo())
      {
        QString type = marker.type.toLower();
        type[0] = type.at(0).toUpper();
        x -= size / 2.f + 2.f;
        symbolPainter->textBoxF(context->painter, {type}, mapcolors::markerSymbolColor, x, y, textatt::LEFT, transparency);
      }
    }
  }
}
