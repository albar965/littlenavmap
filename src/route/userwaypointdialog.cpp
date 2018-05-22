/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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

#include "route/userwaypointdialog.h"
#include "ui_userwaypointdialog.h"

#include "geo/pos.h"
#include "common/unit.h"
#include "common/constants.h"
#include "common/formatter.h"
#include "gui/helphandler.h"
#include "fs/util/coordinates.h"

#include <QPushButton>

UserWaypointDialog::UserWaypointDialog(QWidget *parent, const QString& name, const atools::geo::Pos& pos)
  : QDialog(parent), ui(new Ui::UserWaypointDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  ui->lineEditRouteUserWaypoint->setText(name);
  ui->lineEditRouteUserWaypointLatLon->setText(Unit::coords(pos));

  coordsEdited(QString());

  connect(ui->lineEditRouteUserWaypointLatLon, &QLineEdit::textChanged, this, &UserWaypointDialog::coordsEdited);
  connect(ui->buttonBoxRouteUserWaypoint->button(QDialogButtonBox::Help), &QPushButton::clicked,
          this, &UserWaypointDialog::helpClicked);
}

UserWaypointDialog::~UserWaypointDialog()
{
  delete ui;
}

void UserWaypointDialog::helpClicked()
{
  atools::gui::HelpHandler::openHelpUrl(
    this, lnm::HELP_ONLINE_URL + "EDITFPPOSITION.html", lnm::helpLanguageOnline());
}

QString UserWaypointDialog::getName() const
{
  return ui->lineEditRouteUserWaypoint->text();
}

atools::geo::Pos UserWaypointDialog::getPos() const
{
  return atools::fs::util::fromAnyFormat(ui->lineEditRouteUserWaypointLatLon->text());
}

void UserWaypointDialog::coordsEdited(const QString& text)
{
  Q_UNUSED(text);

  QString message;
  bool valid = formatter::checkCoordinates(message, ui->lineEditRouteUserWaypointLatLon->text());
  ui->buttonBoxRouteUserWaypoint->button(QDialogButtonBox::Ok)->setEnabled(valid);
  ui->labelRouteUserWaypointCoordStatus->setText(message);
}
