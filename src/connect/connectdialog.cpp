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
#include <gui/widgetstate.h>

ConnectDialog::ConnectDialog(QWidget *parent) :
  QDialog(parent), ui(new Ui::ConnectDialog)
{
  ui->setupUi(this);

  ui->buttonBoxConnect->button(QDialogButtonBox::Ok)->setText(tr("&Connect"));
  ui->buttonBoxConnect->button(QDialogButtonBox::Close)->setText(tr("&Disconnect"));

  connect(ui->buttonBoxConnect, &QDialogButtonBox::clicked, this, &ConnectDialog::buttonClicked);
}

ConnectDialog::~ConnectDialog()
{
  delete ui;
}

void ConnectDialog::buttonClicked(QAbstractButton *button)
{
  if(button == ui->buttonBoxConnect->button(QDialogButtonBox::Ok))
  {
    disconnectClicked = false;
    QDialog::accept();
  }
  else if(button == ui->buttonBoxConnect->button(QDialogButtonBox::Close))
  {
    disconnectClicked = true;
    QDialog::reject();
  }
  else if(button == ui->buttonBoxConnect->button(QDialogButtonBox::Cancel))
  {
    disconnectClicked = false;
    QDialog::reject();

  }
}

void ConnectDialog::setConnected(bool connected)
{
  ui->buttonBoxConnect->button(QDialogButtonBox::Close)->setEnabled(connected);
}

QString ConnectDialog::getHostname() const
{
  return ui->lineEditConnectHostname->text();
}

quint16 ConnectDialog::getPort() const
{
  return static_cast<quint16>(ui->spinBoxConnectPort->value());
}

void ConnectDialog::saveState()
{
  atools::gui::WidgetState saver("NavConnect/Remote");
  saver.save({ui->lineEditConnectHostname, ui->spinBoxConnectPort});

}

void ConnectDialog::restoreState()
{
  atools::gui::WidgetState saver("NavConnect/Remote");
  saver.restore({ui->lineEditConnectHostname, ui->spinBoxConnectPort});
}
