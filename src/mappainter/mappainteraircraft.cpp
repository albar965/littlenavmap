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

#include "mappainter/mappainteraircraft.h"

#include "common/constants.h"
#include "geo/calculations.h"
#include "mapgui/mapfunctions.h"
#include "mapgui/mappaintwidget.h"
#include "app/navapp.h"
#include "online/onlinedatacontroller.h"
#include "settings/settings.h"
#include "util/paintercontextsaver.h"

#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

using atools::fs::sc::SimConnectUserAircraft;
using atools::fs::sc::SimConnectAircraft;

MapPainterAircraft::MapPainterAircraft(MapPaintWidget *mapWidget, MapScale *mapScale, PaintContext *paintContext)
  : MapPainterVehicle(mapWidget, mapScale, paintContext)
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  maxNearestAiLabels = settings.getAndStoreValue(lnm::MAP_MAX_NEAREST_AI_LABELS, 5).toInt();
  maxNearestAiLabelsDistNm = atools::geo::nmToMeter(settings.getAndStoreValue(lnm::MAP_MAX_NEAREST_AI_LABELS_DIST_NM, 10.).toFloat());
  maxNearestAiLabelsVertDistFt = settings.getAndStoreValue(lnm::MAP_MAX_NEAREST_AI_LABELS_VERT_DIST_FT, 5000.).toFloat();
}

MapPainterAircraft::~MapPainterAircraft()
{

}

void MapPainterAircraft::render()
{
  const static QMargins MARGINS(100, 100, 100, 100);
  atools::util::PainterContextSaver saver(context->painter);
  const SimConnectUserAircraft& userAircraft = mapPaintWidget->getUserAircraft();

  if(context->objectTypes & map::AIRCRAFT_ALL)
  {
    // Draw AI and online aircraft - not boats ====================================================================
    bool onlineEnabled = context->objectTypes.testFlag(map::AIRCRAFT_ONLINE) && NavApp::isOnlineNetworkActive();
    bool aiEnabled = context->objectTypes.testFlag(map::AIRCRAFT_AI) && NavApp::isConnected();
    const atools::geo::Pos& userPos = userAircraft.getPosition();
    if(aiEnabled || onlineEnabled)
    {
      bool overflow = false;

      // Merge simulator aircraft and online aircraft
      QVector<const SimConnectAircraft *> allAircraft;

      // Get all pure (slowly updated) online aircraft ======================================
      if(onlineEnabled)
      {
        // Filters duplicates from simulator and user aircraft out - remove shadow aircraft
        const QList<SimConnectAircraft> *onlineAircraft =
          NavApp::getOnlinedataController()->getAircraft(context->viewport->viewLatLonAltBox(),
                                                         context->mapLayer, context->lazyUpdate, overflow);

        context->setQueryOverflow(overflow);

        for(const SimConnectAircraft& ac : *onlineAircraft)
          allAircraft.append(&ac);
      }

      // Get all AI and online shadow aircraft ======================================
      for(const SimConnectAircraft& ac : mapPaintWidget->getAiAircraft())
      {
        // Skip boats
        if(ac.isAnyBoat())
          continue;

        // Skip shadow aircraft if online is disabled
        if(!onlineEnabled && ac.isOnlineShadow())
          continue;

        // Skip AI aircraft (means not shadow) if AI is disabled
        if(!aiEnabled && !ac.isOnlineShadow())
          continue;

        allAircraft.append(&ac);
      }

      // Sort by distance to user aircraft
      struct AiDistType
      {
        const SimConnectAircraft *aircraft;
        float x, y;
        float distanceLateralMeter, distanceVerticalFt;
      };

      QVector<AiDistType> aiSorted;
      bool hidden = false;
      float x, y;
      for(const SimConnectAircraft *ac : allAircraft)
      {
        if(wToSBuf(ac->getPosition(), x, y, MARGINS, &hidden))
        {
          if(!hidden)
            aiSorted.append({ac, x, y, userPos.distanceMeterTo(ac->getPosition()),
                             std::abs(userPos.getAltitude() - ac->getActualAltitudeFt())});
        }
      }

      // Sort to have the closest at the start of the list =======================
      std::sort(aiSorted.begin(), aiSorted.end(), [](const AiDistType& ai1, const AiDistType& ai2) -> bool
      {
        // returns ​true if the first argument is less than (i.e. is ordered before) the second.
        return ai1.distanceLateralMeter < ai2.distanceLateralMeter;
      });

      bool hideAiOnGround = OptionData::instance().getFlags().testFlag(opts::MAP_AI_HIDE_GROUND);
      int num = 0;
      for(const AiDistType& adt : aiSorted)
      {
        const SimConnectAircraft& ac = *adt.aircraft;
        if(mapfunc::aircraftVisible(ac, context->mapLayer, hideAiOnGround))
        {
          bool forceLabelNearby = num++ < maxNearestAiLabels &&
                                  adt.distanceLateralMeter < maxNearestAiLabelsDistNm &&
                                  adt.distanceVerticalFt < maxNearestAiLabelsVertDistFt;
          paintAiVehicle(ac, adt.x, adt.y, forceLabelNearby);
        }
      }
    }

    // Draw user aircraft ====================================================================
    if(context->objectTypes.testFlag(map::AIRCRAFT))
    {
      // Use higher accuracy - falls back to normal position if not set
      atools::geo::PosD pos = userAircraft.getPositionD();
      if(pos.isValid())
      {
        bool hidden = false;
        double x, y;
        if(wToS(pos, x, y, DEFAULT_WTOS_SIZE, &hidden))
        {
          if(!hidden)
          {
            paintTurnPath(userAircraft);
            paintUserAircraft(userAircraft, static_cast<float>(x), static_cast<float>(y));
          }
        }
      }
    }
  } // if(context->objectTypes & map::AIRCRAFT_ALL)

  // Wind display depends only on option
  if(context->dOptUserAc(optsac::ITEM_USER_AIRCRAFT_WIND_POINTER) && userAircraft.isValid())
    paintWindPointer(userAircraft, context->screenRect.width() / 2.f, 2.f);
}
