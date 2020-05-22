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

#include "mappainter/mappaintervehicle.h"

#include "mapgui/mapwidget.h"
#include "common/mapcolors.h"
#include "geo/calculations.h"
#include "common/symbolpainter.h"
#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "common/unit.h"
#include "navapp.h"
#include "util/paintercontextsaver.h"
#include "settings/settings.h"
#include "common/vehicleicons.h"
#include "common/aircrafttrack.h"

#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;
using namespace map;
using atools::fs::sc::SimConnectUserAircraft;
using atools::fs::sc::SimConnectAircraft;
using atools::fs::sc::SimConnectData;

MapPainterVehicle::MapPainterVehicle(MapPaintWidget *mapWidget, MapScale *mapScale)
  : MapPainter(mapWidget, mapScale)
{

}

MapPainterVehicle::~MapPainterVehicle()
{

}

void MapPainterVehicle::paintAiVehicle(const PaintContext *context, const SimConnectAircraft& vehicle, bool forceLabel)
{
  if(vehicle.isUser())
    return;

  const Pos& pos = vehicle.getPosition();

  if(!pos.isValid())
    return;

  bool hidden = false;
  float x, y;
  if(wToS(pos, x, y, DEFAULT_WTOS_SIZE, &hidden))
  {
    if(!hidden && x < INVALID_INDEX_VALUE / 2 && y < INVALID_INDEX_VALUE / 2)
    {
      float rotate = calcRotation(context, vehicle);

      if(rotate < map::INVALID_COURSE_VALUE)
      {
        // Position is visible
        context->painter->translate(x, y);
        context->painter->rotate(rotate);

        int modelSize = vehicle.getWingSpan() > 0 ? vehicle.getWingSpan() : vehicle.getModelRadiusCorrected() * 2;
        int minSize = vehicle.getCategory() == atools::fs::sc::BOAT ? 28 : 32;

        int size = std::max(context->sz(context->symbolSizeAircraftAi, minSize), scale->getPixelIntForFeet(modelSize));
        int offset = -(size / 2);

        // Draw symbol
        context->painter->drawPixmap(offset, offset, *NavApp::getVehicleIcons()->pixmapFromCache(vehicle, size, 0));

        context->painter->resetTransform();

        // Build text label
        if(vehicle.getCategory() != atools::fs::sc::BOAT)
        {
          context->szFont(context->textSizeAircraftAi);
          paintTextLabelAi(context, x, y, size, vehicle, forceLabel);
        }
      }
    }
  }
}

void MapPainterVehicle::paintUserAircraft(const PaintContext *context,
                                          const SimConnectUserAircraft& userAircraft, float x, float y)
{
  int modelSize = userAircraft.getWingSpan();
  if(modelSize == 0)
    modelSize = userAircraft.getModelRadiusCorrected() * 2;

  int size = std::max(context->sz(context->symbolSizeAircraftUser, 32), scale->getPixelIntForFeet(modelSize));
  context->szFont(context->textSizeAircraftUser);
  int offset = -(size / 2);

  if(context->dOpt(optsd::ITEM_USER_AIRCRAFT_TRACK_LINE) &&
     userAircraft.getGroundSpeedKts() > 30 &&
     userAircraft.getTrackDegTrue() < atools::fs::sc::SC_INVALID_FLOAT)
  {
    // Get projection corrected rotation angle
    float rotate = scale->getScreenRotation(userAircraft.getTrackDegTrue(),
                                            userAircraft.getPosition(), context->zoomDistanceMeter);

    if(rotate < map::INVALID_COURSE_VALUE)
      symbolPainter->drawTrackLine(context->painter, x, y, size * 2, rotate);
  }

  // Position is visible
  // Get projection corrected rotation angle
  float rotate = calcRotation(context, userAircraft);

  if(rotate < map::INVALID_COURSE_VALUE && x < INVALID_INDEX_VALUE / 2 && y < INVALID_INDEX_VALUE / 2)
  {
    context->painter->translate(x, y);
    context->painter->rotate(atools::geo::normalizeCourse(rotate));

    // Draw symbol
    context->painter->drawPixmap(offset, offset, *NavApp::getVehicleIcons()->pixmapFromCache(userAircraft, size, 0));
    context->painter->resetTransform();

    // Build text label
    paintTextLabelUser(context, x, y, size, userAircraft);
  }
}

float MapPainterVehicle::calcRotation(const PaintContext *context, const SimConnectAircraft& aircraft)
{
  float rotate;
  if(aircraft.getHeadingDegTrue() < atools::fs::sc::SC_INVALID_FLOAT)
    rotate = atools::geo::normalizeCourse(aircraft.getHeadingDegTrue());
  else
    rotate = atools::geo::normalizeCourse(aircraft.getHeadingDegMag() + NavApp::getMagVar(aircraft.getPosition()));

  // Get projection corrected rotation angle
  return scale->getScreenRotation(rotate, aircraft.getPosition(), context->zoomDistanceMeter);
}

void MapPainterVehicle::paintAircraftTrack(const PaintContext *context)
{
  const AircraftTrack& aircraftTrack = mapPaintWidget->getAircraftTrack();

  if(!aircraftTrack.isEmpty())
  {
    context->painter->setPen(mapcolors::aircraftTrailPen(context->sz(context->thicknessTrail, 2)));
    paintTrack(context, aircraftTrack);
  }
}

void MapPainterVehicle::paintTextLabelAi(const PaintContext *context, float x, float y, int size,
                                         const SimConnectAircraft& aircraft, bool forceLabel)
{
  QStringList texts;

  if((aircraft.isOnGround() && context->mapLayer->isAiAircraftGroundText()) || // All AI on ground
     (!aircraft.isOnGround() && context->mapLayer->isAiAircraftText()) || // All AI in the air
     (aircraft.isOnline() && context->mapLayer->isOnlineAircraftText()) || // All online
     forceLabel) // Force label for nearby aircraft
  {
    appendAtcText(texts, aircraft, context->dOpt(optsd::ITEM_AI_AIRCRAFT_REGISTRATION),
                  context->dOpt(optsd::ITEM_AI_AIRCRAFT_TYPE),
                  context->dOpt(optsd::ITEM_AI_AIRCRAFT_AIRLINE),
                  context->dOpt(optsd::ITEM_AI_AIRCRAFT_FLIGHT_NUMBER));

    if(aircraft.getGroundSpeedKts() > 30)
      appendSpeedText(texts, aircraft, context->dOpt(optsd::ITEM_AI_AIRCRAFT_IAS),
                      context->dOpt(optsd::ITEM_AI_AIRCRAFT_GS));

    if(context->dOpt(optsd::ITEM_AI_AIRCRAFT_DEP_DEST) &&
       (!aircraft.getFromIdent().isEmpty() || !aircraft.getToIdent().isEmpty()))
    {
      texts.append(tr("%1 to %2").
                   arg(aircraft.getFromIdent().isEmpty() ? tr("None") : aircraft.getFromIdent()).
                   arg(aircraft.getToIdent().isEmpty() ? tr("None") : aircraft.getToIdent()));
    }

    if(!aircraft.isOnGround())
    {
      if(context->dOpt(optsd::ITEM_AI_AIRCRAFT_HEADING))
      {
        float heading = atools::fs::sc::SC_INVALID_FLOAT;
        if(aircraft.getHeadingDegMag() < atools::fs::sc::SC_INVALID_FLOAT)
          heading = aircraft.getHeadingDegMag();
        else if(aircraft.getHeadingDegTrue() < atools::fs::sc::SC_INVALID_FLOAT)
          heading = atools::geo::normalizeCourse(aircraft.getHeadingDegTrue() -
                                                 NavApp::getMagVar(aircraft.getPosition()));

        if(heading < atools::fs::sc::SC_INVALID_FLOAT)
          texts.append(tr("HDG %3°M").arg(QString::number(heading, 'f', 0)));
      }

      if(context->dOpt(optsd::ITEM_AI_AIRCRAFT_CLIMB_SINK))
        appendClimbSinkText(texts, aircraft);

      if(context->dOpt(optsd::ITEM_AI_AIRCRAFT_ALTITUDE))
      {
        QString upDown;
        if(!context->dOpt(optsd::ITEM_AI_AIRCRAFT_CLIMB_SINK))
          climbSinkPointer(upDown, aircraft);
        texts.append(tr("ALT %1%2").arg(Unit::altFeet(aircraft.getPosition().getAltitude())).arg(upDown));
      }
    }
    textatt::TextAttributes atts(textatt::BOLD);

    // Draw text label
    symbolPainter->textBoxF(context->painter, texts, QPen(Qt::black), x + size / 2, y + size / 2, atts, 255);
  }
}

void MapPainterVehicle::paintTextLabelUser(const PaintContext *context, float x, float y, int size,
                                           const SimConnectUserAircraft& aircraft)
{
  QStringList texts;

  appendAtcText(texts, aircraft, context->dOpt(optsd::ITEM_USER_AIRCRAFT_REGISTRATION),
                context->dOpt(optsd::ITEM_USER_AIRCRAFT_TYPE),
                context->dOpt(optsd::ITEM_USER_AIRCRAFT_AIRLINE),
                context->dOpt(optsd::ITEM_USER_AIRCRAFT_FLIGHT_NUMBER));

  if(aircraft.getGroundSpeedKts() > 30)
  {
    appendSpeedText(texts, aircraft, context->dOpt(optsd::ITEM_USER_AIRCRAFT_IAS),
                    context->dOpt(optsd::ITEM_USER_AIRCRAFT_GS));
  }

  if(context->dOpt(optsd::ITEM_USER_AIRCRAFT_HEADING) && aircraft.getHeadingDegMag() < atools::fs::sc::SC_INVALID_FLOAT)
    texts.append(tr("HDG %3°M").arg(QString::number(aircraft.getHeadingDegMag(), 'f', 0)));

  if(!aircraft.isOnGround() && context->dOpt(optsd::ITEM_USER_AIRCRAFT_CLIMB_SINK))
    appendClimbSinkText(texts, aircraft);

  if(!aircraft.isOnGround() && context->dOpt(optsd::ITEM_USER_AIRCRAFT_ALTITUDE))
  {
    QString upDown;
    if(!context->dOpt(optsd::ITEM_USER_AIRCRAFT_CLIMB_SINK))
      climbSinkPointer(upDown, aircraft);

    texts.append(tr("ALT %1%2").arg(Unit::altFeet(aircraft.getPosition().getAltitude())).arg(upDown));
  }

  textatt::TextAttributes atts(textatt::BOLD);
  atts |= textatt::ROUTE_BG_COLOR;

  // Draw text label
  symbolPainter->textBoxF(context->painter, texts, QPen(Qt::black), x + size / 2.f, y + size / 2.f, atts, 255);
}

void MapPainterVehicle::climbSinkPointer(QString& upDown, const SimConnectAircraft& aircraft)
{
  if(aircraft.getVerticalSpeedFeetPerMin() < atools::fs::sc::SC_INVALID_FLOAT)
  {
    if(aircraft.getVerticalSpeedFeetPerMin() > 100.f)
      upDown = tr(" ▲");
    else if(aircraft.getVerticalSpeedFeetPerMin() < -100.f)
      upDown = tr(" ▼");
  }
}

void MapPainterVehicle::appendClimbSinkText(QStringList& texts, const SimConnectAircraft& aircraft)
{
  if(aircraft.getVerticalSpeedFeetPerMin() < atools::fs::sc::SC_INVALID_FLOAT)
  {
    int vspeed = atools::roundToInt(aircraft.getVerticalSpeedFeetPerMin());
    QString upDown;
    climbSinkPointer(upDown, aircraft);

    if(vspeed < 10.f && vspeed > -10.f)
      vspeed = 0.f;

    texts.append(Unit::speedVertFpm(vspeed) + upDown);
  }
}

void MapPainterVehicle::appendAtcText(QStringList& texts, const SimConnectAircraft& aircraft,
                                      bool registration, bool type, bool airline, bool flightnumber)
{
  QStringList line;
  if(registration)
  {
    if(!aircraft.getAirplaneRegistration().isEmpty())
      line.append(aircraft.getAirplaneRegistration());
    else
      line.append(QString::number(aircraft.getObjectId() + 1));
  }

  if(!aircraft.getAirplaneModel().isEmpty() && type)
    line.append(aircraft.getAirplaneModel());

  if(!line.isEmpty())
    texts.append(line.join(tr(" / ")));
  line.clear();

  if(!aircraft.getAirplaneAirline().isEmpty() && airline)
    line.append(aircraft.getAirplaneAirline());

  if(!aircraft.getAirplaneFlightnumber().isEmpty() && flightnumber)
    line.append(aircraft.getAirplaneFlightnumber());

  if(!line.isEmpty())
    texts.append(line.join(tr(" / ")));
}

void MapPainterVehicle::appendSpeedText(QStringList& texts, const SimConnectAircraft& aircraft,
                                        bool ias, bool gs)
{
  QStringList line;
  if(ias && aircraft.getIndicatedSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT)
    line.append(tr("IAS %1").arg(Unit::speedKts(aircraft.getIndicatedSpeedKts())));

  if(gs && aircraft.getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT)
    line.append(tr("GS %2").arg(Unit::speedKts(aircraft.getGroundSpeedKts())));

  if(!line.isEmpty())
    texts.append(line.join(tr(", ")));
}

void MapPainterVehicle::paintWindPointer(const PaintContext *context,
                                         const atools::fs::sc::SimConnectUserAircraft& aircraft,
                                         int x, int y)
{
  if(aircraft.getWindDirectionDegT() < atools::fs::sc::SC_INVALID_FLOAT)
  {
    symbolPainter->drawWindPointer(context->painter, x, y, WIND_POINTER_SIZE, aircraft.getWindDirectionDegT());
    context->szFont(1.f);
    paintTextLabelWind(context, x, y, WIND_POINTER_SIZE, aircraft);
  }
}

void MapPainterVehicle::paintTextLabelWind(const PaintContext *context, int x, int y, int size,
                                           const SimConnectUserAircraft& aircraft)
{
  if(aircraft.getWindDirectionDegT() < atools::fs::sc::SC_INVALID_FLOAT)
  {
    QStringList texts;

    if(context->dOpt(optsd::ITEM_USER_AIRCRAFT_WIND))
    {
      texts.append(tr("%1 °M").arg(QString::number(atools::geo::normalizeCourse(
                                                     aircraft.getWindDirectionDegT() - aircraft.getMagVarDeg()),
                                                   'f', 0)));

      texts.append(tr("%2").arg(Unit::speedKts(aircraft.getWindSpeedKts())));
    }

    textatt::TextAttributes atts(textatt::BOLD);
    atts |= textatt::ROUTE_BG_COLOR;

    // Draw text label
    symbolPainter->textBoxF(context->painter, texts, QPen(Qt::black), x + size / 2, y + size / 2, atts, 255);
  }
}
