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

// Factor to put on costs for direct connections. Airways <-> Waypoints
static const float DIRECT_COST_FACTORS[11] = {10.f, 8.f, 6.f, 4.f, 3.f, 2.f, 1.5f, 1.25f, 1.2f, 1.1f, 1.f};

using atools::util::HtmlBuilder;

RouteCalcWindow::RouteCalcWindow(QWidget *parent) :
  QObject(parent)
{
  preferenceTexts = QStringList({
    tr("Airways and tracks only.\nFastest calculation."),
    tr("More airways and few direct.\nSlower calculation."),
    tr("More airways and less direct.\nSlower calculation."),
    tr("Airways and less direct.\nSlower calculation."),
    tr("Airways and direct.\nSlower calculation."),
    tr("Airways and direct.\nSlower calculation."),
    tr("Airways and direct.\nSlower calculation."),
    tr("Airways and more direct.\nSlower calculation."),
    tr("Less airways and more direct.\nSlower calculation."),
    tr("Few airways and more direct.\nSlower calculation."),
    tr("Direct using waypoints only.\nSlower calculation.")
  });

  Ui::MainWindow *ui = NavApp::getMainUi();
  widgets = {ui->horizontalSliderRouteCalcAirwayPreference, ui->labelRouteCalcAirwayPreferWaypoint,
             ui->radioButtonRouteCalcAirwayJet, ui->spinBoxRouteCalcCruiseAltitude, ui->radioButtonRouteCalcAirwayAll,
             ui->checkBoxRouteCalcAirwayNoRnav, ui->checkBoxRouteCalcAirwayTrack, ui->radioButtonRouteCalcRadio,
             ui->radioButtonRouteCalcAirwayVictor, ui->radioButtonRouteCalcAirway, ui->checkBoxRouteCalcRadioNdb};

  connect(ui->pushButtonRouteCalc, &QPushButton::clicked, this, &RouteCalcWindow::calculateClicked);
  connect(ui->pushButtonRouteCalcDirect, &QPushButton::clicked, this, &RouteCalcWindow::calculateDirectClicked);
  connect(ui->pushButtonRouteCalcReverse, &QPushButton::clicked, this, &RouteCalcWindow::calculateReverseClicked);
  connect(ui->pushButtonRouteCalcHelp, &QPushButton::clicked, this, &RouteCalcWindow::helpClicked);
  connect(ui->pushButtonRouteCalcAdjustAltitude, &QPushButton::clicked, this, &RouteCalcWindow::adjustAltitudePressed);
  connect(ui->radioButtonRouteCalcAirway, &QRadioButton::clicked, this, &RouteCalcWindow::updateWidgets);
  connect(ui->radioButtonRouteCalcRadio, &QRadioButton::clicked, this, &RouteCalcWindow::updateWidgets);
  connect(ui->comboBoxRouteCalcMode, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &RouteCalcWindow::updateWidgets);
  connect(ui->horizontalSliderRouteCalcAirwayPreference, &QSlider::valueChanged,
          this, &RouteCalcWindow::updatePreferenceLabel);

  connect(ui->dockWidgetRouteCalc->toggleViewAction(), &QAction::toggled,
          this, &RouteCalcWindow::dockVisibilityChanged);

  units = new UnitStringTool();
  units->init({ui->spinBoxRouteCalcCruiseAltitude});

  Q_ASSERT(ui->horizontalSliderRouteCalcAirwayPreference->minimum() == AIRWAY_WAYPOINT_PREF_MIN);
  Q_ASSERT(ui->horizontalSliderRouteCalcAirwayPreference->maximum() == AIRWAY_WAYPOINT_PREF_MAX);
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

void RouteCalcWindow::dockVisibilityChanged(bool visible)
{
  if(visible)
    setCruisingAltitudeFt(NavApp::getRouteCruiseAltFt());
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
  updatePreferenceLabel();
}

void RouteCalcWindow::updatePreferenceLabel()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->labelRouteCalcPreference->setText(preferenceTexts.at(ui->horizontalSliderRouteCalcAirwayPreference->value()));
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
                  arg(flightplan.getEntries().at(fromIndex).getIdent()).
                  arg(flightplan.getEntries().at(fromIndex).getWaypointTypeAsFsxString());

      destination = tr("%1 (%2)").
                    arg(flightplan.getEntries().at(toIndex).getIdent()).
                    arg(flightplan.getEntries().at(toIndex).getWaypointTypeAsFsxString());

      title = tr("<b>Calculate flight plan between legs<br/>%1 and %2</b>").arg(departure).arg(destination);
    }
    else
    {
      title = HtmlBuilder::errorMessage({tr("Cannot calculate flight plan"),
                                         tr("between selected legs."),
                                         tr("Hover mouse over this message for details.")});
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
        departure = tr("%1 (%2)").arg(flightplan.getDepartureName()).arg(flightplan.getDepartureIdent());
      else
        departure = tr("%1 (%2)").
                    arg(flightplan.getEntries().first().getIdent()).
                    arg(flightplan.getEntries().first().getWaypointTypeAsFsxString());

      if(route.hasValidDestination())
        destination = tr("%1 (%2)").arg(flightplan.getDestinationName()).arg(flightplan.getDestinationIdent());
      else
        destination = tr("%1 (%2)").
                      arg(flightplan.getEntries().at(route.getDestinationAirportLegIndex()).getIdent()).
                      arg(flightplan.getEntries().at(route.getDestinationAirportLegIndex()).getWaypointTypeAsFsxString());

      title = tr("<b>From %1 to %2</b>").arg(departure).arg(destination);

    }
    else
    {
      title = HtmlBuilder::errorMessage({tr("Invalid flight plan."),
                                         tr("Set departure and destination first."),
                                         tr("Hover mouse over this message for details.")});
      tooltip = tr("Use the right-click context menu on the map or the airport search (F4)\n"
                   "to select departure and destination first.");
    }
  }
  ui->labelRouteCalcHeader->setText(title);
  ui->labelRouteCalcHeader->setToolTip(tooltip);
}

void RouteCalcWindow::restoreState()
{
  atools::gui::WidgetState(lnm::ROUTE_CALC_DIALOG).restore(widgets);
}

void RouteCalcWindow::saveState()
{
  atools::gui::WidgetState(lnm::ROUTE_CALC_DIALOG).save(widgets);
}

void RouteCalcWindow::preDatabaseLoad()
{

}

void RouteCalcWindow::postDatabaseLoad()
{
  updateWidgets();
}

void RouteCalcWindow::optionsChanged()
{
  units->init({NavApp::getMainUi()->spinBoxRouteCalcCruiseAltitude});
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
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(ui->checkBoxRouteCalcAirwayNoRnav->isEnabled())
    return ui->checkBoxRouteCalcAirwayNoRnav->isChecked();
  else
    return false;
}

bool RouteCalcWindow::isUseTracks() const
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  if(ui->checkBoxRouteCalcAirwayTrack->isEnabled())
    return ui->checkBoxRouteCalcAirwayTrack->isChecked();
  else
    return false;
}

int RouteCalcWindow::getAirwayWaypointPreference() const
{
  return NavApp::getMainUi()->horizontalSliderRouteCalcAirwayPreference->value();
}

bool RouteCalcWindow::isRadionavNdb() const
{
  return NavApp::getMainUi()->checkBoxRouteCalcRadioNdb->isChecked();
}

bool RouteCalcWindow::isCalculateSelection() const
{
  return NavApp::getMainUi()->comboBoxRouteCalcMode->currentIndex() == 1 && fromIndex != -1 && toIndex != -1;
}

float RouteCalcWindow::getAirwayPreferenceCostFactor() const
{
  return DIRECT_COST_FACTORS[NavApp::getMainUi()->horizontalSliderRouteCalcAirwayPreference->value()];
}

void RouteCalcWindow::adjustAltitudePressed()
{
  Ui::MainWindow *ui = NavApp::getMainUi();
  ui->spinBoxRouteCalcCruiseAltitude->setValue(NavApp::getRouteConst().
                                               getAdjustedAltitude(ui->spinBoxRouteCalcCruiseAltitude->value()));
}
