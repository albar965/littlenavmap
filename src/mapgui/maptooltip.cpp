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

#include "maplayer.h"
#include "maptooltip.h"
#include "common/maptypes.h"
#include "mapgui/mapquery.h"
#include "common/formatter.h"
#include "route/routemapobjectlist.h"

#include <common/htmlbuilder.h>
#include <common/maphtmlinfobuilder.h>
#include <common/weatherreporter.h>

#include <QPalette>
#include <QToolTip>

using namespace maptypes;

MapTooltip::MapTooltip(QObject *parent, MapQuery *mapQuery, WeatherReporter *weatherReporter)
  : QObject(parent), query(mapQuery), weather(weatherReporter)
{
#if defined(Q_OS_WIN32)
  iconBackColor = QColor(Qt::transparent);
#else
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
  HtmlBuilder html;

  MapHtmlInfoBuilder info(query, nullptr, false);

  for(const MapAirport& ap : mapSearchResult.airports)
  {
    qDebug() << "Airport" << ap.ident << "id" << ap.id;

    if(checkText(html))
      break;

    if(!html.isEmpty())
      html.hr();

    info.airportText(ap, html, &routeMapObjects, weather, iconBackColor);
  }

  for(const MapVor& vor : mapSearchResult.vors)
  {
    qDebug() << "Vor" << vor.ident << "id" << vor.id;

    if(checkText(html))
      break;

    if(!html.isEmpty())
      html.hr();

    info.vorText(vor, html, iconBackColor);
  }

  for(const MapNdb& ndb : mapSearchResult.ndbs)
  {
    qDebug() << "Ndb" << ndb.ident << "id" << ndb.id;

    if(checkText(html))
      break;

    if(!html.isEmpty())
      html.hr();

    info.ndbText(ndb, html, iconBackColor);
  }

  for(const MapWaypoint& wp : mapSearchResult.waypoints)
  {
    qDebug() << "Waypoint" << wp.ident << "id" << wp.id;

    if(checkText(html))
      break;

    if(!html.isEmpty())
      html.hr();

    info.waypointText(wp, html, iconBackColor);
  }

  for(const MapAirway& airway : mapSearchResult.airways)
  {
    qDebug() << "Airway" << airway.name << "id" << airway.id;

    if(checkText(html))
      break;

    if(!html.isEmpty())
      html.hr();

    info.airwayText(airway, html);
  }

  for(const MapMarker& m : mapSearchResult.markers)
  {
    if(checkText(html))
      break;

    if(!html.isEmpty())
      html.hr();

    info.markerText(m, html);
  }

  if(airportDiagram)
  {
    for(const MapAirport& ap : mapSearchResult.towers)
    {
      if(checkText(html))
        break;

      if(!html.isEmpty())
        html.hr();

      info.towerText(ap, html);
    }
    for(const MapParking& p : mapSearchResult.parkings)
    {
      if(checkText(html))
        break;

      if(!html.isEmpty())
        html.hr();

      info.parkingText(p, html);
    }
    for(const MapHelipad& p : mapSearchResult.helipads)
    {
      if(checkText(html))
        break;

      if(!html.isEmpty())
        html.hr();

      info.helipadText(p, html);
    }
  }
  for(const MapUserpoint& up : mapSearchResult.userPoints)
  {
    if(checkText(html))
      break;

    if(!html.isEmpty())
      html.hr();

    info.userpointText(up, html);
  }
  return html.getHtml();
}

bool MapTooltip::checkText(HtmlBuilder& text)
{
  return text.checklength(MAXLINES, "More ...");
}
