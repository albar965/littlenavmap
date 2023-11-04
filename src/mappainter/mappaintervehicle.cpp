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

#include "mappainter/mappaintervehicle.h"

#include "common/mapcolors.h"
#include "common/symbolpainter.h"
#include "common/unit.h"
#include "common/vehicleicons.h"
#include "geo/calculations.h"
#include "mapgui/maplayer.h"
#include "common/maptypes.h"
#include "mapgui/mappaintwidget.h"
#include "mapgui/mapscale.h"
#include "mapgui/mapscreenindex.h"
#include "app/navapp.h"
#include "fs/sc/simconnectuseraircraft.h"

#include <marble/GeoPainter.h>

#include <QStringBuilder>

using namespace Marble;
using namespace atools::geo;
using namespace map;
using atools::fs::sc::SimConnectUserAircraft;
using atools::fs::sc::SimConnectAircraft;
using atools::fs::sc::SimConnectData;

const float MAX_TURN_PATH_NM = 5.f;

MapPainterVehicle::MapPainterVehicle(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainter(mapWidget, mapScale, paintContext)
{

}

MapPainterVehicle::~MapPainterVehicle()
{

}

void MapPainterVehicle::paintAiVehicle(const SimConnectAircraft& vehicle, float x, float y, bool forceLabelNearby)
{
  if(vehicle.isUser())
    return;

  const Pos& pos = vehicle.getPosition();

  if(!pos.isValid())
    return;

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

    float size = std::max(context->szF(context->symbolSizeAircraftAi, minSize), scale->getPixelForFeet(vehicle.getModelSize()));
    float offset = -(size / 2.f);

    // Draw symbol
    context->painter->drawPixmap(QPointF(offset, offset),
                                 *NavApp::getVehicleIcons()->pixmapFromCache(vehicle, static_cast<int>(size), 0));

    context->painter->resetTransform();

    // Build text label
    if(!vehicle.isAnyBoat())
    {
      context->szFont(context->textSizeAircraftAi);
      paintTextLabelAi(x, y, size, vehicle, forceLabelNearby);
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

void MapPainterVehicle::paintTurnPath(const atools::fs::sc::SimConnectUserAircraft& userAircraft)
{
  if(context->objectDisplayTypes & map::AIRCRAFT_TURN_PATH && userAircraft.isFlying())
  {
    const atools::geo::Pos& aircraftPos = mapPaintWidget->getUserAircraft().getPosition();
    if(aircraftPos.isValid())
    {
      float groundSpeedKts, turnSpeedDegPerSec;
      mapPaintWidget->getScreenIndex()->getAverageGroundAndTurnSpeed(groundSpeedKts, turnSpeedDegPerSec);

      if(groundSpeedKts > 20.f)
      {
        // Draw one line segment every 0.1 NM
        double distanceStepNm = 0.1;

        // Turn in degree at each node
        double turnStep = turnSpeedDegPerSec * (distanceStepNm * 3600. / groundSpeedKts);

        // Current distance from first node (aircraft)
        double curDistance = 0.f;
        double curHeading = userAircraft.getTrackDegTrue() + turnStep;

        double lineWidth = context->szF(context->thicknessUserFeature, mapcolors::markTurnPathPen.width());
        context->painter->setPen(mapcolors::adjustWidth(mapcolors::markTurnPathPen, static_cast<float>(lineWidth)));
        context->painter->setBrush(QBrush(mapcolors::markTurnPathPen.color()));

        // One step is 0.1 NM
        int step = 0;

        float pixelForMaxTurnPath = scale->getPixelForNm(MAX_TURN_PATH_NM);
        if(pixelForMaxTurnPath > 40)
        {
          atools::geo::Pos curPos = aircraftPos;
          atools::geo::LineString line(aircraftPos);

          // Stop either at distance or if turn radius drawn is close to 180°
          while(curDistance < MAX_TURN_PATH_NM &&
                atools::geo::angleAbsDiff(static_cast<double>(userAircraft.getTrackDegTrue()), curHeading) < 180. - std::abs(turnStep))
          {
            // Next point for given distance and heading
            // Use maximal precision to avoid crawling
            atools::geo::Pos newPos = curPos.endpointDouble(atools::geo::nmToMeter(distanceStepNm), curHeading);
            line.append(newPos);

            // Draw mark at each 0.5 NM
            if(step > 0 && (step % 10) == 0)
            {
              double length = lineWidth * 1.2;

              // Draw line segment as mark
              QPointF pt = wToSF(curPos);
              if(!pt.isNull())
              {
                context->painter->translate(pt);
                context->painter->rotate(curHeading - (turnStep / 2.) + 90.);
                context->painter->drawLine(QPointF(0., -length), QPointF(0., length));
                context->painter->resetTransform();
              }
            }

            curPos = newPos;
            curDistance += distanceStepNm;
            curHeading += turnStep;
            curHeading = atools::geo::normalizeCourse(curHeading);
            step++;
          }

          drawPolyline(context->painter, line);
        } // if(pixelForMaxTurnPath > 20)
      } // if(groundSpeedKts > 20.f)
    } // if(aircraftPos.isValid())
  } // if(context->objectDisplayTypes & map::AIRCRAFT_TURN_PATH && userAircraft.isFlying())
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

void MapPainterVehicle::paintTextLabelAi(float x, float y, float size, const SimConnectAircraft& aircraft, bool forceLabelNearby)
{
  QStringList texts;
  bool flying = !aircraft.isOnGround();

  const MapLayer *layer = context->mapLayer;

  if((!flying && layer->isAiAircraftGroundText()) || // All AI on ground
     (flying && layer->isAiAircraftText()) || // All AI in the air
     (aircraft.isOnline() && layer->isOnlineAircraftText()) || // All online
     forceLabelNearby)
  {
    bool text = layer->isAiAircraftText();
    bool detail = layer->isAiAircraftTextDetail(); // Lowest detail
    bool detail2 = layer->isAiAircraftTextDetail2(); // Higher detail
    bool detail3 = layer->isAiAircraftTextDetail3(); // Highest detail

    if(forceLabelNearby)
      detail = detail2 = text;

    if(detail2)
    {
      // Speeds ====================================================================================
      if(aircraft.getGroundSpeedKts() > 1.f)
        appendSpeedText(texts, aircraft,
                        context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_IAS) && flying && detail3,
                        context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_GS),
                        context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_TAS) && flying && detail3);

      if(flying)
      {
        // Departure and destination ====================================================================================
        if(context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_DEP_DEST) &&
           (!aircraft.getFromIdent().isEmpty() || !aircraft.getToIdent().isEmpty()) && detail3)
        {
          texts.append(tr("%1 to %2").
                       arg(aircraft.getFromIdent().isEmpty() ? tr("Unknown") : aircraft.getFromIdent()).
                       arg(aircraft.getToIdent().isEmpty() ? tr("Unknown") : aircraft.getToIdent()));
        }

        // Heading ====================================================================================
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

        // Vertical speed ====================================================================================
        if(context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_CLIMB_SINK))
          appendClimbSinkText(texts, aircraft);

        QStringList altTexts;
        if(context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_INDICATED_ALTITUDE) &&
           aircraft.getIndicatedAltitudeFt() < map::INVALID_ALTITUDE_VALUE && detail3)
          altTexts.append(tr("IND %1").arg(Unit::altFeet(aircraft.getIndicatedAltitudeFt())));

        // Altitude ====================================================================================
        if(context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_ALTITUDE) && aircraft.getActualAltitudeFt() < map::INVALID_ALTITUDE_VALUE)
        {
          QString upDown;
          if(!context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_CLIMB_SINK))
            climbSinkPointer(upDown, aircraft);
          altTexts.append(tr("ALT %1%2").arg(Unit::altFeet(aircraft.getActualAltitudeFt())).arg(upDown));
        }
        if(!altTexts.isEmpty())
          texts.append(altTexts.join(tr(", ")));

        // Bearing to user ====================================================================================
        const Pos& userPos = mapPaintWidget->getUserAircraft().getPosition();
        const Pos& aiPos = aircraft.getPosition();
        if(context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_DIST_BEARING_FROM_USER) && detail3)
        {
          float distMeter = aiPos.distanceMeterTo(userPos);

          if(distMeter < atools::geo::nmToMeter(8000.f))
          {
            QString dist = QString::number(atools::geo::normalizeCourse(userPos.angleDegTo(aiPos) - NavApp::getMagVar(userPos)), 'f', 0);

            texts.append(tr("From User %1 %2°M").
                         arg(Unit::distMeter(distMeter, true /* addUnit */, 5 /* minValPrec */, true /* narrow */)).
                         arg(dist));
          }
        }

        // Coordinates ====================================================================================
        if(context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_COORDINATES) && detail3)
          texts.append(Unit::coords(aiPos));
      } // if(flying)
    } // if(detail)

    // Aircraft information ====================================================================================
    // Formatting depends on list size
    prependAtcText(texts, aircraft,
                   context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_REGISTRATION) && text,
                   context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_TYPE) && detail,
                   context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_AIRLINE) && text,
                   context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_FLIGHT_NUMBER) && text,
                   context->dOptAiAc(optsac::ITEM_AI_AIRCRAFT_TRANSPONDER_CODE) && detail3 && flying,
                   detail2 ? 20 : 8, // elideAirline
                   detail3 ? 150 : 70 // maxTextWidth
                   );

#ifdef DEBUG_INFORMATION_AI
    texts.prepend(QString("[%1%2%3%4%5]").
                  arg(forceLabelNearby ? "F" : "").arg(text ? "T" : "-").
                  arg(detail ? "D" : "-").arg(detail2 ? "2" : "").arg(detail3 ? "3" : ""));
#endif

    int transparency = context->flags2.testFlag(opts2::MAP_AI_TEXT_BACKGROUND) ? 255 : 0;

    // Do not place label far away from icon on lowest zoom levels
    size = std::min(40.f, size);

    // Draw text label
    symbolPainter->textBoxF(context->painter, texts, mapcolors::aircraftAiLabelColor, x + size / 2.f, y + size / 2.f,
                            textatt::NONE, transparency, mapcolors::aircraftAiLabelColorBg);
  } // if((!flying && layer->isAiAircraftGroundText()) ||
}

void MapPainterVehicle::paintTextLabelUser(float x, float y, int size, const SimConnectUserAircraft& aircraft)
{
  QStringList texts;

  if(aircraft.getGroundSpeedKts() > 1.f)
    appendSpeedText(texts, aircraft,
                    context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_IAS) && !aircraft.isOnGround(),
                    context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_GS),
                    context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_TAS) && !aircraft.isOnGround());

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

  // ATC text depends on other formatting ============
  prependAtcText(texts, aircraft, context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_REGISTRATION),
                 context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_TYPE),
                 context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_AIRLINE),
                 context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_FLIGHT_NUMBER),
                 context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_TRANSPONDER_CODE),
                 20, // elideAirline
                 150 // maxTextWidth
                 );

  if(context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_ICE))
  {
    QStringList ice = map::aircraftIcing(aircraft, true /* narrow */);
    if(!ice.isEmpty())
    {
      ice.prepend(tr("Ice %"));
      symbolPainter->textBoxF(context->painter, ice, mapcolors::aircraftUserLabelColor, x - size * 3 / 4, y,
                              textatt::ERROR_COLOR | textatt::LEFT, 255, mapcolors::aircraftUserLabelColorBg);
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
    if(vspeed < 50.f && vspeed > -50.f)
      vspeed = 0.f;

    if(vspeed != 0)
    {
      QString upDown;
      climbSinkPointer(upDown, aircraft);

      texts.append(Unit::speedVertFpm(vspeed) % upDown);
    }
  }
}

void MapPainterVehicle::prependAtcText(QStringList& texts, const SimConnectAircraft& aircraft,
                                       bool registration, bool type, bool airline, bool flightnumber,
                                       bool transponderCode, int elideAirline, int maxTextWidth)
{
  QStringList atctexts;
  if(registration && !aircraft.getAirplaneRegistration().isEmpty())
    atctexts.append(aircraft.getAirplaneRegistration());

  if(type && !aircraft.getAirplaneModel().isEmpty())
    atctexts.append(aircraft.getAirplaneModel());

  if(airline && !aircraft.getAirplaneAirline().isEmpty())
    atctexts.append(atools::elideTextShort(aircraft.getAirplaneAirline(), elideAirline));

  if(flightnumber && !aircraft.getAirplaneFlightnumber().isEmpty())
    atctexts.append(aircraft.getAirplaneFlightnumber());

  if(transponderCode && aircraft.isTransponderCodeValid())
    atctexts.append(tr("XPDR %1").arg(aircraft.getTransponderCodeStr()));

  if(!atctexts.isEmpty())
  {
    if(!texts.isEmpty() || atctexts.size() > 2)
      atctexts = atools::wrapText(atctexts, context->painter->fontMetrics(), maxTextWidth, "/");
    std::copy(atctexts.crbegin(), atctexts.crend(), std::front_inserter(texts));
  }
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

void MapPainterVehicle::paintWindPointer(const atools::fs::sc::SimConnectUserAircraft& aircraft, float x, float y)
{
  if(aircraft.getWindDirectionDegT() < atools::fs::sc::SC_INVALID_FLOAT)
  {
    // Use standard font size since there is no separate size setting
    context->szFont(1.f);
    float windPointerSize = static_cast<float>(QFontMetricsF(context->painter->font()).height()) * 2.f * 1.25f;
    if(aircraft.getWindSpeedKts() >= 1.f)
      symbolPainter->drawWindPointer(context->painter, x, y, windPointerSize, aircraft.getWindDirectionDegT());
    paintTextLabelWind(x, y, windPointerSize, aircraft);
  }
}

void MapPainterVehicle::paintTextLabelWind(float x, float y, float size, const SimConnectUserAircraft& aircraft)
{
  if(aircraft.getWindDirectionDegT() < atools::fs::sc::SC_INVALID_FLOAT)
  {
    float xs, ys;
    QStringList texts;

    textatt::TextAttributes atts = textatt::ROUTE_BG_COLOR;
    if(aircraft.getWindSpeedKts() >= 1.f)
    {
      if(context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_WIND))
      {
        texts.append(tr("%1 °M").
                     arg(QString::number(atools::geo::normalizeCourse(aircraft.getWindDirectionDegT() - aircraft.getMagVarDeg()), 'f', 0)));

        texts.append(tr("%2").arg(Unit::speedKts(aircraft.getWindSpeedKts())));
      }
      xs = x + size / 2.f + 4.f;
      ys = y + size / 2.f;
    }
    else
    {
      atts |= textatt::CENTER | textatt::BELOW;
      texts.append(tr("No wind"));
      xs = x;
      ys = y;
    }

    // Draw text label
    symbolPainter->textBoxF(context->painter, texts, QPen(Qt::black), xs, ys, atts, 255);
  }
}
