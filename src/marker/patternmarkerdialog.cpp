/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "marker/patternmarkerdialog.h"

#include "app/navapp.h"
#include "common/constants.h"
#include "common/mapmarkers.h"
#include "common/mapresult.h"
#include "common/unit.h"
#include "common/unitstringtool.h"
#include "gui/helphandler.h"
#include "gui/runwaytable.h"
#include "gui/dialog.h"
#include "gui/widgetstate.h"
#include "settings/settings.h"

#include "ui_patternmarkerdialog.h"

using atools::geo::Pos;

const QString PatternMarkerDialog::DEFAULT_COLOR_STR = QColor(Qt::darkRed).name(QColor::HexArgb);

PatternMarkerDialog::PatternMarkerDialog(QWidget *parent, const map::PatternMarker& markerParam, const map::MapResult& result,
                                           bool editMode)
  : MarkerDialog(parent, tr("Traffic Pattern"), markerParam, result, editMode), ui(new Ui::PatternMarkerDialog)
{
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setWindowModality(Qt::ApplicationModal);
  ui->setupUi(this);

  // Assign button for base class functions
  setPushButtonColor(ui->pushButtonTrafficPatternColor);

  // Default is OK button
  ui->buttonBoxTrafficPattern->button(QDialogButtonBox::Ok)->setDefault(true);

  // Focus runway table
  ui->tableWidgetTrafficPatternRunway->setFocus();

  // Get airport and create runway table
  map::MapAirport airport;
  if(getResult()->hasAirports())
    airport = getResult()->airports.constFirst();

  runwayTable = new RunwayTable(parent, airport, ui->tableWidgetTrafficPatternRunway,
                                false /* navdata */, false /* addAirportParam */);

  runwayTable->setAirportLabel(ui->labelTrafficPatternAirport);
  runwayTable->setShowPattern(true);

  connect(runwayTable, &RunwayTable::itemSelectionChanged, this, &PatternMarkerDialog::updateRunwayLabel);
  connect(runwayTable, &RunwayTable::doubleClicked, this, &PatternMarkerDialog::runwayTableDoubleClicked);

  connect(ui->checkBoxTrafficPattern45Degree, &QCheckBox::toggled, this, &PatternMarkerDialog::updateWidgets);
  connect(ui->buttonBoxTrafficPattern, &QDialogButtonBox::clicked, this, &PatternMarkerDialog::buttonBoxClicked);
  connect(ui->pushButtonTrafficPatternColor, &QPushButton::clicked, this, &MarkerDialog::colorButtonClicked);

  // Saves original texts and restores them on deletion
  units = new UnitStringTool();
  units->init({ui->doubleSpinBoxTrafficPatternBaseDistance, ui->doubleSpinBoxTrafficPatternDepartureDistance,
               ui->doubleSpinBoxDownwindDistance, ui->spinBoxTrafficPatternAltitude});

  restoreState();
}

PatternMarkerDialog::~PatternMarkerDialog()
{
  // Save dialog position and size
  atools::gui::WidgetState(lnm::PATTERN_MARKER_DIALOG).save(this);

  delete runwayTable;
  delete units;
  delete ui;
}

/* A button box button was clicked */
void PatternMarkerDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxTrafficPattern->button(QDialogButtonBox::Ok))
  {
    saveState();
    QDialog::accept();
  }
  else if(button == ui->buttonBoxTrafficPattern->button(QDialogButtonBox::Help))
    // Keep dialog open
    atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl + "TRAFFICPATTERN.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxTrafficPattern->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void PatternMarkerDialog::runwayTableDoubleClicked()
{
  saveState();
  QDialog::accept();
}

void PatternMarkerDialog::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::PATTERN_MARKER_DIALOG);

  // Load dialog position and size
  widgetState.restore(this);

  if(isEditMode())
    markerToWidgets();
  else
  {
    widgetState.restore({ui->doubleSpinBoxDownwindDistance, ui->spinBoxTrafficPatternAltitude,
                         ui->doubleSpinBoxTrafficPatternBaseDistance, ui->doubleSpinBoxTrafficPatternDepartureDistance,
                         ui->checkBoxTrafficPattern45Degree, ui->comboBoxTrafficPatternTurnDirection, ui->checkBoxTrafficPatternEntryExit});
    marker->color = atools::settings::Settings::instance().valueStr(lnm::PATTERN_MARKER_DIALOG_COLOR, DEFAULT_COLOR_STR);
    widgetsToMarker();
  }

  updateWidgets();
  updateButtonColor();
  runwayTable->init();
}

void PatternMarkerDialog::saveState() const
{
  atools::gui::WidgetState widgetState(lnm::PATTERN_MARKER_DIALOG);
  widgetState.save({this, ui->doubleSpinBoxDownwindDistance, ui->spinBoxTrafficPatternAltitude, ui->doubleSpinBoxTrafficPatternBaseDistance,
                    ui->doubleSpinBoxTrafficPatternDepartureDistance, ui->checkBoxTrafficPattern45Degree,
                    ui->comboBoxTrafficPatternTurnDirection, ui->checkBoxTrafficPatternEntryExit});
  atools::settings::Settings::instance().setValue(lnm::PATTERN_MARKER_DIALOG_COLOR, marker->color.name(QColor::HexArgb));
}

void PatternMarkerDialog::updateRunwayLabel()
{
  map::MapRunway rw;
  map::MapRunwayEnd end;
  runwayTable->getCurrentSelected(rw, end);

  if(rw.patternAlt > 100.f)
    ui->spinBoxTrafficPatternAltitude->setValue(atools::roundToInt(Unit::rev(rw.patternAlt, Unit::altFeetF)));

  if(end.isValid() && (end.pattern == "R" || end.pattern == "L"))
    ui->comboBoxTrafficPatternTurnDirection->setCurrentIndex(end.pattern == "L" ? 0 : 1);
}

void PatternMarkerDialog::updateWidgets()
{
  bool enabled = !ui->checkBoxTrafficPattern45Degree->isChecked();
  ui->doubleSpinBoxTrafficPatternBaseDistance->setEnabled(enabled);
  ui->labelTrafficPatternBaseDistance->setEnabled(enabled);
  ui->doubleSpinBoxTrafficPatternDepartureDistance->setEnabled(enabled);
  ui->labelTrafficPatternDepartureDistance->setEnabled(enabled);
}

void PatternMarkerDialog::markerToWidgets()
{
  ui->comboBoxTrafficPatternTurnDirection->setCurrentIndex(marker->turnRight ? 1 : 0);
  ui->checkBoxTrafficPattern45Degree->setChecked(marker->base45Degree);
  ui->checkBoxTrafficPatternEntryExit->setChecked(marker->showEntryExit);
  ui->doubleSpinBoxDownwindDistance->setValue(Unit::distNmF(marker->downwindParallelDistance));
  ui->doubleSpinBoxTrafficPatternBaseDistance->setValue(Unit::distNmF(marker->finalDistance));
  ui->doubleSpinBoxTrafficPatternDepartureDistance->setValue(Unit::distNmF(marker->departureDistance));
  ui->spinBoxTrafficPatternAltitude->setValue(Unit::altFeetF(marker->position.getAltitude() - runwayTable->getAirport().getAltitude()));

  QString error;
  runwayTable->setPreSelectedRunwayEnd(marker->runwayName, &error);

  if(!error.isEmpty())
    atools::gui::Dialog::warning(this, error);
}

void PatternMarkerDialog::widgetsToMarker()
{
  map::MapRunway rw;
  map::MapRunwayEnd end;
  runwayTable->getCurrentSelected(rw, end);

  bool primary = !end.secondary;
  const map::MapAirport& airport = runwayTable->getAirport();

  marker->airportIdent = airport.displayIdent();
  marker->runwayName = primary ? rw.primaryName : rw.secondaryName;
  marker->turnRight = ui->comboBoxTrafficPatternTurnDirection->currentIndex() == 1;
  marker->base45Degree = ui->checkBoxTrafficPattern45Degree->isChecked();
  marker->showEntryExit = ui->checkBoxTrafficPatternEntryExit->isChecked();
  marker->downwindParallelDistance = Unit::rev(static_cast<float>(ui->doubleSpinBoxDownwindDistance->value()), Unit::distNmF);
  marker->finalDistance = Unit::rev(static_cast<float>(ui->doubleSpinBoxTrafficPatternBaseDistance->value()), Unit::distNmF);
  marker->departureDistance = Unit::rev(static_cast<float>(ui->doubleSpinBoxTrafficPatternDepartureDistance->value()), Unit::distNmF);

  marker->courseTrue = primary ? rw.heading : atools::geo::opposedCourseDeg(rw.heading);
  marker->nav.ident = airport.ident;
  marker->nav.type = map::AIRPORT;
  marker->nav.magvar = airport.magvar;
  marker->runwayLength = atools::roundToInt(rw.length - (primary ? rw.primaryOffset : rw.secondaryOffset));

  float heading = primary ? atools::geo::opposedCourseDeg(rw.heading) : rw.heading;
  Pos pos = rw.position.endpoint(atools::geo::feetToMeter(rw.length / 2.f - (primary ? rw.primaryOffset : rw.secondaryOffset)), heading);

  float altFeet = Unit::rev(static_cast<float>(ui->spinBoxTrafficPatternAltitude->value()), Unit::altFeetF);

#ifdef DEBUG_INFORMATION
  qDebug() << Q_FUNC_INFO << "altitude" << ui->spinBoxTrafficPatternAltitude->value()
           << "airport altitude" << airport.getAltitude()
           << "alt feet" << altFeet;
#endif

  pos.setAltitude(altFeet + airport.getAltitude());
  marker->position = pos;
}
