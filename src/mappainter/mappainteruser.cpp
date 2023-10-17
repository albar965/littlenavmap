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

#include "mappainter/mappainteruser.h"

#include "atools.h"
#include "common/symbolpainter.h"
#include "mapgui/maplayer.h"
#include "app/navapp.h"
#include "query/mapquery.h"
#include "userdata/userdataicons.h"
#include "util/paintercontextsaver.h"

#include <QElapsedTimer>

#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

using namespace Marble;
using namespace atools::geo;
using namespace map;

MapPainterUser::MapPainterUser(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainter(mapWidget, mapScale, paintContext)
{
}

MapPainterUser::~MapPainterUser()
{
}

void MapPainterUser::render()
{
  const GeoDataLatLonAltBox& curBox = context->viewport->viewLatLonAltBox();

  atools::util::PainterContextSaver saver(context->painter);

  context->szFont(context->textSizeUserpoint);

  // Always call paint to fill cache
  paintUserpoints(mapQuery->getUserdataPoints(curBox, context->userPointTypes, context->userPointTypesAll,
                                              context->userPointTypeUnknown, context->distanceNm), context->drawFast);
}

void MapPainterUser::paintUserpoints(const QList<MapUserpoint>& userpoints, bool drawFast)
{
  const static QMargins MARGINS(100, 10, 10, 10);
  bool fill = context->flags2 & opts2::MAP_USERPOINT_TEXT_BACKGROUND;
  UserdataIcons *icons = NavApp::getUserdataIcons();

  // Use margins for text placed on the right side of the object to avoid disappearing at the left screen border
  for(const MapUserpoint& userpoint : userpoints)
  {
    float x, y;
    bool visible = wToSBuf(userpoint.position, x, y, MARGINS);

    if(visible)
    {
      if(context->objCount())
        return;

      if(icons->hasType(userpoint.type) || context->userPointTypeUnknown)
      {
        float size = context->szF(context->symbolSizeUserpoint, context->mapLayer->getUserPointSymbolSize());
        if(userpoint.type == "Logbook")
        {
          x += size / 2.f;
          y += size / 2.f;
        }

        icon::TextPlacement textPlacementHint = icon::ICON_LABEL_LEFT;
        const QPixmap *iconPixmap = icons->getIconPixmap(userpoint.type, atools::roundToInt(size), &textPlacementHint);
        context->painter->drawPixmap(QPointF(x - size / 2.f, y - size / 2.f), *iconPixmap);

        // Do not draw labels for airport add-on marks
        if(context->mapLayer->isUserpointInfo() && !drawFast && !userpoint.isAddon())
        {
          int maxTextLength = context->mapLayer->getMaxTextLengthUserpoint();
          QStringList texts;
          texts.append(atools::elideTextShort(userpoint.ident, maxTextLength));
          QString name = userpoint.name != userpoint.ident ? atools::elideTextShort(userpoint.name, maxTextLength) : QString();
          if(!name.isEmpty())
            texts.append(name);

          textatt::TextAttributes textatts = textatt::NONE;
          float xpos = x, ypos = y;
          float offset = size / 2.f + size / 10.f;

          // Decide text placement by hint given by userpoint type
          switch(textPlacementHint)
          {
            case icon::ICON_LABEL_TOP:
              // NDB - place on top
              textatts = textatt::ABOVE | textatt::CENTER;
              ypos = y - offset;
              break;

            case icon::ICON_LABEL_RIGHT:
              // VOR - alight left and place right
              textatts = textatt::RIGHT;
              xpos = x + offset;
              break;

            case icon::ICON_LABEL_BOTTOM:
              // Place on bottom
              textatts = textatt::BELOW | textatt::CENTER;
              ypos = y + offset;
              break;

            case icon::ICON_LABEL_LEFT:
              // Airports and waypoints - alight right and place left
              textatts = textatt::LEFT;
              xpos = x - offset;
              break;
          }

          symbolPainter->textBoxF(context->painter, texts, QPen(Qt::black), xpos, ypos, textatts, fill ? 255 : 0);

        } // if(context->mapLayer->isUserpointInfo() && !drawFast)
      } // if(icons->hasType(userpoint.type) || context->userPointTypeUnknown)
    } // if(visible)
  } // for(const MapUserpoint& userpoint : userpoints)
}
