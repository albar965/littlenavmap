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

#include "mapgui/mapvisible.h"

#include "airspace/airspacecontroller.h"
#include "atools.h"
#include "common/maptypes.h"
#include "common/unit.h"
#include "fs/common/morareader.h"
#include "gui/mainwindow.h"
#include "mapgui/mapairporthandler.h"
#include "mapgui/maplayer.h"
#include "mapgui/mapmarkhandler.h"
#include "mappainter/mappaintlayer.h"
#include "app/navapp.h"
#include "query/mapquery.h"
#include "userdata/userdatacontroller.h"
#include "util/htmlbuilder.h"
#include "weather/windreporter.h"

#include <QStringBuilder>

MapVisible::MapVisible(MapPaintLayer *paintLayerParam)
  : paintLayer(paintLayerParam)
{

}

MapVisible::~MapVisible()
{

}

/* Update the visible objects indication in the status bar. */
void MapVisible::updateVisibleObjectsStatusBar()
{
  if(simDbEmpty)
  {
    NavApp::getMainWindow()->setMapObjectsShownMessageText(
      atools::util::HtmlBuilder::errorMessage(tr("Database is empty")),
      tr("<p style='white-space:pre'>The currently selected scenery database for the simulator is empty.<br/>Go to: "
           "Main menu -&gt; \"Scenery Library\" -&gt; \"Load Scenery Library\" "
           "or press <code>Ctrl+Shift+L</code>.<br/>"
           "Then select the simulator and press \"Load\".</p>",
         "Keep instructions in sync with translated menus and shortcuts"));
  }
  else
  {
    const MapLayer *layer = paintLayer->getMapLayer();

    if(layer != nullptr && !paintLayer->noRender())
    {
      map::MapTypes shown = paintLayer->getShownMapTypes();
      map::MapDisplayTypes shownDispTypes = paintLayer->getShownMapDisplayTypes();

      QStringList airportShortLabel;
      atools::util::HtmlBuilder tooltip(false);
      tooltip.b(tr("Currently shown on map:"));
      tooltip.table();

      // Collect airport information ==========================================================
      if(layer->isAirport() && shown & map::AIRPORT_ALL_VISIBLE && shown.testFlag(map::AIRPORT))
      {
        tooltip.tr().td();

        QStringList apShort;

        tooltip.b(tr("Airports (AP): "));
        apShort.append(tr("AP"));

        if(shown.testFlag(map::AIRPORT_HARD) || shown.testFlag(map::AIRPORT_SOFT) || shown.testFlag(map::AIRPORT_WATER))
        {
          QStringList runways;
          if(shown.testFlag(map::AIRPORT_HARD))
          {
            runways.append(tr("hard (H)"));
            apShort.append(tr("H"));
          }

          if(shown.testFlag(map::AIRPORT_SOFT) && layer->isAirportMinor())
          {
            runways.append(tr("soft (S)"));
            apShort.append(tr("S"));
          }

          if(shown.testFlag(map::AIRPORT_WATER) && layer->isAirportMinor())
          {
            runways.append(tr("water (W)"));
            apShort.append(tr("W"));
          }
          if(!runways.isEmpty())
            tooltip.text(tr("With ") % atools::strJoin(runways, tr(", "), tr(" and "), tr(" runways."))).br();
        }

        int minRunwayLength = std::max(NavApp::getMapAirportHandler()->getMinimumRunwayFt(), layer->getMinRunwayLength());
        if(minRunwayLength > 0)
        {
          apShort.append(">" % QLocale().toString(minRunwayLength / 100));
          tooltip.text(tr("Having runway length > %1.").arg(Unit::distShortFeet(minRunwayLength))).br();
        }

        QStringList features;
        if(shown.testFlag(map::AIRPORT_EMPTY))
        {
          features.append(tr("empty with zero rating (E)"));
          apShort.append(tr("E"));
        }

        if(shown.testFlag(map::AIRPORT_HELIPAD) && layer->isAirportMinor())
        {
          features.append(tr("heliports (O)"));
          apShort.append(tr("O"));
        }

        if(shown.testFlag(map::AIRPORT_UNLIGHTED))
          features.append(tr("unlighted"));
        else
        {
          features.append(tr("only lighted (L)"));
          apShort.append(tr("L"));
        }

        if(shown.testFlag(map::AIRPORT_NO_PROCS))
          features.append(tr("without procedure"));
        else
        {
          features.append(tr("only with procedure (P)"));
          apShort.append(tr("P"));
        }

        if(shown.testFlag(map::AIRPORT_CLOSED))
        {
          features.append(tr("closed (C)"));
          apShort.append(tr("C"));
        }

        if(shown.testFlag(map::AIRPORT_MILITARY))
        {
          features.append(tr("military (M)"));
          apShort.append(tr("M"));
        }

        if(shown.testFlag(map::AIRPORT_ADDON))
        {
          features.append(tr("add-on display forced (A)"));
          apShort.append(tr("A"));
        }
        if(!features.isEmpty())
          tooltip.text(tr("Showing ") % atools::strJoin(features, tr(", "), tr(" and "), tr(".")));
        tooltip.tdEnd().trEnd();

        airportShortLabel.append(apShort);
      }
      else
        tooltip.tr().td(tr("No airports")).trEnd();

      QStringList navaidLabel, navaidsTooltip;
      // Collect navaid information ==========================================================
      if(layer->isVor() && shown.testFlag(map::VOR))
      {
        navaidLabel.append(tr("V"));
        navaidsTooltip.append(tr("VOR (V)"));
      }

      if(layer->isNdb() && shown.testFlag(map::NDB))
      {
        navaidLabel.append(tr("N"));
        navaidsTooltip.append(tr("NDB (N)"));
      }

      if(layer->isWaypoint() && shown.testFlag(map::WAYPOINT))
      {
        navaidLabel.append(tr("W"));
        navaidsTooltip.append(tr("Waypoints (W)"));
      }

      if(layer->isIls() && shown.testFlag(map::ILS))
      {
        navaidLabel.append(tr("I"));
        navaidsTooltip.append(tr("ILS (I)"));
      }

      if(layer->isIls() && shownDispTypes.testFlag(map::GLS) && NavApp::getMapQueryGui()->hasGls())
      {
        navaidLabel.append(tr("G"));
        navaidsTooltip.append(tr("GLS (G)"));
      }

      if(layer->isAirway() && shown.testFlag(map::AIRWAYV))
      {
        navaidLabel.append(tr("VA"));
        navaidsTooltip.append(tr("Victor Airways (VA)"));
      }

      if(layer->isAirway() && shown.testFlag(map::AIRWAYJ))
      {
        navaidLabel.append(tr("JA"));
        navaidsTooltip.append(tr("Jet Airways (JA)"));
      }

      if(layer->isTrack() && shown.testFlag(map::TRACK))
      {
        navaidLabel.append(tr("T"));
        navaidsTooltip.append(tr("Tracks (T)"));
      }

      if(layer->isHolding() && shown.testFlag(map::HOLDING))
      {
        navaidLabel.append(tr("H"));
        navaidsTooltip.append(tr("Holdings (H)"));
      }

      if(layer->isAirportMsa() && shown.testFlag(map::AIRPORT_MSA))
      {
        navaidLabel.append(tr("MSA"));
        navaidsTooltip.append(tr("MSA Sectors (MSA)"));
      }

      if(layer->isMora() && shownDispTypes.testFlag(map::MORA) && NavApp::getMoraReader()->isDataAvailable())
      {
        navaidLabel.append(tr("MORA"));
        navaidsTooltip.append(tr("MORA Grid (MORA)"));
      }

      QStringList airspacesTooltip, airspaceGroupLabel, airspaceGroupTooltip, airspaceSrcTooltip;
      map::MapAirspaceSources airspaceSources = NavApp::getAirspaceController()->getAirspaceSources();
      if(shown.testFlag(map::AIRSPACE) && airspaceSources & map::AIRSPACE_SRC_ALL)
      {
        map::MapAirspaceFilter airspaceFilter = paintLayer->getShownAirspacesTypesByLayer();
        // Collect airspace information ==========================================================
        for(int i = 0; i < map::MAP_AIRSPACE_TYPE_BITS; i++)
        {
          map::MapAirspaceTypes type(1 << i);
          if(airspaceFilter.types & type)
            airspacesTooltip.append(map::airspaceTypeToString(type));
        }
        std::sort(airspacesTooltip.begin(), airspacesTooltip.end());

        if(airspaceFilter.types & map::AIRSPACE_CLASS_ICAO)
        {
          airspaceGroupLabel.append(tr("ICAO"));
          airspaceGroupTooltip.append(tr("Class A-E (ICAO)"));
        }

        if(airspaceFilter.types & map::AIRSPACE_CLASS_FG)
        {
          airspaceGroupLabel.append(tr("F,G"));
          airspaceGroupTooltip.append(tr("Class F and/or G"));
        }

        if(airspaceFilter.types & map::AIRSPACE_FIR_UIR)
        {
          airspaceGroupLabel.append(tr("FIR,UIR"));
          airspaceGroupTooltip.append(tr("Flight and/or Upper Information Regions"));
        }

        if(airspaceFilter.types & map::AIRSPACE_RESTRICTED)
        {
          airspaceGroupLabel.append(tr("RSTR"));
          airspaceGroupTooltip.append(tr("Restricted (RSTR)"));
        }

        if(airspaceFilter.types & map::AIRSPACE_SPECIAL)
        {
          airspaceGroupLabel.append(tr("SPEC"));
          airspaceGroupTooltip.append(tr("Special (SPEC)"));
        }

        if(airspaceFilter.types & map::AIRSPACE_OTHER || airspaceFilter.types & map::AIRSPACE_CENTER)
        {
          airspaceGroupLabel.append(tr("OTR"));
          airspaceGroupTooltip.append(tr("Centers and others (OTR)"));
        }

        airspaceSrcTooltip = NavApp::getAirspaceController()->getAirspaceSourcesStr();
      }

      if(!navaidsTooltip.isEmpty())
        tooltip.tr().td().b(tr("Navaids: ")).text(navaidsTooltip.join(", ")).tdEnd().trEnd();
      else
        tooltip.tr().td(tr("No navaids")).trEnd();

      QString asText, asGroupText, asSrcText;
      if(shown.testFlag(map::AIRSPACE))
      {
        asText = tr("Airspaces: ");
        asGroupText = tr("Airspace Groups: ");
        asSrcText = tr("Airspace Sources: ");
      }

      if(!airspacesTooltip.isEmpty())
      {
        tooltip.tr().td().b(asSrcText).text(airspaceSrcTooltip.join(", ")).tdEnd().trEnd();
        tooltip.tr().td().b(asGroupText).text(airspaceGroupTooltip.join(", ")).tdEnd().trEnd();
        tooltip.tr().td().b(asText).text(airspacesTooltip.join(", ")).tdEnd().trEnd();
      }
      else
        tooltip.tr().td(tr("No airspaces")).trEnd();

      // Collect AI information ==========================================================
      QStringList aiLabel;
      QStringList ai;
      if(NavApp::isConnected())
      {
        // AI vehicles
        if(shown.testFlag(map::AIRCRAFT_AI) && layer->isAiAircraftLarge())
        {
          QString ac;
          if(!layer->isAiAircraftSmall())
          {
            ac = tr("Aircraft > %1 ft").arg(layer::LARGE_AIRCRAFT_SIZE);
            aiLabel.append("A>");
          }
          else
          {
            ac = tr("Aircraft");
            aiLabel.append("A");
          }

          if(layer->isAiAircraftGround())
          {
            ac.append(tr(" on ground"));
            aiLabel.last().append("G");
          }
          ai.append(ac);
        }

        if(shown.testFlag(map::AIRCRAFT_AI_SHIP) && layer->isAiShipLarge())
        {
          if(!layer->isAiShipSmall())
          {
            ai.append(tr("Ships > %1 ft").arg(layer::LARGE_SHIP_SIZE));
            aiLabel.append("S>");
          }
          else
          {
            ai.append(tr("Ships"));
            aiLabel.append("S");
          }
        }
      }
      if(shown.testFlag(map::AIRCRAFT_AI) && NavApp::isOnlineNetworkActive())
        ai.append(tr("%1 Clients / Aircraft").arg(NavApp::getOnlineNetworkTranslated()));

      if(!ai.isEmpty())
        tooltip.tr().td().b(tr("AI / Multiplayer / online client: ")).text(ai.join(", ")).tdEnd().trEnd();
      else
        tooltip.tr().td(tr("No AI / Multiplayer / online client")).trEnd();

      // Weather ==========================================================
      QStringList weatherLabel;
      if(shownDispTypes.testFlag(map::AIRPORT_WEATHER) && layer->isAirportWeather())
      {
        tooltip.tr().td().b(tr("Airport weather source (AW): ")).
        text(map::mapWeatherSourceString(paintLayer->getWeatherSource())).tdEnd().trEnd();

        if(paintLayer->getWeatherSource() != map::WEATHER_SOURCE_DISABLED)
          weatherLabel.append(tr("AW"));
      }
      else
        tooltip.tr().td(tr("No airport weather shown")).trEnd();

      if(shownDispTypes.testFlag(map::WIND_BARBS) && layer->getWindBarbs() > 0)
      {
        WindReporter *windReporter = NavApp::getWindReporter();
        tooltip.tr().td().b(tr("Wind shown (W): ")).text(windReporter->getLevelText()).
        b(tr(" Wind source: ")).text(windReporter->getSourceText()).tdEnd().trEnd();

        if(windReporter->isWindShown() && windReporter->getSource() != wind::WIND_SOURCE_DISABLED)
          weatherLabel.append(tr("W"));
      }
      else
        tooltip.tr().td(tr("No wind shown")).trEnd();

      // Flight plan and track ==========================================================
      QStringList routeLabel;
      if(shownDispTypes.testFlag(map::FLIGHTPLAN)) // Always shown until cut off distance
      {
        tooltip.tr().td().b(tr("Flight plan (F)")).tdEnd().trEnd();
        routeLabel.append(tr("F"));
      }
      else
        tooltip.tr().td(tr("No wind shown")).trEnd();

      if(shown.testFlag(map::AIRCRAFT_TRAIL)) // Always shown until cut off distance
      {
        tooltip.tr().td().b(tr("Aircraft trail (T)")).tdEnd().trEnd();
        routeLabel.append(tr("T"));
      }
      else
        tooltip.tr().td(tr("No aircraft trail shown")).trEnd();

      if(layer->isUserpoint())
      {
        QStringList types = NavApp::getUserdataController()->getSelectedTypes();
        if(!types.isEmpty())
          tooltip.tr().td().b(tr("Userpoints: ")).text(atools::elideTextShort(types.join(tr(", ")), 160)).tdEnd().trEnd();
      }

      QStringList markTypes = NavApp::getMapMarkHandler()->getMarkTypesText();
      if(!markTypes.isEmpty())
        tooltip.tr().td().b(tr("User features: ")).text(markTypes.join(tr(", "))).tdEnd().trEnd();

      tooltip.tableEnd();

      QStringList label;
      if(!airportShortLabel.isEmpty())
        label.append(airportShortLabel.join(tr(",")));
      if(!navaidLabel.isEmpty())
        label.append(navaidLabel.join(tr(",")));
      if(!airspaceGroupLabel.isEmpty())
        label.append(airspaceGroupLabel.join(tr(",")));
      if(!aiLabel.isEmpty())
        label.append(aiLabel.join(tr(",")));
      if(!weatherLabel.isEmpty())
        label.append(weatherLabel.join(tr(",")));
      if(!routeLabel.isEmpty())
        label.append(routeLabel.join(tr(",")));

      if(label.isEmpty())
        label.append(tr(" — "));

      // Update the statusbar label text and tooltip of the label
      NavApp::getMainWindow()->setMapObjectsShownMessageText(atools::elideTextShort(label.join(tr("/")), 40), tooltip.getHtml());
    } // if(layer != nullptr && !paintLayer->noRender())
    else
      NavApp::getMainWindow()->setMapObjectsShownMessageText(tr(" — "), tr("Nothing shown. Zoom in to see map features."));
  } // if(!NavApp::hasDataInDatabase()) ... else
}

void MapVisible::postDatabaseLoad()
{
  // Remember value since call is expensive
  simDbEmpty = !NavApp::hasDataInSimDatabase();
}
