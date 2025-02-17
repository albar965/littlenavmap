/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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

#include "app/navapp.h"
#include "common/mapcolors.h"
#include "common/maptypes.h"
#include "common/symbolpainter.h"
#include "common/textpointer.h"
#include "common/unit.h"
#include "common/vehicleicons.h"
#include "fs/sc/simconnectuseraircraft.h"
#include "geo/calculations.h"
#include "mapgui/maplayer.h"
#include "mapgui/mappaintwidget.h"
#include "mapgui/mapscale.h"
#include "mapgui/mapscreenindex.h"

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

void MapPainterVehicle::paintAiVehicle(const SimConnectAircraft& vehicle, float x, float y, bool forceLabelNearby) const
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

    // Limit maximum size by 1000 ft
    float size = std::max(context->szF(context->symbolSizeAircraftAi, minSize),
                          scale->getPixelForFeet(std::min(vehicle.getModelSize(), 1000)));
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

void MapPainterVehicle::paintUserAircraft(const SimConnectUserAircraft& userAircraft, float x, float y) const
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

void MapPainterVehicle::paintTurnPath(const atools::fs::sc::SimConnectUserAircraft& userAircraft) const
{
  if(context->objectDisplayTypes.testFlag(map::AIRCRAFT_TURN_PATH) && userAircraft.isFlying())
  {
    const atools::geo::Pos& aircraftPos = mapPaintWidget->getUserAircraft().getPosition();
    if(aircraftPos.isValid())
    {
      float groundSpeedKts, turnSpeedDegPerSec;
      mapPaintWidget->getScreenIndex()->getAverageGroundAndTurnSpeed(groundSpeedKts, turnSpeedDegPerSec);

      if(groundSpeedKts > 20.f)
      {
        // Draw one line segment every distanceStepNm
        double distanceStepNm = groundSpeedKts < 120.f ? 0.02 : 0.1;

        // Turn in degree at each node
        double turnStep = turnSpeedDegPerSec * (distanceStepNm * 3600. / groundSpeedKts);

        // Current distance from first node (aircraft)
        double curDistance = 0.f;
        double curHeading = userAircraft.getTrackDegTrue() + turnStep;

        double lineWidth = context->szF(context->thicknessUserFeature, mapcolors::markTurnPathPen.width());
        context->painter->setPen(mapcolors::adjustWidth(mapcolors::markTurnPathPen, static_cast<float>(lineWidth)));
        context->painter->setBrush(QBrush(mapcolors::markTurnPathPen.color()));

        // One step is 0.1 or 0.01NM
        int step = 0;

        // Step to draw mark
        const int stepMark = atools::roundToInt(1. / distanceStepNm);

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

            // Draw mark at each 1 NM
            if(step > 0 && (step % stepMark) == 0)
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

float MapPainterVehicle::calcRotation(const SimConnectAircraft& aircraft) const
{
  float rotate;
  if(aircraft.getHeadingDegTrue() < atools::fs::sc::SC_INVALID_FLOAT)
    rotate = atools::geo::normalizeCourse(aircraft.getHeadingDegTrue());
  else
    rotate = atools::geo::normalizeCourse(aircraft.getHeadingDegMag() + NavApp::getMagVar(aircraft.getPosition()));

  // Get projection corrected rotation angle
  return scale->getScreenRotation(rotate, aircraft.getPosition(), context->zoomDistanceMeter);
}

void MapPainterVehicle::paintTextLabelAi(float x, float y, float size, const SimConnectAircraft& aircraft, bool forceLabelNearby) const
{
  QStringList texts;
  bool flying = !aircraft.isOnGround();

  const MapLayer *layer = context->mapLayerText;

  if((!flying && layer->isAiAircraftGroundText()) || // All AI on ground
     (flying && layer->isAiAircraftText()) || // All AI in the air
     (aircraft.isOnline() && layer->isOnlineAircraftText()) || // All online
     forceLabelNearby) // Close to user aircraft
  {
    bool hidden = false; // Set by aiDisp() if any value is requested and available but excluded by detail level
    bool drawText = layer->isAiAircraftText(); // Show any text
    bool detail1 = layer->isAiAircraftTextDetail(); // Lowest detail level up to higher zoom
    bool detail2 = layer->isAiAircraftTextDetail2(); // Higher detail
    bool detail3 = layer->isAiAircraftTextDetail3(); // Highest detail on lower zoom only

    if(forceLabelNearby)
      detail1 = detail2 = drawText;

    // Speeds ====================================================================================

    if(!flying)
    {
      // Aircraft on ground ======================================================
      // Omit IAS and TAS on ground
      if(aircraft.getGroundSpeedKts() > 1.f)
        appendSpeedText(texts, aircraft, false /* ias */,
                        aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_GS, detail2, aircraft.getGroundSpeedKts()),
                        false /* tas */);
    }
    else
    {
      // Aircraft flying ======================================================
      if(aircraft.getGroundSpeedKts() > 1.f)
        appendSpeedText(texts, aircraft,
                        aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_IAS, detail3, aircraft.getIndicatedSpeedKts()),
                        aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_GS, detail2, aircraft.getGroundSpeedKts()),
                        aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_TAS, detail3, aircraft.getTrueAirspeedKts()));

      // Departure and destination ====================================================================================
      if(aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_DEP_DEST, detail3, aircraft.getFromIdent() % aircraft.getToIdent()))
      {
        texts.append(tr("%1 to %2").
                     arg(aircraft.getFromIdent().isEmpty() ? tr("Unknown") : aircraft.getFromIdent()).
                     arg(aircraft.getToIdent().isEmpty() ? tr("Unknown") : aircraft.getToIdent()));
      }

      // Heading ====================================================================================
      float heading = atools::fs::sc::SC_INVALID_FLOAT;
      if(aircraft.getHeadingDegMag() < atools::fs::sc::SC_INVALID_FLOAT)
        heading = aircraft.getHeadingDegMag();
      else if(aircraft.getHeadingDegTrue() < atools::fs::sc::SC_INVALID_FLOAT)
        heading = atools::geo::normalizeCourse(aircraft.getHeadingDegTrue() - NavApp::getMagVar(aircraft.getPosition()));

      if(aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_HEADING, detail2, heading))
        texts.append(tr("HDG %3°M").arg(QString::number(heading, 'f', 0)));

      // Bearing to user ====================================================================================
      const Pos& userPos = mapPaintWidget->getUserAircraft().getPosition();
      const Pos& aiPos = aircraft.getPosition();
      float distMeter = aiPos.distanceMeterTo(userPos);
      if(aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_DIST_BEARING_FROM_USER, detail3, distMeter < atools::geo::nmToMeter(8000.f)))
        texts.append(tr("From User %1 %2°M").
                     arg(Unit::distMeter(distMeter, true /* addUnit */, 5 /* minValPrec */, true /* narrow */)).
                     arg(QString::number(atools::geo::normalizeCourse(userPos.angleDegTo(aiPos) - NavApp::getMagVar(userPos)), 'f', 0)));

      // Coordinates ====================================================================================
      if(aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_COORDINATES, detail3))
        texts.append(Unit::coords(aiPos));

      // Vertical speed ====================================================================================
      if(aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_CLIMB_SINK, detail2, aircraft.getVerticalSpeedFeetPerMin()))
        appendClimbSinkText(texts, aircraft);

      // Indicated altitude ====================================================================================
      QStringList altTexts;
      if(aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_INDICATED_ALTITUDE, detail3, aircraft.getIndicatedAltitudeFt()))
        altTexts.append(tr("IND %1").arg(Unit::altFeet(aircraft.getIndicatedAltitudeFt())));

      // Actual altitude ====================================================================================
      if(aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_ALTITUDE, detail1,
                aircraft.getActualAltitudeFt()) && aircraft.isActualAltitudeFullyValid())
        altTexts.append(tr("ALT %1%2").arg(Unit::altFeet(aircraft.getActualAltitudeFt())).arg(QString()));

      QString upDown;
      if(!aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_CLIMB_SINK, detail1, aircraft.getVerticalSpeedFeetPerMin()))
        climbSinkPointer(upDown, aircraft);

      if(!altTexts.isEmpty())
      {
        texts.append(altTexts.join(tr(", ")));
        texts.last().append(upDown);
      }
    } // else flying

    // Aircraft information ====================================================================================
    // Formatting depends on list size
    prependAtcText(texts, aircraft,
                   aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_REGISTRATION, drawText, aircraft.getAirplaneRegistration()),
                   aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_TYPE, detail1, aircraft.getAirplaneModel()),
                   aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_AIRLINE, drawText, aircraft.getAirplaneAirline()),
                   aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_FLIGHT_NUMBER, drawText, aircraft.getAirplaneFlightnumber()),
                   aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_TRANSPONDER_CODE, detail3, aircraft.isTransponderCodeValid() && flying),
                   detail2 ? 20 : 8, // elideAirline
                   detail3 ? 150 : 70 // maxTextWidth
                   );

    // Object ID ====================================================================================
    if(aiDisp(hidden, optsac::ITEM_AI_AIRCRAFT_OBJECT_ID, drawText))
      texts.prepend(tr("ID %1").arg(aircraft.getObjectId()));

#ifdef DEBUG_INFORMATION_AI
    texts.prepend(QString("[%1%2%3%4%5]").
                  arg(forceLabelNearby ? "F" : "").arg(text ? "T" : "-").
                  arg(detail ? "D" : "-").arg(detail2 ? "2" : "").arg(detail3 ? "3" : ""));
#endif

    int transparency = context->flags2.testFlag(opts2::MAP_AI_TEXT_BACKGROUND) ? 255 : 0;

    // Do not place label far away from icon on lowest zoom levels
    size = std::min(40.f, size);

    if(!texts.isEmpty())
    {
      if(hidden)
        texts.last().append(tr("…"));

      // Draw text label
      symbolPainter->textBoxF(context->painter, texts, mapcolors::aircraftAiLabelColor, x + size / 2.f, y + size / 2.f,
                              textatt::NONE, transparency, mapcolors::aircraftAiLabelColorBg);
    }
  } // if((!flying && layer->isAiAircraftGroundText()) ||
}

bool MapPainterVehicle::aiDisp(bool& hidden, optsac::DisplayOptionAiAircraft opts, bool detail, bool available) const
{
  bool enabled = context->dOptAiAc(opts);
  hidden |= enabled && available && !detail;
  return enabled && detail && available;
}

bool MapPainterVehicle::aiDisp(bool& hidden, optsac::DisplayOptionAiAircraft opts, bool detail, float value) const
{
  return aiDisp(hidden, opts, detail, value < atools::fs::sc::SC_INVALID_FLOAT / 2.f);
}

bool MapPainterVehicle::aiDisp(bool& hidden, optsac::DisplayOptionAiAircraft opts, bool detail, const QString& text) const
{
  return aiDisp(hidden, opts, detail, !text.isEmpty());
}

void MapPainterVehicle::paintTextLabelUser(float x, float y, int size, const SimConnectUserAircraft& aircraft) const
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

    if(context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_ALTITUDE) && aircraft.isActualAltitudeFullyValid())
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

void MapPainterVehicle::climbSinkPointer(QString& upDown, const SimConnectAircraft& aircraft) const
{
  if(aircraft.getVerticalSpeedFeetPerMin() < atools::fs::sc::SC_INVALID_FLOAT)
  {
    if(aircraft.getVerticalSpeedFeetPerMin() > 100.f)
      upDown = tr(" %1").arg(TextPointer::getPointerUp());
    else if(aircraft.getVerticalSpeedFeetPerMin() < -100.f)
      upDown = tr(" %1").arg(TextPointer::getPointerDown());
  }
}

void MapPainterVehicle::appendClimbSinkText(QStringList& texts, const SimConnectAircraft& aircraft) const
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
                                       bool transponderCode, int elideAirline, int maxTextWidth) const
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

void MapPainterVehicle::appendSpeedText(QStringList& texts, const SimConnectAircraft& aircraft, bool ias, bool gs, bool tas) const
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

void MapPainterVehicle::paintWindPointer(const atools::fs::sc::SimConnectUserAircraft& aircraft, float x, float y) const
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

void MapPainterVehicle::paintTextLabelWind(float x, float y, float size, const SimConnectUserAircraft& aircraft) const
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
