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

#include "route/routelabel.h"

#include "atools.h"
#include "common/constants.h"
#include "common/formatter.h"
#include "common/unit.h"
#include "fs/perf/aircraftperf.h"
#include "fs/pln/flightplan.h"
#include "fs/util/fsutil.h"
#include "gui/clicktooltiphandler.h"
#include "app/navapp.h"
#include "perf/aircraftperfcontroller.h"
#include "query/airportquery.h"
#include "query/mapquery.h"
#include "route/route.h"
#include "route/routealtitude.h"
#include "route/routecontroller.h"
#include "settings/settings.h"
#include "ui_mainwindow.h"
#include "util/htmlbuilder.h"

#include <QStringBuilder>

using atools::fs::pln::Flightplan;
using atools::util::HtmlBuilder;
using atools::fs::perf::AircraftPerf;
namespace autil = atools::util;
namespace ahtml = atools::util::html;

RouteLabel::RouteLabel(QWidget *parent, const Route& routeParam)
  : QObject(parent), route(routeParam)
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  connect(ui->labelRouteInfo, &QLabel::linkActivated, this, &RouteLabel::flightplanLabelLinkActivated);

  // Show error messages in tooltip on click ========================================
  ui->labelRouteError->installEventFilter(new atools::gui::ClickToolTipHandler(ui->labelRouteError));
  ui->labelRouteError->setVisible(false);

  ui->labelRouteSelection->setVisible(false);
  ui->labelRouteInfo->setVisible(false); // Will be shown if route is created
}

RouteLabel::~RouteLabel()
{

}

void RouteLabel::saveState()
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  settings.setValue(lnm::ROUTE_HEADER_AIRPORTS, headerAirports);
  settings.setValue(lnm::ROUTE_HEADER_DEPARTURE, headerDeparture);
  settings.setValue(lnm::ROUTE_HEADER_ARRIVAL, headerArrival);
  settings.setValue(lnm::ROUTE_HEADER_RUNWAY_TAKEOFF, headerRunwayTakeoff);
  settings.setValue(lnm::ROUTE_HEADER_RUNWAY_LAND, headerRunwayLand);
  settings.setValue(lnm::ROUTE_HEADER_DISTTIME, headerDistTime);
  settings.setValue(lnm::ROUTE_FOOTER_SELECTION, footerSelection);
  settings.setValue(lnm::ROUTE_FOOTER_ERROR, footerError);
}

void RouteLabel::restoreState()
{
  atools::settings::Settings& settings = atools::settings::Settings::instance();
  headerAirports = settings.valueBool(lnm::ROUTE_HEADER_AIRPORTS, true);
  headerDeparture = settings.valueBool(lnm::ROUTE_HEADER_DEPARTURE, true);
  headerArrival = settings.valueBool(lnm::ROUTE_HEADER_ARRIVAL, true);
  headerRunwayTakeoff = settings.valueBool(lnm::ROUTE_HEADER_RUNWAY_TAKEOFF, true);
  headerRunwayLand = settings.valueBool(lnm::ROUTE_HEADER_RUNWAY_LAND, true);
  headerDistTime = settings.valueBool(lnm::ROUTE_HEADER_DISTTIME, true);
  footerSelection = settings.valueBool(lnm::ROUTE_FOOTER_SELECTION, true);
  footerError = settings.valueBool(lnm::ROUTE_FOOTER_ERROR, true);
}

void RouteLabel::styleChanged()
{
  // Need to clear the labels to force style update - otherwise link colors remain the same
  NavApp::getMainUi()->labelRouteInfo->clear();

  // Update later in event queue to avoid obscure problem of disappearing labels
  QTimer::singleShot(0, this, &RouteLabel::updateAll);
}

void RouteLabel::updateAll()
{
  updateHeaderLabel();
  updateFooterSelectionLabel();
  updateFooterErrorLabel();
}

/* Update the dock window top level label */
void RouteLabel::updateHeaderLabel()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  bool visible = !route.isEmpty() &&
                 (headerAirports || headerDistTime || headerRunwayTakeoff || headerDeparture || headerArrival || headerRunwayLand);

  // Hide label if no plan or nothing selected
  ui->labelRouteInfo->setVisible(visible);

  if(visible)
  {
    autil::HtmlBuilder htmlAirports, htmlDistTime, htmlRunwayTakeoffDepart, htmlArrival, htmlRunwayLand;
    if(headerAirports)
      buildHeaderAirports(htmlAirports, true /* widget */);

    if(headerDistTime)
      buildHeaderDistTime(htmlDistTime, true /* widget */);

    if(headerRunwayTakeoff)
      buildHeaderRunwayTakeoff(htmlRunwayTakeoffDepart);

    if(headerDeparture)
      buildHeaderDepart(htmlRunwayTakeoffDepart, true /* widget */); // On the same line as buildHeaderRunwayTakeoff()

    if(headerArrival)
      buildHeaderArrival(htmlArrival, true /* widget */);

    if(headerRunwayLand)
      buildHeaderRunwayLand(htmlRunwayLand);

    // Join all texts with <br>
    ui->labelRouteInfo->setTextFormat(Qt::RichText);
    ui->labelRouteInfo->setText(HtmlBuilder::joinBr({htmlAirports, htmlDistTime, htmlRunwayTakeoffDepart, htmlArrival, htmlRunwayLand}));
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
  autil::HtmlBuilder htmlAirports, htmlTodTod, htmlDistTime, htmlRunwayTakeoff, htmlDepart, htmlArrival, htmlRunwayLand;

  // Header h1
  buildHeaderAirports(htmlAirports, false /* widget */);

  if(!titleOnly)
  {
    buildHeaderTocTod(htmlTodTod);
    buildHeaderDistTime(htmlDistTime, false /* widget */);
    buildHeaderRunwayTakeoff(htmlRunwayTakeoff);
    buildHeaderDepart(htmlDepart, false /* widget */);
    buildHeaderArrival(htmlArrival, false /* widget */);
    buildHeaderRunwayLand(htmlRunwayLand);
  }

  html.append(htmlAirports);

  if(!titleOnly)
    // Tow paragraphs with break separated lines
    html.append(HtmlBuilder::joinP({HtmlBuilder::joinBr({htmlTodTod, htmlDistTime}),
                                    HtmlBuilder::joinBr({htmlRunwayTakeoff, htmlDepart, htmlArrival, htmlRunwayLand})}));
}

void RouteLabel::buildHeaderAirports(atools::util::HtmlBuilder& html, bool widget)
{
  const Flightplan& flightplan = route.getFlightplanConst();
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
                       arg(flightplan.constFirst().getIdent()).
                       arg(flightplan.constFirst().getWaypointTypeAsDisplayString());
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
      ahtml::Flags flags = ahtml::BOLD;

      if(headerDistTime)
        flags |= ahtml::LINK_NO_UL;
      else
        // No distance - add separator underline
        html.u();

      html.a(departureAirport, "lnm://showdeparture", flags);
      if(!departureParking.isEmpty())
        html.text(tr(" / ")).a(departureParking, "lnm://showdepartureparking", flags);

      if(!destinationAirport.isEmpty() && route.getSizeWithoutAlternates() > 1)
        html.text(tr(" to ")).a(destinationAirport, "lnm://showdestination", flags);

      if(!headerDistTime)
        html.uEnd();
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

void RouteLabel::buildHeaderDepart(atools::util::HtmlBuilder& html, bool widget)
{
  // Add procedures to text ==============================================================

  HtmlBuilder departHtml = html.cleared();

  if(route.hasAnyProcedure())
  {
    const proc::MapProcedureLegs& sidLegs = route.getSidLegs();

    if(!sidLegs.isEmpty() && !sidLegs.isCustomDeparture())
    {
      // Add departure procedure to text ===========================================
      // Custom is only shown in runways section
      departHtml.b(tr("Depart"));
      if(!sidLegs.runwayEnd.isValid())
        departHtml.text(tr(" using SID "));
      else
      {
        if(!headerRunwayTakeoff || !widget)
        {
          departHtml.text(tr(" runway "));
          departHtml.b(sidLegs.runwayEnd.name);
        }
        departHtml.text(tr(" using SID "));
      }

      QString sid(sidLegs.procedureFixIdent);
      if(!sidLegs.transitionFixIdent.isEmpty())
        sid += "." % sidLegs.transitionFixIdent;

      departHtml.b(sid);
      departHtml.text(tr(". "));
    }

    html.append(departHtml);
  }
}

void RouteLabel::buildHeaderArrival(atools::util::HtmlBuilder& html, bool widget)
{
  QString approachRunway, starRunway;

  // Add procedures to text ==============================================================

  HtmlBuilder arrHtml = html.cleared();

  if(route.hasAnyProcedure())
  {
    const proc::MapProcedureLegs& starLegs = route.getStarLegs();
    const proc::MapProcedureLegs& arrivalLegs = route.getApproachLegs();

    // Add arrival procedures to text ==============================================================
    if(!starLegs.isEmpty())
    {
      // STAR ==============================================================
      arrHtml.b(tr("Arrive "));
      arrHtml.text(tr(" using STAR "));

      QString star(starLegs.procedureFixIdent);
      if(!starLegs.transitionFixIdent.isEmpty())
        star += "." % starLegs.transitionFixIdent;
      arrHtml.b(star);

      starRunway = starLegs.runway;

      if(!headerRunwayLand || !widget)
      {
        if(!(arrivalLegs.mapType & proc::PROCEDURE_APPROACH))
        {
          // No approach. Direct from STAR to runway.
          arrHtml.text(tr(" at runway "));
          arrHtml.b(starLegs.runway);
        }
        else if(!starLegs.runway.isEmpty())
          arrHtml.text(tr(" (")).b(starLegs.runway).text(tr(") "));
      }

      if(!(arrivalLegs.mapType & proc::PROCEDURE_APPROACH) || arrivalLegs.isCustomApproach())
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
        arrHtml.b(arrivalLegs.displayTypeAndSuffix()).nbsp();
        arrHtml.b(arrivalLegs.procedureFixIdent);

        if(!arrivalLegs.arincName.isEmpty())
          arrHtml.b(tr(" (%1)").arg(arrivalLegs.arincName));

        if(!headerRunwayLand || !widget)
        {
          // Runway =======================
          if(arrivalLegs.runwayEnd.isFullyValid())
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
        else
          arrHtml.text(tr("."));
      }

      approachRunway = arrivalLegs.runwayEnd.name;
    }

    if(widget)
    {
      // Header label widget
      if(!arrHtml.isEmpty())
        html.append(arrHtml);

      // Check STAR and approach runways - these have to match
      if(!approachRunway.isEmpty() && !starRunway.isEmpty() && !atools::fs::util::runwayEqual(approachRunway, starRunway))
        html.br().error(tr("STAR runway \"%1\" not equal to approach runway \"%2\".").arg(starRunway).arg(approachRunway));
    }
    else
      // HTML export or printing
      html.append(arrHtml);
  }
}

void RouteLabel::buildHeaderRunwayTakeoff(atools::util::HtmlBuilder& html)
{
  if(route.hasAnyProcedure())
  {
    AirportQuery *airportQuerySim = NavApp::getAirportQuerySim();
    MapQuery *mapQuery = NavApp::getMapQueryGui();

    if(route.hasValidDeparture())
    {
      // Departure runway information =======================================
      const proc::MapProcedureLegs& departureLegs = route.getSidLegs();
      map::MapRunwayEnd end = departureLegs.runwayEnd; // Navdata
      if(!departureLegs.isEmpty() && end.isFullyValid())
      {
        const RouteLeg& departLeg = route.getDepartureAirportLeg();
        if(departLeg.isValid())
        {
          // Get runway from simulator data by name if possible
          QList<map::MapRunwayEnd> runwayEnds;
          mapQuery->getRunwayEndByNameFuzzy(runwayEnds, end.name, departLeg.getAirport(), false /* navdata */);
          if(!runwayEnds.isEmpty())
            end = runwayEnds.first();

          html.b(tr("Takeoff")).text(tr(" from ")).b(end.name).text(tr(", "));
          html.text(formatter::courseTextFromTrue(end.heading, departLeg.getMagvarStart()), ahtml::NO_ENTITIES);

          map::MapRunway runway = airportQuerySim->getRunwayByEndId(departLeg.getId(), end.id);
          if(runway.isValid())
            html.text(tr(", ")).text(Unit::distShortFeet(runway.length));
          html.text(tr(". "));
        }
      }
    } // if(route.hasValidDeparture())
  } // if(route.hasAnyProcedure())
}

void RouteLabel::buildHeaderRunwayLand(atools::util::HtmlBuilder& html)
{
  if(route.hasAnyProcedure())
  {
    AirportQuery *airportQuerySim = NavApp::getAirportQuerySim();
    MapQuery *mapQuery = NavApp::getMapQueryGui();

    if(route.hasValidDestination())
    {
      // Destination runway information =======================================
      const proc::MapProcedureLegs *apprLegs = nullptr;
      if(route.hasAnyApproachProcedure())
        // Use approach runway information
        apprLegs = &route.getApproachLegs();
      else if(route.hasAnyStarProcedure())
        // Use STAR runway information if available
        apprLegs = &route.getStarLegs();

      if(apprLegs != nullptr)
      {
        map::MapRunwayEnd end = apprLegs->runwayEnd;
        if(!apprLegs->isEmpty() && end.isFullyValid())
        {
          const RouteLeg& destLeg = route.getDestinationAirportLeg();

          if(destLeg.isValid())
          {
            // Get runway from simulator data by name if possible
            QList<map::MapRunwayEnd> runwayEnds;
            mapQuery->getRunwayEndByNameFuzzy(runwayEnds, end.name, destLeg.getAirport(), false /* navdata */);
            if(!runwayEnds.isEmpty())
              end = runwayEnds.first();

            if(end.isFullyValid())
            {
              html.b(tr("Land")).text(tr(" at ")).b(end.name).text(tr(", "));

              QStringList rwAtts;
              rwAtts.append(formatter::courseTextFromTrue(end.heading, destLeg.getMagvarEnd()));

              map::MapRunway runway = airportQuerySim->getRunwayByEndId(destLeg.getId(), end.id);
              if(runway.isValid())
                rwAtts.append(Unit::distShortFeet(runway.length - (end.secondary ? runway.secondaryOffset : runway.primaryOffset)));

              rwAtts.append(Unit::altFeet(destLeg.getAltitude()) % tr(" elevation"));
              if(end.hasAnyVasi())
                rwAtts.append(end.uniqueVasiTypeStr().join(QObject::tr("/")));

              html.text(rwAtts.join(tr(", ")), ahtml::NO_ENTITIES);
            }
            else
              html.b(tr("Land")).text(tr(" at any runway, ")).text(Unit::altFeet(destLeg.getAltitude()) % tr(" elevation"));
            html.text(tr("."));
          }
        }
      }
    } // if(route.hasValidDestination())
  } // if(route.hasAnyProcedure())
}

void RouteLabel::buildHeaderTocTod(atools::util::HtmlBuilder& html)
{
  html.b(tr("Cruising altitude ")).text(Unit::altFeet(route.getCruiseAltitudeFt()));

  if(route.getTopOfClimbDistance() < map::INVALID_DISTANCE_VALUE || route.getTopOfDescentFromDestination() < map::INVALID_DISTANCE_VALUE)
  {
    if(route.getTopOfClimbDistance() < map::INVALID_DISTANCE_VALUE)
      html.text(tr(", ")).b(tr(" departure to top of climb ")).text(Unit::distNm(route.getTopOfClimbDistance()));

    if(route.getTopOfDescentFromDestination() < map::INVALID_DISTANCE_VALUE)
      html.text(tr(", ")).b(tr(" start of descent to destination ")).text(Unit::distNm(route.getTopOfDescentFromDestination()));
  }
  html.text(tr("."));
}

void RouteLabel::buildHeaderDistTime(atools::util::HtmlBuilder& html, bool widget)
{
  if(!route.isEmpty())
  {
    if(route.getSizeWithoutAlternates() > 1)
    {
      if(widget)
        html.u();

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

      if(widget)
        html.uEnd();
    }
  }
}

void RouteLabel::updateFooterErrorLabel()
{
  QString toolTipText;

  if(footerError)
  {
    // Collect errors from all controllers =================================

    // Flight plan ============
    RouteController *routeController = NavApp::getRouteController();
    buildErrorLabel(toolTipText, routeController->getErrorStrings(),
                    tr("<nobr><b>Problems on tab \"Flight Plan\":</b></nobr>", "Synchronize name with tab name"));

    // Elevation profile ============
    buildErrorLabel(toolTipText, NavApp::getAltitudeLegs().getErrorStrings(),
                    tr("<nobr><b>Problems when calculating profile for window \"Flight Plan Elevation Profile\":</b></nobr>",
                       "Synchronize name with window name"));

    // Aircraft performance ============
    buildErrorLabel(toolTipText, NavApp::getAircraftPerfController()->getErrorStrings(),
                    tr("<nobr><b>Problems on tab \"Fuel Report\":</b></nobr>", "Synchronize name with tab name"));
  }

  // Build tooltip message ====================================
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(!toolTipText.isEmpty())
  {
    ui->labelRouteError->setVisible(true);
    ui->labelRouteError->setText(HtmlBuilder::errorMessage(tr("Found problems. Click here for details.")));

    // Disallow text wrapping
    ui->labelRouteError->setToolTip(toolTipText);
    ui->labelRouteError->setStatusTip(tr("Found problems."));
  }
  else
  {
    ui->labelRouteError->setVisible(false);
    ui->labelRouteError->setText(QString());
    ui->labelRouteError->setToolTip(QString());
    ui->labelRouteError->setStatusTip(QString());
  }
}

void RouteLabel::buildErrorLabel(QString& toolTipText, QStringList errors, const QString& header)
{
  if(!errors.isEmpty())
  {
    if(errors.size() > 5)
    {
      errors = errors.mid(0, 5);
      errors.append(tr("More ..."));
    }

    toolTipText.append(header);
    toolTipText.append("<ul>");
    for(const QString& str : qAsConst(errors))
      toolTipText.append("<li>" % str % "</li>");
    toolTipText.append("</ul>");
  }
}

void RouteLabel::updateFooterSelectionLabel()
{
  QList<int> selectLegIndexes;
  float distanceNm = 0.f, fuelGalLbs = 0.f, timeHours = 0.f;
  bool missed = false;

  // Check if enabled in settings
  if(footerSelection)
  {
    NavApp::getRouteController()->getSelectedRouteLegs(selectLegIndexes);

    // Show only for more than two legs
    if(selectLegIndexes.size() > 1)
    {
      std::sort(selectLegIndexes.begin(), selectLegIndexes.end());

      // Distance to first waypoint in selection is ignored
      for(int index = selectLegIndexes.first() + 1; index <= selectLegIndexes.last(); index++)
      {
        const RouteLeg& leg = route.getLegAt(index);
        if(leg.isValid())
        {
          // Check if missed approach is covered
          if(leg.isAnyProcedure() && leg.getProcedureLeg().isMissed())
            missed = true;

          // Skip the airport after the missed approach since it would falsify the result
          if(index == route.getDestinationAirportLegIndex() && missed)
            continue;

          distanceNm += leg.getDistanceTo();

          if(route.isValidProfile())
          {
            const RouteAltitudeLeg& altLeg = route.getAltitudeLegAt(index);

            if(!altLeg.isEmpty())
            {
              fuelGalLbs += altLeg.getFuel(); // Fuel (volume/weight) depends on aircraft performance
              timeHours += altLeg.getTime();
            }
          }
        }
      }
    }
  }

  Ui::MainWindow *ui = NavApp::getMainUi();
  if(selectLegIndexes.size() > 1)
  {
    QStringList texts, tooltip;
    const AircraftPerf& aircraftPerformance = NavApp::getAircraftPerformance();

    // Add texts for label
    texts.append(Unit::distNm(distanceNm));
    texts.append(formatter::formatMinutesHoursLong(timeHours));
    texts.append(Unit::fuelLbsAndGal(aircraftPerformance.toFuelLbs(fuelGalLbs), aircraftPerformance.toFuelGal(fuelGalLbs)));

    // Add texts for tooltip
    tooltip.append(tr("<b>Distance:</b> %1").arg(Unit::distNm(distanceNm)));
    tooltip.append(tr("<b>Time:</b> %1").arg(formatter::formatMinutesHoursLong(timeHours)));
    tooltip.append(tr("<b>Fuel consumption:</b> %1").arg(Unit::fuelLbsAndGalLocalOther(aircraftPerformance.toFuelLbs(fuelGalLbs),
                                                                                       aircraftPerformance.toFuelGal(fuelGalLbs))));

    int first = selectLegIndexes.first();
    int last = selectLegIndexes.last();

    // Ignore airport after missed approach
    if(last == route.getDestinationAirportLegIndex() && missed)
      last--;

    // Two entries selected equals to one flight plan leg
    int numLegs = last - first;

    QString from = route.getLegAt(first).getIdent(), to = route.getLegAt(last).getIdent();
    QString legText = numLegs == 1 ? tr("leg") : tr("legs");

    ui->labelRouteSelection->setVisible(true);
    ui->labelRouteSelection->setText(tr("%L1 %2 from <b>%3</b> to <b>%4</b>: %5").arg(numLegs).arg(legText).
                                     arg(atools::elideTextShort(from, 6)).arg(atools::elideTextShort(to, 6)).arg(texts.join(tr(", "))));

    ui->labelRouteSelection->setToolTip(tr("<p style='white-space:pre'>"
                                             "%L1 flight plan %2 from <b>%3</b> to <b>%4</b> in selection:<br/>"
                                             "%5</p>").
                                        arg(numLegs).arg(legText).
                                        arg(atools::elideTextShort(from, 20)).arg(atools::elideTextShort(to, 20)).
                                        arg(tooltip.join(tr("<br/>"))));
  }
  else
  {
    ui->labelRouteSelection->setVisible(false);
    ui->labelRouteSelection->setText(QString());
    ui->labelRouteSelection->setToolTip(QString());
  }
}
