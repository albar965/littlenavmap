/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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
#include "query/mapquery.h"
#include "common/mapcolors.h"
#include "options/optiondata.h"
#include "common/unit.h"
#include "geo/calculations.h"
#include "util/paintercontextsaver.h"

#include <QPainter>
#include <QApplication>
#include <marble/GeoPainter.h>

using namespace Marble;
using namespace map;

/* Simulator aircraft symbol */
const QVector<QLine> AIRCRAFTLINES({QLine(0, -20, 0, 16), // Body
                                    QLine(-20, 2, 0, -6), QLine(0, -6, 20, 2), // Wings
                                    QLine(-10, 18, 0, 14), QLine(0, 14, 10, 18) // Horizontal stabilizer
                                   });

SymbolPainter::SymbolPainter()
{
}

SymbolPainter::~SymbolPainter()
{

}

QIcon SymbolPainter::createAirportIcon(const map::MapAirport& airport, int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  SymbolPainter().drawAirportSymbol(&painter, airport, size / 2, size / 2, size * 7 / 10, false, false);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createVorIcon(const map::MapVor& vor, int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  SymbolPainter().drawVorSymbol(&painter, vor, size / 2, size / 2, size * 7 / 10, false, false, false);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createNdbIcon(int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  SymbolPainter().drawNdbSymbol(&painter, size / 2, size / 2, size * 8 / 10, false, false);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createWaypointIcon(int size, const QColor& color)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  SymbolPainter().drawWaypointSymbol(&painter, color, size / 2, size / 2, size / 2, false, false);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createUserpointIcon(int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  SymbolPainter().drawUserpointSymbol(&painter, size / 2, size / 2, size / 2, false, false);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createProcedurePointIcon(int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  SymbolPainter().drawProcedureSymbol(&painter, size / 2, size / 2, size / 2, false, false);
  return QIcon(pixmap);
}

QIcon SymbolPainter::createAirspaceIcon(const map::MapAirspace& airspace, int size)
{
  QPixmap pixmap(size, size);
  pixmap.fill(QColor(Qt::transparent));
  QPainter painter(&pixmap);
  prepareForIcon(painter);

  painter.setBackgroundMode(Qt::TransparentMode);
  painter.setPen(mapcolors::penForAirspace(airspace));
  painter.setBrush(mapcolors::colorForAirspaceFill(airspace));
  painter.drawEllipse(2, 2, size - 4, size - 4);
  return QIcon(pixmap);
}

void SymbolPainter::drawAirportSymbol(QPainter *painter, const map::MapAirport& airport,
                                      float x, float y, int size, bool isAirportDiagram, bool fast)
{
  float symsize = atools::roundToInt(size);

  if(airport.longestRunwayLength == 0 && !airport.helipad())
    // Reduce size for airports without runways and without helipads
    symsize = symsize * 4 / 5;

  atools::util::PainterContextSaver saver(painter);
  Q_UNUSED(saver);
  QColor apColor = mapcolors::colorForAirport(airport);

  float radius = symsize / 2.f;
  painter->setBackgroundMode(Qt::OpaqueMode);

  if(airport.flags.testFlag(AP_HARD) && !airport.flags.testFlag(AP_MIL) && !airport.flags.testFlag(AP_CLOSED))
    // Use filled circle
    painter->setBrush(QBrush(apColor));
  else
    // Use white filled circle
    painter->setBrush(QBrush(mapcolors::airportSymbolFillColor));

  if((!fast || isAirportDiagram) && symsize > 5)
  {
    // Draw spikes only for larger symbols
    if(airport.anyFuel() && !airport.flags.testFlag(AP_MIL) && !airport.flags.testFlag(AP_CLOSED) && symsize > 6)
    {
      // Draw fuel spikes
      float fuelRadius = radius * 1.4f;
      if(fuelRadius < radius + 2.f)
        fuelRadius = radius + 2.f;
      painter->setPen(QPen(QBrush(apColor), symsize / 4.f, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(QPointF(x, y - fuelRadius), QPointF(x, y + fuelRadius));
      painter->drawLine(QPointF(x - fuelRadius, y), QPointF(x + fuelRadius, y));
    }
  }

  painter->setPen(QPen(QBrush(apColor), symsize / 5, Qt::SolidLine, Qt::FlatCap));
  painter->drawEllipse(QPointF(x, y), radius, radius);

  if((!fast || isAirportDiagram) && symsize > 5.f)
  {
    if(airport.flags.testFlag(AP_MIL))
      // Military airport
      painter->drawEllipse(QPointF(x, y), radius / 2.f, radius / 2.f);

    if(airport.waterOnly() && symsize > 6.f)
    {
      // Water only runways - draw an anchor
      float lineWidth = symsize / 7.f;
      painter->setPen(QPen(QBrush(apColor), lineWidth, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(QPointF(x, y - radius / 2.f), QPointF(x, y + radius / 2.f));
      painter->drawArc(QRectF(x - radius / 2.f, y - radius / 2.f, radius, radius),
                       0 * 16, // From
                       -180 * 16); // to
    }

    if(airport.helipadOnly() && symsize > 6.f)
    {
      // Only helipads - draw an H
      float lineWidth = symsize / 7.f;
      painter->setPen(QPen(QBrush(apColor), lineWidth, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(QLineF(x - symsize / 5.f, y - radius / 2.f,
                               x - symsize / 5.f, y + radius / 2.f));

      painter->drawLine(QLineF(x - symsize / 5.f, y,
                               x + symsize / 5.f, y));

      painter->drawLine(QLineF(x + symsize / 5.f, y - radius / 2.f,
                               x + symsize / 5.f, y + radius / 2.f));
    }

    if(airport.flags.testFlag(AP_CLOSED) && symsize > 6.f)
    {
      // Cross out whatever was painted before
      painter->setPen(QPen(QBrush(apColor), symsize / 6.f, Qt::SolidLine, Qt::FlatCap));
      painter->drawLine(QLineF(x - radius, y - radius, x + radius, y + radius));
      painter->drawLine(QLineF(x - radius, y + radius, x + radius, y - radius));
    }
  }

  if((!fast || isAirportDiagram) && symsize > 5)
  {
    if(airport.flags.testFlag(AP_HARD) && !airport.flags.testFlag(AP_MIL) &&
       !airport.flags.testFlag(AP_CLOSED) && symsize > 6)
    {
      // Draw line inside circle
      painter->translate(x, y);
      painter->rotate(airport.longestRunwayHeading);
      painter->setPen(QPen(QBrush(mapcolors::airportSymbolFillColor), symsize / 5, Qt::SolidLine, Qt::RoundCap));
      painter->drawLine(QLineF(0, -radius + 2, 0, radius - 2));
      painter->resetTransform();
    }
  }
}

void SymbolPainter::drawWaypointSymbol(QPainter *painter, const QColor& col, int x, int y, int size,
                                       bool fill, bool fast)
{
  atools::util::PainterContextSaver saver(painter);
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
}

void SymbolPainter::drawWindPointer(QPainter *painter, float x, float y, int size, float dir)
{
  atools::util::PainterContextSaver saver(painter);
  painter->setBackgroundMode(Qt::TransparentMode);

  painter->translate(x, y + size / 2);
  painter->rotate(atools::geo::normalizeCourse(dir + 180.f));
  painter->drawPixmap(-size / 2, -size / 2, *windPointerFromCache(size));
  painter->resetTransform();
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

void SymbolPainter::drawUserpointSymbol(QPainter *painter, int x, int y, int size, bool routeFill, bool fast)
{
  atools::util::PainterContextSaver saver(painter);
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
}

void SymbolPainter::drawProcedureSymbol(QPainter *painter, int x, int y, int size, bool routeFill, bool fast)
{
  atools::util::PainterContextSaver saver(painter);
  painter->setBackgroundMode(Qt::TransparentMode);
  if(routeFill)
    painter->setBrush(mapcolors::routeTextBoxColor);
  else
    painter->setBrush(Qt::NoBrush);

  float penSize = fast ? 6.f : 3.f;

  painter->setPen(QPen(mapcolors::routeProcedurePointColor, penSize, Qt::SolidLine, Qt::SquareCap));

  if(!fast)
  {
    int radius = size / 2;
    painter->drawEllipse(x - radius, y - radius, size, size);
  }
  else
    painter->drawPoint(x, y);
}

void SymbolPainter::drawProcedureFlyover(QPainter *painter, int x, int y, int size)
{
  atools::util::PainterContextSaver saver(painter);
  Q_UNUSED(saver);
  painter->setBackgroundMode(Qt::OpaqueMode);

  painter->setPen(mapcolors::routeProcedurePointFlyoverPen);
  painter->setBrush(Qt::NoBrush);

  int radius = size / 2;
  painter->drawEllipse(x - radius, y - radius, size, size);
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

void SymbolPainter::drawVorSymbol(QPainter *painter, const map::MapVor& vor, int x, int y, int size,
                                  bool routeFill, bool fast, int largeSize)
{
  atools::util::PainterContextSaver saver(painter);
  Q_UNUSED(saver);

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

    float sizeF = static_cast<float>(size);
    float radius = sizeF / 2.f;

    if(vor.tacan || vor.vortac)
    {
      // Draw TACAN symbol or VORTAC outline
      // Coordinates take from SVG graphics
      sizeF += 2.f;

      QPolygonF polygon;
      polygon
        << QPointF((420. - 438.) * sizeF / 94., (538. - 583.) * sizeF / 81.)
        << QPointF((439. - 438.) * sizeF / 94., (527. - 583.) * sizeF / 81.)
        << QPointF((425. - 438.) * sizeF / 94., (503. - 583.) * sizeF / 81.)
        << QPointF((406. - 438.) * sizeF / 94., (513. - 583.) * sizeF / 81.)
        << QPointF((378. - 438.) * sizeF / 94., (513. - 583.) * sizeF / 81.)
        << QPointF((359. - 438.) * sizeF / 94., (502. - 583.) * sizeF / 81.)
        << QPointF((345. - 438.) * sizeF / 94., (526. - 583.) * sizeF / 81.)
        << QPointF((364. - 438.) * sizeF / 94., (538. - 583.) * sizeF / 81.)
        << QPointF((378. - 438.) * sizeF / 94., (562. - 583.) * sizeF / 81.)
        << QPointF((378. - 438.) * sizeF / 94., (583. - 583.) * sizeF / 81.)
        << QPointF((406. - 438.) * sizeF / 94., (583. - 583.) * sizeF / 81.)
        << QPointF((406. - 438.) * sizeF / 94., (562. - 583.) * sizeF / 81.)
        << QPointF((420. - 438.) * sizeF / 94., (538. - 583.) * sizeF / 81.);
      double tx = polygon.boundingRect().width() / 2.;
      double ty = polygon.boundingRect().height() / 2. + 1.;
      polygon.translate(tx, ty);
      painter->drawConvexPolygon(polygon);

      if(vor.vortac)
      {
        // Draw the filled VORTAC blocks
        painter->setBrush(mapcolors::vorSymbolColor);
        painter->setPen(QPen(mapcolors::vorSymbolColor, 1, Qt::SolidLine, Qt::SquareCap));

        polygon.clear();
        polygon
          << QPointF((378. - 438.) * sizeF / 94., (513. - 583.) * sizeF / 81.)
          << QPointF((359. - 438.) * sizeF / 94., (502. - 583.) * sizeF / 81.)
          << QPointF((345. - 438.) * sizeF / 94., (526. - 583.) * sizeF / 81.)
          << QPointF((364. - 438.) * sizeF / 94., (538. - 583.) * sizeF / 81.);
        polygon.translate(tx, ty);
        painter->drawConvexPolygon(polygon);

        polygon.clear();
        polygon
          << QPointF((439. - 438.) * sizeF / 94., (527. - 583.) * sizeF / 81.)
          << QPointF((420. - 438.) * sizeF / 94., (538. - 583.) * sizeF / 81.)
          << QPointF((406. - 438.) * sizeF / 94., (513. - 583.) * sizeF / 81.)
          << QPointF((424. - 438.) * sizeF / 94., (503. - 583.) * sizeF / 81.);
        polygon.translate(tx, ty);
        painter->drawConvexPolygon(polygon);

        polygon.clear();
        polygon
          << QPointF((406. - 438.) * sizeF / 94., (562. - 583.) * sizeF / 81.)
          << QPointF((406. - 438.) * sizeF / 94., (583. - 583.) * sizeF / 81.)
          << QPointF((378. - 438.) * sizeF / 94., (583. - 583.) * sizeF / 81.)
          << QPointF((378. - 438.) * sizeF / 94., (562. - 583.) * sizeF / 81.);
        polygon.translate(tx, ty);
        painter->drawConvexPolygon(polygon);
      }
    }
    else
    {
      if(vor.hasDme)
        // DME rectangle
        painter->drawRect(QRectF(-sizeF / 2.f, -sizeF / 2.f, sizeF, sizeF));

      if(!vor.dmeOnly)
      {
        // Draw VOR symbol
        float corner = 2.f;
        QPolygonF polygon;
        polygon << QPointF(-radius / corner, -radius)
                << QPointF(radius / corner, -radius)
                << QPointF(radius, 0.f)
                << QPointF(radius / corner, radius)
                << QPointF(-radius / corner, radius)
                << QPointF(-radius, 0.f);

        painter->drawConvexPolygon(polygon);
      }
    }

    if(largeSize > 0 && !vor.dmeOnly)
    {
      // Draw compass circle and ticks
      painter->setBrush(Qt::NoBrush);
      painter->setPen(QPen(mapcolors::vorSymbolColor, 1.f, Qt::SolidLine, Qt::SquareCap));
      painter->drawEllipse(QPointF(0.f, 0.f), radius * 5.f, radius * 5.f);

      for(int i = 0; i < 360; i += 10)
      {
        if(i == 0)
          painter->drawLine(QLineF(0.f, 0.f, 0.f, -radius * 5.f));
        else if((i % 90) == 0)
          painter->drawLine(QLineF(0.f, static_cast<int>(-radius * 4.f), 0.f, -radius * 5.f));
        else
          painter->drawLine(QLineF(0.f, static_cast<int>(-radius * 4.5f), 0.f, -radius * 5.f));
        painter->rotate(10.f);
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
}

void SymbolPainter::drawNdbSymbol(QPainter *painter, int x, int y, int size, bool routeFill, bool fast)
{
  atools::util::PainterContextSaver saver(painter);
  float sizeF = static_cast<float>(size);

  painter->setBackgroundMode(Qt::TransparentMode);
  if(routeFill)
    painter->setBrush(mapcolors::routeTextBoxColor);
  else
    painter->setBrush(Qt::NoBrush);

  float penSize = fast ? 6.f : 1.5f;

  // Use dotted or solid line depending on size
  painter->setPen(QPen(mapcolors::ndbSymbolColor, penSize,
                       sizeF > 12.f ? Qt::DotLine : Qt::SolidLine, Qt::SquareCap));

  float radius = sizeF / 2.f;

  if(!fast)
  {
    // Draw outer dotted/solid circle
    painter->drawEllipse(QPointF(x, y), radius, radius);

    if(sizeF > 12.f)
      // If big enought draw inner dotted circle
      painter->drawEllipse(QPointF(x, y), radius * 2.f / 3.f, radius * 2.f / 3.f);
  }

  float pointSize = sizeF > 12 ? sizeF / 4.f : sizeF / 3.f;
  painter->setPen(QPen(mapcolors::ndbSymbolColor, pointSize, Qt::SolidLine, Qt::RoundCap));
  painter->drawPoint(x, y);
}

void SymbolPainter::drawMarkerSymbol(QPainter *painter, const map::MapMarker& marker, int x, int y,
                                     int size, bool fast)
{
  atools::util::PainterContextSaver saver(painter);
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
}

void SymbolPainter::drawNdbText(QPainter *painter, const map::MapNdb& ndb, int x, int y,
                                textflags::TextFlags flags, int size, bool fill,
                                const QStringList *addtionalText)
{
  QStringList texts;

  if(flags & textflags::IDENT && flags & textflags::TYPE)
  {
    if(ndb.type.isEmpty())
      texts.append(ndb.ident);
    else
      texts.append(ndb.ident + " (" + (ndb.type == "CP" ? tr("CL") : ndb.type) + ")");
  }
  else if(flags & textflags::IDENT)
    texts.append(ndb.ident);

  if(flags & textflags::FREQ)
    texts.append(QString::number(ndb.frequency / 100., 'f', 1));

  textatt::TextAttributes textAttrs = textatt::BOLD;
  if(flags & textflags::ROUTE_TEXT)
    textAttrs |= textatt::ROUTE_BG_COLOR;

  if(!flags.testFlag(textflags::ABS_POS))
  {
    y += size / 2 + painter->fontMetrics().ascent();
    textAttrs |= textatt::CENTER;
  }

  if(addtionalText != nullptr)
    texts.append(*addtionalText);

  int transparency = fill ? 255 : 0;
  textBox(painter, texts, mapcolors::ndbSymbolColor, x, y, textAttrs, transparency);
}

void SymbolPainter::drawVorText(QPainter *painter, const map::MapVor& vor, int x, int y,
                                textflags::TextFlags flags, int size, bool fill,
                                const QStringList *addtionalText)
{
  QStringList texts;

  if(flags & textflags::IDENT && flags & textflags::TYPE)
    texts.append(vor.ident + " (" + vor.type.left(1) + ")");
  else if(flags & textflags::IDENT)
    texts.append(vor.ident);

  if(flags & textflags::FREQ)
  {
    if(!vor.tacan)
      texts.append(QString::number(vor.frequency / 1000., 'f', 2));
    if(vor.tacan /*|| vor.vortac*/)
      texts.append(vor.channel);
  }

  textatt::TextAttributes textAttrs = textatt::BOLD;
  if(flags & textflags::ROUTE_TEXT)
    textAttrs |= textatt::ROUTE_BG_COLOR;

  if(!flags.testFlag(textflags::ABS_POS))
  {
    x -= size / 2 + 2;
    textAttrs |= textatt::RIGHT;
  }

  if(addtionalText != nullptr)
    texts.append(*addtionalText);

  int transparency = fill ? 255 : 0;
  textBox(painter, texts, mapcolors::vorSymbolColor, x, y, textAttrs, transparency);
}

void SymbolPainter::drawWaypointText(QPainter *painter, const map::MapWaypoint& wp, int x, int y,
                                     textflags::TextFlags flags, int size, bool fill,
                                     const QStringList *addtionalText)
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

  if(addtionalText != nullptr)
    texts.append(*addtionalText);

  int transparency = fill ? 255 : 0;
  textBox(painter, texts, mapcolors::waypointSymbolColor, x, y, textAttrs, transparency);
}

void SymbolPainter::drawAirportText(QPainter *painter, const map::MapAirport& airport, float x, float y,
                                    opts::DisplayOptions dispOpts, textflags::TextFlags flags, int size,
                                    bool diagram, int maxTextLength)
{
  QStringList texts = airportTexts(dispOpts, flags, airport, maxTextLength);
  if(!texts.isEmpty())
  {
    textatt::TextAttributes atts = textatt::BOLD;
    if(airport.flags.testFlag(map::AP_ADDON))
      atts |= textatt::ITALIC | textatt::UNDERLINE;

    if(flags & textflags::ROUTE_TEXT)
      atts |= textatt::ROUTE_BG_COLOR;

    int transparency = diagram ? 130 : 255;
    if(airport.emptyDraw() && !(flags & textflags::ROUTE_TEXT))
      transparency = 0;

    if(!flags.testFlag(textflags::ABS_POS))
      x += size + 2.f;

    if(flags & textflags::NO_BACKGROUND)
      transparency = 0;

    textBoxF(painter, texts, mapcolors::colorForAirport(airport), x, y, atts, transparency);
  }
}

QStringList SymbolPainter::airportTexts(opts::DisplayOptions dispOpts, textflags::TextFlags flags,
                                        const map::MapAirport& airport, int maxTextLength)
{
  QStringList texts;

  if(flags & textflags::IDENT && flags & textflags::NAME && dispOpts & opts::ITEM_AIRPORT_NAME)
    texts.append(atools::elideTextShort(airport.name, maxTextLength) + " (" + airport.ident + ")");
  else if(flags & textflags::IDENT)
    texts.append(airport.ident);
  else if(flags & textflags::NAME)
    texts.append(airport.name);

  if(flags & textflags::INFO)
  {
    if(airport.towerFrequency != 0 && dispOpts & opts::ITEM_AIRPORT_TOWER)
      texts.append(tr("CT ") + QString::number(airport.towerFrequency / 1000., 'f', 3));

    QString autoWeather;
    if(dispOpts & opts::ITEM_AIRPORT_ATIS)
    {
      if(airport.atisFrequency > 0)
        autoWeather = tr("ATIS ") + QString::number(airport.atisFrequency / 1000., 'f', 3);
      else if(airport.awosFrequency > 0)
        autoWeather = tr("AWOS ") + QString::number(airport.awosFrequency / 1000., 'f', 3);
      else if(airport.asosFrequency > 0)
        autoWeather = tr("ASOS ") + QString::number(airport.asosFrequency / 1000., 'f', 3);
    }

    if(!autoWeather.isEmpty())
      texts.append(autoWeather);

    // bool elevUnit = Unit::getUnitAltStr() != Unit::getUnitShortDistStr();
    if(dispOpts & opts::ITEM_AIRPORT_RUNWAY)
      if(airport.longestRunwayLength != 0 || airport.getPosition().getAltitude() != 0.f)
        texts.append(Unit::altFeet(airport.getPosition().getAltitude(),
                                   true /*addUnit*/, true /*narrow*/) + " " +
                     (airport.flags.testFlag(map::AP_LIGHT) ? "L " : "- ") +
                     Unit::distShortFeet(airport.longestRunwayLength,
                                         true /*addUnit*/, true /*narrow*/) + " "
                     // + (airport.unicomFrequency == 0 ? QString() :
                     // QString::number(airport.unicomFrequency / 1000., 'f', 3))
                     );
  }
  return texts;
}

void SymbolPainter::textBox(QPainter *painter, const QStringList& texts, const QPen& textPen, int x, int y,
                            textatt::TextAttributes atts, int transparency)
{
  textBoxF(painter, texts, textPen, x, y, atts, transparency);
}

void SymbolPainter::textBoxF(QPainter *painter, const QStringList& texts, const QPen& textPen,
                             float x, float y, textatt::TextAttributes atts, int transparency)
{
  if(texts.isEmpty())
    return;

  atools::util::PainterContextSaver saver(painter);
  Q_UNUSED(saver);

  QColor backColor;
  if(atts.testFlag(textatt::ROUTE_BG_COLOR))
    backColor = mapcolors::routeTextBoxColor;
  else
    backColor = mapcolors::textBoxColor;

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
    }
  }
  else // Fill background
  {
    painter->setBackgroundMode(Qt::OpaqueMode);
    painter->setBrush(backColor);
    painter->setBackground(backColor);
  }

  if(atts.testFlag(textatt::ITALIC) || atts.testFlag(textatt::BOLD) || atts.testFlag(textatt::UNDERLINE) ||
     atts.testFlag(textatt::OVERLINE))
  {
    QFont f = painter->font();
    f.setBold(atts.testFlag(textatt::BOLD));
    f.setItalic(atts.testFlag(textatt::ITALIC));
    f.setUnderline(atts.testFlag(textatt::UNDERLINE));
    f.setOverline(atts.testFlag(textatt::OVERLINE));
    painter->setFont(f);
  }

  // Draw the text
  QFontMetricsF metrics = painter->fontMetrics();
  float h = static_cast<float>(metrics.height()) - 1.f;
  float yoffset = (texts.size() * h) / 2.f - static_cast<float>(metrics.descent());
  painter->setPen(textPen);

  // Draw text in reverse order to avoid undercut
  for(int i = texts.size() - 1; i >= 0; i--)
  {
    const QString& text = texts.at(i);
    if(text.isEmpty())
      continue;

    float w = static_cast<float>(metrics.width(text));
    float newx = x;
    if(atts.testFlag(textatt::RIGHT))
      newx -= w;
    else if(atts.testFlag(textatt::CENTER))
      newx -= w / 2.f;

    painter->drawText(QPointF(newx, y + yoffset), text);
    yoffset -= h;
  }
}

QRect SymbolPainter::textBoxSize(QPainter *painter, const QStringList& texts, textatt::TextAttributes atts)
{
  QRect retval;
  if(texts.isEmpty())
    return retval;

  atools::util::PainterContextSaver saver(painter);

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
  return retval;
}

const QPixmap *SymbolPainter::windPointerFromCache(int size)
{
  if(windPointerPixmaps.contains(size))
    return windPointerPixmaps.object(size);
  else
  {
    QPixmap *newPx =
      new QPixmap(QIcon(":/littlenavmap/resources/icons/windpointer.svg").pixmap(QSize(size, size)));
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
    QPixmap *newPx =
      new QPixmap(QIcon(":/littlenavmap/resources/icons/trackline.svg").pixmap(QSize(size, size)));
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
