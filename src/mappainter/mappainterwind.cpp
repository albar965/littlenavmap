/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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
#include "query/mapquery.h"
#include "util/paintercontextsaver.h"
#include "navapp.h"
#include "weather/windreporter.h"
#include "grib/windquery.h"

#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

using namespace atools::geo;

MapPainterWind::MapPainterWind(MapPaintWidget *mapWidget, MapScale *mapScale)
  : MapPainter(mapWidget, mapScale)
{
}

MapPainterWind::~MapPainterWind()
{
}

void MapPainterWind::render(PaintContext *context)
{
  bool drawWeather = context->objectDisplayTypes.testFlag(map::WIND_BARBS) && context->mapLayer->isWindBarbs();

  if(!drawWeather)
    return;

  atools::util::PainterContextSaver saver(context->painter);
  Q_UNUSED(saver);

  const atools::grib::WindPosList *windForRect =
    NavApp::getWindReporter()->getWindForRect(context->viewport->viewLatLonAltBox(),
                                              context->mapLayer, context->lazyUpdate);

  if(windForRect != nullptr)
  {
    atools::geo::Rect rect = context->viewportRect;

    // Inflate for half a grid cell size to avoid disappearing symbols at map border
    rect.inflate(0.5f, 0.5f);

    for(const atools::grib::WindPos& windPos : *windForRect)
    {
      if(!windPos.wind.isValid())
        continue;

      if(rect.contains(windPos.pos))
      {
        bool isVisible, isHidden;
        QPoint pos = wToS(windPos.pos, DEFAULT_WTOS_SIZE, &isVisible, &isHidden);
        if(!pos.isNull() && /*isVisible && */ !isHidden)
          drawWindBarb(context, windPos.wind.speed, windPos.wind.dir, pos.x(), pos.y());
      }
    }
  }
}

void MapPainterWind::drawWindBarb(PaintContext *context, float speed, float direction, float x, float y)
{
  float size = context->sz(context->symbolSizeWindBarbs, context->mapLayer->getWindBarbsSymbolSize());
  symbolPainter->drawWindBarbs(context->painter, speed, 0.f, direction, x, y, size,
                               true /* wind barbs */, true /* alt wind */, false /* route */, context->drawFast);
}
