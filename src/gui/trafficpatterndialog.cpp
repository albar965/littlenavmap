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

#include "gui/trafficpatterndialog.h"

#include "atools.h"
#include "common/constants.h"
#include "common/maptypes.h"
#include "common/unit.h"
#include "common/unitstringtool.h"
#include "geo/calculations.h"
#include "gui/helphandler.h"
#include "gui/runwayselection.h"
#include "gui/widgetstate.h"
#include "gui/widgetutil.h"
#include "settings/settings.h"

#include "ui_trafficpatterndialog.h"

#include <QColorDialog>

using atools::geo::Pos;

TrafficPatternDialog::TrafficPatternDialog(QWidget *parent, const map::MapAirport& mapAirport) :
  QDialog(parent), ui(new Ui::TrafficPatternDialog), color(Qt::darkRed)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  ui->buttonBoxTrafficPattern->button(QDialogButtonBox::Ok)->setDefault(true);
  ui->tableWidgetTrafficPatternRunway->setFocus();

  runwaySelection = new RunwaySelection(parent, mapAirport, ui->tableWidgetTrafficPatternRunway);
  runwaySelection->setAirportLabel(ui->labelTrafficPatternAirport);
  runwaySelection->setShowPattern(true);

  connect(runwaySelection, &RunwaySelection::itemSelectionChanged, this, &TrafficPatternDialog::updateRunwayLabel);
  connect(runwaySelection, &RunwaySelection::doubleClicked, this, &TrafficPatternDialog::doubleClicked);

  connect(ui->checkBoxTrafficPattern45Degree, &QCheckBox::toggled, this, &TrafficPatternDialog::updateWidgets);
  connect(ui->buttonBoxTrafficPattern, &QDialogButtonBox::clicked, this, &TrafficPatternDialog::buttonBoxClicked);
  connect(ui->pushButtonTrafficPatternColor, &QPushButton::clicked, this, &TrafficPatternDialog::colorButtonClicked);

  // Saves original texts and restores them on deletion
  units = new UnitStringTool();
  units->init({ui->doubleSpinBoxTrafficPatternBaseDistance, ui->doubleSpinBoxTrafficPatternDepartureDistance,
               ui->doubleSpinBoxDownwindDistance, ui->spinBoxTrafficPatternAltitude});

  restoreState();
}

TrafficPatternDialog::~TrafficPatternDialog()
{
  atools::gui::WidgetState(lnm::TRAFFIC_PATTERN_DIALOG).save(this);

  delete runwaySelection;
  delete units;
  delete ui;
}

/* A button box button was clicked */
void TrafficPatternDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxTrafficPattern->button(QDialogButtonBox::Ok))
  {
    saveState();
    QDialog::accept();
  }
  else if(button == ui->buttonBoxTrafficPattern->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl + "TRAFFICPATTERN.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxTrafficPattern->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void TrafficPatternDialog::doubleClicked()
{
  saveState();
  QDialog::accept();
}

void TrafficPatternDialog::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::TRAFFIC_PATTERN_DIALOG, false);
  widgetState.restore({
    this,
    ui->doubleSpinBoxDownwindDistance,
    ui->spinBoxTrafficPatternAltitude,
    ui->doubleSpinBoxTrafficPatternBaseDistance,
    ui->doubleSpinBoxTrafficPatternDepartureDistance,
    ui->checkBoxTrafficPattern45Degree,
    ui->comboBoxTrafficPatternTurnDirection,
    ui->checkBoxTrafficPatternEntryExit
  });
  color = atools::settings::Settings::instance().valueVar(lnm::TRAFFIC_PATTERN_DIALOG_COLOR, color).value<QColor>();

  updateWidgets();
  updateButtonColor();
  runwaySelection->restoreState();
}

void TrafficPatternDialog::saveState()
{
  atools::gui::WidgetState widgetState(lnm::TRAFFIC_PATTERN_DIALOG, false);
  widgetState.save({
    this,
    ui->doubleSpinBoxDownwindDistance,
    ui->spinBoxTrafficPatternAltitude,
    ui->doubleSpinBoxTrafficPatternBaseDistance,
    ui->doubleSpinBoxTrafficPatternDepartureDistance,
    ui->checkBoxTrafficPattern45Degree,
    ui->comboBoxTrafficPatternTurnDirection,
    ui->checkBoxTrafficPatternEntryExit
  });
  atools::settings::Settings::instance().setValueVar(lnm::TRAFFIC_PATTERN_DIALOG_COLOR, color);
}

void TrafficPatternDialog::colorButtonClicked()
{
  QColor col = QColorDialog::getColor(color, parentWidget());
  if(col.isValid())
  {
    color = col;
    updateButtonColor();
  }
}

void TrafficPatternDialog::updateButtonColor()
{
  atools::gui::util::changeWidgetColor(ui->pushButtonTrafficPatternColor, color);
}

void TrafficPatternDialog::updateRunwayLabel()
{
  map::MapRunway rw;
  map::MapRunwayEnd end;
  runwaySelection->getCurrentSelected(rw, end);

  if(rw.patternAlt > 100.f)
    ui->spinBoxTrafficPatternAltitude->setValue(atools::roundToInt(Unit::rev(rw.patternAlt, Unit::altFeetF)));

  if(end.isValid() && (end.pattern == "R" || end.pattern == "L"))
    ui->comboBoxTrafficPatternTurnDirection->setCurrentIndex(end.pattern == "L" ? 0 : 1);
}

void TrafficPatternDialog::updateWidgets()
{
  bool enabled = !ui->checkBoxTrafficPattern45Degree->isChecked();
  ui->doubleSpinBoxTrafficPatternBaseDistance->setEnabled(enabled);
  ui->labelTrafficPatternBaseDistance->setEnabled(enabled);
  ui->doubleSpinBoxTrafficPatternDepartureDistance->setEnabled(enabled);
  ui->labelTrafficPatternDepartureDistance->setEnabled(enabled);
}

void TrafficPatternDialog::fillPatternMarker(map::PatternMarker& pattern)
{
  map::MapRunway rw;
  map::MapRunwayEnd end;
  runwaySelection->getCurrentSelected(rw, end);

  bool primary = !end.secondary;
  const map::MapAirport& airport = runwaySelection->getAirport();

  // Assign an artifical id to the hold to allow internal identification
  pattern.id = map::getNextUserFeatureId();
  pattern.airportIcao = airport.displayIdent();
  pattern.runwayName = primary ? rw.primaryName : rw.secondaryName;
  pattern.color = color;
  pattern.turnRight = ui->comboBoxTrafficPatternTurnDirection->currentIndex() == 1;
  pattern.base45Degree = ui->checkBoxTrafficPattern45Degree->isChecked();
  pattern.showEntryExit = ui->checkBoxTrafficPatternEntryExit->isChecked();
  pattern.downwindParallelDistance = Unit::rev(static_cast<float>(ui->doubleSpinBoxDownwindDistance->value()), Unit::distNmF);
  pattern.finalDistance = Unit::rev(static_cast<float>(ui->doubleSpinBoxTrafficPatternBaseDistance->value()), Unit::distNmF);
  pattern.departureDistance = Unit::rev(static_cast<float>(ui->doubleSpinBoxTrafficPatternDepartureDistance->value()), Unit::distNmF);

  pattern.courseTrue = primary ? rw.heading : atools::geo::opposedCourseDeg(rw.heading);
  pattern.magvar = airport.magvar;
  pattern.runwayLength = atools::roundToInt(rw.length - (primary ? rw.primaryOffset : rw.secondaryOffset));

  float heading = primary ? atools::geo::opposedCourseDeg(rw.heading) : rw.heading;
  Pos pos = rw.position.endpoint(atools::geo::feetToMeter(rw.length / 2.f - (primary ? rw.primaryOffset : rw.secondaryOffset)), heading);

  float altFeet = Unit::rev(static_cast<float>(ui->spinBoxTrafficPatternAltitude->value()), Unit::altFeetF);
  qDebug() << Q_FUNC_INFO << "altitude" << ui->spinBoxTrafficPatternAltitude->value()
           << "airport altitude" << airport.getAltitude()
           << "alt feet" << altFeet;

  pos.setAltitude(altFeet + airport.getAltitude());
  pattern.position = pos;

  qDebug() << Q_FUNC_INFO << "pattern.position" << pattern.position;
}
