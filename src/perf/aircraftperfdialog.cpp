/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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

#include "perf/aircraftperfdialog.h"
#include "ui_aircraftperfdialog.h"

#include "atools.h"
#include "common/constants.h"
#include "common/formatter.h"
#include "common/textpointer.h"
#include "common/unit.h"
#include "common/unitstringtool.h"
#include "fs/perf/aircraftperf.h"
#include "fs/util/fsutil.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"
#include "util/htmlbuilder.h"

#include <QPushButton>

using atools::fs::perf::AircraftPerf;
using atools::roundToInt;

AircraftPerfDialog::AircraftPerfDialog(QWidget *parent, const atools::fs::perf::AircraftPerf& aircraftPerformance,
                                       const QString& modeText, bool newPerfParam)
  : QDialog(parent), ui(new Ui::AircraftPerfDialog), newPerf(newPerfParam)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  ui->buttonBox->button(QDialogButtonBox::Save)->setText(tr("OK and &Save"));

  setWindowTitle(windowTitle().arg(modeText));

  // Copy performance object
  perf = new AircraftPerf;
  *perf = aircraftPerformance;

  ui->doubleSpinBoxClimbVertSpeed->setSuffix(tr(" %vspeed% %1").arg(TextPointer::getPointerUp()));
  ui->doubleSpinBoxDescentVertSpeed->setSuffix(tr(" %vspeed% %1").arg(TextPointer::getPointerDown()));

  // All widgets that need their unit placeholders replaced on unit changes
  unitWidgets.append({
    ui->comboBoxFuelUnit,
    ui->comboBoxFuelType,
    ui->spinBoxClimbFuelFlow,
    ui->spinBoxCruiseSpeed,
    ui->spinBoxDescentFuelFlow,
    ui->spinBoxCruiseFuelFlow,
    ui->spinBoxContingencyFuel,
    ui->spinBoxClimbSpeed,
    ui->spinBoxExtraFuel,
    ui->spinBoxTaxiFuel,
    ui->spinBoxReserveFuel,
    ui->doubleSpinBoxClimbVertSpeed,
    ui->spinBoxDescentSpeed,
    ui->doubleSpinBoxDescentVertSpeed,
    ui->spinBoxAlternateSpeed,
    ui->spinBoxAlternateFuelFlow,
    ui->spinBoxRunwayLength,
    ui->spinBoxUsableFuel
  });

  restoreState();

  // Adjust vertical speed spin boxes for units
  if(Unit::getUnitVertSpeed() == opts::VERT_SPEED_MS)
  {
    ui->doubleSpinBoxClimbVertSpeed->setDecimals(2);
    ui->doubleSpinBoxClimbVertSpeed->setMinimum(1.);
    ui->doubleSpinBoxClimbVertSpeed->setMaximum(100.);
    ui->doubleSpinBoxClimbVertSpeed->setSingleStep(1.);

    ui->doubleSpinBoxDescentVertSpeed->setDecimals(2);
    ui->doubleSpinBoxDescentVertSpeed->setMinimum(-100.);
    ui->doubleSpinBoxDescentVertSpeed->setMaximum(-1.);
    ui->doubleSpinBoxDescentVertSpeed->setSingleStep(1.);
  }
  else if(Unit::getUnitVertSpeed() == opts::VERT_SPEED_FPM)
  {
    ui->doubleSpinBoxClimbVertSpeed->setDecimals(0);
    ui->doubleSpinBoxClimbVertSpeed->setMinimum(100.);
    ui->doubleSpinBoxClimbVertSpeed->setMaximum(10000.);
    ui->doubleSpinBoxClimbVertSpeed->setSingleStep(100.);

    ui->doubleSpinBoxDescentVertSpeed->setDecimals(0);
    ui->doubleSpinBoxDescentVertSpeed->setMinimum(-10000.);
    ui->doubleSpinBoxDescentVertSpeed->setMaximum(-100.);
    ui->doubleSpinBoxDescentVertSpeed->setSingleStep(100.);
  }

  // Update units
  units = new UnitStringTool();
  units->init(unitWidgets, aircraftPerformance.useFuelAsVolume());

  // Create a backup for reset changes
  perfBackup = new AircraftPerf;
  *perfBackup = aircraftPerformance;

  // Copy from object to dialog
  fromPerfToDialog(perf);

  // Update vertical speed descent rule
  vertSpeedChanged();
  aircraftTypeEdited();
  updateRange();

  if(newPerf)
    ui->tabWidget->setCurrentIndex(0);

  // Fuel by volume or weight changed
  connect(ui->comboBoxFuelUnit, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AircraftPerfDialog::fuelUnitChanged);

  // Update descent rule
  connect(ui->doubleSpinBoxDescentVertSpeed, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &AircraftPerfDialog::vertSpeedChanged);
  connect(ui->spinBoxDescentSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this, &AircraftPerfDialog::vertSpeedChanged);
  connect(ui->spinBoxUsableFuel, QOverload<int>::of(&QSpinBox::valueChanged), this, &AircraftPerfDialog::updateRange);
  connect(ui->spinBoxCruiseSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this, &AircraftPerfDialog::updateRange);
  connect(ui->spinBoxCruiseFuelFlow, QOverload<int>::of(&QSpinBox::valueChanged), this, &AircraftPerfDialog::updateRange);
  connect(ui->spinBoxReserveFuel, QOverload<int>::of(&QSpinBox::valueChanged), this, &AircraftPerfDialog::updateRange);

  connect(ui->lineEditType, &QLineEdit::textEdited, this, &AircraftPerfDialog::aircraftTypeEdited);
  connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &AircraftPerfDialog::buttonBoxClicked);
}

AircraftPerfDialog::~AircraftPerfDialog()
{
  saveState();

  delete perf;
  delete perfBackup;

  delete units;
  delete ui;
}

atools::fs::perf::AircraftPerf AircraftPerfDialog::getAircraftPerf() const
{
  return *perf;
}

void AircraftPerfDialog::restoreState()
{
  atools::gui::WidgetState ws(lnm::AIRCRAFT_PERF_EDIT_DIALOG);
  ws.restore({this, ui->tabWidget});

  switch(OptionData::instance().getUnitFuelAndWeight())
  {
    case opts::FUEL_WEIGHT_GAL_LBS:
      fuelUnit = perf->useFuelAsVolume() ? VOLUME_GAL : WEIGHT_LBS;
      break;
    case opts::FUEL_WEIGHT_LITER_KG:
      fuelUnit = perf->useFuelAsVolume() ? VOLUME_LITER : WEIGHT_KG;
      break;
  }
}

void AircraftPerfDialog::saveState() const
{
  atools::gui::WidgetState ws(lnm::AIRCRAFT_PERF_EDIT_DIALOG);
  ws.save({this, ui->tabWidget});
}

void AircraftPerfDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBox->button(QDialogButtonBox::Ok))
  {
    fromDialogToPerf(perf);
    QDialog::accept();
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Save))
  {
    saveClicked = true;
    fromDialogToPerf(perf);
    QDialog::accept();
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Reset))
  {
    // Reset to backup
    *perf = *perfBackup;
    fromPerfToDialog(perf);
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::RestoreDefaults))
  {
    // Reset to default values
    *perf = AircraftPerf();
    fromPerfToDialog(perf);
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + "AIRCRAFTPERFEDIT.html",
                                             lnm::helpLanguageOnline());
  else if(button == ui->buttonBox->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void AircraftPerfDialog::aircraftTypeEdited()
{
  if(ui->lineEditType->text().isEmpty())
  {
    ui->labelTypeStatus->show();
    ui->labelTypeStatus->setText(atools::util::HtmlBuilder::errorMessage(tr("Aircraft type is empty. "
                                                                            "It is recommended to use official ICAO codes like "
                                                                            "\"B738\", \"BE9L\" or \"C172\".")));
  }
  else if(!atools::fs::util::isAircraftTypeDesignatorValid(ui->lineEditType->text()))
  {
    ui->labelTypeStatus->show();
    ui->labelTypeStatus->setText(atools::util::HtmlBuilder::warningMessage(tr("Aircraft type is probably not valid. "
                                                                              "It is recommended to use official ICAO codes like "
                                                                              "\"B738\", \"BE9L\" or \"C172\".")));
  }
  else
    ui->labelTypeStatus->hide();
}

void AircraftPerfDialog::updateRange()
{
  // Check values and display error message =======================
  if(ui->spinBoxUsableFuel->value() < 1.)
    ui->labelUsableFuelRange->setText(atools::util::HtmlBuilder::errorMessage(tr("Usable fuel not set.")));
  else if(ui->spinBoxUsableFuel->value() <= ui->spinBoxReserveFuel->value())
    ui->labelUsableFuelRange->setText(atools::util::HtmlBuilder::errorMessage(tr("Usable fuel smaller than reserve.")));
  else if(ui->spinBoxCruiseSpeed->value() < 1.)
    ui->labelUsableFuelRange->setText(atools::util::HtmlBuilder::errorMessage(tr("Cruise speed not set.")));
  else if(ui->spinBoxCruiseFuelFlow->value() < 1.)
    ui->labelUsableFuelRange->setText(atools::util::HtmlBuilder::errorMessage(tr("Cruise fuel flow not set.")));
  else if(ui->spinBoxUsableFuel->value() > 1.)
  {
    // All is valid - display range =======================
    float speedKts = Unit::rev(static_cast<float>(ui->spinBoxCruiseSpeed->value()), Unit::speedKtsF);
    float fuelVolWeight = static_cast<float>(ui->spinBoxUsableFuel->value()) -
                          static_cast<float>(ui->spinBoxReserveFuel->value());
    float fuelFlowVolWeight = static_cast<float>(ui->spinBoxCruiseFuelFlow->value());

    float enduranceHours = fuelVolWeight / fuelFlowVolWeight;

    ui->labelUsableFuelRange->setText(tr("Estimated range with reserve %1, %2.").
                                      arg(Unit::distNm(enduranceHours * speedKts)).
                                      arg(formatter::formatMinutesHoursLong(enduranceHours)));
  }
}

void AircraftPerfDialog::vertSpeedChanged()
{
  float descentRateFtPerNm =
    Unit::rev(static_cast<float>(ui->doubleSpinBoxDescentVertSpeed->value()), Unit::speedVertFpmF) * 60.f /
    Unit::rev(ui->spinBoxDescentSpeed->value(), Unit::speedKtsF);

  // 2,3 NM per 1000 ft

  QString txt = tr("Descent Rule of Thumb: %1 per %2 %3.").
                arg(Unit::distNm(1.f / -descentRateFtPerNm * Unit::rev(1000.f, Unit::altFeetF))).
                arg(QLocale().toString(1000.f, 'f', 0)).
                arg(Unit::getUnitAltStr());

  ui->labelDescentRule->setText(txt);
}

void AircraftPerfDialog::fuelUnitChanged()
{
  // Get previous fuel unit (volume/weight)
  bool volOld = isVol(fuelUnit);

  fromDialogToPerf(perf);

  fuelUnit = static_cast<FuelUnit>(ui->comboBoxFuelUnit->currentIndex());

  // Compare old and new fuel unit
  bool volNew = isVol(fuelUnit);
  if(volNew != volOld)
  {
    // Has changed - convert
    if(volNew)
      perf->fromLbsToGal();
    else
      perf->fromGalToLbs();
  }

  // Change widget suffixes
  units->update(volNew, (isMetric(fuelUnit) ? opts::FUEL_WEIGHT_LITER_KG : opts::FUEL_WEIGHT_GAL_LBS));
  fromPerfToDialog(perf);
}

void AircraftPerfDialog::fromPerfToDialog(const atools::fs::perf::AircraftPerf *aircraftPerf)
{
  {
    // Avoid recursion
    QSignalBlocker blocker(ui->comboBoxFuelUnit);
    ui->comboBoxFuelUnit->setCurrentIndex(fuelUnit);
  }

  ui->comboBoxFuelType->setCurrentIndex(aircraftPerf->isJetFuel());

  bool vol = isVol(fuelUnit);

  // Get pass through or conversion function for fuel units
  auto fuelFuncToDlg = fuelUnitsToDialogFunc(fuelUnit);

  if(ui->comboBoxSimulator->findText(aircraftPerf->getSimulator()) == -1)
    // Invalid or empty value
    ui->comboBoxSimulator->setCurrentIndex(0);
  else
    ui->comboBoxSimulator->setCurrentText(aircraftPerf->getSimulator());

  ui->lineEditName->setText(aircraftPerf->getName());
  ui->lineEditType->setText(aircraftPerf->getAircraftType());
  ui->plainTextEditDescription->setPlainText(aircraftPerf->getDescription());

  ui->spinBoxUsableFuel->setValue(roundToInt(fuelFuncToDlg(aircraftPerf->getUsableFuel(), vol)));
  ui->spinBoxReserveFuel->setValue(roundToInt(fuelFuncToDlg(aircraftPerf->getReserveFuel(), vol)));
  ui->spinBoxExtraFuel->setValue(roundToInt(fuelFuncToDlg(aircraftPerf->getExtraFuel(), vol)));
  ui->spinBoxTaxiFuel->setValue(roundToInt(fuelFuncToDlg(aircraftPerf->getTaxiFuel(), vol)));

  ui->spinBoxClimbSpeed->setValue(roundToInt(Unit::speedKtsF(aircraftPerf->getClimbSpeed())));
  ui->doubleSpinBoxClimbVertSpeed->setValue(Unit::speedVertFpmF(aircraftPerf->getClimbVertSpeed()));
  ui->spinBoxClimbFuelFlow->setValue(roundToInt(fuelFuncToDlg(aircraftPerf->getClimbFuelFlow(), vol)));

  ui->spinBoxCruiseSpeed->setValue(roundToInt(Unit::speedKtsF(aircraftPerf->getCruiseSpeed())));
  ui->spinBoxCruiseFuelFlow->setValue(roundToInt(fuelFuncToDlg(aircraftPerf->getCruiseFuelFlow(), vol)));
  ui->spinBoxContingencyFuel->setValue(roundToInt(aircraftPerf->getContingencyFuel()));

  ui->spinBoxDescentSpeed->setValue(roundToInt(Unit::speedKtsF(aircraftPerf->getDescentSpeed())));
  ui->doubleSpinBoxDescentVertSpeed->setValue(-Unit::speedVertFpmF(aircraftPerf->getDescentVertSpeed()));
  ui->spinBoxDescentFuelFlow->setValue(roundToInt(fuelFuncToDlg(aircraftPerf->getDescentFuelFlow(), vol)));

  ui->spinBoxAlternateSpeed->setValue(roundToInt(Unit::speedKtsF(aircraftPerf->getAlternateSpeed())));
  ui->spinBoxAlternateFuelFlow->setValue(roundToInt(fuelFuncToDlg(aircraftPerf->getAlternateFuelFlow(), vol)));

  ui->spinBoxRunwayLength->setValue(roundToInt(Unit::distShortFeetF(aircraftPerf->getMinRunwayLength())));
  ui->comboBoxRunwayType->setCurrentIndex(aircraftPerf->getRunwayType());
}

void AircraftPerfDialog::fromDialogToPerf(atools::fs::perf::AircraftPerf *aircraftPerf) const
{
  bool vol = isVol(fuelUnit);

  // Get pass through or conversion function for fuel units
  auto fuelFunc = fuelUnitsFromDialogFunc(fuelUnit);

  if(ui->comboBoxSimulator->currentIndex() == 0)
    // First index is always "all"
    aircraftPerf->setSimulator(QString());
  else
    aircraftPerf->setSimulator(ui->comboBoxSimulator->currentText());

  aircraftPerf->setName(ui->lineEditName->text());
  aircraftPerf->setAircraftType(ui->lineEditType->text());
  aircraftPerf->setFuelAsVolume(vol);
  aircraftPerf->setJetFuel(ui->comboBoxFuelType->currentIndex()); // 0 = avgas
  aircraftPerf->setDescription(ui->plainTextEditDescription->toPlainText());

  aircraftPerf->setUsableFuel(fuelFunc(ui->spinBoxUsableFuel->value(), vol));
  aircraftPerf->setReserveFuel(fuelFunc(ui->spinBoxReserveFuel->value(), vol));
  aircraftPerf->setExtraFuel(fuelFunc(ui->spinBoxExtraFuel->value(), vol));
  aircraftPerf->setTaxiFuel(fuelFunc(ui->spinBoxTaxiFuel->value(), vol));

  aircraftPerf->setClimbSpeed(Unit::rev(ui->spinBoxClimbSpeed->value(), Unit::speedKtsF));
  aircraftPerf->setClimbVertSpeed(Unit::rev(static_cast<float>(ui->doubleSpinBoxClimbVertSpeed->value()),
                                            Unit::speedVertFpmF));
  aircraftPerf->setClimbFuelFlow(fuelFunc(ui->spinBoxClimbFuelFlow->value(), vol));

  aircraftPerf->setCruiseSpeed(Unit::rev(ui->spinBoxCruiseSpeed->value(), Unit::speedKtsF));
  aircraftPerf->setCruiseFuelFlow(fuelFunc(ui->spinBoxCruiseFuelFlow->value(), vol));
  aircraftPerf->setContingencyFuel(ui->spinBoxContingencyFuel->value());

  aircraftPerf->setDescentSpeed(Unit::rev(ui->spinBoxDescentSpeed->value(), Unit::speedKtsF));
  aircraftPerf->setDescentVertSpeed(-Unit::rev(static_cast<float>(ui->doubleSpinBoxDescentVertSpeed->value()),
                                               Unit::speedVertFpmF));
  aircraftPerf->setDescentFuelFlow(fuelFunc(ui->spinBoxDescentFuelFlow->value(), vol));

  aircraftPerf->setAlternateSpeed(Unit::rev(ui->spinBoxAlternateSpeed->value(), Unit::speedKtsF));
  aircraftPerf->setAlternateFuelFlow(fuelFunc(ui->spinBoxAlternateFuelFlow->value(), vol));

  aircraftPerf->setMinRunwayLength(Unit::rev(ui->spinBoxRunwayLength->value(), Unit::distShortFeetF));
  aircraftPerf->setRunwayType(ui->comboBoxRunwayType->currentIndex());
}

bool AircraftPerfDialog::isMetric(FuelUnit unit)
{
  switch(unit)
  {
    case WEIGHT_LBS:
    case VOLUME_GAL:
      return false;

    case WEIGHT_KG:
    case VOLUME_LITER:
      return true;
  }
  return false;
}

bool AircraftPerfDialog::isVol(FuelUnit unit)
{
  switch(unit)
  {
    case WEIGHT_LBS:
    case WEIGHT_KG:
      return false;

    case VOLUME_GAL:
    case VOLUME_LITER:
      return true;
  }
  return false;
}

std::function<float(float value, bool fuelAsVolume)> AircraftPerfDialog::fuelUnitsToDialogFunc(FuelUnit unit)
{
  switch(unit)
  {
    case WEIGHT_LBS:
    case VOLUME_GAL:
      return Unit::fromCopy;

    case WEIGHT_KG:
    case VOLUME_LITER:
      return Unit::fromUsToMetric;
  }
  return Unit::fromCopy;
}

std::function<float(float value, bool fuelAsVolume)> AircraftPerfDialog::fuelUnitsFromDialogFunc(FuelUnit unit)
{
  switch(unit)
  {
    case WEIGHT_LBS:
    case VOLUME_GAL:
      return Unit::fromCopy;

    case WEIGHT_KG:
    case VOLUME_LITER:
      return Unit::fromMetricToUs;
  }
  return Unit::fromCopy;
}
