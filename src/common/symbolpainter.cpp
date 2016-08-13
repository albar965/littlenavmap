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

#include "symbolpainter.h"

#include "common/maptypes.h"
#include "mapgui/mapquery.h"
#include "common/mapcolors.h"
#include "options/optiondata.h"

#include <QPainter>
#include <QApplication>
#include <marble/GeoPainter.h>

using namespace Marble;
using namespace maptypes;

/* Simulator aircraft symbol */
const QVector<QLine> AIRCRAFTLINES({QLine(0, -20, 0, 16), // Body
                                    QLine(-20, 2, 0, -6), QLine(0, -6, 20, 2), // Wings
                                    QLine(-10, 18, 0, 14), QLine(0, 14, 10, 18) // Horizontal stabilizer
                                   });

SymbolPainter::SymbolPainter(QColor backgroundColor)
{
  iconBackground = backgroundColor;
}

SymbolPainter::SymbolPainter()
{
  iconBackground = QApplication::palette().color(QPalette::Active, QPalette::Window);
}

QIcon SymbolPainter::createAirportIcon(const maptypes::MapAirport& airport, int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(iconBackground);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  SymbolPainter().drawAirportSymbol(&painter, airport, size / 2, size / 2, size * 7 / 10, false, false);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createVorIcon(const maptypes::MapVor& vor, int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(iconBackground);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  SymbolPainter().drawVorSymbol(&painter, vor, size / 2, size / 2, size * 7 / 10, false, false, false);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createNdbIcon(int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(iconBackground);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  SymbolPainter().drawNdbSymbol(&painter, size / 2, size / 2, size * 8 / 10, false, false);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createWaypointIcon(int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(iconBackground);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  SymbolPainter().drawWaypointSymbol(&painter, QColor(), size / 2, size / 2, size / 2, false, false);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createUserpointIcon(int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(iconBackground);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  SymbolPainter().drawUserpointSymbol(&painter, size / 2, size / 2, size / 2, false, false);
  return QIcon(pixmap);
}

void SymbolPainter::drawAirportSymbol(QPainter *painter, const maptypes::MapAirport& airport,
                                      int x, int y, int size, bool isAirportDiagram, bool fast)
{
  if(airport.longestRunwayLength == 0)
    size = size * 4 / 5;

  painter->save();
  QColor apColor = mapcolors::colorForAirport(airport);

  int radius = size / 2;
  painter->setBackgroundMode(Qt::OpaqueMode);

  if(airport.flags.testFlag(AP_HARD) && !airport.flags.testFlag(AP_MIL) && !airport.flags.testFlag(AP_CLOSED))
    // Use filled circle
    painter->setBrush(QBrush(apColor));
  else
    // Use white filled circle
    painter->setBrush(QBrush(mapcolors::airportSymbolFillColor));

  if((!fast || isAirportDiagram) && size > 5)
  {
    // Draw spikes only for larger symbols
    if(airport.anyFuel() && !airport.flags.testFlag(AP_MIL) && !airport.flags.testFlag(AP_CLOSED) && size > 6)
    {
      // Draw fuel spikes
      int fuelRadius = static_cast<int>(std::round(static_cast<double>(radius) * 1.4));
      if(fuelRadius < radius + 2)
        fuelRadius = radius + 2;
      painter->setPen(QPen(QBrush(apColor), size / 4, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(x, y - fuelRadius, x, y + fuelRadius);
      painter->drawLine(x - fuelRadius, y, x + fuelRadius, y);
    }
  }

  painter->setPen(QPen(QBrush(apColor), size / 5, Qt::SolidLine, Qt::FlatCap));
  painter->drawEllipse(QPoint(x, y), radius, radius);

  if((!fast || isAirportDiagram) && size > 5)
  {
    if(airport.flags.testFlag(AP_MIL))
      // Military airport
      painter->drawEllipse(QPoint(x, y), radius / 2, radius / 2);

    if(airport.waterOnly() && size > 6)
    {
      // Water only runways - draw an anchor
      int lineWidth = size / 7;
      painter->setPen(QPen(QBrush(apColor), lineWidth, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(x - lineWidth / 4, y - radius / 2, x - lineWidth / 4, y + radius / 2);
      painter->drawArc(x - radius / 2, y - radius / 2, radius, radius, 0 * 16, -180 * 16);
    }

    if(airport.helipadOnly() && size > 6)
    {
      // Only helipads - draw an H
      int lineWidth = size / 7;
      painter->setPen(QPen(QBrush(apColor), lineWidth, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(x - lineWidth / 4 - size / 5, y - radius / 2,
                        x - lineWidth / 4 - size / 5, y + radius / 2);

      painter->drawLine(x - lineWidth / 4 - size / 5, y,
                        x - lineWidth / 4 + size / 5, y);

      painter->drawLine(x - lineWidth / 4 + size / 5, y - radius / 2,
                        x - lineWidth / 4 + size / 5, y + radius / 2);
    }

    if(airport.flags.testFlag(AP_CLOSED) && size > 6)
    {
      // Cross out whatever was painted before
      painter->setPen(QPen(QBrush(apColor), size / 6.f, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(x - radius, y - radius, x + radius, y + radius);
      painter->drawLine(x - radius, y + radius, x + radius, y - radius);
    }
  }

  if((!fast || isAirportDiagram) && size > 5)
    if(airport.flags.testFlag(AP_HARD) && !airport.flags.testFlag(AP_MIL) &&
       !airport.flags.testFlag(AP_CLOSED) && size > 6)
    {
      // Draw line inside circle
      painter->translate(x, y);
      painter->rotate(airport.longestRunwayHeading);
      painter->setPen(QPen(QBrush(mapcolors::airportSymbolFillColor), size / 5, Qt::SolidLine, Qt::RoundCap));
      painter->drawLine(0, -radius + 2, 0, radius - 2);
      painter->resetTransform();
    }

  painter->restore();
}

void SymbolPainter::drawWaypointSymbol(QPainter *painter, const QColor& col, int x, int y, int size,
                                       bool fill, bool fast)
{
  painter->save();
  painter->setBackgroundMode(Qt::TransparentMode);
  if(fill)
    painter->setBrush(mapcolors::routeTextBoxColor);
  else
    painter->setBrush(Qt::NoBrush);

  float penSize = fast ? 6.f : 1.5f;

  if(col.isValid())
    painter->setPen(QPen(col, penSize, Qt::SolidLine, Qt::SquareCap));
  else
    painter->setPen(QPen(mapcolors::waypointSymbolColor, penSize, Qt::SolidLine, Qt::SquareCap));

  if(!fast && size > 5)
  {
    // Draw a triangle
    int radius = size / 2;
    QPolygon polygon;
    polygon << QPoint(x, y - radius) << QPoint(x + radius, y + radius) << QPoint(x - radius, y + radius);

    painter->drawConvexPolygon(polygon);
  }
  else
    painter->drawPoint(x, y);

  painter->restore();
}

void SymbolPainter::drawUserpointSymbol(QPainter *painter, int x, int y, int size, bool routeFill, bool fast)
{
  painter->save();
  painter->setBackgroundMode(Qt::TransparentMode);
  if(routeFill)
    painter->setBrush(mapcolors::routeTextBoxColor);
  else
    painter->setBrush(Qt::NoBrush);

  float penSize = fast ? 6.f : 2.f;

  painter->setPen(QPen(mapcolors::routeUserPointColor, penSize, Qt::SolidLine, Qt::SquareCap));

  if(!fast)
  {
    int radius = size / 2;
    painter->drawRect(x - radius, y - radius, size, size);
  }
  else
    painter->drawPoint(x, y);

  painter->restore();
}

void SymbolPainter::drawAircraftSymbol(QPainter *painter, int x, int y, int size, bool onGround)
{
  // Create a copy of the line vector
  QVector<QLine> lines(AIRCRAFTLINES);
  for(QLine& l : lines)
  {
    if(size != 40)
    {
      // Scale points of the copy to new size
      l.setP1(QPoint(l.x1() * size / 40, l.y1() * size / 40));
      l.setP2(QPoint(l.x2() * size / 40, l.y2() * size / 40));
    }

    if(x != 0 && y != 0)
      l.translate(x, y);
  }

  painter->setPen(onGround ? mapcolors::aircraftGroundBackPen : mapcolors::aircraftBackPen);
  painter->drawLines(lines);

  painter->setPen(onGround ? mapcolors::aircraftGroundFillPen : mapcolors::aircraftFillPen);
  painter->drawLines(lines);
}

void SymbolPainter::drawVorSymbol(QPainter *painter, const maptypes::MapVor& vor, int x, int y, int size,
                                  bool routeFill, bool fast, int largeSize)
{
  painter->save();
  painter->setBackgroundMode(Qt::TransparentMode);
  if(routeFill)
    painter->setBrush(mapcolors::routeTextBoxColor);
  else
    painter->setBrush(Qt::NoBrush);

  float penSize = fast ? 6.f : 1.5f;

  painter->setPen(QPen(mapcolors::vorSymbolColor, penSize, Qt::SolidLine, Qt::SquareCap));

  if(!fast)
  {
    painter->translate(x, y);

    if(largeSize > 0 && !vor.dmeOnly)
      // If compass ticks are drawn rotate center symbol too
      painter->rotate(vor.magvar);

    int radius = size / 2;
    if(vor.hasDme)
      // DME rectangle
      painter->drawRect(-size / 2, -size / 2, size, size);

    if(!vor.dmeOnly)
    {
      // Draw VOR symbol
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
      // Draw compass circle and ticks
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

  if(!fast)
  {
    if(size > 14)
      painter->setPen(QPen(mapcolors::vorSymbolColor, size / 4, Qt::SolidLine, Qt::RoundCap));
    else
      painter->setPen(QPen(mapcolors::vorSymbolColor, size / 3, Qt::SolidLine, Qt::RoundCap));
  }
  painter->drawPoint(x, y);

  painter->restore();
}

void SymbolPainter::drawNdbSymbol(QPainter *painter, int x, int y, int size, bool routeFill, bool fast)
{
  painter->save();

  painter->setBackgroundMode(Qt::TransparentMode);
  if(routeFill)
    painter->setBrush(mapcolors::routeTextBoxColor);
  else
    painter->setBrush(Qt::NoBrush);

  float penSize = fast ? 6.f : 1.5f;

  // Use dotted or solid line depending on size
  painter->setPen(QPen(mapcolors::ndbSymbolColor, penSize,
                       size > 12 ? Qt::DotLine : Qt::SolidLine, Qt::SquareCap));

  int radius = size / 2;

  if(!fast)
  {
    // Draw outer dotted/solid circle
    painter->drawEllipse(QPoint(x, y), radius, radius);

    if(size > 12)
      // If big enought draw inner dotted circle
      painter->drawEllipse(QPoint(x, y), radius * 2 / 3, radius * 2 / 3);
  }

  int pointSize = size > 12 ? size / 4 : size / 3;
  painter->setPen(QPen(mapcolors::ndbSymbolColor, pointSize, Qt::SolidLine, Qt::RoundCap));
  painter->drawPoint(x, y);

  painter->restore();
}

void SymbolPainter::drawMarkerSymbol(QPainter *painter, const maptypes::MapMarker& marker, int x, int y,
                                     int size, bool fast)
{
  painter->save();
  int radius = size / 2;

  painter->setBackgroundMode(Qt::TransparentMode);
  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(mapcolors::markerSymbolColor, 1.5, Qt::SolidLine, Qt::RoundCap));

  if(!fast && size > 5)
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
                                textflags::TextFlags flags, int size, bool fill)
{
  QStringList texts;

  if(flags & textflags::IDENT && flags & textflags::TYPE)
    texts.append(ndb.ident + " (" + (ndb.type == tr("COMPASS_POINT") ? tr("CP") : ndb.type) + ")");
  else if(flags & textflags::IDENT)
    texts.append(ndb.ident);

  if(flags & textflags::FREQ)
    texts.append(QLocale().toString(ndb.frequency / 100., 'f', 1));

  textatt::TextAttributes textAttrs = textatt::BOLD;
  if(flags & textflags::ROUTE_TEXT)
    textAttrs |= textatt::ROUTE_BG_COLOR;

  if(!flags.testFlag(textflags::ABS_POS))
  {
    y += size / 2 + painter->fontMetrics().ascent();
    textAttrs |= textatt::CENTER;
  }

  int transparency = fill ? 255 : 0;
  textBox(painter, texts, mapcolors::ndbSymbolColor, x, y, textAttrs, transparency);
}

void SymbolPainter::drawVorText(QPainter *painter, const maptypes::MapVor& vor, int x, int y,
                                textflags::TextFlags flags, int size, bool fill)
{
  QStringList texts;

  if(flags & textflags::IDENT && flags & textflags::TYPE)
    texts.append(vor.ident + " (" + vor.type.left(1) + ")");
  else if(flags & textflags::IDENT)
    texts.append(vor.ident);

  if(flags & textflags::FREQ)
    texts.append(QLocale().toString(vor.frequency / 1000., 'f', 2));

  textatt::TextAttributes textAttrs = textatt::BOLD;
  if(flags & textflags::ROUTE_TEXT)
    textAttrs |= textatt::ROUTE_BG_COLOR;

  if(!flags.testFlag(textflags::ABS_POS))
  {
    x -= size / 2 + 2;
    textAttrs |= textatt::RIGHT;
  }

  int transparency = fill ? 255 : 0;
  textBox(painter, texts, mapcolors::vorSymbolColor, x, y, textAttrs, transparency);
}

void SymbolPainter::drawWaypointText(QPainter *painter, const maptypes::MapWaypoint& wp, int x, int y,
                                     textflags::TextFlags flags, int size, bool fill)
{
  QStringList texts;

  if(flags & textflags::IDENT)
    texts.append(wp.ident);

  textatt::TextAttributes textAttrs = textatt::BOLD;
  if(flags & textflags::ROUTE_TEXT)
    textAttrs |= textatt::ROUTE_BG_COLOR;

  if(!flags.testFlag(textflags::ABS_POS))
  {
    x += size / 2 + 2;
    textAttrs |= textatt::LEFT;
  }
  int transparency = fill ? 255 : 0;

  textBox(painter, texts, mapcolors::waypointSymbolColor, x, y, textAttrs, transparency);
}

void SymbolPainter::drawAirportText(QPainter *painter, const maptypes::MapAirport& airport, int x, int y,
                                    textflags::TextFlags flags, int size, bool diagram)
{
  QStringList texts = airportTexts(flags, airport);
  if(!texts.isEmpty())
  {
    textatt::TextAttributes atts = textatt::BOLD;
    if(airport.flags.testFlag(maptypes::AP_ADDON))
      atts |= textatt::ITALIC | textatt::UNDERLINE;

    if(flags & textflags::ROUTE_TEXT)
      atts |= textatt::ROUTE_BG_COLOR;

    int transparency = diagram ? 130 : 255;
    if(airport.empty() && OptionData::instance().getFlags() & opts::MAP_EMPTY_AIRPORTS)
      transparency = 0;

    if(!flags.testFlag(textflags::ABS_POS))
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
                     tr("CT - ") + QLocale().toString(airport.towerFrequency / 1000., 'f', 3));

    QString autoWeather;
    if(airport.atisFrequency > 0)
      autoWeather = tr("ATIS ") + QLocale().toString(airport.atisFrequency / 1000., 'f', 3);
    else if(airport.awosFrequency > 0)
      autoWeather = tr("AWOS ") + QLocale().toString(airport.awosFrequency / 1000., 'f', 3);
    else if(airport.asosFrequency > 0)
      autoWeather = tr("ASOS ") + QLocale().toString(airport.asosFrequency / 1000., 'f', 3);

    if(!tower.isEmpty() || !autoWeather.isEmpty())
      texts.append(tower + (tower.isEmpty() ? QString() : " ") + autoWeather);

    if(airport.longestRunwayLength != 0 || airport.getPosition().getAltitude() != 0.f)
      texts.append(QLocale().toString(airport.getPosition().getAltitude()) + " " +
                   (airport.flags.testFlag(maptypes::AP_LIGHT) ? "L " : "- ") +
                   QLocale().toString(airport.longestRunwayLength / 100) + " " +
                   (airport.unicomFrequency == 0 ? QString() :
                    QLocale().toString(airport.unicomFrequency / 1000., 'f', 3)));
  }
  return texts;
}

void SymbolPainter::textBox(QPainter *painter, const QStringList& texts, const QPen& textPen, int x, int y,
                            textatt::TextAttributes atts, int transparency)
{
  if(texts.isEmpty())
    return;

  painter->save();

  QColor backColor;
  if(atts.testFlag(textatt::ROUTE_BG_COLOR))
    backColor = mapcolors::routeTextBoxColor;
  else
    backColor = mapcolors::textBoxColor;

  if(transparency != 255)
  {
    if(transparency == 0)
      // Do not fill at all
      painter->setBrush(Qt::NoBrush);
    else
    {
      // Use an alpha channel for semi transparency
      backColor.setAlpha(transparency);
      painter->setBrush(backColor);
    }
  }
  else
    // Fill background
    painter->setBrush(backColor);

  if(atts.testFlag(textatt::ITALIC) || atts.testFlag(textatt::BOLD) || atts.testFlag(textatt::UNDERLINE))
  {
    QFont f = painter->font();
    f.setBold(atts.testFlag(textatt::BOLD));
    f.setItalic(atts.testFlag(textatt::ITALIC));
    f.setUnderline(atts.testFlag(textatt::UNDERLINE));
    painter->setFont(f);
  }

  QFontMetrics metrics = painter->fontMetrics();
  int h = metrics.height();

  int yoffset = 0;
  if(transparency != 0)
  {
    // Draw filled rectangles in the background
    painter->setPen(mapcolors::textBackgroundPen);
    for(const QString& text : texts)
    {
      QRect rect = metrics.boundingRect(text);
      rect.setWidth(rect.width() + 2);

      int newx = x;
      if(atts.testFlag(textatt::RIGHT))
        newx -= rect.width();
      else if(atts.testFlag(textatt::CENTER))
        newx -= rect.width() / 2;

      rect.moveTo(newx, y - metrics.ascent() + yoffset - 1);
      painter->drawRect(rect);
      yoffset += h;
    }
  }

  // Draw the text
  yoffset = 0;
  painter->setPen(textPen);
  for(const QString& t : texts)
  {
    int w = metrics.width(t);
    int newx = x;
    if(atts.testFlag(textatt::RIGHT))
      newx -= w;
    else if(atts.testFlag(textatt::CENTER))
      newx -= w / 2;

    painter->drawText(newx, y + yoffset, t);
    yoffset += h;
  }
  painter->restore();
}

QRect SymbolPainter::textBoxSize(QPainter *painter, const QStringList& texts, textatt::TextAttributes atts)
{
  QRect retval;
  if(texts.isEmpty())
    return retval;

  painter->save();

  if(atts.testFlag(textatt::ITALIC) || atts.testFlag(textatt::BOLD) || atts.testFlag(textatt::UNDERLINE))
  {
    QFont f = painter->font();
    f.setBold(atts.testFlag(textatt::BOLD));
    f.setItalic(atts.testFlag(textatt::ITALIC));
    f.setUnderline(atts.testFlag(textatt::UNDERLINE));
    painter->setFont(f);
  }

  QFontMetrics metrics = painter->fontMetrics();
  int h = metrics.height();

  // Increase text box size for each bounding rectangle of text
  int yoffset = 0;
  for(const QString& t : texts)
  {
    int w = metrics.width(t);
    int newx = 0;
    if(atts.testFlag(textatt::RIGHT))
      newx -= w;
    else if(atts.testFlag(textatt::CENTER))
      newx -= w / 2;

    int textW = metrics.width(t);
    if(retval.isNull())
      retval = QRect(newx, yoffset, textW, h);
    else
      retval = retval.united(QRect(newx, yoffset, textW, h));
    // painter->drawText(newx, y + yoffset, t);
    yoffset += h;
  }
  painter->restore();
  return retval;
}
