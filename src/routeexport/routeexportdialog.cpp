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

#include "routeexport/routeexportdialog.h"
#include "routeexport/routeexportdata.h"
#include "ui_routeexportdialog.h"
#include "common/constants.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"
#include "common/unitstringtool.h"

#include <QAbstractButton>
#include <QPushButton>

RouteExportDialog::RouteExportDialog(QWidget *parent, re::RouteExportType routeType)
  : QDialog(parent), ui(new Ui::RouteExportDialog), type(routeType)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);
  ui->setupUi(this);

  widgets.append(ui->checkBoxHeavy);
  widgets.append(ui->lineEditAircraftType);
  widgets.append(ui->lineEditAirline);
  widgets.append(ui->lineEditCallsign);
  widgets.append(ui->lineEditEquipment);
  widgets.append(ui->lineEditEquipmentPrefix);
  widgets.append(ui->lineEditEquipmentSuffix);
  widgets.append(ui->lineEditFlightType);
  widgets.append(ui->lineEditLivery);
  widgets.append(ui->spinBoxPassengers);
  widgets.append(ui->lineEditPilotInCommand);
  widgets.append(ui->lineEditRemarks);
  widgets.append(ui->lineEditTransponder);
  widgets.append(ui->lineEditVoiceType);
  widgets.append(ui->lineEditWakeCategory);
  widgets.append(ui->lineEditRemarks);

  exportDataSaved = new RouteExportData;
  exportData = new RouteExportData;

  switch(type)
  {
    case re::UNKNOWN:
      break;

    case re::VFP:
      ui->lineEditFlightType->setVisible(false);
      ui->labelFlightType->setVisible(false);
      ui->lineEditAircraftType->setVisible(false);
      ui->labelAircraftType->setVisible(false);
      ui->lineEditAirline->setVisible(false);
      ui->labelAirline->setVisible(false);
      ui->lineEditAlternate2->setVisible(false);
      ui->labelAlternate2->setVisible(false);
      ui->lineEditCallsign->setVisible(false);
      ui->labelCallsign->setVisible(false);
      ui->lineEditLivery->setVisible(false);
      ui->labelLivery->setVisible(false);
      ui->spinBoxPassengers->setVisible(false);
      ui->labelPassengers->setVisible(false);
      ui->lineEditTransponder->setVisible(false);
      ui->labelTransponder->setVisible(false);
      ui->lineEditWakeCategory->setVisible(false);
      ui->labelWakeCategory->setVisible(false);

      // <?xml version="1.0" encoding="utf-8"?>
      // <FlightPlan xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema"
      // FlightType="IFR"
      // Equipment="/L"
      // CruiseAltitude="23000"
      // CruiseSpeed="275"
      // DepartureAirport="KBZN"
      // DestinationAirport="KJAC"
      // AlternateAirport="KSLC"
      // Route="DCT 4529N11116W 4527N11114W 4524N11112W 4522N11110W DCT 4520N11110W 4519N11111W/N0276F220 4517N11113W 4516N11115W 4514N11115W/N0275F230 4509N11114W 4505N11113W 4504N11111W 4502N11107W 4500N11105W 4458N11104W 4452N11103W 4450N11105W/N0276F220 4449N11108W 4449N11111W 4449N11113W 4450N11116W 4451N11119W 4450N11119W/N0275F230 4448N11117W 4446N11116W DCT KWYS DCT 4440N11104W 4440N11059W 4439N11055W 4439N11052W 4435N11050W 4430N11050W 4428N11050W 4426N11044W 4427N11041W 4425N11035W 4429N11032W 4428N11031W 4429N11027W 4429N11025W 4432N11024W 4432N11022W 4432N11018W 4428N11017W 4424N11017W 4415N11027W/N0276F220 DCT 4409N11040W 4403N11043W DCT 4352N11039W DCT"
      // Remarks="PBN/D2 DOF/181102 REG/N012SB PER/B RMK/TCAS SIMBRIEF"
      // IsHeavy="false"
      // EquipmentPrefix=""
      // EquipmentSuffix="L"
      // DepartureTime="2035"
      // DepartureTimeAct="0"
      // EnrouteHours="0"
      // EnrouteMinutes="53"
      // FuelHours="2"
      // FuelMinutes="44"
      // VoiceType="Full" />

      break;

    case re::XIVAP:
      ui->checkBoxHeavy->setVisible(false);

      ui->lineEditVoiceType->setVisible(false);
      ui->labelVoiceType->setVisible(false);

      ui->timeEditDepartureActual->setVisible(false);
      ui->labelTimeDepartureActual->setVisible(false);

      ui->lineEditEquipmentPrefix->setVisible(false);
      ui->lineEditEquipmentSuffix->setVisible(false);
      ui->labelEquipmentPrefixSuffix->setVisible(false);

      ui->lineEditLivery->setVisible(false);
      ui->labelLivery->setVisible(false);
      break;

    case re::IVAP:
      ui->checkBoxHeavy->setVisible(false);

      ui->lineEditVoiceType->setVisible(false);
      ui->labelVoiceType->setVisible(false);

      ui->timeEditDepartureActual->setVisible(false);
      ui->labelTimeDepartureActual->setVisible(false);

      ui->lineEditEquipmentPrefix->setVisible(false);
      ui->lineEditEquipmentSuffix->setVisible(false);
      ui->labelEquipmentPrefixSuffix->setVisible(false);

      ui->lineEditLivery->setVisible(false);
      ui->labelLivery->setVisible(false);

      ui->lineEditPilotInCommand->setVisible(false);
      ui->labelPilotInCommand->setVisible(false);

      ui->lineEditAirline->setVisible(false);
      ui->labelAirline->setVisible(false);

      // [FLIGHTPLAN]
      // CALLSIGN=VPI333
      // PIC=NAME
      // FMCROUTE=
      // LIVERY=
      // AIRLINE=VPI
      // SPEEDTYPE=N
      // POB=83
      // ENDURANCE=0215
      // OTHER=X-IvAp CREW OF 2 PILOT VPI07/COPILOT VPI007
      // ALT2ICAO=
      // ALTICAO=LFMP
      // EET=0115
      // DESTICAO=LFBO
      // ROUTE=TINOT UY268 DIVKO UM731 FJR
      // LEVEL=330
      // LEVELTYPE=F
      // SPEED=300
      // DEPTIME=2110
      // DEPICAO=LFKJ
      // TRANSPONDER=S
      // EQUIPMENT=SDFGW
      // WAKECAT=M
      // ACTYPE=B733
      // NUMBER=1
      // FLIGHTTYPE=N
      // RULES=I

      break;
  }

  setWindowTitle(tr("%1 - Export for %2").arg(QApplication::applicationName()).arg(getRouteTypeAsDisplayString(type)));

  connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &RouteExportDialog::buttonBoxClicked);

  // Saves original texts and restores them on deletion
  units = new UnitStringTool();
  units->init({ui->spinBoxTrueAirspeed, ui->spinBoxCruiseAltitude});
}

RouteExportDialog::~RouteExportDialog()
{
  atools::gui::WidgetState(lnm::FLIGHTPLAN_ONLINE_EXPORT + getRouteTypeAsString(type)).save(this);

  delete units;
  delete ui;
  delete exportDataSaved;
  delete exportData;
}

void RouteExportDialog::buttonBoxClicked(QAbstractButton *button)
{
  QDialogButtonBox::StandardButton standardButton = ui->buttonBox->standardButton(button);

  if(standardButton == QDialogButtonBox::Ok)
  {
    saveState();
    dialogToData(*exportData);
    QDialog::accept();
  }
  else if(standardButton == QDialogButtonBox::Cancel)
  {
    QDialog::reject();
  }
  else if(standardButton == QDialogButtonBox::Help)
  {
    atools::gui::HelpHandler::openHelpUrlWeb(
      parentWidget(), lnm::helpOnlineUrl + "ROUTEEXPORT.html", lnm::helpLanguageOnline());
  }
  else if(standardButton == QDialogButtonBox::Reset)
  {
    *exportData = *exportDataSaved;
    dataToDialog(*exportData);
    restoreState();
  }
  // else if(standardButton == QDialogButtonBox::RestoreDefaults)
  // {
  // *exportData = *exportDataSaved;
  // dataToDialog(*exportData);
  // clearDialog();
  // }
}

void RouteExportDialog::dataToDialog(const RouteExportData& data)
{
  ui->lineEditPilotInCommand->setText(data.getPilotInCommand());
  ui->lineEditFlightType->setText(data.getFlightType());
  ui->lineEditFlightRules->setText(data.getFlightRules());
  ui->lineEditCallsign->setText(data.getCallsign());
  ui->lineEditAirline->setText(data.getAirline());
  ui->lineEditLivery->setText(data.getLivery());
  ui->spinBoxPassengers->setValue(data.getPassengers());
  ui->lineEditAircraftType->setText(data.getAircraftType());
  ui->lineEditWakeCategory->setText(data.getWakeCategory());
  ui->checkBoxHeavy->setChecked(data.isHeavy());
  ui->lineEditEquipment->setText(data.getEquipment());
  ui->lineEditEquipmentPrefix->setText(data.getEquipmentPrefix());
  ui->lineEditEquipmentSuffix->setText(data.getEquipmentSuffix());
  ui->lineEditDeparture->setText(data.getDeparture());
  ui->lineEditDestination->setText(data.getDestination());
  ui->lineEditAlternate->setText(data.getAlternate());
  ui->lineEditAlternate2->setText(data.getAlternate2());
  ui->lineEditRoute->setText(data.getRoute());
  ui->spinBoxCruiseAltitude->setValue(data.getCruiseAltitude());
  ui->spinBoxTrueAirspeed->setValue(data.getSpeed());
  ui->timeEditDeparture->setTime(data.getDepartureTime());
  ui->timeEditDepartureActual->setTime(data.getDepartureTimeActual());
  ui->spinBoxEnrouteHour->setValue(data.getEnrouteMinutes() / 60);
  ui->spinBoxEnrouteMin->setValue(data.getEnrouteMinutes() - ui->spinBoxEnrouteHour->value() * 60);
  ui->spinBoxEnduranceHour->setValue(data.getEnduranceMinutes() / 60);
  ui->spinBoxEnduranceMin->setValue(data.getEnduranceMinutes() - ui->spinBoxEnduranceHour->value() * 60);
  ui->lineEditVoiceType->setText(data.getVoiceType());
  ui->lineEditTransponder->setText(data.getTransponder());
  ui->lineEditRemarks->setText(data.getRemarks());
}

void RouteExportDialog::dialogToData(RouteExportData& data)
{
  data.setPilotInCommand(ui->lineEditPilotInCommand->text());
  data.setFlightType(ui->lineEditFlightType->text());
  data.setFlightRules(ui->lineEditFlightRules->text());
  data.setCallsign(ui->lineEditCallsign->text());
  data.setAirline(ui->lineEditAirline->text());
  data.setLivery(ui->lineEditLivery->text());
  data.setPassengers(ui->spinBoxPassengers->value());
  data.setAircraftType(ui->lineEditAircraftType->text());
  data.setWakeCategory(ui->lineEditWakeCategory->text());
  data.setHeavy(ui->checkBoxHeavy->isChecked());
  data.setEquipment(ui->lineEditEquipment->text());
  data.setEquipmentPrefix(ui->lineEditEquipmentPrefix->text());
  data.setEquipmentSuffix(ui->lineEditEquipmentSuffix->text());
  data.setDeparture(ui->lineEditDeparture->text());
  data.setDestination(ui->lineEditDestination->text());
  data.setAlternate(ui->lineEditAlternate->text());
  data.setAlternate2(ui->lineEditAlternate2->text());
  data.setRoute(ui->lineEditRoute->text());
  data.setCruiseAltitude(ui->spinBoxCruiseAltitude->value());
  data.setSpeed(ui->spinBoxTrueAirspeed->value());
  data.setDepartureTime(ui->timeEditDeparture->time());
  data.setDepartureTimeActual(ui->timeEditDepartureActual->time());
  data.setEnrouteMinutes(ui->spinBoxEnrouteMin->value() + ui->spinBoxEnrouteHour->value() * 60);
  data.setEnduranceMinutes(ui->spinBoxEnduranceMin->value() + ui->spinBoxEnduranceHour->value() * 60);
  data.setVoiceType(ui->lineEditVoiceType->text());
  data.setTransponder(ui->lineEditTransponder->text());
  data.setRemarks(ui->lineEditRemarks->text());
}

QString RouteExportDialog::getRouteTypeAsString(re::RouteExportType routeType)
{
  switch(routeType)
  {
    case re::UNKNOWN:
      break;

    case re::VFP:
      return "Vfp";

    case re::IVAP:
      return "Ivap";

    case re::XIVAP:
      return "XIvap";

  }
  return QString();
}

QString RouteExportDialog::getRouteTypeAsDisplayString(re::RouteExportType routeType)
{
  switch(routeType)
  {
    case re::UNKNOWN:
      break;

    case re::VFP:
      return tr("vPilot");

    case re::IVAP:
      return tr("IvAp");

    case re::XIVAP:
      return tr("X-IvAp");

  }
  return QString();
}

RouteExportData RouteExportDialog::getExportData() const
{
  return *exportData;
}

void RouteExportDialog::setExportData(const RouteExportData& value)
{
  *exportData = value;
  *exportDataSaved = value;
  dataToDialog(*exportData);
  restoreState();
}

void RouteExportDialog::clearDialog()
{
  ui->checkBoxHeavy->setChecked(false);
  ui->lineEditAircraftType->clear();
  ui->lineEditAirline->clear();
  ui->lineEditCallsign->clear();
  ui->lineEditEquipment->clear();
  ui->lineEditEquipmentPrefix->clear();
  ui->lineEditEquipmentSuffix->clear();
  ui->lineEditFlightType->clear();
  ui->lineEditFlightRules->clear();
  ui->lineEditLivery->clear();
  ui->spinBoxPassengers->setValue(0);
  ui->lineEditPilotInCommand->clear();
  ui->lineEditRemarks->clear();
  ui->lineEditTransponder->clear();
  ui->lineEditVoiceType->clear();
  ui->lineEditWakeCategory->clear();
  ui->lineEditRemarks->clear();
}

void RouteExportDialog::restoreState()
{
  atools::gui::WidgetState ws(lnm::FLIGHTPLAN_ONLINE_EXPORT + getRouteTypeAsString(type));
  ws.restore(this);
  ws.restore(widgets);
}

void RouteExportDialog::saveState()
{
  atools::gui::WidgetState ws(lnm::FLIGHTPLAN_ONLINE_EXPORT + getRouteTypeAsString(type));
  ws.save(this);
  ws.save(widgets);
}
