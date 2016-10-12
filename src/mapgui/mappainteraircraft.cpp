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

MapPainterAircraft::MapPainterAircraft(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale)
  : MapPainter(mapWidget, mapQuery, mapScale)
{
}

MapPainterAircraft::~MapPainterAircraft()
{

}

void MapPainterAircraft::render(const PaintContext *context)
{
  if(!context->objectTypes.testFlag(AIRCRAFT) && !context->objectTypes.testFlag(maptypes::AIRCRAFT_TRACK))
    // If actions are unchecked return
    return;

  setRenderHints(context->painter);

  context->painter->save();

  if(context->objectTypes.testFlag(maptypes::AIRCRAFT_TRACK))
    paintAircraftTrack(context->painter);

  if(context->objectTypes.testFlag(AIRCRAFT))
    if(mapWidget->isConnected())
      paintAircraft(context);

  context->painter->restore();
}

void MapPainterAircraft::paintAircraft(const PaintContext *context)
{
  const atools::fs::sc::SimConnectData& simData = mapWidget->getSimData();

  const Pos& pos = simData.getPosition();

  if(!pos.isValid())
    return;

  float x, y;
  if(wToS(pos, x, y))
  {
    int size = context->symSize(AIRCRAFT_SYMBOL_SIZE);

    // Position is visible
    context->painter->translate(x, y);
    context->painter->rotate(atools::geo::normalizeCourse(simData.getHeadingDegTrue()));

    // Draw symbol
    symbolPainter->drawAircraftSymbol(context->painter, 0, 0, size,
                                      simData.getFlags() & atools::fs::sc::ON_GROUND);
    context->painter->resetTransform();

    // Build text label
    QStringList texts;
    if(!simData.getAirplaneRegistration().isEmpty())
      texts.append(simData.getAirplaneRegistration());

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

    // Draw text label
    symbolPainter->textBoxF(context->painter, texts,
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
    bool lastVisible = false;

    int x1, y1;
    int x2 = -1, y2 = -1;
    QRect vpRect(painter->viewport());
    wToS(aircraftTrack.first().pos, x1, y1);

    for(int i = 1; i < aircraftTrack.size(); i++)
    {
      const at::AircraftTrackPos& trackPos = aircraftTrack.at(i);
      wToS(trackPos.pos, x2, y2);

      QRect rect(QPoint(x1, y1), QPoint(x2, y2));
      rect = rect.normalized();
      rect.adjust(-1, -1, 1, 1);

      // Current line is visible (most likely)
      bool nowVisible = rect.intersects(vpRect);

      if(lastVisible || nowVisible)
      {
        if(!polyline.isEmpty())
        {
          const QPoint& lastPt = polyline.last();
          // Last line or this one are visible add coords
          if(atools::geo::manhattanDistance(lastPt.x(), lastPt.y(), x2, y2) > AIRCRAFT_TRACK_MIN_LINE_LENGTH)
            polyline.append(QPoint(x1, y1));
        }
        else
          // Always add first visible point
          polyline.append(QPoint(x1, y1));
      }

      if(lastVisible && !nowVisible)
      {
        // Not visible anymore draw previous line segment
        painter->drawPolyline(polyline);
        polyline.clear();
      }

      lastVisible = nowVisible;
      x1 = x2;
      y1 = y2;
    }

    // Draw rest
    if(!polyline.isEmpty())
    {
      polyline.append(QPoint(x2, y2));
      painter->drawPolyline(polyline);
    }
  }
}
