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

#include "userwaypointdialog.h"
#include "ui_userwaypointdialog.h"

#include <QRegularExpressionValidator>

UserWaypointDialog::UserWaypointDialog(QWidget *parent, const QString& name)
  : QDialog(parent), ui(new Ui::UserWaypointDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  ui->lineEditRouteUserWaypoint->setText(name);
}

UserWaypointDialog::~UserWaypointDialog()
{
  delete ui;
}

QString UserWaypointDialog::getName() const
{
  return ui->lineEditRouteUserWaypoint->text();
}
