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

  bool drawWaypoint = context->mapLayer->isUserpoint() && context->objectTypes.testFlag(map::USERDATA);
  if(drawWaypoint && !context->isOverflow())
  {
    // If airways are drawn we also have to go through waypoints
    const QList<MapUserdataPoint> userpoints = mapQuery->getUserdataPoint(curBox, {}, context->distance);
    paintUserpoints(context, userpoints, context->drawFast);
  }
}

void MapPainterUser::paintUserpoints(PaintContext *context, const QList<MapUserdataPoint>& userpoints, bool drawFast)
{
  bool fill = context->flags2 & opts::MAP_NAVAID_TEXT_BACKGROUND;

  for(const MapUserdataPoint& userpoint : userpoints)
  {
    float x, y;
    bool visible = wToS(userpoint.position, x, y);

    if(visible)
    {
      if(context->objCount())
        return;

      int size = context->sz(context->symbolSizeNavaid, context->mapLayerEffective->getUserPointSymbolSize());
      context->painter->drawPixmap(QPointF(x - size / 2, y - size / 2),
                                   *NavApp::getUserdataIcons()->getIconPixmap(userpoint.type, size));

      if(context->mapLayer->isUserpointInfo() && !drawFast)
      {
        int maxTextLength = context->mapLayer->getMaxTextLengthUserpoint();

        QString ident = atools::elideTextShort(userpoint.ident, maxTextLength);
        QString name = userpoint.name != userpoint.ident ?
                       atools::elideTextShort(userpoint.name, maxTextLength) : QString();

        symbolPainter->textBoxF(context->painter, {ident, name}, QPen(Qt::black),
                                x + size / 2, y, textatt::LEFT, fill ? 255 : 0);
      }
    }
  }
}
