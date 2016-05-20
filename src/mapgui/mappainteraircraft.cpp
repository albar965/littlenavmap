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

#include "mappainteraircraft.h"
#include "mapwidget.h"
#include "mapscale.h"
#include "mapgui/mapquery.h"
#include "common/mapcolors.h"
#include "geo/calculations.h"
#include "mapgui/symbolpainter.h"

#include <algorithm>
#include "fs/sc/simconnectdata.h"
#include <marble/GeoDataLineString.h>
#include <marble/GeoPainter.h>
#include <marble/MarbleWidget.h>
#include <marble/ViewportParams.h>

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
  if(!context->objectTypes.testFlag(AIRCRAFT) && !context->objectTypes.testFlag(maptypes::AIRCRAFT_TRAIL))
    return;

  setRenderHints(context->painter);

  context->painter->save();

  if(context->objectTypes.testFlag(maptypes::AIRCRAFT_TRAIL))
    paintAircraftTrack(context->painter);

  if(context->objectTypes.testFlag(AIRCRAFT))
    paintAircraft(context->painter);

  context->painter->restore();
}

void MapPainterAircraft::paintAircraft(GeoPainter *painter)
{
  const atools::fs::sc::SimConnectData& simData = mapWidget->getSimData();
  const Pos& pos = simData.getPosition();

  if(!pos.isValid())
    return;

  int x, y;
  if(wToS(pos, x, y))
  {
    painter->translate(x, y);
    painter->rotate(atools::geo::normalizeCourse(mapWidget->getSimData().getCourseTrue()));

    symbolPainter->drawAircraftSymbol(painter, 0, 0, 40);

    painter->resetTransform();

    QStringList texts;
    // texts.append("Title " + simData.getAirplaneTitle());
    // texts.append("Model " + simData.getAirplaneModel());
    if(!simData.getAirplaneReg().isEmpty())
      texts.append(simData.getAirplaneReg());
    // texts.append("Type " + simData.getAirplaneType());

    if(!simData.getAirplaneAirline().isEmpty() && !simData.getAirplaneFlightnumber().isEmpty())
      texts.append(simData.getAirplaneAirline() + " / " + simData.getAirplaneFlightnumber());

    texts.append("IAS " + QString::number(simData.getIndicatedSpeed(), 'f', 0) + " , " +
                 "GS " + QString::number(simData.getGroundSpeed(), 'f', 0) + " , " +
                 "HDG " + QString::number(simData.getCourseMag(), 'f', 0) + " °M");

    QString upDown;
    if(simData.getVerticalSpeed() > 100)
      upDown = " ⭡";
    else if(simData.getVerticalSpeed() < -100)
      upDown = " ⭣";

    texts.append("ALT " + QString::number(simData.getPosition().getAltitude(), 'f', 0) + " ft" + upDown);

    texts.append("Wind " + QString::number(simData.getWindDirection(), 'f', 0) + " °M / " +
                 QString::number(simData.getWindSpeed(), 'f', 0));

    symbolPainter->textBox(painter, texts, QPen(Qt::black), x + 20, y + 20, textatt::BOLD, 255);
  }
}

void MapPainterAircraft::paintAircraftTrack(GeoPainter *painter)
{
  const QList<Pos>& aircraftTrack = mapWidget->getAircraftTrack();

  if(!aircraftTrack.isEmpty())
  {
    GeoDataLineString lineString;
    lineString.setTessellate(true);

    painter->setPen(QPen(QColor(Qt::black), 2, Qt::DashLine, Qt::FlatCap, Qt::BevelJoin));
    bool lastVisible = true;
    atools::geo::Pos lastPos;
    for(const atools::geo::Pos& pos : aircraftTrack)
    {
      bool visible = isVisible(pos);

      if((visible || lastVisible) && lastPos.isValid())
        lineString.append(GeoDataCoordinates(lastPos.getLonX(), lastPos.getLatY(), 0., DEG));

      if(!visible && lastVisible)
      {
        // End a segment and paint
        painter->drawPolyline(lineString);
        lineString.clear();
      }

      lastVisible = visible;
      lastPos = pos;
    }
    if(!lineString.isEmpty())
      painter->drawPolyline(lineString);

  }
}
