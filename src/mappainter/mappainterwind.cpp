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

MapPainterWind::MapPainterWind(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainter(mapWidget, mapScale, paintContext)
{
}

MapPainterWind::~MapPainterWind()
{
}

void MapPainterWind::render()
{
  bool drawWeather = context->objectDisplayTypes.testFlag(map::WIND_BARBS) && context->mapLayer->isWindBarbs();

  if(!drawWeather)
    return;

  atools::util::PainterContextSaver saver(context->painter);

  bool overflow = false;
  const atools::grib::WindPosList *windForRect =
    NavApp::getWindReporter()->getWindForRect(context->viewport->viewLatLonAltBox(), context->mapLayer, context->lazyUpdate, overflow);
  context->setQueryOverflow(overflow);

  if(windForRect != nullptr)
  {
    atools::geo::Rect rect = context->viewportRect;

    // Inflate for half a grid cell size to avoid disappearing symbols at map border
    rect.inflate(0.5f, 0.5f);

    QList<std::pair<QPointF, const atools::grib::WindPos *> > windPositions;
    for(const atools::grib::WindPos& windPos : *windForRect)
    {
      if(!windPos.wind.isValid())
        continue;

      if(rect.contains(windPos.pos))
      {
        bool isVisible, isHidden;
        QPointF point = wToSF(windPos.pos, DEFAULT_WTOS_SIZE, &isVisible, &isHidden);
        if(!point.isNull() && /*isVisible && */ !isHidden)
          windPositions.append(std::make_pair(point, &windPos));
      }
    }

    float size = context->szF(context->symbolSizeWindBarbs, context->mapLayer->getWindBarbsSymbolSize());
    for(auto windPos : windPositions)
    {
      symbolPainter->drawWindBarbs(context->painter, windPos.second->wind.speed, 0.f, windPos.second->wind.dir,
                                   static_cast<float>(windPos.first.x()), static_cast<float>(windPos.first.y()), size,
                                   true /* wind barbs */, true /* alt wind */, false /* route */, context->drawFast);

      if(context->objCount())
        break;
    }
  }
}
