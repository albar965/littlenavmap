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

#include "route/routecalcwindow.h"
#include "common/unitstringtool.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"
#include "common/constants.h"
#include "common/unit.h"
#include "navapp.h"
#include "atools.h"
#include "route/route.h"
#include "util/htmlbuilder.h"
#include "ui_mainwindow.h"

using atools::util::HtmlBuilder;

RouteCalcWindow::RouteCalcWindow(QWidget *parent) :
  QObject(parent)
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  connect(ui->pushButtonRouteCalc, &QPushButton::clicked, this, &RouteCalcWindow::calculateClicked);
  connect(ui->pushButtonRouteCalcDirect, &QPushButton::clicked, this, &RouteCalcWindow::calculateDirectClicked);
  connect(ui->pushButtonRouteCalcReverse, &QPushButton::clicked, this, &RouteCalcWindow::calculateReverseClicked);
  connect(ui->pushButtonRouteCalcHelp, &QPushButton::clicked, this, &RouteCalcWindow::helpClicked);
  connect(ui->pushButtonRouteCalcAdjustAltitude, &QPushButton::clicked, this, &RouteCalcWindow::adjustAltitudePressed);
  connect(ui->radioButtonRouteCalcAirway, &QRadioButton::clicked, this, &RouteCalcWindow::updateWidgets);
  connect(ui->radioButtonRouteCalcRadio, &QRadioButton::clicked, this, &RouteCalcWindow::updateWidgets);
  connect(ui->comboBoxRouteCalcMode, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &RouteCalcWindow::updateWidgets);

  units = new UnitStringTool();
  units->init({ui->spinBoxRouteCalcCruiseAltitude});
}

RouteCalcWindow::~RouteCalcWindow()
{
  delete units;
}

void RouteCalcWindow::showForFullCalculation()
{
  NavApp::getMainUi()->comboBoxRouteCalcMode->setCurrentIndex(0);
  updateWidgets();
}

void RouteCalcWindow::showForSelectionCalculation(const QList<int>& selectedRows, bool canCalc)
{
  canCalculateSelection = canCalc;
  fromIndex = selectedRows.isEmpty() ? -1 : selectedRows.first();
  toIndex = selectedRows.isEmpty() ? -1 : selectedRows.last();
  NavApp::getMainUi()->comboBoxRouteCalcMode->setCurrentIndex(canCalculateSelection);
  updateWidgets();
}

void RouteCalcWindow::selectionChanged(const QList<int>& selectedRows, bool canCalc)
{
  canCalculateSelection = canCalc;
  fromIndex = selectedRows.isEmpty() ? -1 : selectedRows.first();
  toIndex = selectedRows.isEmpty() ? -1 : selectedRows.last();

  updateWidgets();
}

float RouteCalcWindow::getCruisingAltitudeFt() const
{
  // Convert UI to feet
  return Unit::rev(NavApp::getMainUi()->spinBoxRouteCalcCruiseAltitude->value(), Unit::altFeetF);
}

void RouteCalcWindow::setCruisingAltitudeFt(float altitude)
{
  // feet to UI
  NavApp::getMainUi()->spinBoxRouteCalcCruiseAltitude->setValue(atools::roundToInt(Unit::altFeetF(altitude)));
}

void RouteCalcWindow::helpClicked()
{
  atools::gui::HelpHandler::openHelpUrlWeb(NavApp::getQMainWindow(), lnm::helpOnlineUrl + "ROUTECALC.html",
                                           lnm::helpLanguageOnline());
}

void RouteCalcWindow::updateWidgets()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  bool airway = ui->radioButtonRouteCalcAirway->isChecked();

  ui->checkBoxRouteCalcRadioNdb->setEnabled(!airway);
  ui->radioButtonRouteCalcAirwayAll->setEnabled(airway);
  ui->radioButtonRouteCalcAirwayJet->setEnabled(airway);
  ui->radioButtonRouteCalcAirwayVictor->setEnabled(airway);
  ui->checkBoxRouteCalcAirwayNoRnav->setEnabled(airway && NavApp::hasRouteTypeInDatabase());
  ui->checkBoxRouteCalcAirwayTrack->setEnabled(airway && NavApp::hasTracks());
  ui->horizontalSliderRouteCalcAirwayPreference->setEnabled(airway);
  ui->groupBoxRouteCalcAirwayPrefer->setEnabled(airway);
  ui->labelRouteCalcAirwayPreferAirway->setEnabled(airway);
  ui->labelRouteCalcAirwayPreferWaypoint->setEnabled(airway);

  bool canCalcRoute = NavApp::getRouteConst().canCalcRoute();
  ui->pushButtonRouteCalcAdjustAltitude->setEnabled(canCalcRoute);
  ui->pushButtonRouteCalc->setEnabled(isCalculateSelection() ? canCalculateSelection : canCalcRoute);

  ui->pushButtonRouteCalcDirect->setEnabled(!isCalculateSelection() && canCalcRoute &&
                                            NavApp::getRouteConst().hasEntries());
  ui->pushButtonRouteCalcReverse->setEnabled(!isCalculateSelection() && canCalcRoute);

  updateHeader();
}

void RouteCalcWindow::updateHeader()
{
  const Route& route = NavApp::getRouteConst();
  const atools::fs::pln::Flightplan& flightplan = route.getFlightplan();
  QString departure, destination, title, tooltip;
  Ui::MainWindow *ui = NavApp::getMainUi();

  if(ui->comboBoxRouteCalcMode->currentIndex() == 1)
  {
    // Selection calculation ====================================================
    if(canCalculateSelection)
    {
      departure = tr("%1 (%2)").
                  arg(flightplan.getEntries().at(fromIndex).getIcaoIdent()).
                  arg(flightplan.getEntries().at(fromIndex).getWaypointTypeAsString());

      destination = tr("%1 (%2)").
                    arg(flightplan.getEntries().at(toIndex).getIcaoIdent()).
                    arg(flightplan.getEntries().at(toIndex).getWaypointTypeAsString());

      title = tr("<b>Calculate flight plan between legs<br/>%1 and %2.</b>").arg(departure).arg(destination);
    }
    else
    {
      title = HtmlBuilder::errorMessage({tr("Cannot calculate flight plan"),
                                         tr("between selected legs."),
                                         tr("See tooltip on this message for details.")});
      tooltip = tr("Select a range or two flight plan legs in the flight plan table.\n"
                   "These must be neither a part of a procedure nor a part of an alternate destination.");
    }
  }
  else
  {
    // Full flight plan calculation ====================================================
    if(route.canCalcRoute())
    {
      if(route.hasValidDeparture())
        departure = tr("%1 (%2)").arg(flightplan.getDepartureAiportName()).arg(flightplan.getDepartureIdent());
      else
        departure = tr("%1 (%2)").
                    arg(flightplan.getEntries().first().getIcaoIdent()).
                    arg(flightplan.getEntries().first().getWaypointTypeAsString());

      if(route.hasValidDestination())
        destination = tr("%1 (%2)").arg(flightplan.getDestinationAiportName()).arg(flightplan.getDestinationIdent());
      else
        destination = tr("%1 (%2)").
                      arg(flightplan.getEntries().at(route.getDestinationAirportLegIndex()).getIcaoIdent()).
                      arg(flightplan.getEntries().at(route.getDestinationAirportLegIndex()).getWaypointTypeAsString());

      title = tr("<b>From %1 to %2.</b>").arg(departure).arg(destination);

    }
    else
    {
      title = HtmlBuilder::errorMessage({tr("Invalid flight plan."),
                                         tr("Set departure and destination first."),
                                         tr("See tooltip on this message for details.")});
      tooltip = tr("Use the right-click context menu on the map or the airport search (F4)\n"
                   "to select departure and destination first.");
    }
  }
  ui->labelRouteCalcHeader->setText(title);
  ui->labelRouteCalcHeader->setToolTip(tooltip);
}

void RouteCalcWindow::restoreState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  atools::gui::WidgetState(lnm::ROUTE_CALC_DIALOG).restore(
  {
    ui->horizontalSliderRouteCalcAirwayPreference, ui->labelRouteCalcAirwayPreferWaypoint,
    ui->radioButtonRouteCalcAirwayJet, ui->spinBoxRouteCalcCruiseAltitude, ui->radioButtonRouteCalcAirwayAll,
    ui->checkBoxRouteCalcAirwayNoRnav, ui->radioButtonRouteCalcRadio, ui->radioButtonRouteCalcAirwayVictor,
    ui->radioButtonRouteCalcAirway, ui->checkBoxRouteCalcRadioNdb
  });
}

void RouteCalcWindow::saveState()
{
  Ui::MainWindow *ui = NavApp::getMainUi();

  atools::gui::WidgetState(lnm::ROUTE_CALC_DIALOG).save(
  {
    ui->horizontalSliderRouteCalcAirwayPreference, ui->labelRouteCalcAirwayPreferWaypoint,
    ui->radioButtonRouteCalcAirwayJet, ui->spinBoxRouteCalcCruiseAltitude, ui->radioButtonRouteCalcAirwayAll,
    ui->checkBoxRouteCalcAirwayNoRnav, ui->radioButtonRouteCalcRadio, ui->radioButtonRouteCalcAirwayVictor,
    ui->radioButtonRouteCalcAirway, ui->checkBoxRouteCalcRadioNdb
  });
}

void RouteCalcWindow::preDatabaseLoad()
{

}

void RouteCalcWindow::postDatabaseLoad()
{
  NavApp::getMainUi()->checkBoxRouteCalcAirwayNoRnav->setEnabled(NavApp::hasRouteTypeInDatabase());
}

rd::RoutingType RouteCalcWindow::getRoutingType() const
{
  return NavApp::getMainUi()->radioButtonRouteCalcAirway->isChecked() ? rd::AIRWAY : rd::RADIONNAV;
}

rd::AirwayRoutingType RouteCalcWindow::getAirwayRoutingType() const
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(ui->radioButtonRouteCalcAirwayJet->isChecked())
    return rd::JET;
  else if(ui->radioButtonRouteCalcAirwayVictor->isChecked())
    return rd::VICTOR;
  else
    return rd::BOTH;
}

bool RouteCalcWindow::isAirwayNoRnav() const
{
  return NavApp::getMainUi()->checkBoxRouteCalcAirwayNoRnav->isEnabled() &&
         NavApp::getMainUi()->checkBoxRouteCalcAirwayNoRnav->isChecked();
}

bool RouteCalcWindow::isUseTracks() const
{
  return NavApp::getMainUi()->checkBoxRouteCalcAirwayTrack->isEnabled() &&
         NavApp::getMainUi()->checkBoxRouteCalcAirwayTrack->isChecked();
}

int RouteCalcWindow::getAirwayWaypointPreference() const
{
  return NavApp::getMainUi()->horizontalSliderRouteCalcAirwayPreference->value();
}

int RouteCalcWindow::getAirwayWaypointPreferenceMin() const
{
  return NavApp::getMainUi()->horizontalSliderRouteCalcAirwayPreference->minimum();
}

int RouteCalcWindow::getAirwayWaypointPreferenceMax() const
{
  return NavApp::getMainUi()->horizontalSliderRouteCalcAirwayPreference->maximum();
}

bool RouteCalcWindow::isRadionavNdb() const
{
  return NavApp::getMainUi()->checkBoxRouteCalcRadioNdb->isChecked();
}

bool RouteCalcWindow::isCalculateSelection() const
{
  return NavApp::getMainUi()->comboBoxRouteCalcMode->currentIndex() == 1 && fromIndex != -1 && toIndex != -1;
}

void RouteCalcWindow::adjustAltitudePressed()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->spinBoxRouteCalcCruiseAltitude->setValue(NavApp::getRouteConst().
                                               getAdjustedAltitude(ui->spinBoxRouteCalcCruiseAltitude->value()));
}
