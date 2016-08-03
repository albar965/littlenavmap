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

#include "mapgui/mappainteraircraft.h"

#include "mapgui/mapwidget.h"
#include "common/mapcolors.h"
#include "geo/calculations.h"
#include "common/symbolpainter.h"

#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;
using namespace maptypes;

MapPainterAircraft::MapPainterAircraft(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale,
                                       bool verboseMsg)
  : MapPainter(mapWidget, mapQuery, mapScale, verboseMsg)
{
}

MapPainterAircraft::~MapPainterAircraft()
{

}

void MapPainterAircraft::render(const PaintContext *context)
{
  if(!context->objectTypes.testFlag(AIRCRAFT) && !context->objectTypes.testFlag(maptypes::AIRCRAFT_TRACK))
    return;

  setRenderHints(context->painter);

  context->painter->save();

  if(context->objectTypes.testFlag(maptypes::AIRCRAFT_TRACK))
    paintAircraftTrack(context->painter);

  if(context->objectTypes.testFlag(AIRCRAFT))
    paintAircraft(context);

  context->painter->restore();
}

void MapPainterAircraft::paintAircraft(const PaintContext *context)
{
  const atools::fs::sc::SimConnectData& simData = mapWidget->getSimData();

  const Pos& pos = simData.getPosition();

  if(!pos.isValid())
    return;

  int x, y;
  if(wToS(pos, x, y))
  {
    context->painter->translate(x, y);
    context->painter->rotate(atools::geo::normalizeCourse(simData.getHeadingDegTrue()));

    int size = context->symSize(40);

    symbolPainter->drawAircraftSymbol(context->painter, 0, 0, size,
                                      simData.getFlags() & atools::fs::sc::ON_GROUND);

    context->painter->resetTransform();

    QStringList texts;
    // texts.append("Title " + simData.getAirplaneTitle());
    // texts.append("Model " + simData.getAirplaneModel());
    if(!simData.getAirplaneRegistration().isEmpty())
      texts.append(simData.getAirplaneRegistration());
    // texts.append("Type " + simData.getAirplaneType());

    if(!simData.getAirplaneAirline().isEmpty() && !simData.getAirplaneFlightnumber().isEmpty())
      texts.append(simData.getAirplaneAirline() + " / " + simData.getAirplaneFlightnumber());

    texts.append(tr("IAS %1, GS %2, HDG %3°M").
                 arg(QLocale().toString(simData.getIndicatedSpeedKts(), 'f', 0)).
                 arg(QLocale().toString(simData.getGroundSpeedKts(), 'f', 0)).
                 arg(QLocale().toString(simData.getHeadingDegMag(), 'f', 0)));

    QString upDown;
    if(simData.getVerticalSpeedFeetPerMin() > 100.f)
      upDown = tr(" ▲");
    else if(simData.getVerticalSpeedFeetPerMin() < -100.f)
      upDown = tr(" ▼");

    texts.append(tr("ALT %1 ft%2").
                 arg(QLocale().toString(simData.getPosition().getAltitude(), 'f', 0)).arg(upDown));

    texts.append(tr("Wind %1 °M / %2").
                 arg(QLocale().toString(atools::geo::normalizeCourse(
                                          simData.getWindDirectionDegT() - simData.getMagVarDeg()), 'f', 0)).
                 arg(QLocale().toString(simData.getWindSpeedKts(), 'f', 0)));

    symbolPainter->textBox(context->painter, texts,
                           QPen(Qt::black), x + size / 2, y + size / 2, textatt::BOLD, 255);
  }
}

void MapPainterAircraft::paintAircraftTrack(GeoPainter *painter)
{
  const AircraftTrack& aircraftTrack = mapWidget->getAircraftTrack();

  if(!aircraftTrack.isEmpty())
  {
    QPolygon polyline;

    painter->setPen(mapcolors::aircraftTrackPen);
    bool lastVisible = true;
    int lastX = -1, lastY = -1;

    for(int i = 0; i < aircraftTrack.size(); i++)
    {
      const AircraftTrackPos& trackPos = aircraftTrack.at(i);
      int x, y;
      bool visible = wToS(trackPos.pos, x, y);

      if(visible || lastVisible)
      {
        if(i == 0 || i == aircraftTrack.size() - 1 || atools::geo::manhattanDistance(x, y, lastX, lastY) > 5)
        {
          polyline.append(QPoint(x, y));

          if(!visible && lastVisible)
          {
            // End a segment and paint
            painter->drawPolyline(polyline);
            polyline.clear();
          }

          lastVisible = visible;
          lastX = x;
          lastY = y;
        }
      }
    }
    if(!polyline.isEmpty())
      painter->drawPolyline(polyline);
  }
}
