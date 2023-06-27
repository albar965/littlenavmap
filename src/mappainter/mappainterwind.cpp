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

#include "mappainter/mappainterwind.h"

#include "common/symbolpainter.h"
#include "mapgui/maplayer.h"
#include "util/paintercontextsaver.h"
#include "app/navapp.h"
#include "weather/windreporter.h"
#include "mapgui/mapfunctions.h"

#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

using namespace atools::geo;

MapPainterWind::MapPainterWind(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainter(mapWidget, mapScale, paintContext)
{
}

MapPainterWind::~MapPainterWind()
{
}

void MapPainterWind::render()
{
  if(!context->objectDisplayTypes.testFlag(map::WIND_BARBS) || context->mapLayer->getWindBarbs() == 0)
    return;

  atools::util::PainterContextSaver saver(context->painter);

  const atools::grib::WindPosList *windForRect =
    NavApp::getWindReporter()->getWindForRect(context->viewport->viewLatLonAltBox(), context->mapLayer, context->lazyUpdate,
                                              context->mapLayer->getWindBarbs());

  if(windForRect != nullptr)
  {
    float size = context->szF(context->symbolSizeWindBarbs, context->mapLayer->getWindBarbsSymbolSize());

    int marginsSize = static_cast<int>(size) * 5;
    QMargins margins(marginsSize, marginsSize, marginsSize, marginsSize);

    atools::geo::Rect rect = context->viewportRect;

    // Inflate for half a grid cell size to avoid disappearing symbols at map border
    rect.inflate(0.5f, 0.5f);

    for(const atools::grib::WindPos& windPos : *windForRect)
    {
      if(windPos.wind.isValid() && rect.contains(windPos.pos) &&
         mapfunc::windBarbVisible(windPos.pos, context->mapLayer, context->viewport->projection() == Marble::Spherical))
      {

        QPointF point;
        bool isHidden;
        bool visible = wToSBuf(windPos.pos, point, margins, &isHidden);
        if(visible && !isHidden)
        {
          symbolPainter->drawWindBarbs(context->painter, windPos.wind.speed, 0.f, windPos.wind.dir,
                                       static_cast<float>(point.x()), static_cast<float>(point.y()), size,
                                       true /* wind barbs */, true /* alt wind */, false /* route */, context->drawFast);

          if(context->objCount())
            break;
        }
      }
    }
  }
}
