/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#include "symbolpainter.h"

#include "common/mapcolors.h"
#include "common/maptypes.h"
#include "common/unit.h"
#include "fs/util/fsutil.h"
#include "fs/weather/metarparser.h"
#include "geo/calculations.h"
#include "options/optiondata.h"
#include "textpointer.h"
#include "util/paintercontextsaver.h"

#include <QPainter>
#include <QStringBuilder>

using namespace Marble;
using namespace map;
using atools::geo::angleToQt;
using atools::fs::util::roundComFrequency;

QIcon SymbolPainter::createAirportIcon(const map::MapAirport& airport, int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  SymbolPainter().drawAirportSymbol(&painter, airport, size / 2.f, size / 2.f, size * 7.f / 10.f, false, false, false);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createAirportWeatherIcon(const atools::fs::weather::MetarParser& metar, int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  SymbolPainter().drawAirportWeather(&painter, metar, size / 2.f, size / 2.f, size * 7.f / 10.f,
                                     false /*windPointer*/, false /*windBarbs*/, false /*fast*/);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createAirportMsaIcon(const map::MapAirportMsa& airportMsa, const QFont& font, float symbolScale, int *actualSize)
{
  // Create a temporary pixmap to get the font size
  int size = 100;
  QPixmap pixmapTemp(size, size);
  QPainter painterTemp(&pixmapTemp);
  prepareForIcon(painterTemp);
  painterTemp.setFont(font);

  // Get pixel size based on scale
  size = airportMsaSize(&painterTemp, airportMsa, symbolScale, true);

  // Prepare final drawing canvas
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  if(actualSize != nullptr)
    *actualSize = size;

  SymbolPainter().drawAirportMsa(&painter, airportMsa, size / 2.f, size / 2.f, 0.f, symbolScale,
                                 false /* header */, false /* transparency */, false /* fast */);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createVorIcon(const map::MapVor& vor, int size, bool darkMap)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  SymbolPainter().drawVorSymbol(&painter, vor, size / 2.f, size / 2.f, size * 7.f / 10.f, 0.f,
                                false /* routeFill */, false /* fast */, darkMap);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createNdbIcon(int size, bool darkMap)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  SymbolPainter().drawNdbSymbol(&painter, size / 2.f, size / 2.f, size * 8.f / 10.f, false /* routeFill */, false /* fast */, darkMap);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createAirwayIcon(const map::MapAirway& airway, int size, bool darkMap)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  painter.setPen(QPen(mapcolors::colorForAirwayOrTrack(airway, darkMap), 1.5));

  painter.drawLine(0, 0, size, size);

  return QIcon(pixmap);
}

QIcon SymbolPainter::createWaypointIcon(int size, const QColor& color)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  SymbolPainter().drawWaypointSymbol(&painter, color, size / 2.f, size / 2.f, size / 2.f, false);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createUserpointIcon(int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  SymbolPainter().drawUserpointSymbol(&painter, size / 2.f, size / 2.f, size / 2.f, false);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createProcedurePointIcon(int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  SymbolPainter().drawProcedureSymbol(&painter, size / 2.f, size / 2.f, size / 2.f, false);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createAirspaceIcon(const map::MapAirspace& airspace, int size, int lineThickness, int transparency)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  painter.setBackgroundMode(Qt::TransparentMode);
  painter.setPen(mapcolors::penForAirspace(airspace, lineThickness));
  painter.setBrush(mapcolors::colorForAirspaceFill(airspace, transparency));
  painter.drawEllipse(2, 2, size - 4, size - 4);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createProcedurePreviewIcon(const QColor& color, int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  painter.setBackgroundMode(Qt::OpaqueMode);
  painter.setPen(color);
  painter.setBrush(color);
  painter.drawEllipse(4, 4, size - 8, size - 8);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createHelipadIcon(const MapHelipad& helipad, int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  painter.setBackgroundMode(Qt::TransparentMode);
  drawHelipadSymbol(&painter, helipad, size / 2.f, size / 2.f, size * 0.4f, size * 0.4f, false);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createAircraftTrailIcon(int size, const QPen& pen)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  double margins = pen.widthF();

  painter.setBackgroundMode(Qt::OpaqueMode);
  painter.setPen(pen);
  painter.setBrush(Qt::transparent);
  painter.drawLine(QPointF(margins, margins), QPointF(size - margins, size - margins));
  return QIcon(pixmap);
}

void SymbolPainter::drawHelipadSymbol(QPainter *painter, const map::MapHelipad& helipad, float x, float y, float w, float h, bool fast)
{
  painter->setBrush(mapcolors::colorForSurface(helipad.surface));
  painter->setPen(QPen(mapcolors::helipadOutlineColor, 2., Qt::SolidLine, Qt::FlatCap));

  painter->translate(QPointF(x, y));
  painter->rotate(helipad.heading);

  if(helipad.type == "SQUARE" || helipad.type == "MEDICAL")
    painter->drawRect(QRectF(-w, -h, w * 2., h * 2.));
  else
    painter->drawEllipse(QRectF(-w, -h, w * 2., h * 2.));

  if(!fast)
  {
    if(helipad.type == "MEDICAL")
      painter->setPen(QPen(mapcolors::helipadMedicalOutlineColor, 3, Qt::SolidLine, Qt::FlatCap));

    // Draw the H symbol
    painter->drawLine(QPointF(-w / 3., -h / 2.), QPointF(-w / 3., h / 2.));
    painter->drawLine(QPointF(-w / 3, 0.), QPointF(w / 3., 0.));
    painter->drawLine(QPointF(w / 3., -h / 2.), QPointF(w / 3., h / 2.));

    if(helipad.closed)
    {
      // Cross out
      painter->drawLine(QPointF(-w, -w), QPointF(w, w));
      painter->drawLine(QPointF(-w, w), QPointF(w, -w));
    }
  }
  painter->resetTransform();
}

void SymbolPainter::drawAirportSymbol(QPainter *painter, const map::MapAirport& airport,
                                      float x, float y, float size, bool isAirportDiagram, bool fast, bool addonHighlight)
{
  if(airport.longestRunwayLength == 0 && !airport.helipad())
    // Reduce size for airports without runways and without helipads
    size = size * 4.f / 5.f;

  atools::util::PainterContextSaver saver(painter);

  painter->setBackgroundMode(Qt::OpaqueMode);
  float radius = size / 2.f;

  if(airport.addon() && addonHighlight)
  {
    // Draw addon underlay ==========================
    painter->setBrush(mapcolors::addonAirportBackgroundColor);
    painter->setPen(QPen(mapcolors::addonAirportFrameColor));

    float addonRadius = radius + atools::minmax(3.f, 6.f, radius);
    painter->drawEllipse(QPointF(x, y), addonRadius, addonRadius);
  }

  QColor apColor = mapcolors::colorForAirport(airport);

  if(airport.flags.testFlag(AP_HARD) && !airport.flags.testFlag(AP_MIL) && !airport.flags.testFlag(AP_CLOSED))
    // Use filled circle
    painter->setBrush(QBrush(apColor));
  else
    // Use white filled circle
    painter->setBrush(QBrush(mapcolors::airportSymbolFillColor));

  if((!fast || isAirportDiagram) && size > 5)
  {
    // Draw spikes only for larger symbols
    if(airport.anyFuel() && !airport.flags.testFlag(AP_MIL) && !airport.flags.testFlag(AP_CLOSED) && size > 6.f)
    {
      // Draw fuel spikes
      float fuelRadius = radius * 1.4f;
      if(fuelRadius < radius + 2.f)
        fuelRadius = radius + 2.f;
      painter->setPen(QPen(QBrush(apColor), size / 4.f, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(QPointF(x, y - fuelRadius), QPointF(x, y + fuelRadius));
      painter->drawLine(QPointF(x - fuelRadius, y), QPointF(x + fuelRadius, y));
    }
  }

  painter->setPen(QPen(QBrush(apColor), size / 5.f, Qt::SolidLine, Qt::FlatCap));
  painter->drawEllipse(QPointF(x, y), radius, radius);

  if((!fast || isAirportDiagram) && size > 5.f)
  {
    if(airport.flags.testFlag(AP_MIL))
      // Military airport
      painter->drawEllipse(QPointF(x, y), radius / 2.f, radius / 2.f);

    if(airport.waterOnly() && size > 6.f)
    {
      // Water only runways - draw an anchor
      float lineWidth = size / 7.f;
      painter->setPen(QPen(QBrush(apColor), lineWidth, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(QPointF(x, y - radius / 2.f), QPointF(x, y + radius / 2.f));
      painter->drawArc(QRectF(x - radius / 2.f, y - radius / 2.f, radius, radius),
                       0 * 16, // From
                       -180 * 16); // to
    }

    if(airport.helipadOnly() && size > 6.f)
    {
      // Only helipads - draw an H
      float lineWidth = size / 7.f;
      painter->setPen(QPen(QBrush(apColor), lineWidth, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(QLineF(x - size / 5.f, y - radius / 2.f,
                               x - size / 5.f, y + radius / 2.f));

      painter->drawLine(QLineF(x - size / 5.f, y,
                               x + size / 5.f, y));

      painter->drawLine(QLineF(x + size / 5.f, y - radius / 2.f,
                               x + size / 5.f, y + radius / 2.f));
    }

    if(airport.flags.testFlag(AP_CLOSED) && size > 6.f)
    {
      // Cross out whatever was painted before
      painter->setPen(QPen(QBrush(apColor), size / 6.f, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(QLineF(x - radius, y - radius, x + radius, y + radius));
      painter->drawLine(QLineF(x - radius, y + radius, x + radius, y - radius));
    }
  }

  if((!fast || isAirportDiagram) && size > 5.f)
  {
    if(airport.flags.testFlag(AP_HARD) && !airport.flags.testFlag(AP_MIL) &&
       !airport.flags.testFlag(AP_CLOSED) && size > 6)
    {
      // Draw line inside circle
      painter->translate(x, y);
      painter->rotate(airport.longestRunwayHeading);
      painter->setPen(QPen(QBrush(mapcolors::airportSymbolFillColor), size / 5.f, Qt::SolidLine, Qt::RoundCap));
      painter->drawLine(QLineF(0, -radius + 2, 0, radius - 2));
      painter->resetTransform();
    }
  }
}

void SymbolPainter::drawWaypointSymbol(QPainter *painter, const QColor& col, float x, float y, float size, bool fill)
{
  atools::util::PainterContextSaver saver(painter);
  painter->setBackgroundMode(Qt::TransparentMode);
  if(fill)
    painter->setBrush(mapcolors::routeTextBoxColor);
  else
    painter->setBrush(Qt::NoBrush);

  float lineWidth = std::max(size / 6.f, 1.5f);

  QColor color = col.isValid() ? col : mapcolors::waypointSymbolColor;
  double radius = size / 2.;

  // Draw a triangle
  QPolygonF polygon;
  polygon << QPointF(x, y - radius) << QPointF(x + radius, y + radius) << QPointF(x - radius, y + radius);

  if(size > 4)
    painter->setPen(QPen(color, lineWidth, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
  else
  {
    painter->setPen(QPen(color, 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    painter->setBrush(color);
  }
  painter->drawConvexPolygon(polygon);
}

void SymbolPainter::drawAirportWeather(QPainter *painter, const atools::fs::weather::MetarParser& metar, float x, float y,
                                       float size, bool windPointer, bool windBarbs, bool fast)
{
  if(metar.isParsed() || metar.hasErrors())
  {
    // Determine correct color for flight rules (IFR, etc.) =============================================
    atools::fs::weather::MetarParser::FlightRules flightRules = metar.getFlightRules();
    atools::util::PainterContextSaver saver(painter);

    painter->setBackgroundMode(Qt::OpaqueMode);
    painter->setBackground(mapcolors::weatherBackgoundColor);
    painter->setBrush(mapcolors::weatherBackgoundColor);

    // Rectangle for all drawing centered around x and y
    QRectF rect(x - size / 2.f, y - size / 2.f, size, size);

    // Draw while outline / background circle ===================================
    painter->setPen(mapcolors::weatherBackgoundColor);
    float margin = size / 5.f;
    painter->drawEllipse(rect.marginsAdded(QMarginsF(margin, margin, margin, margin)));

    // Wind pointer and/or barbs =====================================================
    if((windBarbs || windPointer) && !metar.hasErrors())
      drawWindBarbs(painter, metar, x, y, size, windBarbs, false /* altWind */, false /* route */, fast);

    // Draw coverage indicating pies or circles =====================================================

    // Color depending on flight rule
    QColor color;
    if(metar.hasErrors())
      color = mapcolors::weatherErrorColor;
    else
    {
      switch(flightRules)
      {
        case atools::fs::weather::MetarParser::UNKNOWN:
          break;
        case atools::fs::weather::MetarParser::LIFR:
          color = mapcolors::weatherLifrColor;
          break;
        case atools::fs::weather::MetarParser::IFR:
          color = mapcolors::weatherIfrColor;
          break;
        case atools::fs::weather::MetarParser::MVFR:
          color = mapcolors::weatherMvfrColor;
          break;
        case atools::fs::weather::MetarParser::VFR:
          color = mapcolors::weatherVfrColor;
          break;
      }
    }

    float lineWidth = size * 0.2f;
    painter->setPen(QPen(color, lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    if(metar.hasErrors())
    {
      painter->drawEllipse(rect);
      if(!fast)
      {
        // Cross out for no information
        painter->drawLine(QLineF(x - size / 2.f, y - size / 2.f, x + size / 2.f, y + size / 2.f));
        painter->drawLine(QLineF(x + size / 2.f, y - size / 2.f, x - size / 2.f, y + size / 2.f));
      }
    }
    else
    {
      atools::fs::weather::MetarCloud::Coverage maxCoverage = metar.getMaxCoverage();
      switch(maxCoverage)
      {
        case atools::fs::weather::MetarCloud::COVERAGE_NIL:
          painter->drawEllipse(rect);
          if(!fast)
          {
            // Cross out for no information
            painter->drawLine(QLineF(x - size / 2.f, y - size / 2.f, x + size / 2.f, y + size / 2.f));
            painter->drawLine(QLineF(x + size / 2.f, y - size / 2.f, x - size / 2.f, y + size / 2.f));
          }
          break;

        case atools::fs::weather::MetarCloud::COVERAGE_CLEAR:
          painter->drawEllipse(rect);
          break;

        case atools::fs::weather::MetarCloud::COVERAGE_FEW:
          painter->drawEllipse(rect);
          if(!fast)
          {
            painter->setBrush(color);
            painter->setPen(Qt::NoPen);
            painter->drawPie(rect, -270 * 16, -90 * 16);
          }
          break;

        case atools::fs::weather::MetarCloud::COVERAGE_SCATTERED:
          painter->drawEllipse(rect);
          if(!fast)
          {
            painter->setBrush(color);
            painter->setPen(Qt::NoPen);
            painter->drawPie(rect, -270 * 16, -180 * 16);
          }
          break;

        case atools::fs::weather::MetarCloud::COVERAGE_BROKEN:
          painter->drawEllipse(rect);
          if(!fast)
          {
            painter->setBrush(color);
            painter->setPen(Qt::NoPen);
            painter->drawPie(rect, -270 * 16, -270 * 16);
          }
          break;

        case atools::fs::weather::MetarCloud::COVERAGE_OVERCAST:
          painter->setBrush(color);
          painter->drawEllipse(QPointF(x, y), size / 2.f, size / 2.f);
          break;
      }
    }
  }
}

void SymbolPainter::drawWindBarbs(QPainter *painter, const atools::fs::weather::MetarParser& parsedMetar,
                                  float x, float y, float size, bool windBarbs, bool altWind, bool route, bool fast) const
{
  drawWindBarbs(painter, parsedMetar.getPrevailingWindSpeedKnots(), parsedMetar.getGustSpeedKts(),
                parsedMetar.getPrevailingWindDir(), x, y, size, windBarbs, altWind, route, fast);
}

void SymbolPainter::drawWindBarbs(QPainter *painter, float wind, float gust, float dir,
                                  float x, float y, float size, bool windBarbs, bool altWind, bool route, bool fast) const
{
  // Make lines thinner for high altitude wind barbs
  float lineWidth = size * (altWind ? 0.2f : 0.3f);
  float bgLineWidth = lineWidth * 2.5f;

  // Use yellow route color background for wind barbs at flight plan waypoints
  QColor background = route ? mapcolors::routeTextBoxColor : mapcolors::weatherBackgoundColor;

  if(altWind)
  {
    // High altitude wind barbs only - center circle
    painter->setPen(QPen(background, bgLineWidth * .6f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(background);
    painter->drawEllipse(QPointF(x, y), bgLineWidth / 2.f, bgLineWidth / 2.f);
  }

  if(wind >= 2.f && wind < map::INVALID_METAR_VALUE / 2.f &&
     dir >= 0.f && dir < map::INVALID_METAR_VALUE / 2.f)
  {
    // Calculate dimensions ============================

    float barbLength50 = size * 0.7f;
    float barbLength10 = barbLength50;
    float barbLength5 = barbLength10 * 0.65f;
    float barbStep = lineWidth * 1.5f;

    float lineLength = size;
    QVector<int> barbs, barbsGust, *barbsBackground = nullptr;

    lineLength = size;

    // Calculate a list of feathers ========================================
    if(windBarbs && !fast) // Otherwise pointer only
    {
      // Normal wind barbs
      barbs = calculateWindBarbs(lineLength, lineWidth, wind, true /* use 50 knot barbs */);

      float windGust = gust;
      if(windGust >= 2.f && windGust < map::INVALID_METAR_VALUE / 2.f)
      {
        // Gust wind barbs
        lineLength = size;
        barbsGust =
          calculateWindBarbs(lineLength, lineWidth, windGust,
                             barbs.contains(50) /* use 50 knot barbs only if there are ones in the normal wind*/);
        barbsBackground = &barbsGust;
      }
      else
        barbsBackground = &barbs;
    }

    // Draw wind pointer background ============================================================

    QLineF line(0., 0., 0., -lineLength);
    painter->setPen(QPen(background, bgLineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->translate(QPointF(x, y));
    painter->rotate(dir);
    // Line from 0 to 0 - length
    painter->drawLine(line);

    if(windBarbs && !fast)
    {
      // Draw wind barbs background ============================================================
      if(barbsBackground != nullptr && !barbsBackground->isEmpty())
      {
        painter->setPen(QPen(background, bgLineWidth,
                             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(background);
        drawBarbFeathers(painter, *barbsBackground, lineLength, barbLength5, barbLength10, barbLength50, barbStep);
      }

      // Draw wind barbs ============================================================
      if(!barbsGust.isEmpty())
      {
        painter->setPen(QPen(mapcolors::weatherWindGustColor, lineWidth * 0.6f,
                             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(mapcolors::weatherWindGustColor);
        drawBarbFeathers(painter, barbsGust, lineLength, barbLength5, barbLength10, barbLength50, barbStep);
      }

      // Draw wind barbs ============================================================
      if(!barbs.isEmpty())
      {
        painter->setPen(QPen(mapcolors::weatherWindColor, lineWidth * 0.6f,
                             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(mapcolors::weatherWindColor);
        drawBarbFeathers(painter, barbs, lineLength, barbLength5, barbLength10, barbLength50, barbStep);
      }
    }

    // Draw wind pointer  ============================================================
    painter->setPen(QPen(mapcolors::weatherWindColor, lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(background);
    painter->drawLine(line);
    painter->resetTransform();

  }

  if(altWind)
  {
    // High altitude wind barbs only - center circle
    painter->setPen(QPen(mapcolors::weatherWindColor, lineWidth * 0.6f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->drawEllipse(QPointF(x, y), lineWidth, lineWidth);
  }
}

void SymbolPainter::drawBarbFeathers(QPainter *painter, const QVector<int>& barbs, float lineLength, float barbLength5,
                                     float barbLength10, float barbLength50, float barbStep) const
{
  if(!barbs.isEmpty())
  {
    // Lenghten the line for the rectangle
    float barbPos = barbs.constFirst() == 50 ? -lineLength + barbLength50 / 2.f : -lineLength;
    for(int barb : barbs)
    {
      if(barb == 50)
        painter->drawPolygon(QPolygonF({QPointF(0.f, barbPos),
                                        QPointF(-barbLength50, barbPos - barbLength50 / 2.f),
                                        QPointF(0.f, barbPos - barbLength50 / 2.f)}));
      else if(barb == 10)
        painter->drawLine(QLineF(0.f, barbPos, -barbLength10, barbPos - barbLength10 / 2.f));
      else if(barb == 5)
        painter->drawLine(QLineF(0.f, barbPos, -barbLength5, barbPos - barbLength5 / 2.f));

      barbPos += barbStep;
    }
  }
}

QVector<int> SymbolPainter::calculateWindBarbs(float& lineLength, float lineWidth, float wind, bool useBarb50) const
{
  QVector<int> barbs;

  float barbStep = lineWidth * 1.5f;

  // Calculate wind barb types and put then in order from outside towards circle  ===================================
  float tempWind = wind;
  while(tempWind >= 5.f)
  {
    if(tempWind >= 50.f && useBarb50)
    {
      tempWind -= 50.f;
      barbs.append(50);
    }
    else if(tempWind >= 10.f)
    {
      tempWind -= 10.f;
      barbs.append(10);
    }
    else if(tempWind >= 5.f)
    {
      tempWind -= 5.f;
      barbs.append(5);
      break;
    }
  }

  lineLength += barbs.size() * barbStep;

  return barbs;
}

void SymbolPainter::drawWindPointer(QPainter *painter, float x, float y, float size, float dir)
{
  atools::util::PainterContextSaver saver(painter);
  painter->setBackgroundMode(Qt::TransparentMode);

  painter->translate(x, y + size / 2.f);
  painter->rotate(atools::geo::normalizeCourse(dir + 180.f));
  painter->drawPixmap(QPointF(-size / 2.f, -size / 2.f), *windPointerFromCache(atools::roundToInt(size)));
  painter->resetTransform();
}

int SymbolPainter::airportMsaSize(QPainter *painter, const map::MapAirportMsa& airportMsa, float sizeFactor, bool drawDetails)
{
  return painter->fontMetrics().horizontalAdvance("0000") *
         atools::roundToInt(airportMsa.altitudes.size() > 1 || !drawDetails ? sizeFactor : sizeFactor / 2.f);
}

void SymbolPainter::drawAirportMsa(QPainter *painter, const map::MapAirportMsa& airportMsa, float x, float y, float size, float symbolScale,
                                   bool header, bool transparency, bool fast)
{
  atools::util::PainterContextSaver saver(painter);

  // Opaque fill
  QColor opaqueFillColor = mapcolors::msaFillColor;
  opaqueFillColor.setAlpha(255);

  painter->setPen(QPen(mapcolors::msaSymbolColor, 2, Qt::SolidLine, Qt::FlatCap));
  painter->setBackground(transparency ? mapcolors::msaFillColor : opaqueFillColor);
  painter->setBrush(transparency ? mapcolors::msaFillColor : opaqueFillColor);
  painter->setBackgroundMode(Qt::OpaqueMode);

  bool drawDetails = symbolScale > 0.f;

  if(size <= 0.f)
    size = airportMsaSize(painter, airportMsa, symbolScale, drawDetails);

  float radius = size / 2.f;

  // Outer circle with semi-transparent fill
  painter->drawEllipse(QPointF(x, y), radius - 2.f, radius - 2.f);

  if(!fast)
  {
    painter->setBackground(opaqueFillColor);

    if(header && drawDetails)
    {
      // Draw a header label =============================================
      QString heading = tr("MSA %1 %2 (%3, %4)").
                        arg(airportMsa.navIdent).
                        arg(Unit::distNm(airportMsa.radius, true, true)).
                        arg(airportMsa.trueBearing ? tr("°T") : tr("°M")).
                        arg(Unit::getUnitAltStr());

      QRectF bounding = QFontMetricsF(painter->font()).boundingRect(heading);
      QPointF pt(x - bounding.size().width() / 2., y - radius);
      painter->drawText(pt, heading);

      bounding.translate(pt); // Move rect to origin
      painter->setBrush(Qt::transparent);
      painter->drawRect(bounding);
      painter->setBrush(opaqueFillColor);
    }

    // Draw bearing lines and bearing degree labels ======================================
    float magvar = airportMsa.trueBearing ? 0.f : airportMsa.magvar;
    if(airportMsa.altitudes.size() > 1)
    {
      for(int i = 0; i < airportMsa.bearings.size(); i++)
      {
        float bearing = airportMsa.bearings.at(i);
        QString text = tr("%1°").arg(atools::geo::normalizeCourse(bearing));

        // Convert from bearing to angle
        bearing = atools::geo::normalizeCourse(bearing + 180.f + magvar);

        if(bearing < 180.f)
          text.prepend(TextPointer::getPointerLeft());
        else
          text.append(TextPointer::getPointerRight());

        // Line from center to top
        QLineF line(x, y, x, y - radius + 2.f);
        line.setAngle(atools::geo::angleToQt(bearing));

        // Draw sector line
        painter->drawLine(line);

        if(drawDetails)
        {
          // Move origin to label position and draw text
          QSizeF txtsize = QFontMetricsF(painter->font()).boundingRect(text).size();
          painter->translate(line.center());
          painter->rotate(bearing < 180.f ? bearing - 90.f : bearing + 90.f); // Keep text upwards
          painter->drawText(QPointF(-txtsize.width() / 2., txtsize.height() / 3.), text);
          painter->resetTransform();
        }
      }
    }

    // Draw altitude labels =====================================================================
    if(drawDetails)
    {
      for(int i = 0; i < airportMsa.bearings.size(); i++)
      {
        float bearing = atools::geo::normalizeCourse(airportMsa.bearings.at(i) + 180.f + magvar);
        float bearingTo = atools::geo::normalizeCourse(atools::atRoll(airportMsa.bearings, i + 1) + 180.f + magvar);

        // Line from origin to top defining label position - circles with one sector use a label closer to the center
        QLineF line(x, y, x, y - radius * (airportMsa.altitudes.size() > 1 ? 0.70 : 0.4));

        if(airportMsa.altitudes.size() > 1)
          // Rotate for sector bearing
          line.setAngle(atools::geo::angleToQt(bearing + atools::geo::angleAbsDiff2(bearing, bearingTo) / 2.f));

        QString text = Unit::altFeet(airportMsa.altitudes.at(i), false /* addUnit */, true /* narrow */);
        QSizeF txtsize = QFontMetricsF(painter->font()).boundingRect(text).size();
        painter->drawText(QPointF(line.p2().x() - txtsize.width() / 2., line.p2().y() + txtsize.height() / 2.), text);
      }

      // Draw small center circle
      painter->setBackground(opaqueFillColor);
      painter->setBrush(opaqueFillColor);
      painter->drawEllipse(QPointF(x, y), 4., 4.);
    }
  }
}

void SymbolPainter::drawTrackLine(QPainter *painter, float x, float y, int size, float dir)
{
  atools::util::PainterContextSaver saver(painter);
  painter->setBackgroundMode(Qt::TransparentMode);

  painter->translate(x, y);
  painter->rotate(atools::geo::normalizeCourse(dir));
  painter->drawPixmap(-size / 2, -size / 2, *trackLineFromCache(size));
  painter->resetTransform();
}

void SymbolPainter::drawUserpointSymbol(QPainter *painter, float x, float y, float size, bool routeFill)
{
  atools::util::PainterContextSaver saver(painter);
  painter->setBackgroundMode(Qt::TransparentMode);
  if(routeFill)
    painter->setBrush(mapcolors::routeTextBoxColor);
  else
    painter->setBrush(Qt::NoBrush);

  float lineWidth = std::max(size / 5.f, 1.5f);
  painter->setPen(QPen(mapcolors::routeUserPointColor, lineWidth, Qt::SolidLine, Qt::SquareCap));

  float radius = size / 2.f;
  painter->drawRect(QRectF(x - radius, y - radius, size, size));
}

void SymbolPainter::drawProcedureSymbol(QPainter *painter, float x, float y, float size, bool routeFill)
{
  atools::util::PainterContextSaver saver(painter);
  painter->setBackgroundMode(Qt::TransparentMode);
  if(routeFill)
    painter->setBrush(mapcolors::routeTextBoxColor);
  else
    painter->setBrush(Qt::NoBrush);

  float lineWidth = std::max(size / 5.f, 2.0f);
  painter->setPen(QPen(mapcolors::routeProcedurePointColor, lineWidth, Qt::SolidLine, Qt::SquareCap));
  painter->drawEllipse(QPointF(x, y), size / 2.f, size / 2.f);
}

void SymbolPainter::drawLogbookPreviewSymbol(QPainter *painter, float x, float y, float size)
{
  atools::util::PainterContextSaver saver(painter);
  painter->setBackgroundMode(Qt::TransparentMode);
  painter->setBrush(Qt::white);

  float lineWidth = std::max(size / 5.f, 2.0f);
  painter->setPen(QPen(mapcolors::routeLogEntryOutlineColor, lineWidth, Qt::SolidLine, Qt::SquareCap));
  painter->drawEllipse(QPointF(x, y), size / 2.f, size / 2.f);
}

void SymbolPainter::drawProcedureUnderlay(QPainter *painter, float x, float y, float size, bool flyover, bool faf)
{
  if(flyover)
    // Ring to indicate fly over
    drawProcedureFlyover(painter, x, y, size + 12.f);

  if(faf)
    /* Maltese cross to indicate FAF on the map */
    drawProcedureFaf(painter, x, y, size + 18.f);
}

void SymbolPainter::drawProcedureFlyover(QPainter *painter, float x, float y, float size)
{
  atools::util::PainterContextSaver saver(painter);
  painter->setBackgroundMode(Qt::OpaqueMode);

  float lineWidth = std::max(size / 16.f, 2.5f);
  painter->setPen(QPen(mapcolors::routeProcedurePointFlyoverColor, lineWidth, Qt::SolidLine, Qt::SquareCap));
  painter->setBrush(Qt::NoBrush);
  painter->drawEllipse(QPointF(x, y), size / 2.f, size / 2.f);
}

void SymbolPainter::drawProcedureFaf(QPainter *painter, float x, float y, float size)
{
  static QPolygonF polygon(
  {
    QPointF(2., 42.),
    QPointF(29., 31.),
    QPointF(18., 58.),
    QPointF(30., 50.),
    QPointF(42., 58.),
    QPointF(31., 31.),
    QPointF(58., 42.),
    QPointF(50., 30.),
    QPointF(58., 18.),
    QPointF(31., 29.),
    QPointF(42., 2.),
    QPointF(30., 10.),
    QPointF(18., 2.),
    QPointF(29., 29.),
    QPointF(2., 18.),
    QPointF(10., 30.),
    QPointF(2., 42.)
  });

  QPolygonF poly(polygon);
  // Move top left to 0,0
  poly.translate(-2., -2.);

  // Scale for size
  for(QPointF& pt : poly)
    pt = pt * static_cast<double>(size) / 58.;

  double tx = poly.boundingRect().width() / 2.;
  double ty = poly.boundingRect().height() / 2.;
  poly.translate(-tx + x, -ty + y);

  atools::util::PainterContextSaver saver(painter);
  painter->setBackgroundMode(Qt::OpaqueMode);
  painter->setBrush(Qt::black);
  painter->setPen(Qt::black);

  painter->drawPolygon(poly);
}

void SymbolPainter::drawVorSymbol(QPainter *painter, const map::MapVor& vor, float x, float y, float size, float sizeLarge, bool routeFill,
                                  bool fast, bool darkMap)
{
  atools::util::PainterContextSaver saver(painter);

  painter->setBackgroundMode(Qt::TransparentMode);
  if(routeFill)
    painter->setBrush(mapcolors::routeTextBoxColor);
  else
    painter->setBrush(Qt::NoBrush);

  // Use double to avoid type conversions
  double sizeD = static_cast<double>(size);
  double sizeLargeD = static_cast<double>(sizeLarge);
  QColor symbolColor = mapcolors::vorSymbolColor.lighter(darkMap ? 250 : 100);

  if(size > 4)
  {
    float lineWidth = std::max(size / 16.f, 1.5f);
    float roseLineWidth = std::max(size / 20.f, 0.8f);

    painter->setPen(QPen(symbolColor, lineWidth, Qt::SolidLine, Qt::SquareCap));

    painter->translate(x, y);

    if(sizeLarge > 0.f && !vor.dmeOnly)
      // If compass ticks are drawn rotate center symbol too
      painter->rotate(vor.magvar);

    double radiusD = sizeD / 2.;
    double radiusLargeD = sizeLargeD / 2.;

    if(vor.tacan || vor.vortac)
    {
      // Draw TACAN symbol or VORTAC outline
      // Coordinates taken from SVG graphics
      sizeD += 2.;

      QPolygonF polygon;
      polygon
        << QPointF((420. - 438.) * sizeD / 94., (538. - 583.) * sizeD / 81.)
        << QPointF((439. - 438.) * sizeD / 94., (527. - 583.) * sizeD / 81.)
        << QPointF((425. - 438.) * sizeD / 94., (503. - 583.) * sizeD / 81.)
        << QPointF((406. - 438.) * sizeD / 94., (513. - 583.) * sizeD / 81.)
        << QPointF((378. - 438.) * sizeD / 94., (513. - 583.) * sizeD / 81.)
        << QPointF((359. - 438.) * sizeD / 94., (502. - 583.) * sizeD / 81.)
        << QPointF((345. - 438.) * sizeD / 94., (526. - 583.) * sizeD / 81.)
        << QPointF((364. - 438.) * sizeD / 94., (538. - 583.) * sizeD / 81.)
        << QPointF((378. - 438.) * sizeD / 94., (562. - 583.) * sizeD / 81.)
        << QPointF((378. - 438.) * sizeD / 94., (583. - 583.) * sizeD / 81.)
        << QPointF((406. - 438.) * sizeD / 94., (583. - 583.) * sizeD / 81.)
        << QPointF((406. - 438.) * sizeD / 94., (562. - 583.) * sizeD / 81.)
        << QPointF((420. - 438.) * sizeD / 94., (538. - 583.) * sizeD / 81.);
      double tx = polygon.boundingRect().width() / 2.;
      double ty = polygon.boundingRect().height() / 2. + 1.;
      polygon.translate(tx, ty);
      painter->drawConvexPolygon(polygon);

      if(vor.vortac)
      {
        // Draw the filled VORTAC blocks
        painter->setBrush(symbolColor);
        painter->setPen(QPen(symbolColor, lineWidth, Qt::SolidLine, Qt::SquareCap));

        polygon.clear();
        polygon
          << QPointF((378. - 438.) * sizeD / 94., (513. - 583.) * sizeD / 81.)
          << QPointF((359. - 438.) * sizeD / 94., (502. - 583.) * sizeD / 81.)
          << QPointF((345. - 438.) * sizeD / 94., (526. - 583.) * sizeD / 81.)
          << QPointF((364. - 438.) * sizeD / 94., (538. - 583.) * sizeD / 81.);
        polygon.translate(tx, ty);
        painter->drawConvexPolygon(polygon);

        polygon.clear();
        polygon
          << QPointF((439. - 438.) * sizeD / 94., (527. - 583.) * sizeD / 81.)
          << QPointF((420. - 438.) * sizeD / 94., (538. - 583.) * sizeD / 81.)
          << QPointF((406. - 438.) * sizeD / 94., (513. - 583.) * sizeD / 81.)
          << QPointF((424. - 438.) * sizeD / 94., (503. - 583.) * sizeD / 81.);
        polygon.translate(tx, ty);
        painter->drawConvexPolygon(polygon);

        polygon.clear();
        polygon
          << QPointF((406. - 438.) * sizeD / 94., (562. - 583.) * sizeD / 81.)
          << QPointF((406. - 438.) * sizeD / 94., (583. - 583.) * sizeD / 81.)
          << QPointF((378. - 438.) * sizeD / 94., (583. - 583.) * sizeD / 81.)
          << QPointF((378. - 438.) * sizeD / 94., (562. - 583.) * sizeD / 81.);
        polygon.translate(tx, ty);
        painter->drawConvexPolygon(polygon);
      }
    }
    else
    {
      if(vor.hasDme)
        // DME rectangle
        painter->drawRect(QRectF(-sizeD / 2., -sizeD / 2., sizeD, sizeD));

      if(!vor.dmeOnly)
      {
        // Draw VOR symbol
        double corner = 2.;

        QPolygonF polygon;
        polygon << QPointF(-radiusD / corner, -radiusD)
                << QPointF(radiusD / corner, -radiusD)
                << QPointF(radiusD, 0.)
                << QPointF(radiusD / corner, radiusD)
                << QPointF(-radiusD / corner, radiusD)
                << QPointF(-radiusD, 0.);

        painter->drawConvexPolygon(polygon);
      }
    }

    if(sizeLarge > 0.f && !vor.dmeOnly)
    {
      // Draw compass circle and ticks
      painter->setBrush(Qt::NoBrush);
      painter->setPen(QPen(symbolColor, roseLineWidth, Qt::SolidLine, Qt::SquareCap));
      painter->drawEllipse(QPointF(0., 0.), radiusLargeD, radiusLargeD);

      if(!fast)
      {
        for(int i = 0; i < 360; i += 10)
        {
          if(i == 0)
            painter->drawLine(QLineF(0., 0., 0., -radiusLargeD));
          else if((i % 90) == 0)
            painter->drawLine(QLineF(0., -radiusD * 4., 0., -radiusLargeD));
          else
            painter->drawLine(QLineF(0., -radiusD * 4.5, 0., -radiusLargeD));
          painter->rotate(10.);
        }
      }
    }
    painter->resetTransform();

    // Draw dot in center
    if(size > 14)
      painter->setPen(QPen(symbolColor, sizeD / 4., Qt::SolidLine, Qt::RoundCap));
    else
      painter->setPen(QPen(symbolColor, sizeD / 3., Qt::SolidLine, Qt::RoundCap));
  }
  else
    painter->setPen(QPen(symbolColor, sizeD, Qt::SolidLine, Qt::SquareCap));

  painter->drawPoint(QPointF(x, y));
}

void SymbolPainter::drawNdbSymbol(QPainter *painter, float x, float y, float size, bool routeFill, bool fast, bool darkMap)
{
  atools::util::PainterContextSaver saver(painter);

  painter->setBackgroundMode(Qt::TransparentMode);
  if(routeFill)
    painter->setBrush(mapcolors::routeTextBoxColor);
  else
    painter->setBrush(Qt::NoBrush);

  QColor symbolColor = mapcolors::ndbSymbolColor.lighter(darkMap ? 250 : 100);

  if(size > 4.f)
  {
    float lineWidth = std::max(size / 16.f, 1.5f);

    // Use dotted or solid line depending on size
    painter->setPen(QPen(symbolColor, lineWidth,
                         (size > 12.f && !fast) ? Qt::DotLine : Qt::SolidLine, Qt::SquareCap));

    double radius = size / 2.;

    // Draw outer dotted/solid circle
    painter->drawEllipse(QPointF(x, y), radius, radius);

    if(size > 12.f && !fast)
      // If big enought draw inner dotted circle
      painter->drawEllipse(QPointF(x, y), radius * 2. / 3., radius * 2. / 3.);

    double pointSize = size > 12 ? size / 4. : size / 3.;
    painter->setPen(QPen(symbolColor, pointSize, Qt::SolidLine, Qt::RoundCap));
  }
  else
    painter->setPen(QPen(symbolColor, size, Qt::SolidLine, Qt::RoundCap));

  painter->drawPoint(QPointF(x, y));
}

void SymbolPainter::drawMarkerSymbol(QPainter *painter, const map::MapMarker& marker, float x, float y, float size, bool fast)
{
  atools::util::PainterContextSaver saver(painter);

  painter->setBackgroundMode(Qt::TransparentMode);
  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(mapcolors::markerSymbolColor, 1.5, Qt::SolidLine, Qt::RoundCap));

  if(!fast && size > 5.f)
  {
    // Draw rotated lens / ellipse
    double radius = size / 2.;
    painter->translate(x, y);
    painter->rotate(marker.heading);
    painter->drawEllipse(QPointF(0., 0.), radius, radius / 2.);
    painter->resetTransform();
  }

  painter->setPen(QPen(mapcolors::markerSymbolColor, size / 4.f, Qt::SolidLine, Qt::RoundCap));

  painter->drawPoint(QPointF(x, y));
}

void SymbolPainter::drawNdbText(QPainter *painter, const map::MapNdb& ndb, float x, float y, textflags::TextFlags flags, float size,
                                bool fill, bool darkMap, textatt::TextAttributes atts, const QStringList *addtionalText)
{
  QStringList texts;

  if(flags & textflags::IDENT && flags & textflags::TYPE)
  {
    if(ndb.type.isEmpty())
      texts.append(ndb.ident);
    else
      texts.append(tr("%1 (%2)").arg(ndb.ident).arg(ndb.type == "CP" ? tr("CL") : ndb.type));
  }
  else if(flags & textflags::IDENT)
    texts.append(ndb.ident);

  if(flags & textflags::FREQ)
    texts.append(QString::number(ndb.frequency / 100., 'f', 1));

  if(flags & textflags::ROUTE_TEXT)
    atts |= textatt::ROUTE_BG_COLOR;

  if(!flags.testFlag(textflags::ABS_POS))
  {
    if(!(atts & textatt::PLACE_ALL))
      atts |= textatt::CENTER | textatt::BELOW;
    adjustPos(x, y, size / 2.f, atts);
  }

  if(addtionalText != nullptr && !addtionalText->isEmpty())
  {
    if(flags.testFlag(textflags::ELLIPSE_IDENT))
    {
      if(!texts.isEmpty())
        // Ignore additional texts and add ellipsis
        texts.first() = texts.constFirst() % tr("…", "Dots used indicate additional text in map");
    }
    else
      texts.append(*addtionalText);
  }

  textBoxF(painter, texts, mapcolors::ndbSymbolColor.lighter(darkMap ? 250 : 100), x, y, atts, fill ? 255 : 0);
}

void SymbolPainter::drawVorText(QPainter *painter, const map::MapVor& vor, float x, float y, textflags::TextFlags flags, float size,
                                bool fill, bool darkMap, textatt::TextAttributes atts, const QStringList *addtionalText)
{
  QStringList texts;

  if(flags & textflags::IDENT && flags & textflags::TYPE)
  {
    if(vor.type.isEmpty())
      texts.append(vor.ident);
    else
      texts.append(tr("%1 (%2)").arg(vor.ident).arg(vor.type.at(0)));
  }
  else if(flags & textflags::IDENT)
    texts.append(vor.ident);

  if(flags & textflags::FREQ)
  {
    if(!vor.tacan)
      texts.append(QString::number(vor.frequency / 1000., 'f', 2));
    if(vor.tacan /*|| vor.vortac*/)
      texts.append(vor.channel);
  }

  if(flags & textflags::ROUTE_TEXT)
    atts |= textatt::ROUTE_BG_COLOR;

  if(!flags.testFlag(textflags::ABS_POS))
  {
    if(!(atts & textatt::PLACE_ALL))
      atts |= textatt::LEFT;
    adjustPos(x, y, size, atts);
  }

  if(addtionalText != nullptr && !addtionalText->isEmpty())
  {
    if(flags.testFlag(textflags::ELLIPSE_IDENT))
    {
      if(!texts.isEmpty())
        // Ignore additional texts and add ellipsis
        texts.first() = texts.constFirst() % tr("…", "Dots used indicate additional text in map");
    }
    else
      texts.append(*addtionalText);
  }

  textBoxF(painter, texts, mapcolors::vorSymbolColor.lighter(darkMap ? 250 : 100), x, y, atts, fill ? 255 : 0);
}

void SymbolPainter::adjustPos(float& x, float& y, float size, textatt::TextAttributes atts)
{
  if((atts & textatt::PLACE_ALL_VERT) && atts & textatt::PLACE_ALL_HORIZ)
    // Reduce distance for diagonal placement
    size /= 1.5f;

  if(atts & textatt::RIGHT)
    x += size;
  else if(atts & textatt::LEFT)
    x -= size;

  if(atts & textatt::ABOVE)
    y -= size;
  else if(atts & textatt::BELOW)
    y += size;
}

void SymbolPainter::drawWaypointText(QPainter *painter, const map::MapWaypoint& wp, float x, float y,
                                     textflags::TextFlags flags, float size, bool fill, textatt::TextAttributes atts,
                                     const QStringList *addtionalText)
{
  QStringList texts;

  if(flags.testFlag(textflags::IDENT))
    texts.append(wp.ident);

  if(flags.testFlag(textflags::ROUTE_TEXT))
    atts |= textatt::ROUTE_BG_COLOR;

  if(!flags.testFlag(textflags::ABS_POS))
  {
    if(!(atts & textatt::PLACE_ALL))
      atts |= textatt::RIGHT;
    adjustPos(x, y, size, atts);
  }

  if(addtionalText != nullptr && !addtionalText->isEmpty())
  {
    if(flags.testFlag(textflags::ELLIPSE_IDENT))
    {
      if(!texts.isEmpty())
        // Ignore additional texts and add ellipsis
        texts.first() = texts.constFirst() % tr("…", "Dots used indicate additional text in map");
    }
    else
      texts.append(*addtionalText);
  }

  textBoxF(painter, texts, mapcolors::waypointSymbolColor, x, y, atts, fill ? 255 : 0);
}

void SymbolPainter::drawAirportText(QPainter *painter, const map::MapAirport& airport, float x, float y,
                                    optsd::DisplayOptionsAirport dispOpts, textflags::TextFlags flags, float size,
                                    bool diagram, int maxTextLength, textatt::TextAttributes atts)
{
  // Get layer and options dependent texts
  QStringList texts = airportTexts(dispOpts, flags, airport, maxTextLength);

  if(!texts.isEmpty())
  {
    if(airport.addon())
      atts |= textatt::ITALIC | textatt::UNDERLINE;

    if(airport.closed())
      atts |= textatt::STRIKEOUT;

    if(flags.testFlag(textflags::ROUTE_TEXT))
      atts |= textatt::ROUTE_BG_COLOR;

    if(flags.testFlag(textflags::LOG_TEXT))
      atts |= textatt::LOG_BG_COLOR;

    int transparency = diagram ? 180 : 255;
    // No background for empty airports except if they are part of the route or log
    if(airport.emptyDraw() && !flags.testFlag(textflags::ROUTE_TEXT) && !flags.testFlag(textflags::LOG_TEXT))
      transparency = 0;

    if(!flags.testFlag(textflags::ABS_POS))
    {
      if(!(atts & textatt::PLACE_ALL))
        atts |= textatt::RIGHT;
      adjustPos(x, y, size, atts);
    }

    if(flags & textflags::NO_BACKGROUND)
      transparency = 0;

    textBoxF(painter, texts, mapcolors::colorForAirport(airport), x, y, atts, transparency);
  }
}

QStringList SymbolPainter::airportTexts(optsd::DisplayOptionsAirport dispOpts, textflags::TextFlags flags,
                                        const map::MapAirport& airport, int maxTextLength)
{
  QStringList texts;

  if(flags & textflags::INFO)
  {
    if(airport.towerFrequency != 0 && dispOpts & optsd::ITEM_AIRPORT_TOWER)
      texts.append(tr("CT ") % QString::number(roundComFrequency(airport.towerFrequency), 'f', 3));

    QString autoWeather;
    if(dispOpts & optsd::ITEM_AIRPORT_ATIS)
    {
      if(airport.atisFrequency > 0)
        autoWeather = tr("ATIS ") % QString::number(roundComFrequency(airport.atisFrequency), 'f', 3);
      else if(airport.awosFrequency > 0)
        autoWeather = tr("AWOS ") % QString::number(roundComFrequency(airport.awosFrequency), 'f', 3);
      else if(airport.asosFrequency > 0)
        autoWeather = tr("ASOS ") % QString::number(roundComFrequency(airport.asosFrequency), 'f', 3);
    }

    if(!autoWeather.isEmpty())
      texts.append(autoWeather);

    // bool elevUnit = Unit::getUnitAltStr() != Unit::getUnitShortDistStr();
    if(dispOpts & optsd::ITEM_AIRPORT_RUNWAY)
      if(airport.longestRunwayLength != 0 || airport.getAltitude() != 0.f)
        texts.append(Unit::altFeet(airport.getAltitude(), true /*addUnit*/, true /*narrow*/) % " " %
                     (airport.flags.testFlag(map::AP_LIGHT) ? "L " : "- ") %
                     Unit::distShortFeet(airport.longestRunwayLength, true /*addUnit*/, true /*narrow*/) % " ");
  }

  // Build ident/name combination - flags are layer dependent
  if(flags & textflags::IDENT && flags & textflags::NAME && dispOpts & optsd::ITEM_AIRPORT_NAME)
  {
    if(texts.isEmpty())
    {
      texts.prepend(atools::elideTextShort(airport.name, maxTextLength));
      texts.prepend(airport.displayIdent());
    }
    else
      texts.prepend(atools::elideTextShort(airport.name, maxTextLength) % " (" % airport.displayIdent() % ")");
  }
  else if(flags & textflags::IDENT)
    texts.prepend(airport.displayIdent());
  else if(flags & textflags::NAME)
    texts.prepend(airport.name);
  return texts;
}

void SymbolPainter::textBox(QPainter *painter, const QStringList& texts, const QPen& textPen, int x, int y,
                            textatt::TextAttributes atts, int transparency, const QColor& backgroundColor)
{
  textBoxF(painter, texts, textPen, atools::roundToInt(x), atools::roundToInt(y), atts, transparency, backgroundColor);
}

void SymbolPainter::textBoxF(QPainter *painter, QStringList texts, QPen textPen, float x, float y, textatt::TextAttributes atts,
                             int transparency, const QColor& backgroundColor)
{
  // Added margins to background retangle to avoid letters touching the border
  // Windows needs different margins due to fontengine=freetype
#ifdef Q_OS_WINDOWS
  const static QMarginsF TEXT_MARGINS(2., 2., 1., 0.); // Margins for normal fonts added
  const static QMarginsF TEXT_MARGINS_SMALL(1., 1., 1., 0.); // Margins for small fonts added
  const static QMarginsF TEXT_MARGINS_UNDERLINE(2., -1., 1., 1.); // Margins for underlined text added
#else
  const static QMarginsF TEXT_MARGINS(2., -1., 2., 0.); // Margins for normal fonts added
  const static QMarginsF TEXT_MARGINS_SMALL(1., 0., 1., 0.); // Margins for small fonts added
  const static QMarginsF TEXT_MARGINS_UNDERLINE(2., -1., 1., 1.); // Margins for underlined text added
#endif

  // Remove empty lines
  texts.removeAll(QString());

  if(texts.isEmpty())
    return;

  atools::util::PainterContextSaver saver(painter);

  // Determine background and text colors ======================
  QColor backColor(backgroundColor);
  if(!backColor.isValid())
  {
    if(atts.testFlag(textatt::ROUTE_BG_COLOR))
      backColor = mapcolors::routeTextBoxColor;
    else if(atts.testFlag(textatt::LOG_BG_COLOR))
      backColor = mapcolors::logTextBoxColor;
    else
      backColor = mapcolors::textBoxColor;
  }

  if(atts.testFlag(textatt::WARNING_COLOR))
  {
    backColor = Qt::white;
    textPen.setColor(Qt::red);
    transparency = 255;
  }
  if(atts.testFlag(textatt::ERROR_COLOR))
  {
    backColor = Qt::red;
    textPen.setColor(Qt::white);
    transparency = 255;
  }

  // Determine fill ======================
  bool fill = false;
  if(transparency != 255)
  {
    if(transparency == 0) // Do not fill at all
    {
      painter->setBackgroundMode(Qt::TransparentMode);
      painter->setBrush(Qt::NoBrush);
    }
    else
    {
      // Use an alpha channel for semi transparency
      painter->setBackgroundMode(Qt::OpaqueMode);
      backColor.setAlpha(transparency);
      painter->setBrush(backColor);
      painter->setBackground(backColor);
      fill = true;
    }
  }
  else
  {
    // Fill background
    painter->setBackgroundMode(Qt::OpaqueMode);
    painter->setBrush(backColor);
    painter->setBackground(backColor);
    fill = true;
  }

  // Text attributes =============================================
  if(atts.testFlag(textatt::ITALIC) || atts.testFlag(textatt::BOLD) || atts.testFlag(textatt::UNDERLINE) ||
     atts.testFlag(textatt::OVERLINE) || atts.testFlag(textatt::STRIKEOUT))
  {
    QFont f = painter->font();

    if(atts.testFlag(textatt::BOLD))
      f.setBold(true);

    if(atts.testFlag(textatt::ITALIC))
      f.setItalic(true);

    if(atts.testFlag(textatt::UNDERLINE))
      f.setUnderline(true);

    if(atts.testFlag(textatt::OVERLINE))
      f.setOverline(true);

    if(atts.testFlag(textatt::STRIKEOUT))
      f.setStrikeOut(true);

    painter->setFont(f);
  }

  // Calculate font sizes =========================
  QFontMetricsF metrics(painter->font());
  double height = metrics.height() - 1.;
  double totalHeight = height * texts.size();
  double yoffset = 0.;

  // Calculate vertical reference point ====================
  if(atts.testFlag(textatt::BELOW))
    // Reference point at top to place text below an icon
    yoffset = 0;
  else if(atts.testFlag(textatt::ABOVE))
    // Reference point at bottom of text stack to place text on top of an icon
    yoffset = -totalHeight;
  else
    // Center text vertically
    yoffset = -totalHeight / 2.f;

  // Draw background rectangle if and calculate text positions ===================
  painter->setPen(Qt::NoPen);

  QVector<QPointF> textPt;
  for(const QString& text : qAsConst(texts))
  {
    QRectF boundingRect = metrics.boundingRect(text);
    double w = boundingRect.width();
    double newx = x;
    if(atts.testFlag(textatt::LEFT))
      // Reference point is at the right of the text (right-aligned) to place text at the left of an icon
      newx -= w;
    else if(atts.testFlag(textatt::CENTER))
      newx -= w / 2.f;
    // else LEFT  Reference point is at the left of the text (left-aligned) to place text at the right of an icon

    QPointF pt(newx, y + yoffset);
    textPt.append(pt);

    if(fill)
    {
      boundingRect.moveTo(pt);

      if(atts.testFlag(textatt::NO_ROUND_RECT))
      {
        if(boundingRect.height() < 14)
          // Use smaller margins for small fonts
          painter->drawRect(boundingRect.marginsAdded(TEXT_MARGINS_SMALL));
        else
          // Extend bottom margin for underlined letters
          painter->drawRect(boundingRect.marginsAdded(atts.testFlag(textatt::UNDERLINE) ? TEXT_MARGINS_UNDERLINE : TEXT_MARGINS));
      }
      else
      {
        if(boundingRect.height() < 14)
          // Use smaller margins for small fonts
          painter->drawRoundedRect(boundingRect.marginsAdded(TEXT_MARGINS_SMALL), 2., 2.);
        else
          // Extend bottom margin for underlined letters
          painter->drawRoundedRect(boundingRect.marginsAdded(atts.testFlag(textatt::UNDERLINE) ? TEXT_MARGINS_UNDERLINE : TEXT_MARGINS),
                                   4., 4.);
      }
    }
    yoffset += height;
  }

  painter->setBackgroundMode(Qt::TransparentMode);
  painter->setBrush(Qt::NoBrush);
  painter->setPen(textPen);

  // Draw texts =================================
  QPointF ascent(0., metrics.ascent());
  for(int i = 0; i < texts.size(); i++)
    painter->drawText(textPt.at(i) + ascent, texts.at(i));
}

QRectF SymbolPainter::textBoxSize(QPainter *painter, const QStringList& texts, textatt::TextAttributes atts)
{
  QRectF retval;
  if(texts.isEmpty())
    return retval;

  atools::util::PainterContextSaver saver(painter);

  if(atts.testFlag(textatt::ITALIC) || atts.testFlag(textatt::BOLD) || atts.testFlag(textatt::UNDERLINE))
  {
    QFont font = painter->font();
    font.setBold(atts.testFlag(textatt::BOLD) || font.bold());
    font.setItalic(atts.testFlag(textatt::ITALIC));
    font.setUnderline(atts.testFlag(textatt::UNDERLINE));
    painter->setFont(font);
  }

  QFontMetricsF metrics(painter->font());
  double h = metrics.height();

  // Increase text box size for each bounding rectangle of text
  double yoffset = 0.;
  for(const QString& t : texts)
  {
    double w = metrics.horizontalAdvance(t);
    double newx = 0.;
    if(atts.testFlag(textatt::LEFT))
      newx -= w;
    else if(atts.testFlag(textatt::CENTER))
      newx -= w / 2.;

    double textW = metrics.horizontalAdvance(t);
    if(retval.isNull())
      retval = QRectF(newx, yoffset, textW, h);
    else
      retval = retval.united(QRectF(newx, yoffset, textW, h));
    // painter->drawText(newx, y + yoffset, t);
    yoffset += h;
  }
  return retval;
}

const QPixmap *SymbolPainter::windPointerFromCache(int size)
{
  if(windPointerPixmaps.contains(size))
    return windPointerPixmaps.object(size);
  else
  {
    QPixmap *newPx = new QPixmap(QIcon(":/littlenavmap/resources/icons/windpointer.svg").pixmap(QSize(size, size)));
    windPointerPixmaps.insert(size, newPx);
    return newPx;
  }
}

const QPixmap *SymbolPainter::trackLineFromCache(int size)
{
  if(trackLinePixmaps.contains(size))
    return trackLinePixmaps.object(size);
  else
  {
    QPixmap *newPx = new QPixmap(QIcon(":/littlenavmap/resources/icons/trackline.svg").pixmap(QSize(size, size)));
    trackLinePixmaps.insert(size, newPx);
    return newPx;
  }
}

void SymbolPainter::prepareForIcon(QPainter& painter)
{
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
}
