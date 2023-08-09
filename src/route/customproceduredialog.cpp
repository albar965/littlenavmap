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

#include "route/customproceduredialog.h"

#include "common/constants.h"
#include "common/maptypes.h"
#include "common/unit.h"
#include "common/unitstringtool.h"
#include "geo/calculations.h"
#include "gui/helphandler.h"
#include "gui/runwayselection.h"
#include "gui/widgetstate.h"
#include "ui_customproceduredialog.h"

#include <QPushButton>
#include <QStringBuilder>

CustomProcedureDialog::CustomProcedureDialog(QWidget *parent, const map::MapAirport& mapAirport, bool departureParam,
                                             const QString& dialogHeader)
  : QDialog(parent), ui(new Ui::CustomProcedureDialog), departure(departureParam)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  runwaySelection = new RunwaySelection(parent, mapAirport, ui->tableWidgetCustomProcRunway);
  runwaySelection->setAirportLabel(ui->labelCustomProcAirport);

  connect(runwaySelection, &RunwaySelection::doubleClicked, this, &CustomProcedureDialog::doubleClicked);
  connect(runwaySelection, &RunwaySelection::itemSelectionChanged, this, &CustomProcedureDialog::updateWidgets);

  connect(ui->spinBoxCustomProcAlt, QOverload<int>::of(&QSpinBox::valueChanged), this, &CustomProcedureDialog::updateWidgets);
  connect(ui->spinBoxCustomProcAngle, QOverload<int>::of(&QSpinBox::valueChanged), this, &CustomProcedureDialog::updateWidgets);
  connect(ui->buttonBoxCustomProc, &QDialogButtonBox::clicked, this, &CustomProcedureDialog::buttonBoxClicked);
  connect(ui->doubleSpinBoxCustomProcDist, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &CustomProcedureDialog::updateWidgets);

  restoreState();

  setWindowTitle(QApplication::applicationName() % tr(" - Select Runway"));
  ui->labelCustomProcRunway->setText(dialogHeader);

  // Show or hide widgets not relevant for departure
  ui->labelCustomProcSlope->setHidden(departure);

  // Altitude
  ui->labelCustomProcRunwayAlt->setHidden(departure);
  ui->spinBoxCustomProcAlt->setHidden(departure);

  // Angle
  ui->spinBoxCustomProcAngle->setHidden(departure);
  ui->labelCustomProcRunwayAngle->setHidden(departure);
  ui->labelCustomProcAngle->setHidden(departure);

  if(departure)
  {
    ui->labelCustomProcRunwayDist->setText(("&Length of the exended runway center line:"));
    ui->doubleSpinBoxCustomProcDist->setToolTip(("Distance from the takeoff position at the runway end to the end of the departure."));
  }
  else
  {
    ui->labelCustomProcRunwayDist->setText(("&Start of final to runway threshold:"));
    ui->doubleSpinBoxCustomProcDist->setToolTip(("Distance from the start of the final leg to the runway threshold."));
  }

  // Saves original texts and restores them on deletion
  units = new UnitStringTool();
  units->init({ui->doubleSpinBoxCustomProcDist, ui->spinBoxCustomProcAlt});
}

CustomProcedureDialog::~CustomProcedureDialog()
{
  atools::gui::WidgetState(departure ? lnm::CUSTOM_DEPARTURE_DIALOG : lnm::CUSTOM_APPROACH_DIALOG).save(this);

  delete runwaySelection;
  delete units;
  delete ui;
}

void CustomProcedureDialog::doubleClicked()
{
  saveState();
  QDialog::accept();
}

void CustomProcedureDialog::restoreState()
{
  atools::gui::WidgetState widgetState(departure ? lnm::CUSTOM_DEPARTURE_DIALOG : lnm::CUSTOM_APPROACH_DIALOG, false);
  // Angle not saved on purpose
  widgetState.restore({this, ui->doubleSpinBoxCustomProcDist, ui->spinBoxCustomProcAlt});

  runwaySelection->restoreState();
  updateWidgets();

  ui->tableWidgetCustomProcRunway->setFocus();
}

void CustomProcedureDialog::saveState()
{
  atools::gui::WidgetState widgetState(departure ? lnm::CUSTOM_DEPARTURE_DIALOG : lnm::CUSTOM_APPROACH_DIALOG, false);
  widgetState.save({this, ui->doubleSpinBoxCustomProcDist, ui->spinBoxCustomProcAlt});
}

void CustomProcedureDialog::getSelected(map::MapRunway& runway, map::MapRunwayEnd& end) const
{
  runwaySelection->getCurrentSelected(runway, end);
}

float CustomProcedureDialog::getLegDistance() const
{
  return Unit::rev(static_cast<float>(ui->doubleSpinBoxCustomProcDist->value()), Unit::distNmF);
}

float CustomProcedureDialog::getLegOffsetAngle() const
{
  return static_cast<float>(ui->spinBoxCustomProcAngle->value());
}

float CustomProcedureDialog::getEntryAltitude() const
{
  return Unit::rev(static_cast<float>(ui->spinBoxCustomProcAlt->value()), Unit::altFeetF);
}

/* A button box button was clicked */
void CustomProcedureDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxCustomProc->button(QDialogButtonBox::Ok))
  {
    saveState();
    QDialog::accept();
  }
  else if(button == ui->buttonBoxCustomProc->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(
      parentWidget(), lnm::helpOnlineUrl + "CUSTOMPROCEDURE.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxCustomProc->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void CustomProcedureDialog::updateWidgets()
{
  if(!departure)
  {
    // Update labels only for destination

    // Slope =======================
    float height = atools::geo::feetToMeter(getEntryAltitude());
    float dist = atools::geo::nmToMeter(getLegDistance());
    ui->labelCustomProcSlope->setText(tr("Approach slope %1°").
                                      arg(QLocale().toString(atools::geo::toDegree(std::atan2(height, dist)), 'f', 1)));

    // Heading =======================
    map::MapRunway runway;
    map::MapRunwayEnd end;
    runwaySelection->getCurrentSelected(runway, end);

    if(runway.isValid() && end.isValid())
    {
      float angle = end.heading - runwaySelection->getAirport().magvar + getLegOffsetAngle();
      ui->labelCustomProcAngle->setText(tr("Final course to runway %1 %2 is %3°M").
                                        arg(end.name).arg(atools::almostEqual(getLegOffsetAngle(), 0.f) ? QString() : tr("with offset ")).
                                        arg(QLocale().toString(angle, 'f', 0)));
    }
    else
      ui->labelCustomProcAngle->clear();
  }
  else
  {
    ui->labelCustomProcSlope->clear();
    ui->labelCustomProcAngle->clear();
  }

  ui->buttonBoxCustomProc->button(QDialogButtonBox::Ok)->setEnabled(runwaySelection->hasRunways());
}
