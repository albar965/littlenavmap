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

#include "mappainter/mappainterairspace.h"

#include "common/mapcolors.h"
#include "common/textplacement.h"
#include "util/paintercontextsaver.h"
#include "route/route.h"
#include "mapgui/maplayer.h"
#include "airspace/airspacecontroller.h"
#include "app/navapp.h"
#include "mapgui/mapscale.h"
#include "util/polygontools.h"

#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>
#include <marble/GeoDataLinearRing.h>
#include <marble/ViewportParams.h>

#include <QElapsedTimer>

using namespace Marble;
using namespace atools::geo;
using namespace map;

MapPainterAirspace::MapPainterAirspace(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainter(mapWidget, mapScale, paintContext)
{
}

MapPainterAirspace::~MapPainterAirspace()
{

}

void MapPainterAirspace::render()
{
  // Tolerances to detect straight line - trying all values on array until a sufficiently long line was found
  const static float MAX_ANGLES[] = {5.f, 30.f};

  AirspaceController *controller = NavApp::getAirspaceController();

  if(!context->mapLayer->isAnyAirspace() || !(context->objectTypes.testFlag(map::AIRSPACE)))
    return;

  context->startTimer("Airspace");

  // Get online and offline airspace and merge then into one list =============
  const GeoDataLatLonAltBox& curBox = context->viewport->viewLatLonAltBox();
  AirspaceVector airspaces;

  bool overflow = false;
  controller->getAirspaces(airspaces, curBox, context->mapLayer, context->airspaceFilterByLayer, context->route->getCruiseAltitudeFt(),
                           context->viewContext == Marble::Animation, map::AIRSPACE_SRC_ALL, overflow);
  context->setQueryOverflow(overflow);

  const OptionData& optionData = OptionData::instance();
  int displayThicknessAirspace = optionData.getDisplayThicknessAirspace();
  int displayTransparencyAirspace = optionData.getDisplayTransparencyAirspace();

  // Collect visible airspaces ==================================================================================
  struct DrawAirspace
  {
    DrawAirspace(const MapAirspace *airspaceParam, const QVector<QPolygonF *> polygonsParam)
      : airspace(airspaceParam), polygons(std::move(polygonsParam))
    {
    }

    const MapAirspace *airspace;
    const QVector<QPolygonF *> polygons;
  };

  QVector<DrawAirspace> visibleAirspaces;
  if(!airspaces.isEmpty())
  {
    Marble::GeoPainter *painter = context->painter;

    painter->setBackgroundMode(Qt::TransparentMode);

    // Draw airspace polygons ==================================================================================
    for(const MapAirspace *airspace : qAsConst(airspaces))
    {
      if(!(airspace->type & context->airspaceFilterByLayer.types))
        continue;

      if(!airspace->hasValidGeometry())
        continue;

      const QPen airspacePen = mapcolors::penForAirspace(*airspace, displayThicknessAirspace);
      QPen pen = airspacePen; // Modified for online airspaces

      // Check if coordinates overlap with viewport
      if(context->viewportRect.overlaps(airspace->bounding))
      {
        if(context->objCount())
          return;

        // Get cached geometry =====================
        const LineString *lineString = controller->getAirspaceGeometry(airspace->combinedId());
        if(lineString != nullptr)
        {
          if(airspace->isOnline())
            // Make online airspace line thicker
            pen.setWidthF(airspacePen.widthF() * 1.5);

          painter->setPen(pen);

          // Fast draw does not fill
          if(!context->drawFast)
            painter->setBrush(mapcolors::colorForAirspaceFill(*airspace, displayTransparencyAirspace));

          // Convert to screen polygons probably cutting them and removing duplicate points =====================
          const QVector<QPolygonF *> polygons = createPolygons(*lineString, context->screenRect);

          // Add for text placement later
          visibleAirspaces.append(DrawAirspace(airspace, polygons));

#ifdef DEBUG_DUMP_AIRSPACE
          static QSet<int> airspacesDumped;

          if(!airspacesDumped.contains(airspace->id))
          {
            airspacesDumped.insert(airspace->id);
            QString debug("\n" + airspace->name + "," + airspace->multipleCode + "\n");

            int i = 0;
            for(const QPolygonF *poly : polygons)
            {
              debug.append("\nQPolygonF polygon({\n");
              for(const QPointF& pt : *poly)
                ///* 13 */ {0, 3}, /* -> 3 */
                debug.append(QString("/* %1 */ {%2, %3}, /* ->  */\n").arg(i++).arg(pt.x(), 0, 'f', 1).arg(pt.y(), 0, 'f', 1));
              debug.append("});\n");
            }
            qDebug().noquote().nospace() << Q_FUNC_INFO << debug;
          }
#endif

          // Draw airspace polygon =================================================================
          for(int i = 0; i < polygons.size(); i++)
          {
#ifdef DEBUG_COLOR_AIRSPACE_POLYS
            if(i == 0)
              painter->setPen(QPen(QColor(255, 0, 0, 128), 10.));
            else if(i == 1)
              painter->setPen(QPen(QColor(0, 255, 0, 128), 10.));
            else if(i == 2)
              painter->setPen(QPen(QColor(0, 0, 255, 128), 10.));
#endif
            drawPolygon(painter, *polygons.at(i));
          }
        } // if(lineString != nullptr)
      } // if(context->viewportRect.overlaps(airspace->bounding))

      // Draw online airspace circle =================================================================
      if(airspace->isOnline())
      {
        // Draw center circle for online airspace with less transparency and darker
        QBrush brush = painter->brush();
        QColor color = brush.color();
        color.setAlphaF(color.alphaF() * 2.f);
        brush.setColor(color.darker(200));
        painter->setBrush(brush);

        pen.setWidthF(airspacePen.widthF() * 2.);
        painter->setPen(pen);

        // Draw circle with 1 NM and at least 3 pixel radius
        drawCircle(painter, airspace->position, std::max(scale->getPixelForNm(0.5f), 3.f));
      }
    } // for(const MapAirspace *airspace : qAsConst(airspaces))

    // Draw airspace labels ==================================================================================
    // Collection from options dialog
    bool name = optionData.getDisplayOptionsAirspace().testFlag(optsd::AIRSPACE_NAME),
         restrictiveName = optionData.getDisplayOptionsAirspace().testFlag(optsd::AIRSPACE_RESTRICTIVE_NAME),
         type = optionData.getDisplayOptionsAirspace().testFlag(optsd::AIRSPACE_TYPE),
         altitude = optionData.getDisplayOptionsAirspace().testFlag(optsd::AIRSPACE_ALTITUDE),
         com = optionData.getDisplayOptionsAirspace().testFlag(optsd::AIRSPACE_COM);

    // Do not draw while moving . Also all text options have to be enabled
    if(context->viewContext == Marble::Still && (name || restrictiveName || type || altitude || com) && !context->drawFast)
    {
      QFontMetrics metrics(painter->fontMetrics());
      context->szFont(context->textSizeAirspace * context->mapLayer->getAirspaceFontScale());

      // Prepare text placement without arrows
      TextPlacement textPlacement(painter, this, context->screenRect);
      textPlacement.setArrowForEmpty(false);
      textPlacement.setArrowLeft(QString());
      textPlacement.setArrowRight(QString());
      textPlacement.setTextOnTopOfLine(false);

      // Look at all visible airspaces collected earlier ===============================
      painter->setBrush(mapcolors::textBoxColorAirspace);
      painter->setBackground(mapcolors::textBoxColorAirspace);
      painter->setBackgroundMode(Qt::OpaqueMode);

      for(const DrawAirspace& visibleAirspace : qAsConst(visibleAirspaces))
      {
        const map::MapAirspace *airspace = visibleAirspace.airspace;

        // Check if layer option enables text display for this airspace type
        if(airspace->type & context->airspaceTextsByLayer)
        {
          // Build text depending on options
          QString airspaceText =
            atools::strJoin(map::airspaceNameMap(*airspace, 20, name, restrictiveName, type, altitude, com), tr(" / "));

          if(!airspaceText.isEmpty())
          {
            QPen textPen = mapcolors::penForAirspace(*airspace, displayThicknessAirspace);
            textPen.setColor(textPen.color().darker(150));
            painter->setPen(textPen);

            // Iterate over different angle tolerances which combine segments to one
            bool drawn = false;
            for(float maxAngle : MAX_ANGLES)
            {
              // Already painted label - done and exit loop
              if(drawn)
                break;

              // Airspace can consist of more than one polygon for Mercator
              for(const QPolygonF *polygon : visibleAirspace.polygons)
              {
                // Already painted label - exit loop
                if(drawn)
                  break;

                // Calculate a list of longest line segments which are good for text placement
                atools::util::PolygonLineDistances lineDists =
                  atools::util::PolygonLineDistance::getLongPolygonLines(*polygon, context->screenRect, 5, maxAngle);

                // Try all lines from longest to shortest until text was drawn
                for(atools::util::PolygonLineDistance& lineDist : lineDists)
                {
                  // Increase distance to line for very long horizontal lines which result in misplaced text due to great circle route
                  float offset = 2.f;
                  double dx = std::abs(lineDist.getLine().dx());
                  if(dx > 8000.)
                    offset = 60.f;
                  else if(dx > 5000.)
                    offset = 40.f;
                  else if(dx > 2000.)
                    offset = 20.f;
                  else if(dx > 1000.)
                    offset = 10.f;
                  else if(dx > 500.)
                    offset = 5.f;
                  // else offset = 2.f;

                  // Line length plus buffer to allow overshoot
                  float lineLength = lineDist.getLength() + 30.f;
                  float airspaceTextLength = metrics.horizontalAdvance(airspaceText);

                  if(lineLength > airspaceTextLength)
                  {
                    textPlacement.setLineWidth(static_cast<float>(painter->pen().widthF()));

                    // Get polygon orientation for calculating inside
                    atools::util::Orientation orientation = atools::util::getPolygonOrientation(*polygon);

                    // Do not show text on concave polygon parts which can be a result of the angle tolerance
                    if(!((lineDist.getDirection() == atools::util::PolygonLineDistance::DIR_LEFT &&
                          orientation == atools::util::CLOCKWISE) ||
                         (lineDist.getDirection() == atools::util::PolygonLineDistance::DIR_RIGHT &&
                          orientation == atools::util::COUNTERCLOCKWISE)))
                    {
#ifdef DEBUG_INFORMATION_PAINT_POLYGON
                      {
                        atools::util::PainterContextSaver save(painter);

                        painter->setPen(QPen(QColor(0, 0, 255, 50), 10.));
                        painter->drawLine(lineDist.getLine());

                        painter->setPen(QPen());
                        QString txt;
                        if(lineDist.getDirection() == atools::util::PolygonLineDistance::DIR_UNKNOWN)
                          txt = "   U";
                        else if(lineDist.getDirection() == atools::util::PolygonLineDistance::DIR_NONE)
                          txt = "   N";
                        else if(lineDist.getDirection() == atools::util::PolygonLineDistance::DIR_RIGHT)
                          txt = "   R";
                        else if(lineDist.getDirection() == atools::util::PolygonLineDistance::DIR_LEFT)
                          txt = "   L";

                        if(orientation == atools::util::CLOCKWISE)
                          txt.append(" CW");
                        else if(orientation == atools::util::COUNTERCLOCKWISE)
                          txt.append(" CCW");

                        painter->drawText(lineDist.getLine().center(), txt);
                      }
#endif

#ifdef DEBUG_INFORMATION_PAINT_POLYGON_PX
                      airspaceText.prepend(QString("[%1px]").arg(dx, 0, 'f', 0));
#endif

                      // Rotate text to show at the inside of the polygon ==================
                      TextPlacement::Side side = TextPlacement::CENTER;
                      if(orientation == atools::util::CLOCKWISE)
                        side = TextPlacement::RIGHT;
                      else if(orientation == atools::util::COUNTERCLOCKWISE)
                        side = TextPlacement::LEFT;

                      // Draw text and set flag to exit loops =====================================================
                      textPlacement.drawTextAlongOneLine(airspaceText, lineDist.getAngle(), lineDist.getCenter(), lineLength, side, offset);

                      drawn = true;
                      break;
                    } // if(!((lineDist.getDirection() == atools::util::PolygonLineDistance::DIR_LEFT && ...
                  } // if(lineLength > 40)
                } // for(atools::util::PolygonLineDistance& lineDist : lineDists)
              } // for(const QPolygonF *polygon : polygons)
            } // for(float maxAngle : maxAngles)
          } // if(!airspaceText.isEmpty())
        } // if((airspace->type & context->airspaceTextsByLayer))
      } // for(const DrawAirspace& drawAirspace : drawAirspaces)
    } // if(context->viewContext == Marble::Still && (name || restrictiveName || type || altitude || com))

    // Delete airspace polygons ====================================================
    for(const DrawAirspace& drawAirspace : qAsConst(visibleAirspaces))
      releasePolygons(drawAirspace.polygons);

  } // if(!airspaces.isEmpty())
  context->endTimer("Airspace");
}
