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

#include "connect/connectdialog.h"

#include "common/constants.h"
#include "gui/widgetstate.h"
#include "settings/settings.h"
#include "ui_connectdialog.h"

#include <QDebug>
#include <QPushButton>

using atools::settings::Settings;

ConnectDialog::ConnectDialog(QWidget *parent) :
  QDialog(parent), ui(new Ui::ConnectDialog)
{
  ui->setupUi(this);

  ui->comboBoxConnectHostname->setAutoCompletion(true);
  ui->comboBoxConnectHostname->setAutoCompletionCaseSensitivity(Qt::CaseInsensitive);

  // Change button texts
  ui->buttonBoxConnect->button(QDialogButtonBox::Ok)->setText(tr("&Connect"));
  ui->buttonBoxConnect->button(QDialogButtonBox::Reset)->setText(tr("&Disconnect"));

  // Get a signal for any button
  connect(ui->buttonBoxConnect, &QDialogButtonBox::clicked, this, &ConnectDialog::buttonBoxClicked);
}

ConnectDialog::~ConnectDialog()
{
  delete ui;
}

/* A button box button was clicked */
void ConnectDialog::buttonBoxClicked(QAbstractButton *button)
{
  qDebug() << "host" << ui->comboBoxConnectHostname->currentText();

  qDebug() << "host cur index" << ui->comboBoxConnectHostname->currentIndex();
  for(int i = 0; i < ui->comboBoxConnectHostname->count(); i++)
    qDebug() << "host list" << ui->comboBoxConnectHostname->itemText(i);

  if(button == ui->buttonBoxConnect->button(QDialogButtonBox::Ok))
  {
    bool foundEntryInComboList = false;
    int cnt = ui->comboBoxConnectHostname->count();
    QString curtxt = ui->comboBoxConnectHostname->currentText();

    // Check if the current text from the edit is already in the combo box history
    for(int i = 0; i < cnt; i++)
    {
      QString itemtxt = ui->comboBoxConnectHostname->itemText(i);
      if(itemtxt.compare(curtxt, Qt::CaseInsensitive) == 0)
      {
        foundEntryInComboList = true;
        break;
      }
    }

    if(!foundEntryInComboList)
      // Add to combo box
      ui->comboBoxConnectHostname->addItem(ui->comboBoxConnectHostname->currentText());

    QDialog::accept();
  }
  else if(button == ui->buttonBoxConnect->button(QDialogButtonBox::Reset))
  {
    // Disconnect button clicked
    emit disconnectClicked();
  }
  else if(button == ui->buttonBoxConnect->button(QDialogButtonBox::Close))
    QDialog::reject();
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
  atools::gui::WidgetState widgetState(lnm::NAVCONNECT_REMOTE);
  widgetState.save({ui->comboBoxConnectHostname, ui->spinBoxConnectPort, ui->checkBoxConnectOnStartup});

  // Save combo entries separately
  QStringList entries;
  for(int i = 0; i < ui->comboBoxConnectHostname->count(); i++)
    entries.append(ui->comboBoxConnectHostname->itemText(i));

  Settings::instance().setValue(lnm::NAVCONNECT_REMOTEHOSTS, entries);
}

void ConnectDialog::restoreState()
{
  QStringList entries = Settings::instance().valueStrList(lnm::NAVCONNECT_REMOTEHOSTS);
  entries.removeDuplicates();

  if(entries.isEmpty())
    // Use localhost as default
    ui->comboBoxConnectHostname->addItem("localhost");
  else
  {
    for(const QString& entry : entries)
      if(!entry.isEmpty())
        ui->comboBoxConnectHostname->addItem(entry);
  }

  atools::gui::WidgetState(lnm::NAVCONNECT_REMOTE).restore({ui->comboBoxConnectHostname,
                                                            ui->spinBoxConnectPort,
                                                            ui->checkBoxConnectOnStartup});
}
