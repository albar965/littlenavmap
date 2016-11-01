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
#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"

#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;
using namespace maptypes;
using atools::fs::sc::SimConnectUserAircraft;
using atools::fs::sc::SimConnectAircraft;
using atools::fs::sc::SimConnectData;

uint qHash(const MapPainterAircraft::Key& key)
{
  return key.size | (key.type << 8) | (key.ground << 10) | (key.user << 11);
}

bool MapPainterAircraft::Key::operator==(const MapPainterAircraft::Key& other) const
{
  return type == other.type && ground == other.ground && user == other.user && size == other.size;
}

MapPainterAircraft::MapPainterAircraft(MapWidget *mapWidget, MapQuery *mapQuery, MapScale *mapScale)
  : MapPainter(mapWidget, mapQuery, mapScale)
{

}

MapPainterAircraft::~MapPainterAircraft()
{

}

void MapPainterAircraft::render(const PaintContext *context)
{
  if(!context->objectTypes.testFlag(AIRCRAFT) &&
     !context->objectTypes.testFlag(AIRCRAFT_AI) &&
     !context->objectTypes.testFlag(maptypes::AIRCRAFT_TRACK))
    // If actions are unchecked return
    return;

  if(!mapWidget->isConnected())
    return;

  setRenderHints(context->painter);

  context->painter->save();

  if(context->objectTypes.testFlag(maptypes::AIRCRAFT_TRACK))
    paintAircraftTrack(context->painter);

  if(mapWidget->distance() < DISTANCE_CUT_OFF_AI_LIMIT)
  {
    if(context->objectTypes.testFlag(AIRCRAFT_AI))
    {
      for(const SimConnectAircraft& ac : mapWidget->getAiAircraft())
        paintAiAircraft(context, ac);
    }
  }

  if(context->objectTypes.testFlag(AIRCRAFT))
    paintUserAircraft(context, mapWidget->getUserAircraft());

  context->painter->restore();
}

void MapPainterAircraft::paintAiAircraft(const PaintContext *context,
                                         const SimConnectAircraft& aiAircraft)
{
  if(!context->mapLayerEffective->isAirportDiagram() && aiAircraft.isOnGround())
    return;

  if(aiAircraft.isUser())
    return;

  const Pos& pos = aiAircraft.getPosition();

  if(!pos.isValid())
    return;

  float x, y;
  if(wToS(pos, x, y))
  {
    int size = std::max(32, scale->getPixelIntForFeet(aiAircraft.getWingSpan()));
    int offset = -(size / 2);

    // Position is visible
    context->painter->translate(x, y);
    context->painter->rotate(atools::geo::normalizeCourse(aiAircraft.getHeadingDegTrue()));

    // Draw symbol
    context->painter->drawPixmap(offset, offset, *pixmapFromCache(aiAircraft, size, false));

    context->painter->resetTransform();

    // Build text label
    paintTextLabel(size, context, x, y, aiAircraft);
  }
}

void MapPainterAircraft::paintUserAircraft(const PaintContext *context,
                                           const SimConnectUserAircraft& userAircraft)
{
  const Pos& pos = userAircraft.getPosition();

  if(!pos.isValid())
    return;

  float x, y;
  if(wToS(pos, x, y))
  {
    int size = std::max(36, scale->getPixelIntForFeet(userAircraft.getWingSpan()));
    int offset = -(size / 2);

    // Position is visible
    context->painter->translate(x, y);
    context->painter->rotate(atools::geo::normalizeCourse(userAircraft.getHeadingDegTrue()));

    // Draw symbol
    context->painter->drawPixmap(offset, offset, *pixmapFromCache(userAircraft, size, true));
    context->painter->resetTransform();

    // Build text label
    paintTextLabel(size, context, x, y, userAircraft);
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

void MapPainterAircraft::paintTextLabel(int size, const PaintContext *context, float x, float y,
                                        const SimConnectAircraft& aircraft)
{
  if(!aircraft.isUser() && mapWidget->distance() > 10)
    return;

  QStringList texts;

  if((aircraft.isOnGround() && context->mapLayerEffective->isAirportDiagramDetail2() &&
      !aircraft.isUser()) || // All AI on ground
     (!aircraft.isOnGround() && !aircraft.isUser()) || // All AI in the air
     aircraft.isUser()) // User aircraft
  {
    if(!aircraft.getAirplaneRegistration().isEmpty())
      texts.append(aircraft.getAirplaneRegistration() + " / " + aircraft.getAirplaneModel());

    if(!aircraft.getAirplaneAirline().isEmpty() && !aircraft.getAirplaneFlightnumber().isEmpty())
      texts.append(aircraft.getAirplaneAirline() + " / " + aircraft.getAirplaneFlightnumber());
  }

  if(aircraft.getGroundSpeedKts() > 30)
  {
    texts.append(tr("IAS %1, GS %2, HDG %3°M").
                 arg(QLocale().toString(aircraft.getIndicatedSpeedKts(), 'f', 0)).
                 arg(QLocale().toString(aircraft.getGroundSpeedKts(), 'f', 0)).
                 arg(QLocale().toString(aircraft.getHeadingDegMag(), 'f', 0)));
  }

  if(!aircraft.isOnGround())
  {
    QString upDown;
    if(aircraft.getVerticalSpeedFeetPerMin() > 100.f)
      upDown = tr(" ▲");
    else if(aircraft.getVerticalSpeedFeetPerMin() < -100.f)
      upDown = tr(" ▼");

    texts.append(tr("ALT %1 ft%2").
                 arg(QLocale().toString(aircraft.getPosition().getAltitude(), 'f', 0)).arg(upDown));
  }

  const SimConnectUserAircraft *userAircraft = dynamic_cast<const SimConnectUserAircraft *>(&aircraft);
  if(userAircraft != nullptr && !aircraft.isOnGround())
  {
    texts.append(tr("Wind %1 °M / %2").
                 arg(QLocale().toString(atools::geo::normalizeCourse(
                                          userAircraft->getWindDirectionDegT() - userAircraft->getMagVarDeg()),
                                        'f', 0)).
                 arg(QLocale().toString(userAircraft->getWindSpeedKts(), 'f', 0)));
  }

  textatt::TextAttributes atts(textatt::BOLD);

  if(aircraft.isUser())
    atts |= textatt::ROUTE_BG_COLOR;

  // Draw text label
  symbolPainter->textBoxF(context->painter, texts, QPen(Qt::black), x + size / 2, y + size / 2, atts, 255);
}

const QPixmap *MapPainterAircraft::pixmapFromCache(const SimConnectAircraft& ac, int size,
                                                   bool user)
{
  Key key;

  if(ac.getCategory() == atools::fs::sc::HELICOPTER)
    key.type = AC_HELICOPTER;
  else if(ac.getEngineType() == atools::fs::sc::JET)
    key.type = AC_JET;
  else
    key.type = AC_SMALL;

  key.ground = ac.isOnGround();
  key.user = user;
  key.size = size;
  return pixmapFromCache(key);
}

const QPixmap *MapPainterAircraft::pixmapFromCache(const Key& key)
{
  if(pixmaps.contains(key))
    return pixmaps.object(key);
  else
  {
    QString name = ":/littlenavmap/resources/icons/aircraft";
    switch(key.type)
    {
      case AC_SMALL:
        name += "_small";
        break;
      case AC_JET:
        name += "_jet";
        break;
      case AC_HELICOPTER:
        name += "_helicopter";
        break;
    }
    if(key.ground)
      name += "_ground";
    if(key.user)
      name += "_user";

    QPixmap *newPx = new QPixmap(QIcon(name).pixmap(QSize(key.size, key.size)));
    pixmaps.insert(key, newPx);
    return newPx;
  }
}
