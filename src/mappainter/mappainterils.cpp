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

#include "mappainter/mappainterils.h"

#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "query/airportquery.h"
#include "query/mapquery.h"
#include "geo/calculations.h"
#include "common/mapcolors.h"
#include "util/paintercontextsaver.h"
#include "route/route.h"

#include <QElapsedTimer>
#include <QStringBuilder>

#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;
using map::MapIls;

MapPainterIls::MapPainterIls(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainter(mapWidget, mapScale, paintContext)
{
}

MapPainterIls::~MapPainterIls()
{
}

void MapPainterIls::render()
{
  if(context->mapLayer->isIls())
  {
    context->startTimer("ILS");

    // Get ILS from flight plan which are also painted in the profile
    QVector<map::MapIls> routeIls;
    QSet<int> routeIlsIds;
    if(context->objectDisplayTypes.testFlag(map::FLIGHTPLAN))
    {
      routeIls = context->route->getDestRunwayIlsMap();
      for(const map::MapIls& ils : qAsConst(routeIls))
        routeIlsIds.insert(ils.id);
    }

    const GeoDataLatLonBox& curBox = context->viewport->viewLatLonAltBox();
    Marble::GeoPainter *painter = context->painter;

    int x, y;
    if((context->objectTypes.testFlag(map::ILS) || context->objectDisplayTypes.testFlag(map::GLS)) &&
       context->objectTypes.testFlag(map::AIRPORT))
    {
      bool overflow = false;
      const QList<MapIls> *ilsList = mapQuery->getIls(curBox, context->mapLayer, context->lazyUpdate, overflow);
      context->setQueryOverflow(overflow);

      if(ilsList != nullptr)
      {
        atools::util::PainterContextSaver saver(painter);

        for(const MapIls& ils : qAsConst(*ilsList))
        {
          if(routeIlsIds.contains(ils.id))
            // Part of flight plan - paint later
            continue;

          if(ils.isAnyGlsRnp() && !context->objectDisplayTypes.testFlag(map::GLS))
            continue;

          if(!ils.isAnyGlsRnp() && !context->objectTypes.testFlag(map::ILS))
            continue;

          // Need to get the real ILS size on the screen for Mercator projection - otherwise feather may vanish
          bool visible = wToS(ils.position, x, y, scale->getScreeenSizeForRect(ils.bounding));

          if(!visible)
            // Check bounding rect for visibility
            visible = ils.bounding.overlaps(context->viewportRect);

          if(visible)
          {
            if(context->objCount())
              return;

            // Check if airport is to be shown - hide ILS if not - show ILS if no airport ident in navaid
            if(!ils.airportIdent.isEmpty())
            {
              map::MapAirport airport = airportQuery->getAirportByIdent(ils.airportIdent);
              if(airport.isValid() && !airport.isVisible(context->objectTypes, context->mimimumRunwayLengthFt, context->mapLayer))
                continue;
            }

            drawIlsSymbol(ils, context->drawFast);
          }
        }
      }
    }

    // Paint ILS from approach
    for(const MapIls& ils : qAsConst(routeIls))
    {
      bool visible = wToS(ils.position, x, y, scale->getScreeenSizeForRect(ils.bounding));

      if(!visible)
        visible = ils.bounding.overlaps(context->viewportRect);

      if(visible)
        drawIlsSymbol(ils, context->drawFast);
    }
    context->endTimer("ILS");
  }
}

void MapPainterIls::drawIlsSymbol(const map::MapIls& ils, bool fast)
{
  Marble::GeoPainter *painter = context->painter;

  atools::util::PainterContextSaver saver(painter);

  if(!ils.hasGeometry)
    return;

  bool isIls = !ils.isAnyGlsRnp();

  QColor fillColor = isIls ? mapcolors::ilsFillColor : mapcolors::glsFillColor;
  QColor symColor = isIls ? mapcolors::ilsSymbolColor : mapcolors::glsSymbolColor;
  QColor textColor = isIls ? mapcolors::ilsTextColor : mapcolors::glsTextColor;
  QPen centerPen = isIls ? mapcolors::ilsCenterPen : mapcolors::glsCenterPen;

  painter->setBackgroundMode(Qt::TransparentMode);
  painter->setBrush(fast ? QBrush(Qt::transparent) : QBrush(fillColor));
  painter->setPen(QPen(symColor, 2, Qt::SolidLine, Qt::FlatCap));

  QSize size = scale->getScreeenSizeForRect(ils.bounding);
  bool visible;
  // Use visible dummy here since we need to call the method that also returns coordinates outside the screen
  QPointF pmid = wToSF(ils.posmid, size, &visible);
  QPointF origin = wToSF(ils.position, size, &visible);

  QPointF p1 = wToSF(ils.pos1, size, &visible);
  QPointF p2 = wToSF(ils.pos2, size, &visible);

  if(context->mapLayer->isIlsDetail())
  {
    if(!isIls)
      painter->drawPolygon(QPolygonF({origin, p1, p2, origin}));
    else if(ils.hasGlideslope())
    {
      painter->drawPolygon(QPolygonF({origin, p1, p2, origin}));
      painter->drawPolyline(QPolygonF({p1, pmid, p2}));
    }
    else
      painter->drawPolygon(QPolygonF({origin, p1, pmid, p2, origin}));
  }
  else
    // Simplified polygon
    painter->drawPolygon(QPolygonF({origin, p1, p2, origin}));

  if(!context->drawFast)
  {
    if(context->mapLayer->isIlsDetail())
    {
      // Draw dashed center line
      painter->setPen(centerPen);

      if(isIls)
        painter->drawLine(origin, pmid);
      else
        painter->drawLine(origin, QLineF(p1, p2).center());
    }

    // Draw ILS text -----------------------------------
    QString text;
    if(context->mapLayer->isIlsInfo())
      text = map::ilsText(ils);
    else if(context->mapLayer->isIlsIdent())
      text = ils.ident;

    if(!text.isEmpty())
    {
      // Add space at start and end to avoid letters touching the background rectangle border
      text = " " % text % " ";

      context->szFont(context->textSizeNavaid);
      painter->setPen(QPen(textColor, 0.5f, Qt::SolidLine, Qt::FlatCap));
      painter->translate(origin);

      // Position GLS and RNP on the botton and ILS on the top of the feather
      float width = ils.localizerWidth();
      if(ils.isAnyGlsRnp())
        width = -width;

      // Rotate to draw the text upwards so it is readable
      float rotate = ils.displayHeading > 180.f ?
                     ils.displayHeading + 90.f - width / 2.f :
                     atools::geo::opposedCourseDeg(ils.displayHeading) + 90.f + width / 2.f;

      // get an approximation of the ILS length
      int featherLen = static_cast<int>(std::roundf(scale->getPixelForMeter(nmToMeter(FEATHER_LEN_NM), rotate)));

      if(featherLen > MIN_LENGHT_FOR_TEXT)
      {
        if(context->flags2 & opts2::MAP_NAVAID_TEXT_BACKGROUND)
        {
          painter->setBackground(Qt::white);
          painter->setBackgroundMode(Qt::OpaqueMode);
        }

        QFontMetricsF metrics(painter->font());
        double texth = ils.isAnyGlsRnp() ? metrics.height() : -metrics.descent();

        // Cut text to feather length
        text = metrics.elidedText(text, Qt::ElideRight, featherLen);
        double textw = metrics.horizontalAdvance(text);
        double textpos = ils.displayHeading > 180. ? (featherLen - textw) / 2. : -(featherLen + textw) / 2.;

        painter->rotate(rotate);
        painter->drawText(QPointF(textpos, texth), text);
        painter->resetTransform();
      }
    }
  }
}
