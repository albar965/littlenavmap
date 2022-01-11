/*****************************************************************************
* Copyright 2015-2021 Alexander Barthel alex@littlenavmap.org
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

#include "route/routelabel.h"

#include "navapp.h"

#include "route/route.h"
#include "route/routealtitude.h"
#include "fs/util/fsutil.h"
#include "query/airportquery.h"
#include "common/constants.h"
#include "atools.h"
#include "util/htmlbuilder.h"
#include "ui_mainwindow.h"
#include "fs/pln/flightplan.h"
#include "common/unit.h"
#include "common/formatter.h"
#include "settings/settings.h"

#include <QStringBuilder>

using atools::fs::pln::Flightplan;
using atools::util::HtmlBuilder;
namespace autil = atools::util;
namespace ahtml = atools::util::html;

RouteLabel::RouteLabel(QWidget *parent, const Route& routeParam)
  : QObject(parent), route(routeParam)
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  connect(ui->labelRouteInfo, &QLabel::linkActivated, this, &RouteLabel::flightplanLabelLinkActivated);
}

RouteLabel::~RouteLabel()
{

}

void RouteLabel::saveState()
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  settings.setValue(lnm::ROUTE_HEADER_AIRPORT, headerAirports);
  settings.setValue(lnm::ROUTE_HEADER_DEPARTDEST, headerDepartDest);
  settings.setValue(lnm::ROUTE_HEADER_RUNWAY, headerRunway);
  settings.setValue(lnm::ROUTE_HEADER_DISTTIME, headerDistTime);
}

void RouteLabel::restoreState()
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  headerAirports = settings.valueBool(lnm::ROUTE_HEADER_AIRPORT, true);
  headerDepartDest = settings.valueBool(lnm::ROUTE_HEADER_DEPARTDEST, true);
  headerRunway = settings.valueBool(lnm::ROUTE_HEADER_RUNWAY, true);
  headerDistTime = settings.valueBool(lnm::ROUTE_HEADER_DISTTIME, true);
}

/* Update the dock window top level label */
void RouteLabel::updateWindowLabel()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  bool visible = !route.isEmpty() && (headerAirports || headerDepartDest || headerRunway || headerDistTime);

  // Hide label if no plan or nothing selected
  ui->labelRouteInfo->setVisible(visible);

  if(visible)
  {
    autil::HtmlBuilder htmlAirports, htmlDeparArr, htmlRunway, htmlDistTime;
    if(headerAirports)
      buildHeaderAirports(htmlAirports, true /* widget */);

    if(headerDepartDest)
      buildHeaderDepartArrival(htmlDeparArr, true /* widget */);

    if(headerRunway)
      buildHeaderRunway(htmlRunway);

    if(headerDistTime)
      buildHeaderDistTime(htmlDistTime);

    // Join all texts with <br>
    ui->labelRouteInfo->setText(HtmlBuilder::joinBr({htmlAirports, htmlDeparArr, htmlRunway, htmlDistTime}));
  }
  else
    ui->labelRouteInfo->clear();
}

void RouteLabel::buildHtmlText(atools::util::HtmlBuilder& html)
{
  buildPrintText(html, false /* titleOnly */);
}

void RouteLabel::buildPrintText(atools::util::HtmlBuilder& html, bool titleOnly)
{
  buildHeaderAirports(html, false /* widget */);

  if(!titleOnly)
  {
    html.p();
    buildHeaderTocTod(html);
    html.pEnd();

    html.p();
    buildHeaderDepartArrival(html, false /* widget */);
    html.pEnd();

    html.p();
    buildHeaderRunway(html);
    html.pEnd();

    html.p();
    buildHeaderDistTime(html);
    html.pEnd();
  }
}

void RouteLabel::buildHeaderAirports(atools::util::HtmlBuilder& html, bool widget)
{
  const Flightplan& flightplan = route.getFlightplan();
  QString departureAirport, departureParking, destinationAirport;

  // Add departure to text ==============================================================
  if(route.hasValidDeparture())
  {
    departureAirport = tr("%1 (%2)").
                       arg(route.getDepartureAirportLeg().getName()).
                       arg(route.getDepartureAirportLeg().getDisplayIdent());

    if(route.getDepartureAirportLeg().getDepartureParking().isValid())
      departureParking = map::parkingNameOrNumber(route.getDepartureAirportLeg().getDepartureParking());
    else if(route.getDepartureAirportLeg().getDepartureStart().isValid())
    {
      const map::MapStart& start = route.getDepartureAirportLeg().getDepartureStart();
      if(route.hasDepartureHelipad())
        departureParking += tr("Helipad %1").arg(start.runwayName);
      else if(route.hasDepartureRunway())
        departureParking += tr("Runway %1").arg(start.runwayName);
      else
        departureParking += tr("Unknown Start");
    }
  }
  else
  {
    departureAirport = tr("%1 (%2)").
                       arg(flightplan.getEntries().first().getIdent()).
                       arg(flightplan.getEntries().first().getWaypointTypeAsDisplayString());
  }

  // Add destination to text ==============================================================
  if(route.hasValidDestination())
    destinationAirport = tr("%1 (%2)").
                         arg(route.getDestinationAirportLeg().getName()).
                         arg(route.getDestinationAirportLeg().getDisplayIdent());
  else
    destinationAirport = tr("%1 (%2)").
                         arg(flightplan.at(route.getDestinationAirportLegIndex()).getIdent()).
                         arg(flightplan.at(route.getDestinationAirportLegIndex()).getWaypointTypeAsDisplayString());

  if(!route.isEmpty())
  {
    if(widget)
    {
      // Add airports with links ==============================
      html.a(departureAirport, "lnm://showdeparture", ahtml::LINK_NO_UL | ahtml::BOLD);
      if(!departureParking.isEmpty())
        html.nbsp().a(departureParking, "lnm://showdepartureparking", ahtml::LINK_NO_UL | ahtml::BOLD);

      if(!destinationAirport.isEmpty() && route.getSizeWithoutAlternates() > 1)
        html.text(tr(" to ")).a(destinationAirport, "lnm://showdestination", ahtml::LINK_NO_UL | ahtml::BOLD);
    }
    else
    {
      // Simple text for print and HTML ==============================
      QString header(departureAirport);
      if(!destinationAirport.isEmpty() && route.getSizeWithoutAlternates() > 1)
        header.append(tr(" to ") % destinationAirport);
      html.h1(header);
    }
  }
}

void RouteLabel::buildHeaderDepartArrival(atools::util::HtmlBuilder& html, bool widget)
{
  QString approachRunway, starRunway;

  // Add procedures to text ==============================================================

  HtmlBuilder departHtml = html.cleared(), arrHtml = html.cleared();

  if(route.hasAnyProcedure())
  {
    const proc::MapProcedureLegs& sidLegs = route.getSidLegs();
    const proc::MapProcedureLegs& starLegs = route.getStarLegs();
    const proc::MapProcedureLegs& arrivalLegs = route.getApproachLegs();

    if(!sidLegs.isEmpty() && !sidLegs.isCustomDeparture())
    {
      // Add departure procedure to text ===========================================
      // Custom is only shown in runways section
      departHtml.b(tr("Depart"));
      if(!sidLegs.runwayEnd.isValid())
        departHtml.text(tr(" using SID "));
      else
      {
        departHtml.text(tr(" runway "));
        departHtml.b(sidLegs.runwayEnd.name);
        departHtml.text(tr(" using SID "));
      }

      departHtml.b(sidLegs.approachFixIdent);

      if(arrivalLegs.mapType & proc::PROCEDURE_ARRIVAL_ALL || starLegs.mapType & proc::PROCEDURE_ARRIVAL_ALL)
        departHtml.text(tr(". "));
    }

    // Add arrival procedures to text ==============================================================
    if(!starLegs.isEmpty())
    {
      // STAR ==============================================================
      arrHtml.b(tr("Arrive "));
      arrHtml.text(tr(" using STAR "));

      QString star(starLegs.approachFixIdent);
      if(!starLegs.transitionFixIdent.isEmpty())
        star += "." + starLegs.transitionFixIdent;
      arrHtml.b(star);

      starRunway = starLegs.procedureRunway;

      if(!(arrivalLegs.mapType & proc::PROCEDURE_APPROACH))
      {
        // No approach. Direct from STAR to runway.
        arrHtml.text(tr(" at runway "));
        arrHtml.b(starLegs.procedureRunway);
      }
      else if(!starLegs.procedureRunway.isEmpty())
        arrHtml.text(tr(" (")).b(starLegs.procedureRunway).text(tr(") "));

      if(!(arrivalLegs.mapType & proc::PROCEDURE_APPROACH))
        arrHtml.text(tr(". "));
    }

    if(arrivalLegs.mapType & proc::PROCEDURE_TRANSITION)
    {
      // Approach transition ==============================================================
      if(!starLegs.isEmpty())
        arrHtml.text(tr(" via "));
      else
        arrHtml.b(tr("Arrive ")).text(tr(" via "));
      arrHtml.b(arrivalLegs.transitionFixIdent);
    }

    if(arrivalLegs.mapType & proc::PROCEDURE_APPROACH)
    {
      // Approach ==============================================================
      // Custom is only shown in runways section
      if(!arrivalLegs.isCustomApproach())
      {
        if(arrivalLegs.mapType & proc::PROCEDURE_TRANSITION || !starLegs.isEmpty())
          arrHtml.text(tr(" and "));
        else
          arrHtml.b(tr("Arrive ")).text(tr(" via "));

        // Type and suffix =======================
        QString type(arrivalLegs.displayApproachType());
        if(!arrivalLegs.approachSuffix.isEmpty())
          type += tr("-%1").arg(arrivalLegs.approachSuffix);

        arrHtml.b(type).nbsp();
        arrHtml.b(arrivalLegs.approachFixIdent);

        if(!arrivalLegs.approachArincName.isEmpty())
          arrHtml.b(tr(" (%1) ").arg(arrivalLegs.approachArincName));

        // Runway =======================
        if(arrivalLegs.runwayEnd.isValid() && !arrivalLegs.runwayEnd.name.isEmpty())
        {
          // Add runway for approach
          arrHtml.text(arrHtml.isEmpty() ? tr("At runway ") : tr(" at runway "));
          arrHtml.b(arrivalLegs.runwayEnd.name);
          arrHtml.text(tr(". "));
        }
        else
          // Add runway text
          arrHtml.text(arrHtml.isEmpty() ? tr("At runway.") : tr(" at runway."));
      }

      approachRunway = arrivalLegs.runwayEnd.name;
    }

    if(widget)
    {
      // Header label widget
      html.append(departHtml);
      if(!arrHtml.isEmpty())
      {
        if(!departHtml.isEmpty())
          html.br();
        html.append(arrHtml);
      }

      // Check STAR and approach runways - these have to match
      if(!approachRunway.isEmpty() && !starRunway.isEmpty() && !atools::fs::util::runwayEqual(approachRunway, starRunway))
        html.br().error(tr("STAR runway \"%1\" not equal to approach runway \"%2\".").arg(starRunway).arg(approachRunway));
    }
    else
    {
      // HTML export or printing
      if(!departHtml.isEmpty() || !arrHtml.isEmpty())
      {
        html.p();
        if(!departHtml.isEmpty())
          html.append(departHtml);
        if(!arrHtml.isEmpty())
          html.nbsp().append(arrHtml);
        html.pEnd();
      }
    }
  }
}

void RouteLabel::buildHeaderRunway(atools::util::HtmlBuilder& html)
{
  if(route.hasAnyProcedure())
  {
    if(route.hasValidDeparture())
    {
      // Departure runway information =======================================
      const proc::MapProcedureLegs& departureLegs = route.getSidLegs();
      const map::MapRunwayEnd& end = departureLegs.runwayEnd;
      if(!departureLegs.isEmpty() && end.isValid())
      {
        const RouteLeg& departLeg = route.getDepartureAirportLeg();

        if(departLeg.isValid())
        {
          map::MapRunway runway = NavApp::getAirportQuerySim()->getRunwayByEndId(departLeg.getId(), end.id);
          if(runway.isValid())
          {
            html.b(tr("Takeoff")).text(tr(" from ")).b(end.name).text(tr(", "));
            html.text(formatter::courseTextFromTrue(end.heading, departLeg.getMagvar()), ahtml::NO_ENTITIES);
            html.text(tr(", ")).text(Unit::distShortFeet(runway.length)).text(tr(". "));
          }
        }
      }
    } // if(route.hasValidDeparture())

    if(route.hasValidDestination())
    {
      // Destination runway information =======================================
      const proc::MapProcedureLegs& apprLegs = route.getApproachLegs();
      const map::MapRunwayEnd& end = apprLegs.runwayEnd;
      if(!apprLegs.isEmpty() && end.isValid())
      {
        const RouteLeg& destLeg = route.getDestinationAirportLeg();

        if(destLeg.isValid())
        {
          map::MapRunway runway = NavApp::getAirportQuerySim()->getRunwayByEndId(destLeg.getId(), end.id);
          if(runway.isValid())
          {
            html.b(tr("Land")).text(tr(" at ")).b(end.name).text(tr(", "));

            QStringList rwAtts;
            rwAtts.append(formatter::courseTextFromTrue(end.heading, destLeg.getMagvar()));
            rwAtts.append(Unit::distShortFeet(runway.length - (end.secondary ? runway.secondaryOffset : runway.primaryOffset)));
            rwAtts.append(Unit::altFeet(destLeg.getAltitude()) % tr(" elevation"));
            if(end.hasAnyVasi())
              rwAtts.append(end.uniqueVasiTypeStr().join(QObject::tr("/")));

            html.text(rwAtts.join(tr(", ")), ahtml::NO_ENTITIES);
            html.text(tr(". "));
          }
        }
      }
    } // if(route.hasValidDestination())
  } // if(route.hasAnyProcedure())
}

void RouteLabel::buildHeaderTocTod(atools::util::HtmlBuilder& html)
{
  html.p().b(tr("Cruising altitude ")).text(Unit::altFeet(route.getCruisingAltitudeFeet()));

  if(route.getTopOfClimbDistance() < map::INVALID_DISTANCE_VALUE || route.getTopOfDescentFromDestination() < map::INVALID_DISTANCE_VALUE)
  {
    if(route.getTopOfClimbDistance() < map::INVALID_DISTANCE_VALUE)
      html.text(tr(", ")).b(tr(" departure to top of climb ")).text(Unit::distNm(route.getTopOfClimbDistance()));

    if(route.getTopOfDescentFromDestination() < map::INVALID_DISTANCE_VALUE)
      html.text(tr(", ")).b(tr(" start of descent to destination ")).text(Unit::distNm(route.getTopOfDescentFromDestination()));
  }
  html.text(tr(".")).pEnd();
}

void RouteLabel::buildHeaderDistTime(atools::util::HtmlBuilder& html)
{
  if(!route.isEmpty())
  {
    if(route.getSizeWithoutAlternates() > 1)
    {
      html.b(tr("Distance ")).text(Unit::distNm(route.getTotalDistance()));

      if(route.getAltitudeLegs().getTravelTimeHours() > 0.f)
      {
        if(route.getSizeWithoutAlternates() > 1)
          html.text(tr(", ")).b(tr("time "));
        else
          html.b(tr("Time "));

        html.text(formatter::formatMinutesHoursLong(route.getAltitudeLegs().getTravelTimeHours()));
      }
      html.text(tr("."));
    }
  }
}
