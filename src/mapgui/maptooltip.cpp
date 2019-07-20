/*****************************************************************************
* Copyright 2015-2019 Alexander Barthel alex@littlenavmap.org
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
#include "airspace/airspacecontroller.h"
#include "options/optiondata.h"
#include "sql/sqlrecord.h"
#include "weather/windreporter.h"
#include "grib/windquery.h"

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
      html.textBar(10);

    info.aircraftText(mapSearchResult.userAircraft, html);
    info.aircraftProgressText(mapSearchResult.userAircraft, html, route,
                              false /* show more/less switch */, false /* true if less info mode */);

    numEntries++;
  }

  // Online Aircraft ===========================================================================
  for(const SimConnectAircraft& aircraft : mapSearchResult.onlineAircraft)
  {
    if(checkText(html, numEntries))
      return html.getHtml();

    if(!html.isEmpty())
      html.textBar(10);

    info.aircraftText(aircraft, html);
    info.aircraftProgressText(aircraft, html, Route(),
                              false /* show more/less switch */, false /* true if less info mode */);

    numEntries++;
  }

  // AI Aircraft ===========================================================================
  for(const SimConnectAircraft& aircraft : mapSearchResult.aiAircraft)
  {
    if(checkText(html, numEntries))
      return html.getHtml();

    if(!html.isEmpty())
      html.textBar(10);

    info.aircraftText(aircraft, html);
    info.aircraftProgressText(aircraft, html, Route(),
                              false /* show more/less switch */, false /* true if less info mode */);

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
        html.textBar(10);

      info.procedurePointText(ap, html, &route);

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
        html.textBar(10);

      map::WeatherContext currentWeatherContext;

      mainWindow->buildWeatherContextForTooltip(currentWeatherContext, airport);
      info.airportText(airport, currentWeatherContext, html, &route);

      numEntries++;
    }
  }

  // Logbook entries ===========================================================================
  for(const MapLogbookEntry& entry : mapSearchResult.logbookEntries)
  {
    if(checkText(html, numEntries))
      return html.getHtml();

    if(!html.isEmpty())
      html.textBar(10);

    info.logEntryText(entry, html);

    numEntries++;
  }

  // Navaids ===========================================================================
  if(opts & opts::TOOLTIP_NAVAID)
  {
    for(const MapUserpoint& up : mapSearchResult.userpoints)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.textBar(10);

      info.userpointText(up, html);

      numEntries++;
    }

    for(const MapVor& vor : mapSearchResult.vors)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.textBar(10);

      info.vorText(vor, html);

      numEntries++;
    }

    for(const MapNdb& ndb : mapSearchResult.ndbs)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.textBar(10);

      info.ndbText(ndb, html);

      numEntries++;
    }

    for(const MapWaypoint& wp : mapSearchResult.waypoints)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.textBar(10);

      info.waypointText(wp, html);

      numEntries++;
    }

    for(const MapMarker& m : mapSearchResult.markers)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.textBar(10);

      info.markerText(m, html);

      numEntries++;
    }

    for(const MapIls& ils : mapSearchResult.ils)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.textBar(10);

      info.ilsText(ils, html);

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
        html.textBar(10);

      info.towerText(ap, html);

      numEntries++;
    }
    for(const MapParking& p : mapSearchResult.parkings)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.textBar(10);

      info.parkingText(p, html);

      numEntries++;
    }
    for(const MapHelipad& p : mapSearchResult.helipads)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.textBar(10);

      info.helipadText(p, html);

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
        html.textBar(10);

      info.userpointTextRoute(up, html);

      numEntries++;
    }

    for(const MapAirway& airway : mapSearchResult.airways)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.textBar(10);

      info.airwayText(airway, html);

      numEntries++;
    }
  }

  // High altitude winds ===========================================================================
  if(opts & opts::TOOLTIP_WIND && mapSearchResult.windPos.isValid())
  {
    atools::grib::WindPosVector winds = NavApp::getWindReporter()->getWindStackForPos(mapSearchResult.windPos);
    if(!winds.isEmpty())
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.textBar(10);

      info.windText(winds, html, NavApp::getWindReporter()->getAltitude());

      numEntries++;
    }
  }

  // Airspaces ===========================================================================
  if(opts & opts::TOOLTIP_AIRSPACE)
  {
    // Put all online airspace on top of the list to have consistent ordering with menus and info windows
    MapSearchResult res = mapSearchResult.moveOnlineAirspacesToFront();

    for(const MapAirspace& airspace : res.airspaces)
    {
      if(checkText(html, numEntries))
        return html.getHtml();

      if(!html.isEmpty())
        html.textBar(10);

      atools::sql::SqlRecord onlineRec;
      if(airspace.isOnline())
        onlineRec = NavApp::getAirspaceController()->getOnlineAirspaceRecordById(airspace.id);

      info.airspaceText(airspace, onlineRec, html);

      numEntries++;
    }
  }

  QString str = html.getHtml();
  if(str.endsWith("<br/>"))
    str.chop(5);

#ifdef DEBUG_INFORMATION_TOOLTIP

  qDebug().noquote().nospace() << Q_FUNC_INFO << html.getHtml();

#endif

  return str;

}

/* Check if the result HTML has more than the allowed number of lines and add a "more" text */
bool MapTooltip::checkText(HtmlBuilder& html, int numEntries)
{
  if(numEntries >= MAX_ENTRIES)
  {
    html.textBar(10).b(tr("More ..."));
    return true;
  }

  return html.checklength(MAX_LINES, tr("More ..."));
}
