/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#include "connectdialog.h"
#include "ui_connectdialog.h"
#include <QPushButton>

ConnectDialog::ConnectDialog(QWidget *parent) :
  QDialog(parent), ui(new Ui::ConnectDialog)
{
  ui->setupUi(this);

  ui->buttonBoxConnect->button(QDialogButtonBox::Ok)->setText(tr("&Connect"));

  connect(ui->buttonBoxConnect, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(ui->buttonBoxConnect, &QDialogButtonBox::rejected, this, &QDialog::reject);

}

ConnectDialog::~ConnectDialog()
{
  delete ui;
}

QString ConnectDialog::getHostname() const
{
  return ui->lineEditConnectHostname->text();
}

quint16 ConnectDialog::getPort() const
{
  return static_cast<quint16>(ui->spinBoxConnectPort->value());
}
