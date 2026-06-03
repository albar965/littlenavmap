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

#include "marker/holdingmarkerdialog.h"

#include "app/navapp.h"
#include "common/constants.h"
#include "common/mapmarkers.h"
#include "common/mapresult.h"
#include "common/maptypes.h"
#include "common/unit.h"
#include "common/unitstringtool.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"
#include "settings/settings.h"
#include "ui_holdingmarkerdialog.h"

#include <QColor>

const QString HoldingMarkerDialog::DEFAULT_COLOR_STR = QColor(Qt::darkBlue).name(QColor::HexArgb);

HoldingMarkerDialog::HoldingMarkerDialog(QWidget *parent, const map::HoldingMarker& markerParam, const map::MapResult& result, bool editMode)
  : MarkerDialog(parent, tr("Holding"), markerParam, result, editMode), ui(new Ui::HoldingMarkerDialog)
{
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setWindowModality(Qt::ApplicationModal);
  ui->setupUi(this);

  // Assign buttons for base class functions
  setPushButtonColor(ui->pushButtonHoldColor);
  setLabelHeader(ui->labelHoldNavaid);

  // Default is OK button
  ui->buttonBoxHold->button(QDialogButtonBox::Ok)->setDefault(true);
  ui->comboBoxHoldTurnDirection->setFocus();

  connect(ui->buttonBoxHold, &QDialogButtonBox::clicked, this, &HoldingMarkerDialog::buttonBoxClicked);
  connect(ui->pushButtonHoldColor, &QPushButton::clicked, this, &MarkerDialog::colorButtonClicked);
  connect(ui->spinBoxHoldSpeed, QOverload<int>::of(&QSpinBox::valueChanged), this, &HoldingMarkerDialog::updateLengthLabel);
  connect(ui->doubleSpinBoxHoldTime, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &HoldingMarkerDialog::updateLengthLabel);

  // Saves original texts and restores them on deletion
  units = new UnitStringTool();
  units->init({ui->spinBoxHoldSpeed, ui->spinBoxHoldAltitude});

  restoreState();
}

HoldingMarkerDialog::~HoldingMarkerDialog()
{
  // Save dialog position and size
  atools::gui::WidgetState(lnm::HOLDING_MARKER_DIALOG).save(this);

  delete units;
  delete ui;
}

/* A button box button was clicked */
void HoldingMarkerDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxHold->button(QDialogButtonBox::Ok))
  {
    saveState();
    QDialog::accept();
  }
  else if(button == ui->buttonBoxHold->button(QDialogButtonBox::Help))
    // Keep dialog open
    atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl + "HOLD.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxHold->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void HoldingMarkerDialog::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::HOLDING_MARKER_DIALOG);

  // Load dialog position and size
  widgetState.restore(this);

  if(isEditMode())
    markerToWidgets();
  else
  {
    widgetState.restore({ui->spinBoxHoldCourse, ui->spinBoxHoldSpeed, ui->spinBoxHoldAltitude, ui->doubleSpinBoxHoldTime,
                         ui->comboBoxHoldTurnDirection});
    marker->holding.color = atools::settings::Settings::instance().valueStr(lnm::HOLDING_MARKER_DIALOG_COLOR, DEFAULT_COLOR_STR);
    fillMarkerFromResultAndWidgets();
  }

  updateHeader();
  updateLengthLabel();
  updateButtonColor();
}

void HoldingMarkerDialog::saveState() const
{
  atools::gui::WidgetState widgetState(lnm::HOLDING_MARKER_DIALOG);
  widgetState.save({this, ui->spinBoxHoldCourse, ui->spinBoxHoldSpeed, ui->spinBoxHoldAltitude, ui->doubleSpinBoxHoldTime,
                    ui->comboBoxHoldTurnDirection});
  atools::settings::Settings::instance().setValue(lnm::HOLDING_MARKER_DIALOG_COLOR, marker->holding.color.name(QColor::HexArgb));
}

void HoldingMarkerDialog::updateLengthLabel()
{
  float minutes = static_cast<float>(ui->doubleSpinBoxHoldTime->value());
  float distance = static_cast<float>(Unit::rev(ui->spinBoxHoldSpeed->value(), Unit::speedKtsF) * minutes / 60.);

  ui->labelHoldLength->setText(tr("Straight section length %1.<br/>"
                                  "Total time to complete is %2 minutes.").
                               arg(Unit::distNm(distance)).
                               arg(QLocale().toString(minutes * 2.f + 2.f, 'f', 1)));
}

void HoldingMarkerDialog::fillMarkerFromResultAndWidgets()
{
  marker->text.clear();

  // Clear holding data
  map::MapHolding& holdingRef = marker->holding;
  holdingRef.color = marker->holding.color;
  holdingRef.id = -1;
  holdingRef.name.clear();
  holdingRef.speedLimit = holdingRef.length = holdingRef.minAltititude = holdingRef.maxAltititude = 0.f;
  holdingRef.airportIdent.clear();

  // Save altitude
  float altitude = marker->getAltitude();

  // Get navaid information from result
  atools::geo::Pos position;
  getResult()->getParams({map::AIRPORT, map::VOR, map::NDB, map::WAYPOINT, map::USERPOINT}, nullptr, &position, &holdingRef.nav);
  if(!(holdingRef.nav.magvar < map::INVALID_MAGVAR))
    holdingRef.nav.magvar = NavApp::getMagVar(position, 0.f);

  if(position.isValidRange())
    marker->position = holdingRef.position = position;

  // Assign saved altitude again
  marker->position.setAltitude(altitude);

  marker->text = getResult()->markerLabel();

  // Fall back to ident
  if(marker->text.isEmpty())
    marker->text = holdingRef.getIdent();

  widgetsToMarker();
}

void HoldingMarkerDialog::markerToWidgets()
{
  map::MapHolding& holding = marker->holding;
  ui->spinBoxHoldAltitude->setValue(Unit::altFeetF(holding.position.getAltitude()));
  ui->comboBoxHoldTurnDirection->setCurrentIndex(holding.turnLeft ? 1 : 0);
  ui->doubleSpinBoxHoldTime->setValue(holding.time);
  ui->spinBoxHoldSpeed->setValue(Unit::speedKtsF(holding.speedKts));
  ui->spinBoxHoldCourse->setValue(atools::geo::normalizeCourse(holding.courseTrue - holding.nav.magvar));
}

void HoldingMarkerDialog::widgetsToMarker()
{
  map::MapHolding& holding = marker->holding;
  marker->position.setAltitude(Unit::rev(ui->spinBoxHoldAltitude->value(), Unit::altFeetF));
  holding.position.setAltitude(marker->position.getAltitude());
  holding.turnLeft = ui->comboBoxHoldTurnDirection->currentIndex() == 1;
  holding.time = static_cast<float>(ui->doubleSpinBoxHoldTime->value());
  holding.speedKts = Unit::rev(ui->spinBoxHoldSpeed->value(), Unit::speedKtsF);
  holding.courseTrue = atools::geo::normalizeCourse(ui->spinBoxHoldCourse->value() + holding.nav.magvar);
}
