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
#include "geo/marbleconverter.h"
#include "mapgui/maplayer.h"
#include "mapgui/mappaintwidget.h"

#include <marble/ViewportParams.h>
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

text::Flag PaintContext::airportTextFlags() const
{
  // Build and draw airport text
  text::Flag textflags = text::NO_FLAG;

  if(mapLayerText->isAirportInfo())
    textflags = text::INFO;

  if(mapLayerText->isAirportIdent())
    textflags = textflags | text::IDENT;

  if(mapLayerText->isAirportName())
    textflags = textflags | text::NAME;

  return textflags;
}

text::Flag PaintContext::airportTextFlagsMinor() const
{
  // Build and draw airport text
  text::Flag textflags = text::NO_FLAG;

  if(mapLayerText->isAirportMinorInfo())
    textflags = text::INFO;

  if(mapLayerText->isAirportMinorIdent())
    textflags = textflags | text::IDENT;

  if(mapLayerText->isAirportMinorName())
    textflags = textflags | text::NAME;

  return textflags;
}

text::Flag PaintContext::airportTextFlagsRoute() const
{
  // Show ident always on route
  text::Flag textflags = text::IDENT;

  // Use more more detailed text for flight plan
  if(mapLayerRouteText->isAirportRouteInfo())
    textflags = textflags | text::NAME | text::INFO;

  return textflags;
}

text::Flag PaintContext::airportTextFlagsLog() const
{
  return text::IDENT;
}

text::Attribute PaintContext::airportTextAtts() const
{
  return flags2.testFlag(opts2::MAP_AIRPORT_TEXT_BACKGROUND) ? text::NO_ATTRIBUTE : text::NO_BACKGROUND;
}

text::Attribute PaintContext::airportTextAttsMinor() const
{
  return flags2.testFlag(opts2::MAP_AIRPORT_TEXT_BACKGROUND) ? text::NO_ATTRIBUTE : text::NO_BACKGROUND;
}

text::Attribute PaintContext::airportTextAttsRoute() const
{
  text::Attribute atts = text::ROUTE_BG_COLOR;

  if(!(flags2 & opts2::MAP_ROUTE_TEXT_BACKGROUND))
    atts = atts | text::NO_BACKGROUND;
  return atts;
}

text::Attribute PaintContext::textAttsLog() const
{
  text::Attribute atts = text::LOG_BG_COLOR;

  if(!(flags2 & opts2::MAP_ROUTE_TEXT_BACKGROUND))
    atts = atts | text::NO_BACKGROUND;
  return atts;
}

bool PaintContext::visibleAndResolves(const atools::geo::Line& line) const
{
  return viewportRect.overlaps(line.boundingRect()) && viewport->resolves(mconvert::toGdc(line.getPos1()), mconvert::toGdc(line.getPos2()));
}

bool PaintContext::visibleAndResolves(const atools::geo::Rect& rect) const
{
  return viewportRect.overlaps(rect) && viewport->resolves(mconvert::toGdc(rect));
}
