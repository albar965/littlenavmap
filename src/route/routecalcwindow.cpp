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
#include "gui/clicktooltiphandler.h"
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
  connect(ui->pushButtonRouteCalcTrackDownload, &QPushButton::clicked, this, &RouteCalcWindow::downloadTrackClicked);
  connect(ui->pushButtonRouteCalcHelp, &QPushButton::clicked, this, &RouteCalcWindow::helpClicked);
  connect(ui->pushButtonRouteCalcAdjustAltitude, &QPushButton::clicked, this, &RouteCalcWindow::adjustAltitudePressed);
  connect(ui->radioButtonRouteCalcAirway, &QRadioButton::clicked, this, &RouteCalcWindow::updateWidgets);
  connect(ui->radioButtonRouteCalcRadio, &QRadioButton::clicked, this, &RouteCalcWindow::updateWidgets);
  connect(ui->radioButtonRouteCalcFull, &QRadioButton::toggled, this, &RouteCalcWindow::updateWidgets);
  connect(ui->radioButtonRouteCalcSelection, &QRadioButton::toggled, this, &RouteCalcWindow::updateWidgets);
  connect(ui->horizontalSliderRouteCalcAirwayPreference, &QSlider::valueChanged,
          this, &RouteCalcWindow::updatePreferenceLabel);

  units = new UnitStringTool();
  units->init({ui->spinBoxRouteCalcCruiseAltitude});

  // Show error messages in tooltip on click ========================================
  ui->labelRouteCalcHeader->installEventFilter(new atools::gui::ClickToolTipHandler(ui->labelRouteCalcHeader));

  Q_ASSERT(ui->horizontalSliderRouteCalcAirwayPreference->minimum() == AIRWAY_WAYPOINT_PREF_MIN);
  Q_ASSERT(ui->horizontalSliderRouteCalcAirwayPreference->maximum() == AIRWAY_WAYPOINT_PREF_MAX);
}

RouteCalcWindow::~RouteCalcWindow()
{
  delete units;
}

void RouteCalcWindow::showForFullCalculation()
{
  NavApp::getMainUi()->radioButtonRouteCalcFull->setChecked(true);
  updateWidgets();
}

void RouteCalcWindow::showForSelectionCalculation(const QList<int>& selectedRows, bool canCalc)
{
  canCalculateSelection = canCalc;
  fromIndex = selectedRows.isEmpty() ? -1 : selectedRows.first();
  toIndex = selectedRows.isEmpty() ? -1 : selectedRows.last();
  NavApp::getMainUi()->radioButtonRouteCalcSelection->setChecked(true);
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

  ui->pushButtonRouteCalcDirect->setEnabled(canCalcRoute && NavApp::getRouteConst().hasEntries());
  ui->pushButtonRouteCalcReverse->setEnabled(canCalcRoute);

  QString msg = tr("Use downloaded NAT, PACOTS or AUSOTS tracks.\n"
                   "Best track will be selected automatically.\n"
                   "Ensure to use the correct flight level.\n"
                   "Otherwise, tracks will not be used.");
  QString err = tr("\n\nNo tracks available. Press the download button or\n"
                   "go to \"Flight Plan\" -> \"Download Tracks\" to fetch tracks.",
                   "Keep translation in sync with menu items");

  if(NavApp::hasTracks())
    ui->checkBoxRouteCalcAirwayTrack->setToolTip(msg);
  else
    ui->checkBoxRouteCalcAirwayTrack->setToolTip(msg + err);

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
  QString departure, destination, title, tooltip, statustip;
  Ui::MainWindow *ui = NavApp::getMainUi();

  if(ui->radioButtonRouteCalcSelection->isChecked())
  {
    // Selection calculation ====================================================
    if(canCalculateSelection)
    {
      const RouteLeg& fromLeg = route.getLegAt(fromIndex);
      const RouteLeg& toLeg = route.getLegAt(toIndex);
      departure = tr("%1 (%2)").arg(fromLeg.getDisplayIdent()).arg(fromLeg.getMapObjectTypeName());
      destination = tr("%1 (%2)").arg(toLeg.getDisplayIdent()).arg(toLeg.getMapObjectTypeName());
      title = tr("<b>Calculate flight plan between legs<br/>%1 and %2</b>").arg(departure).arg(destination);
    }
    else
    {
      title = HtmlBuilder::errorMessage({tr("Cannot calculate flight plan between selected legs."),
                                         tr("Click here for details.")});
      tooltip = tr("Select a range of legs or two flight plan legs in the flight plan table.\n"
                   "The selection may not be part of a procedure or an alternate.\n"
                   "It can include the end of a departure or the start of an arrival procedure.");
    }
  }
  else
  {
    // Full flight plan calculation ====================================================
    if(route.canCalcRoute())
    {
      const RouteLeg& departLeg = route.getDepartureAirportLeg();
      const RouteLeg& destLeg = route.getDestinationAirportLeg();
      if(route.hasValidDeparture())
        departure = tr("%1 (%2)").arg(departLeg.getName()).arg(departLeg.getDisplayIdent());
      else
        departure = tr("%1 (%2)").arg(departLeg.getDisplayIdent()).arg(departLeg.getMapObjectTypeName());

      if(route.hasValidDestination())
        destination = tr("%1 (%2)").arg(destLeg.getName()).arg(destLeg.getDisplayIdent());
      else
        destination = tr("%1 (%2)").arg(destLeg.getDisplayIdent()).arg(destLeg.getMapObjectTypeName());

      title = tr("<b>Calculate flight plan from<br/>%1 to %2</b>").arg(departure).arg(destination);
    }
    else
    {
      title = HtmlBuilder::errorMessage({tr("Set departure and destination first."), tr("Click here for details.")});
      tooltip = tr("<p style='white-space:pre'>Use the right-click context menu on the map or the airport search (<code>F4</code>)<br/>"
                   "to select departure and destination first.</p>");
      statustip = tr("Select departure and destination first");
    }
  }
  ui->labelRouteCalcHeader->setText(title);
  ui->labelRouteCalcHeader->setToolTip(tooltip);
  ui->labelRouteCalcHeader->setStatusTip(statustip);
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
  return NavApp::getMainUi()->radioButtonRouteCalcSelection->isChecked();
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
