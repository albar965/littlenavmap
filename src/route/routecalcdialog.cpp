/*****************************************************************************
* Copyright 2015-2022 Alexander Barthel alex@littlenavmap.org
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

#include "route/routecalcdialog.h"
#include "common/unitstringtool.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"
#include "common/constants.h"
#include "route/routecontroller.h"
#include "common/unit.h"
#include "navapp.h"
#include "atools.h"
#include "route/route.h"
#include "util/htmlbuilder.h"
#include "gui/clicktooltiphandler.h"
#include "ui_mainwindow.h"
#include "ui_routecalcdialog.h"

// Factor to put on costs for direct connections. Airways <-> Waypoints
static const float DIRECT_COST_FACTORS[11] = {10.f, 8.f, 6.f, 4.f, 3.f, 2.f, 1.6f, 1.4f, 1.3f, 1.2f, 1.f};

using atools::util::HtmlBuilder;

RouteCalcDialog::RouteCalcDialog(QWidget *parent)
  : QDialog(parent), ui(new Ui::RouteCalcDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::NonModal);

  ui->setupUi(this);

  // Copy main menu actions to allow using shortcuts in the non-modal dialog too
  addActions(NavApp::getMainWindowActions());

  preferenceTexts = QStringList({
    tr("Airways and tracks only.\nFastest calculation."),
    tr("More airways and few direct."),
    tr("More airways and less direct."),
    tr("Airways and less direct."),
    tr("Airways and direct."),
    tr("Airways and direct."),
    tr("Airways and direct."),
    tr("Airways and more direct."),
    tr("Less airways and more direct."),
    tr("Few airways and more direct."),
    tr("Direct using waypoints only.")
  });

  widgets = {ui->horizontalSliderRouteCalcAirwayPreference, ui->labelRouteCalcAirwayPreferWaypoint,
             ui->radioButtonRouteCalcAirwayJet, ui->spinBoxRouteCalcCruiseAltitude, ui->radioButtonRouteCalcAirwayAll,
             ui->checkBoxRouteCalcAirwayNoRnav, ui->checkBoxRouteCalcAirwayTrack, ui->radioButtonRouteCalcRadio,
             ui->radioButtonRouteCalcAirwayVictor, ui->radioButtonRouteCalcAirway, ui->checkBoxRouteCalcRadioNdb};

  ui->buttonBox->button(QDialogButtonBox::Apply)->setText(tr("&Calculate"));

  connect(ui->pushButtonRouteCalcDirect, &QPushButton::clicked, this, &RouteCalcDialog::calculateDirectClicked);
  connect(ui->pushButtonRouteCalcReverse, &QPushButton::clicked, this, &RouteCalcDialog::calculateReverseClicked);
  connect(ui->pushButtonRouteCalcTrackDownload, &QPushButton::clicked, this, &RouteCalcDialog::downloadTrackClicked);
  connect(ui->pushButtonRouteCalcAdjustAltitude, &QPushButton::clicked, this, &RouteCalcDialog::adjustAltitudePressed);
  connect(ui->radioButtonRouteCalcAirway, &QRadioButton::clicked, this, &RouteCalcDialog::updateWidgets);
  connect(ui->radioButtonRouteCalcRadio, &QRadioButton::clicked, this, &RouteCalcDialog::updateWidgets);
  connect(ui->radioButtonRouteCalcFull, &QRadioButton::toggled, this, &RouteCalcDialog::updateWidgets);
  connect(ui->radioButtonRouteCalcSelection, &QRadioButton::toggled, this, &RouteCalcDialog::updateWidgets);
  connect(ui->horizontalSliderRouteCalcAirwayPreference, &QSlider::valueChanged, this, &RouteCalcDialog::updatePreferenceLabel);

  units = new UnitStringTool();
  units->init({ui->spinBoxRouteCalcCruiseAltitude});

  // Show error messages in tooltip on click ========================================
  ui->labelRouteCalcHeader->installEventFilter(new atools::gui::ClickToolTipHandler(ui->labelRouteCalcHeader));

  Q_ASSERT(ui->horizontalSliderRouteCalcAirwayPreference->minimum() == AIRWAY_WAYPOINT_PREF_MIN);
  Q_ASSERT(ui->horizontalSliderRouteCalcAirwayPreference->maximum() == AIRWAY_WAYPOINT_PREF_MAX);

  connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &RouteCalcDialog::buttonBoxClicked);
}

RouteCalcDialog::~RouteCalcDialog()
{
  delete units;
}

void RouteCalcDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBox->button(QDialogButtonBox::Apply))
  {
    calculating = true;
    updateWidgets();
    emit calculateClicked();
    calculating = false;
    updateWidgets();
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(NavApp::getQMainWindow(), lnm::helpOnlineUrl + "ROUTECALC.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBox->button(QDialogButtonBox::Close))
    QDialog::hide();
}

void RouteCalcDialog::showForFullCalculation()
{
  ui->radioButtonRouteCalcFull->setChecked(true);
  updateWidgets();
}

void RouteCalcDialog::showForSelectionCalculation()
{
  selectionChanged();
  ui->radioButtonRouteCalcSelection->setChecked(true);
}

void RouteCalcDialog::selectionChanged()
{
  routeChanged();
}

void RouteCalcDialog::routeChanged()
{
  QList<int> selLegIndexes;
  NavApp::getRouteController()->getSelectedRouteLegs(selLegIndexes);
  std::sort(selLegIndexes.begin(), selLegIndexes.end());

  fromIndex = selLegIndexes.isEmpty() ? -1 : selLegIndexes.constFirst();
  toIndex = selLegIndexes.isEmpty() ? -1 : selLegIndexes.constLast();

  canCalculateSelection = NavApp::getRouteConst().canCalcSelection(fromIndex, toIndex);

  updateWidgets();
}

float RouteCalcDialog::getCruisingAltitudeFt() const
{
  // Convert UI to feet
  return Unit::rev(ui->spinBoxRouteCalcCruiseAltitude->value(), Unit::altFeetF);
}

void RouteCalcDialog::setCruisingAltitudeFt(float altitude)
{
  // feet to UI
  ui->spinBoxRouteCalcCruiseAltitude->setValue(atools::roundToInt(Unit::altFeetF(altitude)));
}

void RouteCalcDialog::updateWidgets()
{
  bool airway = ui->radioButtonRouteCalcAirway->isChecked();
  if(calculating)
    setDisabled(true);
  else
  {
    setDisabled(false);

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
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(isCalculateSelection() ? canCalculateSelection : canCalcRoute);

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
  }

  updateHeader();
  updatePreferenceLabel();
}

void RouteCalcDialog::updatePreferenceLabel()
{
  ui->labelRouteCalcPreference->setText(preferenceTexts.at(ui->horizontalSliderRouteCalcAirwayPreference->value()));
}

void RouteCalcDialog::updateHeader()
{
  const Route& route = NavApp::getRouteConst();
  QString departure, destination, title, tooltip, statustip;

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

      title = tr("<b>Calculate flight plan from<br/>%1 to %2</b><hr/>").arg(departure).arg(destination);

      title.append(tr("Direct distance is %1. Flight plan distance is %2.").
                   arg(Unit::distMeter(departLeg.getPosition().distanceMeterTo(destLeg.getPosition()))).
                   arg(Unit::distNm(route.getTotalDistance())));
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

void RouteCalcDialog::restoreState()
{
  atools::gui::WidgetState state(lnm::ROUTE_CALC_DIALOG);
  state.restore(this);
  state.restore(widgets);
}

void RouteCalcDialog::saveState()
{
  atools::gui::WidgetState state(lnm::ROUTE_CALC_DIALOG);
  state.save(widgets);
  state.save(this);
}

void RouteCalcDialog::preDatabaseLoad()
{

}

void RouteCalcDialog::postDatabaseLoad()
{
  updateWidgets();
}

void RouteCalcDialog::optionsChanged()
{
  units->init({ui->spinBoxRouteCalcCruiseAltitude});
}

rd::RoutingType RouteCalcDialog::getRoutingType() const
{
  return ui->radioButtonRouteCalcAirway->isChecked() ? rd::AIRWAY : rd::RADIONNAV;
}

rd::AirwayRoutingType RouteCalcDialog::getAirwayRoutingType() const
{
  if(ui->radioButtonRouteCalcAirwayJet->isChecked())
    return rd::JET;
  else if(ui->radioButtonRouteCalcAirwayVictor->isChecked())
    return rd::VICTOR;
  else
    return rd::BOTH;
}

bool RouteCalcDialog::isAirwayNoRnav() const
{
  if(ui->checkBoxRouteCalcAirwayNoRnav->isEnabled())
    return ui->checkBoxRouteCalcAirwayNoRnav->isChecked();
  else
    return false;
}

bool RouteCalcDialog::isUseTracks() const
{
  if(ui->checkBoxRouteCalcAirwayTrack->isEnabled())
    return ui->checkBoxRouteCalcAirwayTrack->isChecked();
  else
    return false;
}

int RouteCalcDialog::getAirwayWaypointPreference() const
{
  return ui->horizontalSliderRouteCalcAirwayPreference->value();
}

bool RouteCalcDialog::isRadionavNdb() const
{
  return ui->checkBoxRouteCalcRadioNdb->isChecked();
}

bool RouteCalcDialog::isCalculateSelection() const
{
  return ui->radioButtonRouteCalcSelection->isChecked();
}

float RouteCalcDialog::getAirwayPreferenceCostFactor() const
{
  return DIRECT_COST_FACTORS[ui->horizontalSliderRouteCalcAirwayPreference->value()];
}

void RouteCalcDialog::adjustAltitudePressed()
{
  ui->spinBoxRouteCalcCruiseAltitude->setValue(NavApp::getRouteConst().getAdjustedAltitude(ui->spinBoxRouteCalcCruiseAltitude->value()));
}

void RouteCalcDialog::showEvent(QShowEvent *)
{
  if(!position.isNull())
    move(position);
}

void RouteCalcDialog::hideEvent(QHideEvent *)
{
  position = geometry().topLeft();
}
