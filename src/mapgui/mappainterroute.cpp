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

#include "mapgui/mappainterroute.h"

#include "mapgui/mapwidget.h"
#include "common/symbolpainter.h"
#include "mapgui/maplayer.h"
#include "common/mapcolors.h"
#include "geo/calculations.h"
#include "route/routecontroller.h"
#include "mapgui/mapscale.h"

#include <QBitArray>
#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;

MapPainterRoute::MapPainterRoute(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale,
                                 RouteController *controller, bool verbose)
  : MapPainter(mapWidget, mapQuery, mapScale, verbose), routeController(controller)
{
}

MapPainterRoute::~MapPainterRoute()
{

}

void MapPainterRoute::render(const PaintContext *context)
{
  if(!context->objectTypes.testFlag(maptypes::ROUTE))
    return;

  setRenderHints(context->painter);

  context->painter->save();

  // Use layer independent of declutter factor
  paintRoute(context);

  context->painter->restore();
}

void MapPainterRoute::paintRoute(const PaintContext *context)
{
  const RouteMapObjectList& routeMapObjects = routeController->getRouteMapObjects();

  context->painter->setBrush(Qt::NoBrush);

  // Collect coordinates for text placement and lines first
  QList<QPoint> textCoords;
  QList<float> textBearing;
  QStringList texts;
  QList<int> textLineLengths;

  // Collect start and end points of legs and visibility
  QList<QPoint> startPoints;
  QBitArray visibleStartPoints(routeMapObjects.size(), false);
  GeoDataLineString linestring;
  linestring.setTessellate(true);

  if(!routeMapObjects.isEmpty() && context->mapLayerEffective->isAirportDiagram())
  {
    const RouteMapObject& first = routeMapObjects.at(0);
    if(first.getMapObjectType() == maptypes::AIRPORT)
    {
      int size = 100;

      Pos startPos;
      if(routeMapObjects.hasDepartureParking())
      {
        startPos = first.getParking().position;
        size = first.getParking().radius;
      }
      else if(routeMapObjects.hasDepartureStart())
        startPos = first.getStart().position;

      if(startPos.isValid())
      {
        QPoint pt = wToS(startPos);

        if(!pt.isNull())
        {
          int w = std::max(10, scale->getPixelIntForFeet(size, 90));
          int h = std::max(10, scale->getPixelIntForFeet(size, 0));

          context->painter->setPen(QPen(mapcolors::routeOutlineColor, 9,
                                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
          context->painter->drawEllipse(pt, w, h);
          context->painter->setPen(QPen(mapcolors::routeColor, 5,
                                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
          context->painter->drawEllipse(pt, w, h);
        }
      }
    }
  }

  for(int i = 0; i < routeMapObjects.size(); i++)
  {
    const RouteMapObject& obj = routeMapObjects.at(i);
    linestring.append(GeoDataCoordinates(obj.getPosition().getLonX(), obj.getPosition().getLatY(), 0, DEG));

    int x, y;
    visibleStartPoints.setBit(i, wToS(obj.getPosition(), x, y));

    if(i > 0 && !context->drawFast)
    {
      int lineLength = simpleDistance(x, y, startPoints.at(i - 1).x(), startPoints.at(i - 1).y());
      if(lineLength > 80)
      {
        QString text(QLocale().toString(obj.getDistanceTo(), 'f', 0) + tr(" nm  / ") +
                     QLocale().toString(obj.getCourseToRhumb(), 'f', 0) + tr("°M"));

        int textw = context->painter->fontMetrics().width(text);
        if(textw > lineLength)
          textw = lineLength;

        int xt, yt;
        float brg;
        Pos p1(obj.getPosition()), p2(routeMapObjects.at(i - 1).getPosition());
        if(findTextPos(p1, p2, context->painter,
                       textw, context->painter->fontMetrics().height(), xt, yt, &brg))
        {
          textCoords.append(QPoint(xt, yt));
          textBearing.append(brg);
          texts.append(text);
          textLineLengths.append(lineLength);
        }
      }
      else
      {
        textCoords.append(QPoint());
        textBearing.append(0.f);
        texts.append(QString());
        textLineLengths.append(0);
      }
    }
    startPoints.append(QPoint(x, y));
  }

  // Draw outer line
  context->painter->setPen(QPen(mapcolors::routeOutlineColor, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  context->painter->drawPolyline(linestring);

  // Draw innner line
  context->painter->setPen(QPen(mapcolors::routeColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  context->painter->drawPolyline(linestring);

  if(!context->drawFast)
  {
    // Draw text along lines
    // painter->setBrush(mapcolors::routeTextBoxColor);
    context->painter->setPen(QPen(Qt::black, 2, Qt::SolidLine, Qt::FlatCap));
    int i = 0;
    for(const QPoint& textCoord : textCoords)
    {
      QString text = texts.at(i);
      if(!text.isEmpty())
      {
        Qt::TextElideMode elide = Qt::ElideRight;
        float rotate, brg = textBearing.at(i);
        if(brg < 180.)
        {
          text += tr(" ►");
          elide = Qt::ElideLeft;
          rotate = brg - 90.f;
        }
        else
        {
          text = tr("◄ ") + text;
          elide = Qt::ElideRight;
          rotate = brg + 90.f;
        }

        QFontMetrics metrics = context->painter->fontMetrics();

        if(visibleStartPoints.testBit(i) && visibleStartPoints.testBit(i + 1))
          text = metrics.elidedText(text, elide, textLineLengths.at(i));

        context->painter->translate(textCoord.x(), textCoord.y());
        context->painter->rotate(rotate);
        // painter->drawRect(-metrics.width(text) / 2 - 2, -metrics.height(), metrics.width(text) + 4, metrics.height());
        context->painter->drawText(-metrics.width(text) / 2, -metrics.descent() - 2, text);
        context->painter->resetTransform();
      }
      i++;
    }
  }

  // Draw symbols
  int i = 0;
  for(const QPoint& pt : startPoints)
  {
    if(visibleStartPoints.testBit(i))
    {
      int x = pt.x();
      int y = pt.y();
      const RouteMapObject& obj = routeMapObjects.at(i);
      maptypes::MapObjectTypes type = obj.getMapObjectType();
      switch(type)
      {
        case maptypes::INVALID:
          paintWaypoint(context, mapcolors::routeInvalidPointColor, x, y, obj.getWaypoint());
          break;
        case maptypes::USER:
          paintUserpoint(context, x, y);
          break;
        case maptypes::AIRPORT:
          paintAirport(context, x, y, obj.getAirport());
          break;
        case maptypes::VOR:
          paintVor(context, x, y, obj.getVor());
          break;
        case maptypes::NDB:
          paintNdb(context, x, y, obj.getNdb());
          break;
        case maptypes::WAYPOINT:
          paintWaypoint(context, QColor(), x, y, obj.getWaypoint());
          break;
      }
    }
    i++;
  }

  // Draw symbol text
  i = 0;
  for(const QPoint& pt : startPoints)
  {
    if(visibleStartPoints.testBit(i))
    {
      int x = pt.x();
      int y = pt.y();
      const RouteMapObject& obj = routeMapObjects.at(i);
      maptypes::MapObjectTypes type = obj.getMapObjectType();
      switch(type)
      {
        case maptypes::INVALID:
          paintText(context, mapcolors::routeInvalidPointColor, x, y, obj.getIdent());
          break;
        case maptypes::USER:
          paintText(context, mapcolors::routeUserPointColor, x, y, obj.getIdent());
          break;
        case maptypes::AIRPORT:
          paintAirportText(context, x, y, obj.getAirport());
          break;
        case maptypes::VOR:
          paintVorText(context, x, y, obj.getVor());
          break;
        case maptypes::NDB:
          paintNdbText(context, x, y, obj.getNdb());
          break;
        case maptypes::WAYPOINT:
          paintWaypointText(context, x, y, obj.getWaypoint());
          break;
      }
    }
    i++;
  }
}

void MapPainterRoute::paintAirport(const PaintContext *context, int x, int y, const maptypes::MapAirport& obj)
{
  symbolPainter->drawAirportSymbol(context->painter, obj, x, y,
                                   context->symSize(context->mapLayer->getAirportSymbolSize()), false, false);
}

void MapPainterRoute::paintAirportText(const PaintContext *context, int x, int y,
                                       const maptypes::MapAirport& obj)
{
  textflags::TextFlags flags = textflags::IDENT | textflags::ROUTE_TEXT;

  if(context->mapLayer->isAirportRouteInfo())
    flags |= textflags::NAME | textflags::INFO;

  symbolPainter->drawAirportText(context->painter, obj, x, y, flags,
                                 context->symSize(context->mapLayer->getAirportSymbolSize()),
                                 context->mapLayer->isAirportDiagram(), true, false);
}

void MapPainterRoute::paintVor(const PaintContext *context, int x, int y, const maptypes::MapVor& obj)
{
  bool large = context->mapLayer->isVorLarge();
  symbolPainter->drawVorSymbol(context->painter, obj, x, y,
                               context->symSize(context->mapLayer->getVorSymbolSize()),
                               true, false, large);
}

void MapPainterRoute::paintVorText(const PaintContext *context, int x, int y, const maptypes::MapVor& obj)
{
  textflags::TextFlags flags = textflags::ROUTE_TEXT;
  if(context->mapLayer->isVorRouteIdent())
    flags |= textflags::IDENT;

  if(context->mapLayer->isVorRouteInfo())
    flags |= textflags::FREQ | textflags::INFO | textflags::TYPE;

  symbolPainter->drawVorText(context->painter, obj, x, y, flags,
                             context->symSize(context->mapLayer->getVorSymbolSize()), true, false);
}

void MapPainterRoute::paintNdb(const PaintContext *context, int x, int y, const maptypes::MapNdb& obj)
{
  symbolPainter->drawNdbSymbol(context->painter, obj, x, y,
                               context->symSize(context->mapLayer->getNdbSymbolSize()), true, false);
}

void MapPainterRoute::paintNdbText(const PaintContext *context, int x, int y, const maptypes::MapNdb& obj)
{
  textflags::TextFlags flags = textflags::ROUTE_TEXT;
  if(context->mapLayer->isNdbRouteIdent())
    flags |= textflags::IDENT;

  if(context->mapLayer->isNdbRouteInfo())
    flags |= textflags::FREQ | textflags::INFO | textflags::TYPE;

  symbolPainter->drawNdbText(context->painter, obj, x, y, flags,
                             context->symSize(context->mapLayer->getNdbSymbolSize()), true, false);
}

void MapPainterRoute::paintWaypoint(const PaintContext *context, const QColor& col, int x, int y,
                                    const maptypes::MapWaypoint& obj)
{
  symbolPainter->drawWaypointSymbol(context->painter, obj, col, x, y,
                                    context->symSize(context->mapLayer->getWaypointSymbolSize()), true, false);
}

void MapPainterRoute::paintWaypointText(const PaintContext *context, int x, int y,
                                        const maptypes::MapWaypoint& obj)
{
  textflags::TextFlags flags = textflags::ROUTE_TEXT;
  if(context->mapLayer->isWaypointRouteName())
    flags |= textflags::IDENT;

  symbolPainter->drawWaypointText(context->painter, obj, x, y, flags,
                                  context->symSize(context->mapLayer->getWaypointSymbolSize()), true, false);
}

void MapPainterRoute::paintUserpoint(const PaintContext *context, int x, int y)
{
  symbolPainter->drawUserpointSymbol(context->painter, x, y,
                                     context->symSize(
                                       context->mapLayer->getWaypointSymbolSize()), true, false);
}

void MapPainterRoute::paintText(const PaintContext *context, const QColor& color, int x, int y,
                                const QString& text)
{
  if(text.isEmpty())
    return;

  symbolPainter->textBox(context->painter, {text}, color,
                         x + context->symSize(context->mapLayer->getWaypointSymbolSize()) / 2 + 2,
                         y, textatt::BOLD | textatt::ROUTE_BG_COLOR, 255);
}
