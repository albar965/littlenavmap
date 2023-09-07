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

#include "route/routecalcdialog.h"

#include "atools.h"
#include "common/constants.h"
#include "common/unit.h"
#include "common/unitstringtool.h"
#include "gui/clicktooltiphandler.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"
#include "app/navapp.h"
#include "route/route.h"
#include "route/routecontroller.h"
#include "ui_routecalcdialog.h"
#include "util/htmlbuilder.h"

// Factor to put on costs for direct connections. Airways <-> Waypoints
// Sync withAIRWAY_WAYPOINT_PREF_MIN and AIRWAY_WAYPOINT_PREF_MAX
static const QVector<float> DIRECT_COST_FACTORS({10.00f, 09.00f, 08.00f, 07.00f, 06.00f, 04.00f, 03.00f,
                                                 02.00f, // Center/both
                                                 01.50f, 01.30f, 01.20f, 01.15f, 01.10f, 01.05f, 01.00f});

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
    tr("More airways and few direct."),
    tr("More airways and less direct."),
    tr("More airways and less direct."),
    tr("Airways and less direct."),
    tr("Airways and direct."),
    tr("Airways and direct."), // Center / both
    tr("Airways and direct."),
    tr("Airways and more direct."),
    tr("Less airways and more direct."),
    tr("Less airways and more direct."),
    tr("Few airways and more direct."),
    tr("Few airways and more direct."),
    tr("Direct using waypoints only.")
  });

  Q_ASSERT(preferenceTexts.size() == DIRECT_COST_FACTORS.size());
  Q_ASSERT(DIRECT_COST_FACTORS.size() == AIRWAY_WAYPOINT_PREF_MAX - AIRWAY_WAYPOINT_PREF_MIN + 1);

  widgets = {ui->horizontalSliderRouteCalcAirwayPref, ui->labelRouteCalcAirwayPreferWaypoint,
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
  connect(ui->horizontalSliderRouteCalcAirwayPref, &QSlider::valueChanged, this, &RouteCalcDialog::updatePreferenceLabel);

  units = new UnitStringTool();
  units->init({ui->spinBoxRouteCalcCruiseAltitude});

  // Show error messages in tooltip on click ========================================
  ui->labelRouteCalcHeader->installEventFilter(new atools::gui::ClickToolTipHandler(ui->labelRouteCalcHeader));

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
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    emit calculateClicked();
    calculating = false;
    updateWidgets();
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(NavApp::getQMainWidget(), lnm::helpOnlineUrl + "ROUTECALC.html", lnm::helpLanguageOnline());
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
  {
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    ui->buttonBox->button(QDialogButtonBox::Close)->setEnabled(false);
    ui->pushButtonRouteCalcDirect->setEnabled(false);
    ui->pushButtonRouteCalcReverse->setEnabled(false);
  }
  else
  {
    ui->checkBoxRouteCalcRadioNdb->setEnabled(!airway);
    ui->radioButtonRouteCalcAirwayAll->setEnabled(airway);
    ui->radioButtonRouteCalcAirwayJet->setEnabled(airway);
    ui->radioButtonRouteCalcAirwayVictor->setEnabled(airway);
    ui->checkBoxRouteCalcAirwayNoRnav->setEnabled(airway && NavApp::hasRouteTypeInDatabase());
    ui->checkBoxRouteCalcAirwayTrack->setEnabled(airway && NavApp::hasTracks());
    ui->horizontalSliderRouteCalcAirwayPref->setEnabled(airway);
    ui->groupBoxRouteCalcAirwayPrefer->setEnabled(airway);
    ui->labelRouteCalcAirwayPreferAirway->setEnabled(airway);
    ui->labelRouteCalcAirwayPreferWaypoint->setEnabled(airway);

    bool canCalcRoute = NavApp::getRouteConst().canCalcRoute();
    ui->pushButtonRouteCalcAdjustAltitude->setEnabled(canCalcRoute);
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(isCalculateSelection() ? canCalculateSelection : canCalcRoute);
    ui->buttonBox->button(QDialogButtonBox::Close)->setEnabled(true);

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
#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "horizontalSliderRouteCalcAirwayPreference->value()" << ui->horizontalSliderRouteCalcAirwayPref->value();
#endif

  ui->labelRouteCalcPreference->setText(preferenceTexts.at(ui->horizontalSliderRouteCalcAirwayPref->value()));
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
      departure = tr("%1 (%2)").arg(fromLeg.getDisplayIdent()).arg(fromLeg.getMapTypeName());
      destination = tr("%1 (%2)").arg(toLeg.getDisplayIdent()).arg(toLeg.getMapTypeName());
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
        departure = tr("%1 (%2)").arg(departLeg.getDisplayIdent()).arg(departLeg.getMapTypeName());

      if(route.hasValidDestination())
        destination = tr("%1 (%2)").arg(destLeg.getName()).arg(destLeg.getDisplayIdent());
      else
        destination = tr("%1 (%2)").arg(destLeg.getDisplayIdent()).arg(destLeg.getMapTypeName());

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

  if(!state.contains(ui->horizontalSliderRouteCalcAirwayPref))
    // Correct value to center if settings file is new
    ui->horizontalSliderRouteCalcAirwayPref->setValue(AIRWAY_WAYPOINT_PREF_CENTER);

  // Adjust slider to new values since they might be restored from settings
  ui->horizontalSliderRouteCalcAirwayPref->setMinimum(AIRWAY_WAYPOINT_PREF_MIN);
  ui->horizontalSliderRouteCalcAirwayPref->setMaximum(AIRWAY_WAYPOINT_PREF_MAX);
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
  return ui->horizontalSliderRouteCalcAirwayPref->value();
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
  return DIRECT_COST_FACTORS.at(ui->horizontalSliderRouteCalcAirwayPref->value());
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
