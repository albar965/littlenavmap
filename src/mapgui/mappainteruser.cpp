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

#include "mapgui/mappainteruser.h"

#include "common/symbolpainter.h"
#include "common/mapcolors.h"
#include "common/unit.h"
#include "mapgui/mapwidget.h"
#include "common/textplacement.h"
#include "util/paintercontextsaver.h"
#include "mapgui/maplayer.h"
#include "query/mapquery.h"
#include "userdata/userdataicons.h"
#include "navapp.h"
#include "atools.h"

#include <QElapsedTimer>

#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

using namespace Marble;
using namespace atools::geo;
using namespace map;

MapPainterUser::MapPainterUser(MapWidget *mapWidget, MapScale *mapScale)
  : MapPainter(mapWidget, mapScale)
{
}

MapPainterUser::~MapPainterUser()
{
}

void MapPainterUser::render(PaintContext *context)
{
  const GeoDataLatLonAltBox& curBox = context->viewport->viewLatLonAltBox();

  atools::util::PainterContextSaver saver(context->painter);
  Q_UNUSED(saver);

  context->szFont(context->textSizeNavaid);

  // Always call paint to fill cache
  paintUserpoints(context,
                  mapQuery->getUserdataPoints(curBox, context->userPointTypes, context->userPointTypesAll,
                                              context->userPointTypeUnknown,
                                              context->distance), context->drawFast);
}

void MapPainterUser::paintUserpoints(PaintContext *context, const QList<MapUserpoint>& userpoints, bool drawFast)
{
  bool fill = context->flags2 & opts::MAP_NAVAID_TEXT_BACKGROUND;
  UserdataIcons *icons = NavApp::getUserdataIcons();

  for(const MapUserpoint& userpoint : userpoints)
  {
    float x, y;
    bool visible = wToS(userpoint.position, x, y);

    if(visible)
    {
      if(context->objCount())
        return;

      if(icons->hasType(userpoint.type) || context->userPointTypeUnknown)
      {
        int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getUserPointSymbolSize());
        context->painter->drawPixmap(QPointF(x - size / 2, y - size / 2),
                                     *icons->getIconPixmap(userpoint.type, size));

        if(context->mapLayer->isUserpointInfo() && !drawFast)
        {
          int maxTextLength = context->mapLayer->getMaxTextLengthUserpoint();

          // Avoid showing same text twice
          QStringList texts;
          texts.append(atools::elideTextShort(userpoint.ident, maxTextLength));
          QString name = userpoint.name != userpoint.ident ?
                         atools::elideTextShort(userpoint.name, maxTextLength) : QString();
          if(!name.isEmpty())
            texts.append(name);

          symbolPainter->textBoxF(context->painter, texts, QPen(Qt::black),
                                  x + size / 2, y, textatt::LEFT, fill ? 255 : 0);
        }
      }
    }
  }
}
