/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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
#include "navapp.h"
#include "util/htmlbuilder.h"
#include "common/htmlinfobuilder.h"
#include "gui/mainwindow.h"
#include "route/route.h"
#include "query/airspacequery.h"
#include "options/optiondata.h"
#include "sql/sqlrecord.h"

#include <QPalette>
#include <QToolTip>

using namespace map;
using atools::util::HtmlBuilder;
using atools::fs::sc::SimConnectAircraft;
using atools::fs::sc::SimConnectUserAircraft;

MapTooltip::MapTooltip(MainWindow *parentWindow)
  : mainWindow(parentWindow), mapQuery(NavApp::getMapQuery()), weather(NavApp::getWeatherReporter())
{
  qDebug() << Q_FUNC_INFO;
}

MapTooltip::~MapTooltip()
{
  qDebug() << Q_FUNC_INFO;
}

QString MapTooltip::buildTooltip(const map::MapSearchResult& mapSearchResult,
                                 const QList<proc::MapProcedurePoint>& procPoints,
                                 const Route& route, bool airportDiagram)
{
  opts::DisplayTooltipOptions opts = OptionData::instance().getDisplayTooltipOptions();

  HtmlBuilder html(false);
  HtmlInfoBuilder info(mainWindow, false);
  int numEntries = 0;

  // Append HTML text for all objects found in order of importance (airports first, etc.)
  // Objects are separated by a horizontal ruler
  // If max number of entries or lines is exceeded return the html

  // User Aircraft ===========================================================================
  if(mapSearchResult.userAircraft.getPosition().isValid())
  {
    if(checkText(html, numEntries))
      return html.getHtml();

    if(!html.isEmpty())
      html.hr();

    html.p();
    info.aircraftText(mapSearchResult.userAircraft, html);
    info.aircraftProgressText(mapSearchResult.userAircraft, html, route);
    html.pEnd();
    numEntries++;
  }

  // Online Aircraft ===========================================================================
  for(const SimConnectAircraft& aircraft : mapSearchResult.onlineAircraft)
  {
    if(checkText(html, numEntries))
      return html.getHtml();

    if(!html.isEmpty())
      html.hr();

    html.p();
    info.aircraftText(aircraft, html);
    info.aircraftProgressText(aircraft, html, Route());
    html.pEnd();
    numEntries++;
  }

  // AI Aircraft ===========================================================================
  for(const SimConnectAircraft& aircraft : mapSearchResult.aiAircraft)
  {
    if(checkText(html, numEntries))
      return html.getHtml();

    if(!html.isEmpty())
      html.hr();

    html.p();
    info.aircraftText(aircraft, html);
    info.aircraftProgressText(aircraft, html, Route());
    html.pEnd();
    numEntries++;
  }

  // Navaids from procedure points ===========================================================================
  if(opts & opts::TOOLTIP_NAVAID)
  {
    for(const proc::MapProcedurePoint& ap : procPoints)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.hr();

      html.p();
      info.procedurePointText(ap, html);
      html.pEnd();
      numEntries++;
    }
  }

  // Airports ===========================================================================
  if(opts & opts::TOOLTIP_AIRPORT)
  {
    for(const MapAirport& airport : mapSearchResult.airports)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.hr();

      map::WeatherContext currentWeatherContext;

      html.p();
      mainWindow->buildWeatherContextForTooltip(currentWeatherContext, airport);
      info.airportText(airport, currentWeatherContext, html, &route);
      html.pEnd();
      numEntries++;
    }
  }

  // Navaids ===========================================================================
  if(opts & opts::TOOLTIP_NAVAID)
  {
    for(const MapUserpoint& up : mapSearchResult.userpoints)
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

    for(const MapVor& vor : mapSearchResult.vors)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.hr();

      html.p();
      info.vorText(vor, html);
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
      info.ndbText(ndb, html);
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
      info.waypointText(wp, html);
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
  }

  // Airport stuff ===========================================================================
  if(airportDiagram && opts & opts::TOOLTIP_AIRPORT)
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

  if(opts & opts::TOOLTIP_NAVAID)
  {
    for(const MapUserpointRoute& up : mapSearchResult.userPointsRoute)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.hr();

      html.p();
      info.userpointTextRoute(up, html);
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
  }

  // Airspaces ===========================================================================
  if(opts & opts::TOOLTIP_AIRSPACE)
  {
    for(const MapAirspace& airspace : mapSearchResult.airspaces)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.hr();

      atools::sql::SqlRecord onlineRec;
      if(airspace.online)
        onlineRec = NavApp::getAirspaceQueryOnline()->getAirspaceRecordById(airspace.id);

      html.p();
      info.airspaceText(airspace, onlineRec, html);
      html.pEnd();
      numEntries++;
    }
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
