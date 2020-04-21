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

#include "gui/holddialog.h"

#include "ui_holddialog.h"

#include "common/constants.h"
#include "common/maptypes.h"
#include "common/formatter.h"
#include "common/unitstringtool.h"
#include "gui/helphandler.h"
#include "gui/widgetstate.h"
#include "gui/widgetutil.h"
#include "settings/settings.h"
#include "common/unit.h"
#include "geo/calculations.h"
#include "common/proctypes.h"

#include <QColor>
#include <QColorDialog>
#include <navapp.h>

HoldDialog::HoldDialog(QWidget *parent, const map::MapSearchResult& resultParam, const atools::geo::Pos& positionParam)
  : QDialog(parent), ui(new Ui::HoldDialog), color(Qt::darkBlue)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  // Copy result
  result = new map::MapSearchResult;
  *result = resultParam;

  // Remove duplicates
  result->clearAllButFirst();

  // Remove all except airport, VOR, NDB and waypoints
  result->clear(~(map::AIRPORT | map::VOR | map::NDB | map::WAYPOINT));

  position = new atools::geo::Pos;
  *position = positionParam;

  connect(ui->buttonBoxHold, &QDialogButtonBox::clicked, this, &HoldDialog::buttonBoxClicked);
  connect(ui->pushButtonHoldColor, &QPushButton::clicked, this, &HoldDialog::colorButtonClicked);
  connect(ui->spinBoxHoldSpeed, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &HoldDialog::updateLength);
  connect(ui->doubleSpinBoxHoldTime, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
          this, &HoldDialog::updateLength);

  // Saves original texts and restores them on deletion
  units = new UnitStringTool();
  units->init({ui->spinBoxHoldSpeed, ui->spinBoxHoldAltitude});

  restoreState();
}

HoldDialog::~HoldDialog()
{
  atools::gui::WidgetState(lnm::HOLD_DIALOG).save(this);

  delete units;
  delete ui;
  delete result;
  delete position;
}

/* A button box button was clicked */
void HoldDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxHold->button(QDialogButtonBox::Ok))
  {
    saveState();
    QDialog::accept();
  }
  else if(button == ui->buttonBoxHold->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(parentWidget(), lnm::helpOnlineUrl + "HOLD.html",
                                             lnm::helpLanguageOnline());
  else if(button == ui->buttonBoxHold->button(QDialogButtonBox::Cancel))
    QDialog::reject();
}

void HoldDialog::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::HOLD_DIALOG, false);
  widgetState.restore({this,
                       ui->spinBoxHoldCourse,
                       ui->spinBoxHoldSpeed,
                       ui->spinBoxHoldAltitude,
                       ui->doubleSpinBoxHoldTime,
                       ui->comboBoxHoldTurnDirection});
  color = atools::settings::Settings::instance().valueVar(lnm::HOLD_DIALOG_COLOR, color).value<QColor>();

  updateWidgets();
  updateLength();
  updateButtonColor();
}

void HoldDialog::saveState()
{
  atools::gui::WidgetState widgetState(lnm::HOLD_DIALOG, false);
  widgetState.save({this,
                    ui->spinBoxHoldCourse,
                    ui->spinBoxHoldSpeed,
                    ui->spinBoxHoldAltitude,
                    ui->doubleSpinBoxHoldTime,
                    ui->comboBoxHoldTurnDirection});
  atools::settings::Settings::instance().setValueVar(lnm::HOLD_DIALOG_COLOR, color);
}

void HoldDialog::colorButtonClicked()
{
  QColor col = QColorDialog::getColor(color, parentWidget());
  if(col.isValid())
  {
    color = col;
    updateButtonColor();
  }
}

void HoldDialog::updateButtonColor()
{
  atools::gui::util::changeWidgetColor(ui->pushButtonHoldColor, color);
}

void HoldDialog::updateLength()
{
  float speedKts = Unit::rev(ui->spinBoxHoldSpeed->value(), Unit::speedKtsF);
  float mins = static_cast<float>(ui->doubleSpinBoxHoldTime->value());
  float dist = static_cast<float>(speedKts * mins / 60.);

  ui->labelHoldLength->setText(tr("Straight section length %1.<br/>"
                                  "Total time to complete is %2 minutes.").
                               arg(Unit::distNm(dist)).
                               arg(QLocale().toString(mins * 2.f + 2.f, 'f', 1)));
}

void HoldDialog::updateWidgets()
{
  QString text;

  if(result->hasAirports())
    text = map::airportText(result->airports.first());
  else if(result->hasVor())
    text = map::vorText(result->vors.first());
  else if(result->hasNdb())
    text = map::ndbText(result->ndbs.first());
  else if(result->hasWaypoints())
    text = map::waypointText(result->waypoints.first());
  else
    text = Unit::coords(*position);

  ui->labelHoldNavaid->setText(tr("<p><b>%1</b></p>").arg(text));
}

void HoldDialog::fillHold(map::Hold& hold)
{
  hold.navIdent.clear();
  hold.position = *position;
  hold.navType = map::NONE;

  if(result->hasAirports())
  {
    hold.navIdent = result->airports.first().ident;
    hold.position = result->airports.first().position;
    hold.magvar = result->airports.first().magvar;
    hold.navType = map::AIRPORT;
  }
  else if(result->hasVor())
  {
    const map::MapVor& vor = result->vors.first();
    hold.navIdent = vor.ident;
    hold.position = vor.position;
    hold.magvar = vor.magvar;
    hold.vorDmeOnly = vor.dmeOnly;
    hold.vorHasDme = vor.hasDme;
    hold.vorTacan = vor.tacan;
    hold.vorVortac = vor.vortac;

    hold.navType = map::VOR;
  }
  else if(result->hasNdb())
  {
    hold.navIdent = result->ndbs.first().ident;
    hold.position = result->ndbs.first().position;
    hold.magvar = result->ndbs.first().magvar;
    hold.navType = map::NDB;
  }
  else if(result->hasWaypoints())
  {
    hold.navIdent = result->waypoints.first().ident;
    hold.position = result->waypoints.first().position;
    hold.magvar = result->waypoints.first().magvar;
    hold.navType = map::WAYPOINT;
  }
  else
    // Use calculated declination
    hold.magvar = NavApp::getMagVar(*position);

  hold.color = color;
  hold.position.setAltitude(Unit::rev(ui->spinBoxHoldAltitude->value(), Unit::altFeetF));
  hold.turnLeft = ui->comboBoxHoldTurnDirection->currentIndex() == 1;
  hold.minutes = static_cast<float>(ui->doubleSpinBoxHoldTime->value());
  hold.speedKts = Unit::rev(ui->spinBoxHoldSpeed->value(), Unit::speedKtsF);
  hold.courseTrue = atools::geo::normalizeCourse(ui->spinBoxHoldCourse->value() + hold.magvar);
}
