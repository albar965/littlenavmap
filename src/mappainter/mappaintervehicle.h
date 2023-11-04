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

#ifndef LITTLENAVMAP_MAPPAINTERVECHICLE_H
#define LITTLENAVMAP_MAPPAINTERVECHICLE_H

#include "mappainter/mappainter.h"

#include <QCache>

namespace Marble {
class GeoDataLineString;
}

namespace atools {
namespace fs {
namespace sc {
class SimConnectAircraft;
class SimConnectUserAircraft;
}

}

}

class MapWidget;

/*
 * Drawing functions for the simulator user aircraft and aircraft track
 */
class MapPainterVehicle :
  public MapPainter
{
  Q_DECLARE_TR_FUNCTIONS(MapPainter)

public:
  MapPainterVehicle(MapPaintWidget *mapPaintWidget, MapScale *mapScale, PaintContext *paintContext);
  virtual ~MapPainterVehicle() override;

  virtual void render() override = 0;

protected:

  /* Draw a green turn path indicator */
  void paintTurnPath(const atools::fs::sc::SimConnectUserAircraft& userAircraft);

  void paintUserAircraft(const atools::fs::sc::SimConnectUserAircraft& userAircraft, float x, float y);
  void paintAiVehicle(const atools::fs::sc::SimConnectAircraft& vehicle, float x, float y, bool forceLabelNearby);

  void paintTextLabelUser(float x, float y, int size, const atools::fs::sc::SimConnectUserAircraft& aircraft);
  void paintTextLabelAi(float x, float y, float size, const atools::fs::sc::SimConnectAircraft& aircraft, bool forceLabelNearby);
  void appendClimbSinkText(QStringList& texts, const atools::fs::sc::SimConnectAircraft& aircraft);
  void prependAtcText(QStringList& texts, const atools::fs::sc::SimConnectAircraft& aircraft,
                      bool registration, bool type, bool airline, bool flightnumber, bool transponderCode, int elideAirline,
                      int maxTextWidth);
  void appendSpeedText(QStringList& texts, const atools::fs::sc::SimConnectAircraft& aircraft, bool ias, bool gs, bool tas);
  void climbSinkPointer(QString& upDown, const atools::fs::sc::SimConnectAircraft& aircraft);

  void paintWindPointer(const atools::fs::sc::SimConnectUserAircraft& aircraft, float x, float y);
  void paintTextLabelWind(float x, float y, float size, const atools::fs::sc::SimConnectUserAircraft& aircraft);

  /* Calculate rotation for aircraft icon */
  float calcRotation(const atools::fs::sc::SimConnectAircraft& aircraft);

};

#endif // LITTLENAVMAP_MAPPAINTERVECHICLE_H
