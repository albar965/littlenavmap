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

#include "route/customproceduredialog.h"

#include "common/unitstringtool.h"
#include "common/constants.h"
#include "geo/calculations.h"
#include "common/unit.h"
#include "gui/runwayselection.h"

#include "ui_customproceduredialog.h"

#include "gui/helphandler.h"
#include "gui/widgetstate.h"
#include "settings/settings.h"

#include <QPushButton>

CustomProcedureDialog::CustomProcedureDialog(QWidget *parent, const map::MapAirport& mapAirport) :
  QDialog(parent), ui(new Ui::CustomProcedureDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  runwaySelection = new RunwaySelection(parent, mapAirport, ui->tableWidgetCustomProcRunway);
  runwaySelection->setAirportLabel(ui->labelCustomProcAirport);

  connect(runwaySelection, &RunwaySelection::doubleClicked, this, &CustomProcedureDialog::doubleClicked);

  connect(ui->buttonBoxCustomProc, &QDialogButtonBox::clicked, this, &CustomProcedureDialog::buttonBoxClicked);
  connect(ui->spinBoxCustomProcAlt, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &CustomProcedureDialog::updateWidgets);
  connect(ui->doubleSpinBoxCustomProcDist, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
          this, &CustomProcedureDialog::updateWidgets);

  restoreState();

  // Saves original texts and restores them on deletion
  units = new UnitStringTool();
  units->init({ui->doubleSpinBoxCustomProcDist, ui->spinBoxCustomProcAlt});
}

CustomProcedureDialog::~CustomProcedureDialog()
{
  atools::gui::WidgetState(lnm::CUSTOM_PROCEDURE_DIALOG).save(this);

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
  atools::gui::WidgetState widgetState(lnm::CUSTOM_PROCEDURE_DIALOG, false);
  widgetState.restore({
    this,
    ui->doubleSpinBoxCustomProcDist,
    ui->spinBoxCustomProcAlt
  });

  runwaySelection->restoreState();
  updateWidgets();
}

void CustomProcedureDialog::saveState()
{
  atools::gui::WidgetState widgetState(lnm::CUSTOM_PROCEDURE_DIALOG, false);
  widgetState.save({
    this,
    ui->doubleSpinBoxCustomProcDist,
    ui->spinBoxCustomProcAlt
  });
}

void CustomProcedureDialog::getSelected(map::MapRunway& runway, map::MapRunwayEnd& end) const
{
  runwaySelection->getCurrentSelected(runway, end);
}

float CustomProcedureDialog::getEntryDistance() const
{
  return Unit::rev(static_cast<float>(ui->doubleSpinBoxCustomProcDist->value()), Unit::distNmF);
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
  float height = atools::geo::feetToMeter(getEntryAltitude());
  float dist = atools::geo::nmToMeter(getEntryDistance());
  ui->labelCustomProcSlope->setText(tr("Approach slope %1°").
                                    arg(QLocale().toString(atools::geo::toDegree(std::atan2(height, dist)), 'f', 1)));
}
