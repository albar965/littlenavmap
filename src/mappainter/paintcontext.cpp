/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "mappainter/paintcontext.h"

#include "common/mapcolors.h"
#include "common/maptypes.h"
#include "mapgui/maplayer.h"
#include "mapgui/mappaintwidget.h"
#include <marble/GeoPainter.h>

using namespace Marble;
using namespace atools::geo;
using atools::roundToInt;

/* Minimum points to use for a circle */
const float CIRCLE_MIN_POINTS = 32.f;
/* Maximum points to use for a circle */
const float CIRCLE_MAX_POINTS = 92.f;

void PaintContext::szFont(float scale) const
{
  mapcolors::scaleFont(painter, scale * sizeAll, &defaultFont);
}

textflags::TextFlags PaintContext::airportTextFlags() const
{
  // Build and draw airport text
  textflags::TextFlags textflags = textflags::NONE;

  if(mapLayerText->isAirportInfo())
    textflags = textflags::INFO;

  if(mapLayerText->isAirportIdent())
    textflags |= textflags::IDENT;

  if(mapLayerText->isAirportName())
    textflags |= textflags::NAME;

  if(!flags2.testFlag(opts2::MAP_AIRPORT_TEXT_BACKGROUND))
    textflags |= textflags::NO_BACKGROUND;

  return textflags;
}

textflags::TextFlags PaintContext::airportTextFlagsMinor() const
{
  // Build and draw airport text
  textflags::TextFlags textflags = textflags::NONE;

  if(mapLayerText->isAirportMinorInfo())
    textflags = textflags::INFO;

  if(mapLayerText->isAirportMinorIdent())
    textflags |= textflags::IDENT;

  if(mapLayerText->isAirportMinorName())
    textflags |= textflags::NAME;

  if(!flags2.testFlag(opts2::MAP_AIRPORT_TEXT_BACKGROUND))
    textflags |= textflags::NO_BACKGROUND;

  return textflags;
}

textflags::TextFlags PaintContext::airportTextFlagsRoute(bool drawAsRoute, bool drawAsLog) const
{
  // Show ident always on route
  textflags::TextFlags textflags = textflags::IDENT;

  if(drawAsRoute)
    textflags |= textflags::ROUTE_TEXT;

  if(drawAsLog)
    textflags |= textflags::LOG_TEXT;

  // Use more more detailed text for flight plan
  if(mapLayerRouteText->isAirportRouteInfo())
    textflags |= textflags::NAME | textflags::INFO;

  if(!(flags2 & opts2::MAP_ROUTE_TEXT_BACKGROUND))
    textflags |= textflags::NO_BACKGROUND;

  return textflags;
}
