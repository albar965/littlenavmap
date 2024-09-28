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

#include "mappainter/mappainteraltitude.h"

#include "app/navapp.h"
#include "atools.h"
#include "common/mapcolors.h"
#include "fs/common/morareader.h"
#include "geo/line.h"
#include "geo/marbleconverter.h"
#include "mapgui/maplayer.h"
#include "mapgui/mapscale.h"
#include "util/paintercontextsaver.h"

#include <QElapsedTimer>

#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;

MapPainterAltitude::MapPainterAltitude(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainter(mapWidget, mapScale, paintContext)
{
}

MapPainterAltitude::~MapPainterAltitude()
{
}

void MapPainterAltitude::render()
{
  using atools::fs::common::MoraReader;

  if(!context->objectDisplayTypes.testFlag(map::MORA))
    return;

  if(context->mapLayer->isMora())
  {
    MoraReader *moraReader = NavApp::getMoraReader();
    if(moraReader->isDataAvailable())
    {
      atools::util::PainterContextSaver paintContextSaver(context->painter);

      // Use width and style from pen but override transparency
      QColor gridCol = context->darkMap ? mapcolors::minimumAltitudeGridPenDark.color() : mapcolors::minimumAltitudeGridPen.color();
      gridCol.setAlphaF(atools::minmax(0., 1., 1. - context->transparencyMora));
      QPen pen = context->darkMap ? mapcolors::minimumAltitudeGridPenDark : mapcolors::minimumAltitudeGridPen;
      pen.setColor(gridCol);
      context->painter->setPen(pen);

      // Get covered one degree coordinate rectangles
      const GeoDataLatLonBox& curBox = context->viewport->viewLatLonAltBox();
      int west = static_cast<int>(curBox.west(mconvert::DEG));
      int east = static_cast<int>(curBox.east(mconvert::DEG));
      int north = static_cast<int>(curBox.north(mconvert::DEG));
      int south = static_cast<int>(curBox.south(mconvert::DEG));

      // Split at anit-meridian if needed
      QVector<std::pair<int, int> > ranges;
      if(west <= east)
        ranges.append(std::make_pair(west - 1, east));
      else
      {
        ranges.append(std::make_pair(west - 1, 179));
        ranges.append(std::make_pair(-180, east));
      }

      // Altitude values
      QVector<int> altitudes;
      // Minimum rectangle width on screen in pixel
      float minWidth = std::numeric_limits<float>::max();
      // Center points for rectangles for text placement
      QVector<GeoDataCoordinates> centers;

      // Draw rectangles and collect other values for text placement ================================
      for(int laty = south; laty <= north + 1; laty++)
      {
        // Iterate over anti-meridian split
        for(const std::pair<int, int>& range : ranges)
        {
          for(int lonx = range.first; lonx <= range.second; lonx++)
          {
            int moraFt100 = moraReader->getMoraFt(lonx, laty);
            if(moraFt100 > 10 && moraFt100 != MoraReader::OCEAN && moraFt100 != MoraReader::UNKNOWN &&
               moraFt100 != MoraReader::ERROR)
            {
              // Build rectangle
              float lonxF = static_cast<float>(lonx);
              float latyF = static_cast<float>(laty);
              drawLine(context->painter, Line(lonxF, latyF, lonxF + 1.f, latyF));
              drawLine(context->painter, Line(lonxF + 1.f, latyF, lonxF + 1, latyF - 1.f));
              drawLine(context->painter, Line(lonxF + 1.f, latyF - 1, lonxF, latyF - 1.f));
              drawLine(context->painter, Line(lonxF, latyF - 1.f, lonxF, latyF));

              if(!context->drawFast)
              {
                // Calculate rectangle screen width
                bool visibleDummy;
                QPointF leftPt = wToSF(mconvert::toGdc(lonx, laty - .5f), DEFAULT_WTOS_SIZE, &visibleDummy);
                QPointF rightPt = wToSF(mconvert::toGdc(lonx + 1.f, laty - .5f), DEFAULT_WTOS_SIZE, &visibleDummy);

                minWidth = std::min(static_cast<float>(QLineF(leftPt, rightPt).length()), minWidth);

                centers.append(mconvert::toGdc(lonx + .5f, laty - .5f));
                altitudes.append(moraFt100);
              }
            }
          } // for(int lonx = range.first; lonx <= range.second; lonx++)
        } // for(const std::pair<int, int>& range : ranges)
      } // for(int laty = south; laty <= north + 1; laty++)

      // Draw texts =================================================================
      if(!context->drawFast && minWidth > 20.f)
      {
        // Adjust minimum and maximum font height based on rectangle width
        minWidth = std::max(minWidth * 0.6f, 25.f);
        minWidth = std::min(minWidth * 0.6f, 150.f);

        // Do not use transparency but override from options
        QColor textCol = context->darkMap ? mapcolors::minimumAltitudeNumberColorDark : mapcolors::minimumAltitudeNumberColor;
        textCol.setAlphaF(atools::minmax(0., 1., 1. - context->transparencyMora));
        context->painter->setPen(textCol);

        float fontSize = minWidth * context->textSizeMora;
        QFont font = context->painter->font();
        font.setItalic(true);
        font.setPixelSize(atools::roundToInt(fontSize));
        context->painter->setFont(font);

        QFontMetricsF fontmetrics = context->painter->fontMetrics();

        if(fontmetrics.height() > 4)
        {
          // Draw big thousands numbers ===============================
          bool visible, hidden;
          QVector<QPointF> baseline;
          for(int i = 0; i < centers.size(); i++)
          {
            QPointF pt = wToSF(centers.at(i), DEFAULT_WTOS_SIZE, &visible, &hidden);

            if(!hidden)
            {
              QString numTxt = QString::number(altitudes.at(i) / 10);
              qreal w = fontmetrics.width(numTxt);
              pt += QPointF(-w * 0.7, fontmetrics.height() / 2. - fontmetrics.descent());

              context->painter->drawText(pt, numTxt);
              baseline.append(QPointF(pt.x() + w, pt.y()));
            }
            else
              baseline.append(QPointF());
          }

          // Draw smaller hundreds numbers ==============================
          font.setPixelSize(atools::roundToInt(fontSize * .6f));
          context->painter->setFont(font);
          fontmetrics = context->painter->fontMetrics();

          for(int i = 0; i < centers.size(); i++)
          {
            QPointF pt = baseline.at(i);
            if(!pt.isNull())
            {
              int alt = altitudes.at(i);
              QString smallNumTxt = QString::number(alt - (alt / 10 * 10));
              pt.setY(pt.y() + fontmetrics.ascent() / 3.f);
              context->painter->drawText(pt, smallNumTxt);
            }
          }
        } // if(fontmetrics.height() > ...)
      } // if(!context->drawFast)
    } // if(moraReader->isDataAvailable())
  } // if(context->mapLayer->isMinimumAltitude())
}
