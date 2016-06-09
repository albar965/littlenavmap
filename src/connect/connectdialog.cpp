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
#include "logging/loggingdefs.h"
#include "ui_connectdialog.h"
#include <QPushButton>
#include <gui/widgetstate.h>
#include <settings/settings.h>
#include <QSettings>

using atools::settings::Settings;

ConnectDialog::ConnectDialog(QWidget *parent) :
  QDialog(parent), ui(new Ui::ConnectDialog)
{
  ui->setupUi(this);

  ui->comboBoxConnectHostname->setAutoCompletion(true);
  ui->comboBoxConnectHostname->setAutoCompletionCaseSensitivity(Qt::CaseInsensitive);

  ui->buttonBoxConnect->button(QDialogButtonBox::Ok)->setText(tr("&Connect"));
  ui->buttonBoxConnect->button(QDialogButtonBox::Reset)->setText(tr("&Disconnect"));
  // ui->buttonBoxConnect->button(QDialogButtonBox::Close)->setText(tr("&Disconnect"));

  connect(ui->buttonBoxConnect, &QDialogButtonBox::clicked, this, &ConnectDialog::buttonClicked);
}

ConnectDialog::~ConnectDialog()
{
  delete ui;
}

void ConnectDialog::buttonClicked(QAbstractButton *button)
{
  qDebug() << "host" << ui->comboBoxConnectHostname->currentText();

  qDebug() << "host cur index" << ui->comboBoxConnectHostname->currentIndex();
  for(int i = 0; i < ui->comboBoxConnectHostname->count(); i++)
    qDebug() << "host list" << ui->comboBoxConnectHostname->itemText(i);

  if(button == ui->buttonBoxConnect->button(QDialogButtonBox::Ok))
  {
    disconnectClicked = false;

    bool found = false;
    int cnt = ui->comboBoxConnectHostname->count();
    QString ctxt = ui->comboBoxConnectHostname->currentText();
    for(int i = 0; i < cnt; i++)
    {
      QString itxt = ui->comboBoxConnectHostname->itemText(i);
      if(itxt.compare(ctxt, Qt::CaseInsensitive) == 0)
      {
        found = true;
        break;
      }
    }

    if(!found)
      ui->comboBoxConnectHostname->addItem(ui->comboBoxConnectHostname->currentText());

    QDialog::accept();
  }
  else if(button == ui->buttonBoxConnect->button(QDialogButtonBox::Reset))
  {
    disconnectClicked = true;
    QDialog::reject();
  }
  else if(button == ui->buttonBoxConnect->button(QDialogButtonBox::Close))
  {
    disconnectClicked = false;
    QDialog::reject();
  }
}

void ConnectDialog::setConnected(bool connected)
{
  ui->buttonBoxConnect->button(QDialogButtonBox::Reset)->setEnabled(connected);
}

bool ConnectDialog::isConnectOnStartup() const
{
  return ui->checkBoxConnectOnStartup->isChecked();
}

QString ConnectDialog::getHostname() const
{
  return ui->comboBoxConnectHostname->currentText();
}

quint16 ConnectDialog::getPort() const
{
  return static_cast<quint16>(ui->spinBoxConnectPort->value());
}

void ConnectDialog::saveState()
{
  atools::gui::WidgetState saver("NavConnect/Remote");
  saver.save({ui->comboBoxConnectHostname, ui->spinBoxConnectPort, ui->checkBoxConnectOnStartup});

  QStringList entries;
  for(int i = 0; i < ui->comboBoxConnectHostname->count(); i++)
    entries.append(ui->comboBoxConnectHostname->itemText(i));

  Settings::instance()->setValue("NavConnect/RemoteHosts", entries);
}

void ConnectDialog::restoreState()
{
  QStringList entries = Settings::instance()->value("NavConnect/RemoteHosts").toStringList();
  entries.removeDuplicates();

  if(entries.isEmpty())
    ui->comboBoxConnectHostname->addItem("localhost");
  else
  {
    for(const QString& entry : entries)
      if(!entry.isEmpty())
        ui->comboBoxConnectHostname->addItem(entry);
  }

  atools::gui::WidgetState saver("NavConnect/Remote");
  saver.restore({ui->comboBoxConnectHostname, ui->spinBoxConnectPort, ui->checkBoxConnectOnStartup});
}
