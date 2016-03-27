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

#include "mappainternav.h"
#include "symbolpainter.h"

#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "mapgui/mapquery.h"
#include "geo/calculations.h"
#include "common/maptypes.h"
#include "common/mapcolors.h"

#include <QElapsedTimer>

#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

using namespace Marble;
using namespace atools::geo;
using namespace maptypes;

MapPainterNav::MapPainterNav(Marble::MarbleWidget *widget, MapQuery *mapQuery, MapScale *mapScale,
                             bool verboseMsg)
  : MapPainter(widget, mapQuery, mapScale, verboseMsg)
{
}

MapPainterNav::~MapPainterNav()
{
}

void MapPainterNav::paint(const PaintContext *context)
{

  if(context->mapLayer == nullptr)
    return;

  bool drawFast = widget->viewContext() == Marble::Animation;

  const GeoDataLatLonAltBox& curBox = context->viewport->viewLatLonAltBox();
  QElapsedTimer t;
  t.start();

  setRenderHints(context->painter);

  if(context->mapLayer->isAirway() &&
     (context->objectTypes.testFlag(maptypes::AIRWAYJ) || context->objectTypes.testFlag(maptypes::AIRWAYV)))
  {
    const QList<MapAirway> *airways = query->getAirways(curBox, context->mapLayer, drawFast);
    if(airways != nullptr)
    {
      if(widget->viewContext() == Marble::Still && verbose)
      {
        qDebug() << "Number of airways" << airways->size();
        qDebug() << "Time for query" << t.elapsed() << " ms";
        qDebug() << curBox.toString();
        qDebug() << *context->mapLayer;
        t.restart();
      }

      QList<GeoDataCoordinates> textCoords;
      QList<qreal> textBearing;

      for(const MapAirway& airway : *airways)
      {
        if(airway.type == "JET" && !context->objectTypes.testFlag(maptypes::AIRWAYJ))
          continue;
        if(airway.type == "VICTOR" && !context->objectTypes.testFlag(maptypes::AIRWAYV))
          continue;

        if(airway.type == "VICTOR")
          context->painter->setPen(QPen(QColor::fromRgb(150, 150, 150), 1.5));
        else
          context->painter->setPen(QPen(QColor::fromRgb(100, 100, 100), 1.5));

        int x1, y1, x2, y2;
        bool visible1 = wToS(airway.from, x1, y1);
        bool visible2 = wToS(airway.to, x2, y2);

        if(!visible1 && !visible2)
        {
          GeoDataLatLonBox airwaybox(airway.bounding.getNorth(), airway.bounding.getSouth(),
                                     airway.bounding.getEast(), airway.bounding.getWest(),
                                     GeoDataCoordinates::Degree);
          visible1 = airwaybox.intersects(curBox);
        }

        if(visible1 || visible2)
        {
          GeoDataCoordinates from(airway.from.getLonX(), airway.from.getLatY(), 0,
                                  GeoDataCoordinates::Degree);
          GeoDataCoordinates to(airway.to.getLonX(), airway.to.getLatY(), 0,
                                GeoDataCoordinates::Degree);
          GeoDataLineString line;
          line.setTessellate(true);
          line << from << to;

          qreal init = normalizeCourse(from.bearing(to, GeoDataCoordinates::Degree,
                                                    GeoDataCoordinates::InitialBearing));
          qreal final = normalizeCourse(from.bearing(to, GeoDataCoordinates::Degree,
                                                     GeoDataCoordinates::FinalBearing));
          textBearing.append((init + final) / 2.);
          textCoords.append(from.interpolate(to, 0.5));
          context->painter->drawPolyline(line);
        }
        else
        {
          textBearing.append(0.);
          textCoords.append(GeoDataCoordinates());
        }
      }

      int x, y;
      // Draw airway text along lines
      context->painter->setPen(QColor::fromRgb(80, 80, 80));
      int i = 0;
      for(const GeoDataCoordinates& coord : textCoords)
      {
        if(textCoords.at(i).isValid() && wToS(coord, x, y))
        {
          const MapAirway& airway = airways->at(i);
          QString text;

          if(context->mapLayer->isAirwayIdent())
            text += airway.name;

          if(context->mapLayer->isAirwayInfo())
            text += QString("/") + airway.type.at(0) + "/" +
                    QString::number(airway.fragment) + "/" + QString::number(airway.sequence);

          qreal rotate, brg = textBearing.at(i);
          if(brg > 180.)
            rotate = brg + 90.;
          else
            rotate = brg - 90.;

          context->painter->translate(x, y);
          context->painter->rotate(rotate);
          context->painter->drawText(-context->painter->fontMetrics().width(text) / 2,
                                     context->painter->fontMetrics().ascent(), text);
          context->painter->resetTransform();
        }
        i++;
      }
    }
  }

  if(context->mapLayer->isWaypoint() && context->objectTypes.testFlag(maptypes::WAYPOINT))
  {
    const QList<MapWaypoint> *waypoints = query->getWaypoints(curBox, context->mapLayer, drawFast);
    if(waypoints != nullptr)
    {
      if(widget->viewContext() == Marble::Still && verbose)
      {
        qDebug() << "Number of waypoints" << waypoints->size();
        qDebug() << "Time for query" << t.elapsed() << " ms";
        qDebug() << curBox.toString();
        qDebug() << *context->mapLayer;
        t.restart();
      }

      for(const MapWaypoint& waypoint : *waypoints)
      {
        int x, y;
        bool visible = wToS(waypoint.position, x, y);

        if(visible)
          symbolPainter->drawWaypointSymbol(context->painter, waypoint, QColor(), x, y,
                                            context->mapLayer->getWaypointSymbolSize(), false, drawFast);

        if(context->mapLayer->isWaypointName())
          symbolPainter->drawWaypointText(context->painter, waypoint, x, y, textflags::IDENT,
                                          context->mapLayer->getWaypointSymbolSize(),
                                          false, drawFast);
      }
    }
  }

  if(context->mapLayer->isVor() && context->objectTypes.testFlag(maptypes::VOR))
  {
    const QList<MapVor> *vors = query->getVors(curBox, context->mapLayer, drawFast);
    if(vors != nullptr)
    {
      if(widget->viewContext() == Marble::Still && verbose)
      {
        qDebug() << "Number of vors" << vors->size();
        qDebug() << "Time for query" << t.elapsed() << " ms";
        qDebug() << curBox.toString();
        qDebug() << *context->mapLayer;
        t.restart();
      }

      for(const MapVor& vor : *vors)
      {
        int x, y;
        bool visible = wToS(vor.position, x, y);

        if(visible)
        {
          symbolPainter->drawVorSymbol(context->painter, vor, x, y,
                                       context->mapLayer->getVorSymbolSize(), false, drawFast,
                                       context->mapLayer->isVorLarge() ? context->mapLayer->getVorSymbolSize()
                                       * 5 : 0);

          textflags::TextFlags flags;

          if(context->mapLayer->isVorInfo())
            flags = textflags::IDENT | textflags::TYPE | textflags::FREQ;
          else if(context->mapLayer->isVorIdent())
            flags = textflags::IDENT;

          symbolPainter->drawVorText(context->painter, vor, x, y,
                                     flags, context->mapLayer->getVorSymbolSize(), false, drawFast);
        }
      }
    }
  }

  if(context->mapLayer->isNdb() && context->objectTypes.testFlag(maptypes::NDB))
  {
    const QList<MapNdb> *ndbs = query->getNdbs(curBox, context->mapLayer, drawFast);
    if(ndbs != nullptr)
    {
      if(widget->viewContext() == Marble::Still && verbose)
      {
        qDebug() << "Number of ndbs" << ndbs->size();
        qDebug() << "Time for query" << t.elapsed() << " ms";
        qDebug() << curBox.toString();
        qDebug() << *context->mapLayer;
        t.restart();
      }

      for(const MapNdb& ndb : *ndbs)
      {
        int x, y;
        bool visible = wToS(ndb.position, x, y);

        if(visible)
        {
          symbolPainter->drawNdbSymbol(context->painter, ndb, x, y,
                                       context->mapLayer->getNdbSymbolSize(), false, drawFast);

          textflags::TextFlags flags;

          if(context->mapLayer->isNdbInfo())
            flags = textflags::IDENT | textflags::TYPE | textflags::FREQ;
          else if(context->mapLayer->isNdbIdent())
            flags = textflags::IDENT;

          symbolPainter->drawNdbText(context->painter, ndb, x, y,
                                     flags, context->mapLayer->getNdbSymbolSize(), false, drawFast);
        }
      }
    }
  }

  if(context->mapLayer->isMarker() && context->objectTypes.testFlag(maptypes::ILS))
  {
    const QList<MapMarker> *markers = query->getMarkers(curBox, context->mapLayer, drawFast);
    if(markers != nullptr)
    {
      if(widget->viewContext() == Marble::Still && verbose)
      {
        qDebug() << "Number of marker" << markers->size();
        qDebug() << "Time for query" << t.elapsed() << " ms";
        qDebug() << curBox.toString();
        qDebug() << *context->mapLayer;
        t.restart();
      }

      for(const MapMarker& marker : *markers)
      {
        int x, y;
        bool visible = wToS(marker.position, x, y);

        if(visible)
        {
          symbolPainter->drawMarkerSymbol(context->painter, marker, x, y,
                                          context->mapLayer->getNdbSymbolSize(), drawFast);

          if(context->mapLayer->isMarkerInfo())
          {
            QString type = marker.type.toLower();
            type[0] = type.at(0).toUpper();
            x -= context->mapLayer->getMarkerSymbolSize() / 2 + 2;
            symbolPainter->textBox(context->painter, {type}, mapcolors::markerSymbolColor, x, y,
                                   textatt::BOLD | textatt::RIGHT, 0);
          }
        }
      }
    }
  }

  if(widget->viewContext() == Marble::Still && verbose)
    qDebug() << "Time for paint" << t.elapsed() << " ms";
}
