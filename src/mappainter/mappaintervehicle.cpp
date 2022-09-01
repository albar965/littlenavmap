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

#include "common/aircrafttrack.h"
#include "common/mapcolors.h"
#include "common/symbolpainter.h"
#include "common/unit.h"
#include "common/vehicleicons.h"
#include "geo/calculations.h"
#include "mapgui/maplayer.h"
#include "common/maptypes.h"
#include "mapgui/mapscale.h"
#include "mapgui/mapwidget.h"
#include "navapp.h"
#include "settings/settings.h"
#include "util/paintercontextsaver.h"
#include "fs/sc/simconnectuseraircraft.h"

#include <marble/GeoPainter.h>

#include <QStringBuilder>

using namespace Marble;
using namespace atools::geo;
using namespace map;
using atools::fs::sc::SimConnectUserAircraft;
using atools::fs::sc::SimConnectAircraft;
using atools::fs::sc::SimConnectData;

MapPainterVehicle::MapPainterVehicle(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainter(mapWidget, mapScale, paintContext)
{

}

MapPainterVehicle::~MapPainterVehicle()
{

}

void MapPainterVehicle::paintAiVehicle(const SimConnectAircraft& vehicle, bool forceLabel)
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
    if(!hidden)
    {
      float rotate = calcRotation(vehicle);

      if(rotate < map::INVALID_COURSE_VALUE)
      {
        // Position is visible
        context->painter->translate(x, y);
        context->painter->rotate(rotate);

        int minSize;
        if(vehicle.isUser())
          minSize = 32;
        else
          minSize = vehicle.isAnyBoat() ? context->mapLayer->getAiAircraftSize() - 4 : context->mapLayer->getAiAircraftSize();

        int size = std::max(context->sz(context->symbolSizeAircraftAi, minSize), scale->getPixelIntForFeet(vehicle.getModelSize()));
        int offset = -(size / 2);

        // Draw symbol
        context->painter->drawPixmap(offset, offset, *NavApp::getVehicleIcons()->pixmapFromCache(vehicle, size, 0));

        context->painter->resetTransform();

        // Build text label
        if(!vehicle.isAnyBoat())
        {
          context->szFont(context->textSizeAircraftAi);
          paintTextLabelAi(x, y, size, vehicle, forceLabel);
        }
      }
    }
  }
}

void MapPainterVehicle::paintUserAircraft(const SimConnectUserAircraft& userAircraft, float x, float y)
{
  int size = std::max(context->sz(context->symbolSizeAircraftUser, 32), scale->getPixelIntForFeet(userAircraft.getModelSize()));
  context->szFont(context->textSizeAircraftUser);
  int offset = -(size / 2);

  if(context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_TRACK_LINE) && userAircraft.getGroundSpeedKts() > 30 &&
     userAircraft.getTrackDegTrue() < atools::fs::sc::SC_INVALID_FLOAT)
  {
    // Get projection corrected rotation angle
    float rotate = scale->getScreenRotation(userAircraft.getTrackDegTrue(), userAircraft.getPosition(), context->zoomDistanceMeter);

    if(rotate < map::INVALID_COURSE_VALUE)
      symbolPainter->drawTrackLine(context->painter, x, y, size * 2, rotate);
  }

  // Position is visible
  // Get projection corrected rotation angle
  float rotate = calcRotation(userAircraft);

  if(rotate < map::INVALID_COURSE_VALUE)
  {
    context->painter->translate(x, y);
    context->painter->rotate(atools::geo::normalizeCourse(rotate));

    // Draw symbol
    context->painter->drawPixmap(offset, offset, *NavApp::getVehicleIcons()->pixmapFromCache(userAircraft, size, 0));
    context->painter->resetTransform();

    // Build text label
    paintTextLabelUser(x, y, size, userAircraft);
  }
}

float MapPainterVehicle::calcRotation(const SimConnectAircraft& aircraft)
{
  float rotate;
  if(aircraft.getHeadingDegTrue() < atools::fs::sc::SC_INVALID_FLOAT)
    rotate = atools::geo::normalizeCourse(aircraft.getHeadingDegTrue());
  else
    rotate = atools::geo::normalizeCourse(aircraft.getHeadingDegMag() + NavApp::getMagVar(aircraft.getPosition()));

  // Get projection corrected rotation angle
  return scale->getScreenRotation(rotate, aircraft.getPosition(), context->zoomDistanceMeter);
}

void MapPainterVehicle::paintAircraftTrack()
{
  const AircraftTrack& aircraftTrack = NavApp::getAircraftTrack();

  if(!aircraftTrack.isEmpty())
  {
    context->painter->setPen(mapcolors::aircraftTrailPen(context->sz(context->thicknessTrail, 2)));

    for(const LineString& line : aircraftTrack.getLineStrings())
      drawLineString(context->painter, line);
  }
}

void MapPainterVehicle::paintTextLabelAi(float x, float y, int size, const SimConnectAircraft& aircraft, bool forceLabel)
{
  QStringList texts;

  if((aircraft.isOnGround() && context->mapLayer->isAiAircraftGroundText()) || // All AI on ground
     (!aircraft.isOnGround() && context->mapLayer->isAiAircraftText()) || // All AI in the air
     (aircraft.isOnline() && context->mapLayer->isOnlineAircraftText()) || // All online
     forceLabel) // Force label for nearby aircraft
  {
    bool detail = context->mapLayer->isAiAircraftTextDetail();
    bool detail2 = context->mapLayer->isAiAircraftTextDetail2();

    appendAtcText(texts, aircraft, context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_REGISTRATION),
                  context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_TYPE) && detail,
                  context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_AIRLINE) && detail2,
                  context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_FLIGHT_NUMBER) && detail2,
                  context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_TRANSPONDER_CODE) && detail2);

    if(detail)
    {
      if(aircraft.getGroundSpeedKts() > 30)
        appendSpeedText(texts, aircraft,
                        context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_IAS),
                        context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_GS),
                        context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_TAS));

      if(context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_DEP_DEST) &&
         (!aircraft.getFromIdent().isEmpty() || !aircraft.getToIdent().isEmpty()) && detail2)
      {
        texts.append(tr("%1 to %2").
                     arg(aircraft.getFromIdent().isEmpty() ? tr("Unknown") : aircraft.getFromIdent()).
                     arg(aircraft.getToIdent().isEmpty() ? tr("Unknown") : aircraft.getToIdent()));
      }

      if(!aircraft.isOnGround())
      {
        if(context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_HEADING))
        {
          float heading = atools::fs::sc::SC_INVALID_FLOAT;
          if(aircraft.getHeadingDegMag() < atools::fs::sc::SC_INVALID_FLOAT)
            heading = aircraft.getHeadingDegMag();
          else if(aircraft.getHeadingDegTrue() < atools::fs::sc::SC_INVALID_FLOAT)
            heading = atools::geo::normalizeCourse(aircraft.getHeadingDegTrue() - NavApp::getMagVar(aircraft.getPosition()));

          if(heading < atools::fs::sc::SC_INVALID_FLOAT)
            texts.append(tr("HDG %3°M").arg(QString::number(heading, 'f', 0)));
        }

        if(context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_CLIMB_SINK))
          appendClimbSinkText(texts, aircraft);

        QStringList altTexts;
        if(context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_INDICATED_ALTITUDE) &&
           aircraft.getIndicatedAltitudeFt() < map::INVALID_ALTITUDE_VALUE)
          altTexts.append(tr("IND %1").arg(Unit::altFeet(aircraft.getIndicatedAltitudeFt())));

        if(context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_ALTITUDE) && aircraft.getActualAltitudeFt() < map::INVALID_ALTITUDE_VALUE)
        {
          QString upDown;
          if(!context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_CLIMB_SINK))
            climbSinkPointer(upDown, aircraft);
          altTexts.append(tr("ALT %1%2").arg(Unit::altFeet(aircraft.getActualAltitudeFt())).arg(upDown));
        }
        if(!altTexts.isEmpty())
          texts.append(altTexts.join(tr(", ")));
      }

      const Pos& pos = aircraft.getPosition();
      const Pos& userPos = mapPaintWidget->getUserAircraft().getPosition();
      if(context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_DIST_BEARING_FROM_USER) && detail2)
      {
        float distMeter = pos.distanceMeterTo(userPos);

        if(distMeter < atools::geo::nmToMeter(8000.f))
          texts.append(tr("From User %1 %2°M").
                       arg(Unit::distMeter(distMeter, true /* addUnit */, 5 /* minValPrec */, true /* narrow */)).
                       arg(QString::number(atools::geo::normalizeCourse(userPos.angleDegTo(pos) - NavApp::getMagVar(userPos)), 'f', 0)));
      }

      if(context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_COORDINATES) && detail2)
        texts.append(Unit::coords(pos));
    } // if(detail)

    int transparency = context->flags2.testFlag(opts2::MAP_AI_TEXT_BACKGROUND) ? 255 : 0;

    // Draw text label
    symbolPainter->textBoxF(context->painter, texts, mapcolors::aircraftAiLabelColor, x + size / 2.f, y + size / 2.f,
                            textatt::NONE, transparency, mapcolors::aircraftAiLabelColorBg);
  }
}

void MapPainterVehicle::paintTextLabelUser(float x, float y, int size, const SimConnectUserAircraft& aircraft)
{
  QStringList texts;

  appendAtcText(texts, aircraft, context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_REGISTRATION),
                context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_TYPE),
                context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_AIRLINE),
                context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_FLIGHT_NUMBER),
                context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_TRANSPONDER_CODE));

  if(aircraft.getGroundSpeedKts() > 30)
  {
    appendSpeedText(texts, aircraft,
                    context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_IAS),
                    context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_GS),
                    context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_TAS));
  }

  if(context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_HEADING) &&
     aircraft.getHeadingDegMag() < atools::fs::sc::SC_INVALID_FLOAT)
    texts.append(tr("HDG %3°M").arg(QString::number(aircraft.getHeadingDegMag(), 'f', 0)));

  if(!aircraft.isOnGround() && context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_CLIMB_SINK))
    appendClimbSinkText(texts, aircraft);

  if(!aircraft.isOnGround() && (context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_ALTITUDE) ||
                                context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_INDICATED_ALTITUDE) ||
                                context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_ALT_ABOVE_GROUND)))
  {
    QStringList altText;
    if(context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_INDICATED_ALTITUDE))
      altText.append(tr("IND %1").arg(Unit::altFeet(aircraft.getIndicatedAltitudeFt())));

    if(context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_ALTITUDE))
      altText.append(tr("ALT %1").arg(Unit::altFeet(aircraft.getActualAltitudeFt())));

    if(!aircraft.isOnGround() && context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_ALT_ABOVE_GROUND))
      altText.append(tr("AGL %1").arg(Unit::altFeet(aircraft.getAltitudeAboveGroundFt())));

    if(!altText.isEmpty())
      texts.append(altText.join(tr(", ")));
  }

  if(context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_COORDINATES))
    texts.append(Unit::coords(aircraft.getPosition()));

  int transparency = context->flags2.testFlag(opts2::MAP_USER_TEXT_BACKGROUND) ? 255 : 0;

  if(context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_ICE))
  {
    QStringList ice = map::aircraftIcing(aircraft, true /* narrow */);
    if(!ice.isEmpty())
    {
      ice.prepend(tr("Ice %"));
      symbolPainter->textBoxF(context->painter, ice, mapcolors::aircraftUserLabelColor, x - size * 3 / 4, y,
                              textatt::ERROR_COLOR | textatt::RIGHT, 255, mapcolors::aircraftUserLabelColorBg);
    }
  }

  // Draw text label
  symbolPainter->textBoxF(context->painter, texts, mapcolors::aircraftUserLabelColor, x + size * 3 / 4, y,
                          textatt::NONE, transparency, mapcolors::aircraftUserLabelColorBg);
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

    texts.append(Unit::speedVertFpm(vspeed) % upDown);
  }
}

void MapPainterVehicle::appendAtcText(QStringList& texts, const SimConnectAircraft& aircraft,
                                      bool registration, bool type, bool airline, bool flightnumber,
                                      bool transponderCode)
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

  if(transponderCode && aircraft.isTransponderCodeValid())
    texts.append(tr("XPDR %1").arg(aircraft.getTransponderCodeStr()));
}

void MapPainterVehicle::appendSpeedText(QStringList& texts, const SimConnectAircraft& aircraft,
                                        bool ias, bool gs, bool tas)
{
  QStringList line;
  if(ias && aircraft.getIndicatedSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT / 2.f)
    line.append(tr("IAS %1").arg(Unit::speedKts(aircraft.getIndicatedSpeedKts())));

  if(gs && aircraft.getGroundSpeedKts() < atools::fs::sc::SC_INVALID_FLOAT / 2.f)
    line.append(tr("GS %2").arg(Unit::speedKts(aircraft.getGroundSpeedKts())));

  if(tas && aircraft.getTrueAirspeedKts() < atools::fs::sc::SC_INVALID_FLOAT / 2.f)
    line.append(tr("TAS %1").arg(Unit::speedKts(aircraft.getTrueAirspeedKts())));

  if(!line.isEmpty())
    texts.append(line.join(tr(", ")));
}

void MapPainterVehicle::paintWindPointer(const atools::fs::sc::SimConnectUserAircraft& aircraft, int x, int y)
{
  if(aircraft.getWindDirectionDegT() < atools::fs::sc::SC_INVALID_FLOAT)
  {
    if(aircraft.getWindSpeedKts() >= 1.f)
      symbolPainter->drawWindPointer(context->painter, x, y, WIND_POINTER_SIZE, aircraft.getWindDirectionDegT());
    context->szFont(1.f);
    paintTextLabelWind(x, y, WIND_POINTER_SIZE, aircraft);
  }
}

void MapPainterVehicle::paintTextLabelWind(int x, int y, int size,
                                           const SimConnectUserAircraft& aircraft)
{
  if(aircraft.getWindDirectionDegT() < atools::fs::sc::SC_INVALID_FLOAT)
  {
    int xs, ys;
    QStringList texts;

    textatt::TextAttributes atts = textatt::ROUTE_BG_COLOR;
    if(aircraft.getWindSpeedKts() >= 1.f)
    {
      if(context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_WIND))
      {
        texts.append(tr("%1 °M").arg(QString::number(atools::geo::normalizeCourse(
                                                       aircraft.getWindDirectionDegT() - aircraft.getMagVarDeg()),
                                                     'f', 0)));

        texts.append(tr("%2").arg(Unit::speedKts(aircraft.getWindSpeedKts())));
      }
      xs = x + size / 2 + 4;
      ys = y + size / 2;
    }
    else
    {
      atts |= textatt::CENTER;
      texts.append(tr("No wind"));
      xs = x;
      ys = y + size / 2;
    }

    // Draw text label
    symbolPainter->textBoxF(context->painter, texts, QPen(Qt::black), xs, ys, atts, 255);
  }
}
