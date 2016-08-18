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

#include "maptooltip.h"

#include "common/maptypes.h"
#include "util/htmlbuilder.h"
#include "common/htmlinfobuilder.h"

#include <QPalette>
#include <QToolTip>

using namespace maptypes;
using atools::util::HtmlBuilder;

MapTooltip::MapTooltip(QObject *parent, MapQuery *mapQuery, WeatherReporter *weatherReporter)
  : QObject(parent), query(mapQuery), weather(weatherReporter)
{
#if defined(Q_OS_WIN32)
  iconBackColor = QColor(Qt::transparent);
#else
  // Avoid unreadable icons for some linux distributions that have a black tooltip background
  iconBackColor = QToolTip::palette().color(QPalette::Inactive, QPalette::ToolTipBase);
#endif
}

MapTooltip::~MapTooltip()
{
}

QString MapTooltip::buildTooltip(const maptypes::MapSearchResult& mapSearchResult,
                                 const RouteMapObjectList& routeMapObjects,
                                 bool airportDiagram)
{
  HtmlBuilder html(false);
  HtmlInfoBuilder info(query, nullptr, false);
  int numEntries = 0;

  // Append HTML text for all objects found in order of importance (airports first, etc.)
  // Objects are separated by a horizontal ruler
  // If max number of entries or lines is exceeded return the html
  for(const MapAirport& ap : mapSearchResult.airports)
  {
    if(checkText(html, numEntries))
      return html.getHtml();

    if(!html.isEmpty())
      html.hr();

    html.p();
    info.airportText(ap, html, &routeMapObjects, weather, iconBackColor);
    html.pEnd();
    numEntries++;
  }

  for(const MapVor& vor : mapSearchResult.vors)
  {
    if(checkText(html, numEntries))
      return html.getHtml();

    if(!html.isEmpty())
      html.hr();

    html.p();
    info.vorText(vor, html, iconBackColor);
    html.pEnd();
    numEntries++;
  }

  for(const MapNdb& ndb : mapSearchResult.ndbs)
  {
    if(checkText(html, numEntries))
      return html.getHtml();

    if(!html.isEmpty())
      html.hr();

    html.p();
    info.ndbText(ndb, html, iconBackColor);
    html.pEnd();
    numEntries++;
  }

  for(const MapWaypoint& wp : mapSearchResult.waypoints)
  {
    if(checkText(html, numEntries))
      return html.getHtml();

    if(!html.isEmpty())
      html.hr();

    html.p();
    info.waypointText(wp, html, iconBackColor);
    html.pEnd();
    numEntries++;
  }

  for(const MapAirway& airway : mapSearchResult.airways)
  {
    if(checkText(html, numEntries))
      return html.getHtml();

    if(!html.isEmpty())
      html.hr();

    html.p();
    info.airwayText(airway, html);
    html.pEnd();
    numEntries++;
  }

  for(const MapMarker& m : mapSearchResult.markers)
  {
    if(checkText(html, numEntries))
      return html.getHtml();

    if(!html.isEmpty())
      html.hr();

    html.p();
    info.markerText(m, html);
    html.pEnd();
    numEntries++;
  }

  if(airportDiagram)
  {
    for(const MapAirport& ap : mapSearchResult.towers)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.hr();

      html.p();
      info.towerText(ap, html);
      html.pEnd();
      numEntries++;
    }
    for(const MapParking& p : mapSearchResult.parkings)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.hr();

      html.p();
      info.parkingText(p, html);
      html.pEnd();
      numEntries++;
    }
    for(const MapHelipad& p : mapSearchResult.helipads)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.hr();

      html.p();
      info.helipadText(p, html);
      html.pEnd();
      numEntries++;
    }
  }

  for(const MapUserpoint& up : mapSearchResult.userPoints)
  {
    if(checkText(html, numEntries))
      return html.getHtml();

    if(!html.isEmpty())
      html.hr();

    html.p();
    info.userpointText(up, html);
    html.pEnd();
    numEntries++;
  }
  return html.getHtml();
}

/* Check if the result HTML has more than the allowed number of lines and add a "more" text */
bool MapTooltip::checkText(HtmlBuilder& html, int numEntries)
{
  if(numEntries >= MAX_ENTRIES)
  {
    html.hr().b(tr("More ..."));
    return true;
  }

  return html.checklength(MAX_LINES, tr("More ..."));
}
