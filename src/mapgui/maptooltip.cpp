/*****************************************************************************
* Copyright 2015-2020 Alexander Barthel alex@littlenavmap.org
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

#include "mapgui/maptooltip.h"

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
#include "route/routealtitudeleg.h"
#include "mapgui/mappaintwidget.h"

#include <QPalette>
#include <QToolTip>

using namespace map;
using atools::util::HtmlBuilder;
using atools::fs::sc::SimConnectAircraft;
using atools::fs::sc::SimConnectUserAircraft;

/* Number of characters in the divider */
static const int TEXT_BAR_LENGTH = 20;

MapTooltip::MapTooltip(MainWindow *parentWindow)
  : mainWindow(parentWindow), weather(NavApp::getWeatherReporter())
{
  qDebug() << Q_FUNC_INFO;
}

MapTooltip::~MapTooltip()
{
  qDebug() << Q_FUNC_INFO;
}

QString MapTooltip::buildTooltip(const map::MapResult& mapSearchResult, const atools::geo::Pos& pos, const Route& route,
                                 bool airportDiagram)
{
  optsd::DisplayTooltipOptions opts = OptionData::instance().getDisplayTooltipOptions();

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << mapSearchResult;
#endif

  HtmlBuilder html(false);
  HtmlInfoBuilder info(mainWindow, NavApp::getMapPaintWidgetGui(),
                       false /* infoParam */, false /* infoParam */, opts.testFlag(optsd::TOOLTIP_VERBOSE));
  int numEntries = 0;
  bool userAircraftPrinted = false, overflow = false;

  // Append HTML text for all objects found in order of importance (airports first, etc.)
  // Objects are separated by a horizontal ruler
  // If max number of entries or lines is exceeded return the html

  // User Aircraft ===========================================================================
  if(!overflow && opts.testFlag(optsd::TOOLTIP_AIRCRAFT_USER))
  {
    if(mapSearchResult.userAircraft.isValid())
    {
      if(checkText(html))
      {
        overflow = true;
      }
      else
      {
        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.aircraftText(mapSearchResult.userAircraft.getAircraft(), html);
        info.aircraftProgressText(mapSearchResult.userAircraft.getAircraft(), html, route,
                                  false /* show more/less switch */, false /* true if less info mode */);
        userAircraftPrinted = true;
        numEntries++;
      }
    }
  }

  if(opts.testFlag(optsd::TOOLTIP_AIRCRAFT_AI))
  {
    // Online Aircraft ===========================================================================
    if(!overflow)
    {
      for(const map::MapOnlineAircraft& aircraft : mapSearchResult.onlineAircraft)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.aircraftText(aircraft.getAircraft(), html);
        info.aircraftProgressText(aircraft.getAircraft(), html, Route(),
                                  false /* show more/less switch */, false /* true if less info mode */);

        numEntries++;
      }
    }

    // AI Aircraft ===========================================================================
    if(!overflow)
    {
      for(const map::MapAiAircraft& aircraft : mapSearchResult.aiAircraft)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.aircraftText(aircraft.getAircraft(), html);
        info.aircraftProgressText(aircraft.getAircraft(), html, Route(),
                                  false /* show more/less switch */, false /* true if less info mode */);

        numEntries++;
      }
    }
  }

  // Navaids from procedure points ===========================================================================
  if(!overflow)
  {
    if(opts.testFlag(optsd::TOOLTIP_NAVAID))
    {
      for(const proc::MapProcedurePoint& ap : mapSearchResult.procPoints)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.procedurePointText(ap, html, &route);

        numEntries++;
      }
    }
  }

  // Holds ===========================================================================
  if(!overflow)
  {
    for(const MapHolding& entry : mapSearchResult.holdings)
    {
      if(checkText(html))
      {
        overflow = true;
        break;
      }

      if(!html.isEmpty())
        html.textBar(TEXT_BAR_LENGTH);

      info.holdingText(entry, html);

      numEntries++;
    }
  }

  // Traffic pattern ===========================================================================
  if(!overflow)
  {
    for(const TrafficPattern& entry : mapSearchResult.trafficPatterns)
    {
      if(checkText(html))
      {
        overflow = true;
        break;
      }

      if(!html.isEmpty())
        html.textBar(TEXT_BAR_LENGTH);

      info.trafficPatternText(entry, html);

      numEntries++;
    }
  }

  // Range rings ===========================================================================
  if(!overflow)
  {
    for(const RangeMarker& entry : mapSearchResult.rangeMarkers)
    {
      if(checkText(html))
      {
        overflow = true;
        break;
      }

      if(!html.isEmpty())
        html.textBar(TEXT_BAR_LENGTH);

      info.rangeMarkerText(entry, html);

      numEntries++;
    }
  }

  // Logbook entries ===========================================================================
  if(!overflow)
  {
    if(opts.testFlag(optsd::TOOLTIP_NAVAID))
    {
      for(const MapLogbookEntry& entry : mapSearchResult.logbookEntries)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.logEntryText(entry, html);

        numEntries++;
      }
    }
  }

  // Userpoints ===========================================================================
  if(!overflow)
  {
    if(opts.testFlag(optsd::TOOLTIP_NAVAID))
    {
      for(const MapUserpoint& up : mapSearchResult.userpoints)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.userpointText(up, html);

        numEntries++;
      }
    }
  }

  // Airports ===========================================================================
  if(!overflow)
  {
    if(opts.testFlag(optsd::TOOLTIP_AIRPORT))
    {
      for(const MapAirport& airport : mapSearchResult.airports)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        map::WeatherContext currentWeatherContext;

        mainWindow->buildWeatherContextForTooltip(currentWeatherContext, airport);
        info.airportText(airport, currentWeatherContext, html, &route);

        numEntries++;
      }
    }
  }

  // Navaids ===========================================================================
  if(opts.testFlag(optsd::TOOLTIP_NAVAID))
  {
    if(!overflow)
    {
      for(const MapVor& vor : mapSearchResult.vors)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.vorText(vor, html);

        numEntries++;
      }
    }

    if(!overflow)
    {
      for(const MapNdb& ndb : mapSearchResult.ndbs)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.ndbText(ndb, html);

        numEntries++;
      }
    }

    if(!overflow)
    {
      for(const MapWaypoint& wp : mapSearchResult.waypoints)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.waypointText(wp, html);

        numEntries++;
      }
    }

    if(!overflow)
    {
      for(const MapMarker& m : mapSearchResult.markers)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.markerText(m, html);

        numEntries++;
      }
    }

    if(!overflow)
    {
      for(const MapIls& ils : mapSearchResult.ils)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.ilsTextInfo(ils, html);

        numEntries++;
      }
    }
  }

  // Airport stuff ===========================================================================
  if(airportDiagram && opts.testFlag(optsd::TOOLTIP_AIRPORT))
  {
    if(!overflow)
    {
      for(const MapAirport& ap : mapSearchResult.towers)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.towerText(ap, html);

        numEntries++;
      }
    }

    if(!overflow)
    {
      for(const MapParking& p : mapSearchResult.parkings)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.parkingText(p, html);

        numEntries++;
      }
    }

    if(!overflow)
    {
      for(const MapHelipad& p : mapSearchResult.helipads)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.helipadText(p, html);

        numEntries++;
      }
    }
  }

  if(opts.testFlag(optsd::TOOLTIP_NAVAID))
  {
    if(!overflow)
    {
      for(const MapUserpointRoute& up : mapSearchResult.userpointsRoute)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.userpointTextRoute(up, html);

        numEntries++;
      }
    }

    if(!overflow)
    {
      for(const MapAirway& airway : mapSearchResult.airways)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        info.airwayText(airway, html);

        numEntries++;
      }
    }
  }

  // High altitude winds ===========================================================================
  if(!overflow)
  {
    if(opts.testFlag(optsd::TOOLTIP_WIND) && mapSearchResult.windPos.isValid())
    {
      WindReporter *windReporter = NavApp::getWindReporter();
      atools::grib::WindPosVector winds = windReporter->getWindStackForPos(mapSearchResult.windPos);
      if(!winds.isEmpty())
      {
        if(checkText(html))
        {
          overflow = true;
        }
        else
        {
          if(!html.isEmpty())
            html.textBar(TEXT_BAR_LENGTH);

          info.windText(winds, html, windReporter->getAltitude(), windReporter->getSourceText());

#ifdef DEBUG_INFORMATION
          html.hr().small(QString("Pos(%1, %2), alt(%3)").
                          arg(mapSearchResult.windPos.getLonX()).arg(mapSearchResult.windPos.getLatY()).
                          arg(windReporter->getAltitude(), 0, 'f', 2)).br();

          html.small(windReporter->getDebug(mapSearchResult.windPos));
#endif
          numEntries++;
        }
      }
    }
  }

  // Airspaces ===========================================================================
  if(!overflow)
  {
    if(opts.testFlag(optsd::TOOLTIP_AIRSPACE))
    {
      // Put all online airspace on top of the list to have consistent ordering with menus and info windows
      MapResult res = mapSearchResult.moveOnlineAirspacesToFront();

      for(const MapAirspace& airspace : res.airspaces)
      {
        if(checkText(html))
        {
          overflow = true;
          break;
        }

        if(!html.isEmpty())
          html.textBar(TEXT_BAR_LENGTH);

        atools::sql::SqlRecord onlineRec;
        if(airspace.isOnline())
          onlineRec = NavApp::getAirspaceController()->getOnlineAirspaceRecordById(airspace.id);

        info.airspaceText(airspace, onlineRec, html);

        numEntries++;
      }
    }
  }

  QString str;

  // Prepend distance and bearing information ================================
  if(!html.isEmpty())
  {
    HtmlBuilder temp = html.cleared();
    info.bearingAndDistanceTexts(pos, NavApp::getMagVar(pos), temp, userAircraftPrinted);
    if(!temp.isEmpty())
      temp.textBar(TEXT_BAR_LENGTH);
    str = temp.getHtml() + html.getHtml();

    if(str.endsWith("<br/>"))
      str.chop(5);
  }

#ifdef DEBUG_INFORMATION_TOOLTIP

  qDebug().noquote().nospace() << Q_FUNC_INFO << html.getHtml();

#endif

  return str;

}

/* Check if the result HTML has more than the allowed number of lines and add a "more" text */
bool MapTooltip::checkText(HtmlBuilder& html)
{
  return html.checklengthTextBar(MAX_LINES, tr("More ..."), TEXT_BAR_LENGTH);
}
