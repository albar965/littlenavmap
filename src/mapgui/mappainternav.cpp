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

#include "mapgui/mappainternav.h"

#include "common/symbolpainter.h"
#include "common/mapcolors.h"
#include "mapgui/mapwidget.h"

#include <QElapsedTimer>

#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

using namespace Marble;
using namespace atools::geo;
using namespace maptypes;

MapPainterNav::MapPainterNav(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale, bool verboseMsg)
  : MapPainter(mapWidget, mapQuery, mapScale, verboseMsg)
{
}

MapPainterNav::~MapPainterNav()
{
}

void MapPainterNav::render(const PaintContext *context)
{
  const GeoDataLatLonAltBox& curBox = context->viewport->viewLatLonAltBox();
  QElapsedTimer t;
  t.start();

  setRenderHints(context->painter);

  bool drawAirway = context->mapLayer->isAirway() &&
                    (context->objectTypes.testFlag(maptypes::AIRWAYJ) ||
                     context->objectTypes.testFlag(maptypes::AIRWAYV));
  if(drawAirway)
  {
    // Draw airway lines
    const QList<MapAirway> *airways = query->getAirways(curBox, context->mapLayer, context->drawFast);
    if(airways != nullptr)
    {
      if(context->viewContext == Marble::Still && verbose)
      {
        qDebug() << "Number of airways" << airways->size();
        qDebug() << "Time for query" << t.elapsed() << " ms";
        qDebug() << curBox.toString();
        qDebug() << *context->mapLayer;
        t.restart();
      }

      paintAirways(context, airways, curBox, context->drawFast);
    }
  }

  bool drawWaypoint = context->mapLayer->isWaypoint() && context->objectTypes.testFlag(maptypes::WAYPOINT);

  if(drawWaypoint || drawAirway)
  {
    const QList<MapWaypoint> *waypoints = query->getWaypoints(curBox, context->mapLayer, context->drawFast);
    if(waypoints != nullptr)
    {
      if(context->viewContext == Marble::Still && verbose)
      {
        qDebug() << "Number of waypoints" << waypoints->size();
        qDebug() << "Time for query" << t.elapsed() << " ms";
        qDebug() << curBox.toString();
        qDebug() << *context->mapLayer;
        t.restart();
      }

      paintWaypoints(context, waypoints, drawWaypoint, context->drawFast);
    }
  }

  if(context->mapLayer->isVor() && context->objectTypes.testFlag(maptypes::VOR))
  {
    const QList<MapVor> *vors = query->getVors(curBox, context->mapLayer, context->drawFast);
    if(vors != nullptr)
    {
      if(context->viewContext == Marble::Still && verbose)
      {
        qDebug() << "Number of vors" << vors->size();
        qDebug() << "Time for query" << t.elapsed() << " ms";
        qDebug() << curBox.toString();
        qDebug() << *context->mapLayer;
        t.restart();
      }

      paintVors(context, vors, context->drawFast);
    }
  }

  if(context->mapLayer->isNdb() && context->objectTypes.testFlag(maptypes::NDB))
  {
    const QList<MapNdb> *ndbs = query->getNdbs(curBox, context->mapLayer, context->drawFast);
    if(ndbs != nullptr)
    {
      if(context->viewContext == Marble::Still && verbose)
      {
        qDebug() << "Number of ndbs" << ndbs->size();
        qDebug() << "Time for query" << t.elapsed() << " ms";
        qDebug() << curBox.toString();
        qDebug() << *context->mapLayer;
        t.restart();
      }

      paintNdbs(context, ndbs, context->drawFast);
    }
  }

  if(context->mapLayer->isMarker() && context->objectTypes.testFlag(maptypes::ILS))
  {
    const QList<MapMarker> *markers = query->getMarkers(curBox, context->mapLayer, context->drawFast);
    if(markers != nullptr)
    {
      if(context->viewContext == Marble::Still && verbose)
      {
        qDebug() << "Number of marker" << markers->size();
        qDebug() << "Time for query" << t.elapsed() << " ms";
        qDebug() << curBox.toString();
        qDebug() << *context->mapLayer;
        t.restart();
      }

      paintMarkers(context, markers, context->drawFast);
    }
  }

  if(context->viewContext == Marble::Still && verbose)
    qDebug() << "Time for paint" << t.elapsed() << " ms";
}

void MapPainterNav::paintAirways(const PaintContext *context, const QList<MapAirway> *airways,
                                 const GeoDataLatLonAltBox& curBox, bool fast)
{
  QFontMetrics metrics = context->painter->fontMetrics();
  // Line as text (avoid floating point compare) and index into texts
  QHash<QString, int> lines;
  QList<QString> texts;
  QList<int> awIndex;

  for(int i = 0; i < airways->size(); i++)
  {
    const MapAirway& airway = airways->at(i);

    if(airway.type == maptypes::JET && !context->objectTypes.testFlag(maptypes::AIRWAYJ))
      continue;
    if(airway.type == maptypes::VICTOR && !context->objectTypes.testFlag(maptypes::AIRWAYV))
      continue;

    if(airway.type == maptypes::VICTOR)
      context->painter->setPen(QPen(mapcolors::airwayVictorColor, 1.5));
    else if(airway.type == maptypes::JET)
      context->painter->setPen(QPen(mapcolors::airwayJetColor, 1.5));
    else if(airway.type == maptypes::BOTH)
      context->painter->setPen(QPen(mapcolors::airwayBothColor, 1.5));

    int x1, y1, x2, y2;
    bool visible1 = wToS(airway.from, x1, y1);
    bool visible2 = wToS(airway.to, x2, y2);

    if(!visible1 && !visible2)
    {
      GeoDataLatLonBox airwaybox(airway.bounding.getNorth(), airway.bounding.getSouth(),
                                 airway.bounding.getEast(), airway.bounding.getWest(), DEG);
      visible1 = airwaybox.intersects(curBox);
    }

    if(visible1 || visible2)
    {
      GeoDataCoordinates from(airway.from.getLonX(), airway.from.getLatY(), 0, DEG);
      GeoDataCoordinates to(airway.to.getLonX(), airway.to.getLatY(), 0, DEG);
      GeoDataLineString line;
      line.setTessellate(true);
      line << from << to;

      context->painter->drawPolyline(line);

      if(!fast)
      {
        QString text;
        if(context->mapLayer->isAirwayIdent())
          text += airway.name;
        if(context->mapLayer->isAirwayInfo())
        {
          text += QString(tr(" / ")) + maptypes::airwayTypeToShortString(airway.type);
          if(airway.minalt)
            text += QString(tr(" / ")) + QLocale().toString(airway.minalt) + tr(" ft");
        }

        if(!text.isEmpty())
        {
          QString lineText = line.first().toString(GeoDataCoordinates::Decimal, 3) + tr("|") +
                             line.last().toString(GeoDataCoordinates::Decimal, 3);
          int index = lines.value(lineText, -1);
          if(index == -1)
            index = lines.value(line.last().toString(GeoDataCoordinates::Decimal, 3) + tr("|") +
                                line.first().toString(GeoDataCoordinates::Decimal, 3), -1);

          if(index != -1)
          {
            QString oldTxt(texts.at(index));
            if(oldTxt != text)
              texts[index] = texts.at(index) + ", " + text;
          }
          else if(!text.isEmpty())
          {
            texts.append(text);
            lines.insert(lineText, texts.size() - 1);
            awIndex.append(i);
          }
        }
      }
    }
  }

  int i = 0;
  context->painter->setPen(mapcolors::airwayTextColor);
  for(const QString& text : texts)
  {
    const MapAirway& airway = airways->at(awIndex.at(i));
    int xt = -1, yt = -1;
    float textBearing;
    if(findTextPos(airway.from, airway.to, context->painter, metrics.width(text), metrics.height() * 2,
                   xt, yt, &textBearing))
    {
      float rotate;
      if(textBearing > 180.f)
        rotate = textBearing + 90.f;
      else
        rotate = textBearing - 90.f;

      context->painter->translate(xt, yt);
      context->painter->rotate(rotate);
      context->painter->drawText(-context->painter->fontMetrics().width(text) / 2,
                                 context->painter->fontMetrics().ascent(), text);
      context->painter->resetTransform();
    }
    i++;
  }
}

void MapPainterNav::paintWaypoints(const PaintContext *context, const QList<MapWaypoint> *waypoints,
                                   bool drawWaypoint, bool drawFast)
{
  bool drawAirwayV = context->mapLayer->isAirway() && context->objectTypes.testFlag(maptypes::AIRWAYV);
  bool drawAirwayJ = context->mapLayer->isAirway() && context->objectTypes.testFlag(maptypes::AIRWAYJ);

  for(const MapWaypoint& waypoint : *waypoints)
  {
    if(!(drawWaypoint || (drawAirwayV && waypoint.hasVictor) || (drawAirwayJ && waypoint.hasJet)))
      continue;

    int x, y;
    bool visible = wToS(waypoint.position, x, y);

    if(visible)
    {
      int size = context->symSize(context->mapLayerEffective->getWaypointSymbolSize());
      symbolPainter->drawWaypointSymbol(context->painter, waypoint, QColor(), x, y,
                                        size, false, drawFast);

      if(context->mapLayer->isWaypointName() ||
         (context->mapLayer->isAirwayIdent() && (drawAirwayV || drawAirwayJ)))
        symbolPainter->drawWaypointText(context->painter, waypoint, x, y, textflags::IDENT,
                                        size, false, drawFast);
    }
  }
}

void MapPainterNav::paintVors(const PaintContext *context, const QList<MapVor> *vors, bool drawFast)
{
  for(const MapVor& vor : *vors)
  {
    int x, y;
    bool visible = wToS(vor.position, x, y);

    if(visible)
    {
      int size = context->symSize(context->mapLayerEffective->getVorSymbolSize());
      symbolPainter->drawVorSymbol(context->painter, vor, x, y,
                                   size, false, drawFast,
                                   context->mapLayerEffective->isVorLarge() ? size * 5 : 0);

      textflags::TextFlags flags;

      if(context->mapLayer->isVorInfo())
        flags = textflags::IDENT | textflags::TYPE | textflags::FREQ;
      else if(context->mapLayer->isVorIdent())
        flags = textflags::IDENT;

      symbolPainter->drawVorText(context->painter, vor, x, y,
                                 flags, size, false, drawFast);
    }
  }
}

void MapPainterNav::paintNdbs(const PaintContext *context, const QList<MapNdb> *ndbs, bool drawFast)
{
  for(const MapNdb& ndb : *ndbs)
  {
    int x, y;
    bool visible = wToS(ndb.position, x, y);

    if(visible)
    {
      int size = context->symSize(context->mapLayerEffective->getNdbSymbolSize());
      symbolPainter->drawNdbSymbol(context->painter, ndb, x, y, size, false, drawFast);

      textflags::TextFlags flags;

      if(context->mapLayer->isNdbInfo())
        flags = textflags::IDENT | textflags::TYPE | textflags::FREQ;
      else if(context->mapLayer->isNdbIdent())
        flags = textflags::IDENT;

      symbolPainter->drawNdbText(context->painter, ndb, x, y, flags, size, false, drawFast);
    }
  }
}

void MapPainterNav::paintMarkers(const PaintContext *context, const QList<MapMarker> *markers, bool drawFast)
{
  for(const MapMarker& marker : *markers)
  {
    int x, y;
    bool visible = wToS(marker.position, x, y);

    if(visible)
    {
      int size = context->symSize(context->mapLayerEffective->getMarkerSymbolSize());
      symbolPainter->drawMarkerSymbol(context->painter, marker, x, y, size, drawFast);

      if(context->mapLayer->isMarkerInfo())
      {
        QString type = marker.type.toLower();
        type[0] = type.at(0).toUpper();
        x -= size / 2 + 2;
        symbolPainter->textBox(context->painter, {type}, mapcolors::markerSymbolColor, x, y,
                               textatt::BOLD | textatt::RIGHT, 0);
      }
    }
  }
}
