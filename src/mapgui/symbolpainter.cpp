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

#include "common/maptypes.h"
#include "mapquery.h"
#include "symbolpainter.h"
#include "common/mapcolors.h"

#include <QPainter>

#include <marble/GeoPainter.h>

using namespace Marble;

SymbolPainter::SymbolPainter()
{
}

void SymbolPainter::drawAirportSymbol(QPainter *painter, const maptypes::MapAirport& ap, int x, int y,
                                      int size, bool isAirportDiagram, bool fast)
{
  using namespace maptypes;

  painter->save();
  QColor apColor = mapcolors::colorForAirport(ap);

  int radius = size / 2;
  painter->setBackgroundMode(Qt::OpaqueMode);

  if(ap.flags.testFlag(AP_HARD) && !ap.flags.testFlag(AP_MIL) && !ap.flags.testFlag(AP_CLOSED))
    // Use filled circle
    painter->setBrush(QBrush(apColor));
  else
    // Use white filled circle
    painter->setBrush(QBrush(mapcolors::airportSymbolFillColor));

  if(!fast || isAirportDiagram)
    if(ap.anyFuel() && !ap.flags.testFlag(AP_MIL) && !ap.flags.testFlag(AP_CLOSED) && size > 6)
    {
    // Draw fuel spikes
    int fuelRadius = static_cast<int>(std::round(static_cast<double>(radius) * 1.4));
    if(fuelRadius < radius + 2)
      fuelRadius = radius + 2;
    painter->setPen(QPen(QBrush(apColor), size / 4, Qt::SolidLine, Qt::FlatCap));
    painter->drawLine(x, y - fuelRadius, x, y + fuelRadius);
    painter->drawLine(x - fuelRadius, y, x + fuelRadius, y);
    }

  painter->setPen(QPen(QBrush(apColor), size / 5, Qt::SolidLine, Qt::FlatCap));
  painter->drawEllipse(QPoint(x, y), radius, radius);

  if(!fast || isAirportDiagram)
  {
    if(ap.flags.testFlag(AP_MIL))
      painter->drawEllipse(QPoint(x, y), radius / 2, radius / 2);

    if(ap.waterOnly() && size > 6)
    {
      int lineWidth = size / 7;
      painter->setPen(QPen(QBrush(apColor), lineWidth, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(x - lineWidth / 4, y - radius / 2, x - lineWidth / 4, y + radius / 2);
      painter->drawArc(x - radius / 2, y - radius / 2, radius, radius, 0 * 16, -180 * 16);
    }

    if(ap.helipadOnly() && size > 6)
    {
      int lineWidth = size / 7;
      painter->setPen(QPen(QBrush(apColor), lineWidth, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(x - lineWidth / 4 - size / 5, y - radius / 2,
                        x - lineWidth / 4 - size / 5, y + radius / 2);

      painter->drawLine(x - lineWidth / 4 - size / 5, y,
                        x - lineWidth / 4 + size / 5, y);

      painter->drawLine(x - lineWidth / 4 + size / 5, y - radius / 2,
                        x - lineWidth / 4 + size / 5, y + radius / 2);
    }

    if(ap.flags.testFlag(AP_CLOSED) && size > 6)
    {
      // Cross out whatever was painted before
      painter->setPen(QPen(QBrush(apColor), size / 7, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(x - radius, y - radius, x + radius, y + radius);
      painter->drawLine(x - radius, y + radius, x + radius, y - radius);
    }
  }

  if(!fast || isAirportDiagram)
    if(ap.flags.testFlag(AP_HARD) && !ap.flags.testFlag(AP_MIL) && !ap.flags.testFlag(AP_CLOSED) && size > 6)
    {
      // Draw line inside circle
      painter->translate(x, y);
      painter->rotate(ap.longestRunwayHeading);
      painter->setPen(QPen(QBrush(mapcolors::airportSymbolFillColor), size / 5, Qt::SolidLine, Qt::RoundCap));
      painter->drawLine(0, -radius + 2, 0, radius - 2);
      painter->resetTransform();
    }

  painter->restore();
}

void SymbolPainter::drawWaypointSymbol(QPainter *painter, const maptypes::MapWaypoint& wp, int x, int y,
                                       int size, bool fill, bool fast)
{
  Q_UNUSED(wp);
  painter->save();
  painter->setBackgroundMode(Qt::TransparentMode);
  if(fill)
    painter->setBrush(QColor(Qt::white));
  else
    painter->setBrush(Qt::NoBrush);

  painter->setPen(QPen(mapcolors::waypointSymbolColor, 1.5, Qt::SolidLine, Qt::SquareCap));

  if(!fast)
  {
    int radius = size / 2;
    QPolygon polygon;
    polygon << QPoint(x, y - radius)
            << QPoint(x + radius, y + radius)
            << QPoint(x - radius, y + radius);

    painter->drawConvexPolygon(polygon);
  }
  else
    painter->drawPoint(x, y);

  painter->restore();
}

void SymbolPainter::drawVorSymbol(QPainter *painter, const maptypes::MapVor& vor, int x, int y, int size,
                                  bool fill, bool fast, int largeSize)
{
  painter->save();
  painter->setBackgroundMode(Qt::TransparentMode);
  if(fill)
    painter->setBrush(QColor(Qt::white));
  else
    painter->setBrush(Qt::NoBrush);

  painter->setPen(QPen(mapcolors::vorSymbolColor, 1.5, Qt::SolidLine, Qt::SquareCap));

  if(!fast)
  {
    painter->translate(x, y);

    if(largeSize > 0 && !vor.dmeOnly)
      painter->rotate(-vor.magvar);

    int radius = size / 2;
    if(vor.hasDme)
      painter->drawRect(-size / 2, -size / 2, size, size);
    if(!vor.dmeOnly)
    {
      int corner = 2;
      QPolygon polygon;
      polygon << QPoint(-radius / corner, -radius)
              << QPoint(radius / corner, -radius)
              << QPoint(radius, 0)
              << QPoint(radius / corner, radius)
              << QPoint(-radius / corner, radius)
              << QPoint(-radius, 0);

      painter->drawConvexPolygon(polygon);
    }

    if(largeSize > 0 && !vor.dmeOnly)
    {
      painter->setBrush(Qt::NoBrush);
      painter->setPen(QPen(mapcolors::vorSymbolColor, 1, Qt::SolidLine, Qt::SquareCap));
      painter->drawEllipse(QPoint(0, 0), radius * 5, radius * 5);

      for(int i = 0; i < 360; i += 10)
      {
        if(i == 0)
          painter->drawLine(0, 0, 0, -radius * 5);
        else if((i % 90) == 0)
          painter->drawLine(0, static_cast<int>(-radius * 4), 0, -radius * 5);
        else
          painter->drawLine(0, static_cast<int>(-radius * 4.5), 0, -radius * 5);
        painter->rotate(10);
      }
    }
    painter->resetTransform();

  }

  if(size > 14)
    painter->setPen(QPen(mapcolors::vorSymbolColor, size / 4, Qt::SolidLine, Qt::RoundCap));
  else
    painter->setPen(QPen(mapcolors::vorSymbolColor, size / 3, Qt::SolidLine, Qt::RoundCap));
  painter->drawPoint(x, y);

  painter->drawPoint(x, y);
  painter->restore();
}

void SymbolPainter::drawNdbSymbol(QPainter *painter, const maptypes::MapNdb& ndb, int x, int y, int size,
                                  bool fill, bool fast)
{
  Q_UNUSED(ndb);
  painter->save();

  painter->setBackgroundMode(Qt::TransparentMode);
  if(fill)
    painter->setBrush(QColor(Qt::white));
  else
    painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(mapcolors::ndbSymbolColor, 1.5, size > 12 ? Qt::DotLine : Qt::SolidLine, Qt::RoundCap));

  int radius = size / 2;

  if(!fast)
    // Draw outer dotted circle
    painter->drawEllipse(QPoint(x, y), radius, radius);

  if(size > 12)
  {
    if(!fast)
      // If big enought draw inner dotted circle
      painter->drawEllipse(QPoint(x, y), radius * 2 / 3, radius * 2 / 3);
    painter->setPen(QPen(mapcolors::ndbSymbolColor, size / 4, Qt::SolidLine, Qt::RoundCap));
  }
  else
    painter->setPen(QPen(mapcolors::ndbSymbolColor, size / 3, Qt::SolidLine, Qt::RoundCap));

  painter->drawPoint(x, y);

  painter->restore();
}

void SymbolPainter::drawMarkerSymbol(QPainter *painter, const maptypes::MapMarker& marker, int x, int y,
                                     int size, bool fast)
{
  Q_UNUSED(marker);
  painter->save();
  int radius = size / 2;

  painter->setBackgroundMode(Qt::TransparentMode);
  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(mapcolors::markerSymbolColor, 1.5, Qt::SolidLine, Qt::RoundCap));

  if(!fast)
  {
    // Draw rotated lens / ellipse
    painter->translate(x, y);
    painter->rotate(marker.heading);
    painter->drawEllipse(QPoint(0, 0), radius, radius / 2);
    painter->resetTransform();
  }

  painter->setPen(QPen(mapcolors::markerSymbolColor, size / 4, Qt::SolidLine, Qt::RoundCap));

  painter->drawPoint(x, y);

  painter->restore();
}

void SymbolPainter::drawNdbText(QPainter *painter, const maptypes::MapNdb& ndb, int x, int y,
                                textflags::TextFlags flags, int size, bool fill, bool fast)
{
  QStringList texts;

  if(flags & textflags::IDENT && flags & textflags::TYPE)
    texts.append(ndb.ident + " (" + (ndb.type == "COMPASS_POINT" ? "CP" : ndb.type) + ")");
  else if(flags & textflags::IDENT)
    texts.append(ndb.ident);

  if(flags & textflags::FREQ)
    texts.append(QString::number(ndb.frequency / 100., 'f', 1));

  textatt::TextAttributes textAttrs = textatt::BOLD | textatt::CENTER;
  if(flags & textflags::ROUTE_TEXT)
    textAttrs |= textatt::ROUTE_BG_COLOR;

  y += size / 2 + painter->fontMetrics().ascent();
  int transparency = fill ? 255 : 0;
  textBox(painter, texts, mapcolors::ndbSymbolColor, x, y, textAttrs, transparency);
}

void SymbolPainter::drawVorText(QPainter *painter, const maptypes::MapVor& vor, int x, int y,
                                textflags::TextFlags flags, int size, bool fill, bool fast)
{
  QStringList texts;

  if(flags & textflags::IDENT && flags & textflags::TYPE)
  {
    QString range;
    if(vor.range < 40)
      range = "T";
    else if(vor.range < 62)
      range = "L";
    else if(vor.range < 200)
      range = "H";
    texts.append(vor.ident + " (" + range + ")");
  }
  else if(flags & textflags::IDENT)
    texts.append(vor.ident);

  if(flags & textflags::FREQ)
    texts.append(QString::number(vor.frequency / 1000., 'f', 2));

  textatt::TextAttributes textAttrs = textatt::BOLD | textatt::RIGHT;
  if(flags & textflags::ROUTE_TEXT)
    textAttrs |= textatt::ROUTE_BG_COLOR;

  x -= size / 2 + 2;
  int transparency = fill ? 255 : 0;
  textBox(painter, texts, mapcolors::vorSymbolColor, x, y, textAttrs, transparency);
}

void SymbolPainter::drawWaypointText(QPainter *painter, const maptypes::MapWaypoint& wp, int x, int y,
                                     textflags::TextFlags flags, int size, bool fill, bool fast)
{
  QStringList texts;

  if(flags & textflags::IDENT)
    texts.append(wp.ident);

  x += size / 2 + 2;
  int transparency = fill ? 255 : 0;

  textatt::TextAttributes textAttrs = textatt::BOLD | textatt::LEFT;
  if(flags & textflags::ROUTE_TEXT)
    textAttrs |= textatt::ROUTE_BG_COLOR;

  textBox(painter, texts, mapcolors::waypointSymbolColor, x, y, textAttrs, transparency);
}

void SymbolPainter::textBox(QPainter *painter, const QStringList& texts, const QPen& textPen, int x, int y,
                            textatt::TextAttributes atts, int transparency)
{
  if(texts.isEmpty())
    return;

  painter->save();

  QColor backColor = atts & textatt::ROUTE_BG_COLOR ? mapcolors::routeTextBoxColor : mapcolors::textBoxColor;

  if(transparency != 255)
  {
    if(transparency == 0)
      painter->setBrush(Qt::NoBrush);
    else
    {
      backColor.setAlpha(transparency);
      painter->setBrush(backColor);
    }
  }
  else
    painter->setBrush(backColor);

  QFontMetrics metrics = painter->fontMetrics();
  int h = metrics.height();

  int yoffset = 0;
  if(transparency != 0)
  {
    painter->setPen(mapcolors::textBackgroundPen);
    for(const QString& t : texts)
    {
      int w = metrics.width(t);
      int newx = x - 2;
      // if(atts.testFlag(textatt::LEFT))
      // newx = x;
      if(atts.testFlag(textatt::RIGHT))
        newx -= w;
      else if(atts.testFlag(textatt::CENTER))
        newx -= w / 2;

      // painter->drawRoundedRect(x - 2, y - h + metrics.descent() + yoffset, w + 4, h, 5, 5);
      painter->drawRect(newx, y - h + metrics.descent() + yoffset, w + 4, h);
      yoffset += h;
    }
  }

  if(atts.testFlag(textatt::ITALIC) || atts.testFlag(textatt::BOLD) || atts.testFlag(textatt::UNDERLINE))
  {
    QFont f = painter->font();
    f.setBold(atts.testFlag(textatt::BOLD));
    f.setItalic(atts.testFlag(textatt::ITALIC));
    f.setUnderline(atts.testFlag(textatt::UNDERLINE));
    painter->setFont(f);
  }

  yoffset = 0;
  painter->setPen(textPen);
  for(const QString& t : texts)
  {
    int w = metrics.width(t);
    int newx = x;
    // if(atts.testFlag(textatt::LEFT))
    // newx = x;
    if(atts.testFlag(textatt::RIGHT))
      newx -= w;
    else if(atts.testFlag(textatt::CENTER))
      newx -= w / 2;

    painter->drawText(newx, y + yoffset, t);
    yoffset += h;
  }
  painter->restore();
}

void SymbolPainter::drawAirportText(QPainter *painter, const maptypes::MapAirport& airport, int x, int y,
                                    textflags::TextFlags flags, int size, bool diagram, bool fill, bool fast)
{
  QStringList texts = airportTexts(flags, airport);
  if(!texts.isEmpty())
  {
    textatt::TextAttributes atts = textatt::BOLD;
    if(airport.flags.testFlag(maptypes::AP_ADDON))
      atts |= textatt::ITALIC | textatt::UNDERLINE;

    if(flags & textflags::ROUTE_TEXT)
      atts |= textatt::ROUTE_BG_COLOR;

    int transparency = diagram ? 180 : 255;
    if(!airport.scenery())
      transparency = 0;

    x += size + 2;

    textBox(painter, texts, mapcolors::colorForAirport(airport), x, y, atts, transparency);
  }
}

QStringList SymbolPainter::airportTexts(textflags::TextFlags flags, const maptypes::MapAirport& airport)
{
  QStringList texts;

  if(flags & textflags::IDENT && flags & textflags::NAME)
    texts.append(airport.name + " (" + airport.ident + ")");
  else if(flags & textflags::IDENT)
    texts.append(airport.ident);
  else if(flags & textflags::NAME)
    texts.append(airport.name);

  if(flags & textflags::INFO)
  {
    QString tower = (airport.towerFrequency == 0 ? QString() :
                     "CT - " + QString::number(airport.towerFrequency / 1000., 'f', 2));

    QString autoWeather;
    if(airport.atisFrequency > 0)
      autoWeather = "ATIS " + QString::number(airport.atisFrequency / 1000., 'f', 2);
    else if(airport.awosFrequency > 0)
      autoWeather = "AWOS " + QString::number(airport.awosFrequency / 1000., 'f', 2);
    else if(airport.asosFrequency > 0)
      autoWeather = "ASOS " + QString::number(airport.asosFrequency / 1000., 'f', 2);

    if(!tower.isEmpty() || !autoWeather.isEmpty())
      texts.append(tower + (tower.isEmpty() ? QString() : " ") + autoWeather);

    texts.append(QString::number(airport.altitude) + " " +
                 (airport.flags.testFlag(maptypes::AP_LIGHT) ? "L " : "- ") +
                 QString::number(airport.longestRunwayLength / 100) + " " +
                 (airport.unicomFrequency == 0 ? QString() :
                  QString::number(airport.unicomFrequency / 1000., 'f', 2)));
  }
  return texts;
}
