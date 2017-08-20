/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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
#include "gui/helphandler.h"
#include "settings/settings.h"
#include "ui_connectdialog.h"
#include "fs/sc/datareaderthread.h"

#include <QDebug>
#include <QUrl>
#include <QPushButton>
#include <QRadioButton>

using atools::settings::Settings;
using atools::gui::HelpHandler;

ConnectDialog::ConnectDialog(QWidget *parent, bool simConnectAvailable)
  : QDialog(parent), ui(new Ui::ConnectDialog), simConnect(simConnectAvailable)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  ui->setupUi(this);

  if(!simConnect)
  {
    ui->checkBoxConnectFetchAiAircraftFsx->hide();
    ui->checkBoxConnectFetchAiShipFsx->hide();
    ui->radioButtonConnectDirectFsx->hide();
    ui->spinBoxConnectUpdateRateFsx->hide();
    ui->labelConnectUpdateRateFsx->hide();

#if !defined(Q_OS_WIN32)
    // Show the warning label only in Windows
    ui->labelConnectHeader->hide();
#endif
  }
  else
    ui->labelConnectHeader->hide();

  ui->comboBoxConnectHostname->setAutoCompletion(true);
  ui->comboBoxConnectHostname->setAutoCompletionCaseSensitivity(Qt::CaseInsensitive);

  // Change button texts
  ui->buttonBoxConnect->button(QDialogButtonBox::Ok)->setText(tr("&Connect"));
  ui->buttonBoxConnect->button(QDialogButtonBox::Ok)->
  setToolTip(tr("Try to connect to a local or remote simulator.\n"
                "Will retry to connect if \"Connect automatically\" "
                "is checked."));

  ui->buttonBoxConnect->button(QDialogButtonBox::Reset)->setText(tr("&Disconnect"));
  ui->buttonBoxConnect->button(QDialogButtonBox::Reset)->
  setToolTip(tr("Disconnect from a local or remote simulator and stop all reconnect attempts."));

  ui->buttonBoxConnect->button(QDialogButtonBox::Close)->
  setToolTip(tr("Close the dialog without changing the current connection status."));

  // Get a signal for any button
  connect(ui->buttonBoxConnect, &QDialogButtonBox::clicked, this, &ConnectDialog::buttonBoxClicked);
  connect(ui->checkBoxConnectOnStartup, &QRadioButton::toggled, this, &ConnectDialog::autoConnectToggled);
  connect(ui->checkBoxConnectOnStartup, &QRadioButton::toggled, this, &ConnectDialog::updateButtonStates);
  connect(ui->pushButtonConnectDeleteHostname, &QPushButton::clicked, this, &ConnectDialog::deleteClicked);

  connect(ui->checkBoxConnectFetchAiAircraftXp, &QCheckBox::toggled, this, &ConnectDialog::fetchOptionsChanged);
  connect(ui->checkBoxConnectFetchAiAircraftFsx, &QCheckBox::toggled, this, &ConnectDialog::fetchOptionsChanged);
  connect(ui->checkBoxConnectFetchAiShipFsx, &QCheckBox::toggled, this, &ConnectDialog::fetchOptionsChanged);

  connect(ui->radioButtonConnectDirectFsx, &QRadioButton::toggled, this, &ConnectDialog::updateButtonStates);
  connect(ui->radioButtonConnectDirectXp, &QRadioButton::toggled, this, &ConnectDialog::updateButtonStates);

  connect(ui->comboBoxConnectHostname, &QComboBox::editTextChanged, this, &ConnectDialog::updateButtonStates);

  connect(ui->spinBoxConnectUpdateRateFsx, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &ConnectDialog::directUpdateRateChanged);
  connect(ui->spinBoxConnectUpdateRateXp, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &ConnectDialog::directUpdateRateChanged);
}

ConnectDialog::~ConnectDialog()
{
  delete ui;
}

/* A button box button was clicked */
void ConnectDialog::buttonBoxClicked(QAbstractButton *button)
{
  qDebug() << "host" << ui->comboBoxConnectHostname->currentText();

  // qDebug() << "host cur index" << ui->comboBoxConnectHostname->currentIndex();
  // for(int i = 0; i < ui->comboBoxConnectHostname->count(); i++)
  // qDebug() << "host list" << ui->comboBoxConnectHostname->itemText(i);

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
  else if(button == ui->buttonBoxConnect->button(QDialogButtonBox::Help))
    HelpHandler::openHelpUrl(this, lnm::HELP_ONLINE_URL + "CONNECT.html", lnm::helpLanguages());
  else if(button == ui->buttonBoxConnect->button(QDialogButtonBox::Close))
    QDialog::reject();
}

void ConnectDialog::deleteClicked()
{
  ui->comboBoxConnectHostname->removeItem(ui->comboBoxConnectHostname->currentIndex());
  updateButtonStates();
}

void ConnectDialog::updateButtonStates()
{
  if(!simConnect && ui->radioButtonConnectDirectFsx->isChecked())
  {
    ui->radioButtonConnectRemote->setChecked(true);
    ui->radioButtonConnectDirectFsx->setChecked(false);
    ui->radioButtonConnectDirectXp->setChecked(false);
  }

  ui->pushButtonConnectDeleteHostname->setEnabled(
    ui->comboBoxConnectHostname->count() > 0 && ui->radioButtonConnectRemote->isChecked());

  ui->buttonBoxConnect->button(QDialogButtonBox::Ok)->setEnabled(
    !ui->comboBoxConnectHostname->currentText().isEmpty() ||
    ui->radioButtonConnectDirectFsx->isChecked() ||
    ui->radioButtonConnectDirectXp->isChecked());

  ui->comboBoxConnectHostname->setEnabled(ui->radioButtonConnectRemote->isChecked());
  ui->spinBoxConnectPort->setEnabled(ui->radioButtonConnectRemote->isChecked());

  ui->spinBoxConnectUpdateRateFsx->setEnabled(ui->radioButtonConnectDirectFsx->isChecked());
  ui->checkBoxConnectFetchAiAircraftFsx->setEnabled(ui->radioButtonConnectDirectFsx->isChecked());
  ui->checkBoxConnectFetchAiShipFsx->setEnabled(ui->radioButtonConnectDirectFsx->isChecked());

  ui->spinBoxConnectUpdateRateXp->setEnabled(ui->radioButtonConnectDirectXp->isChecked());
  ui->checkBoxConnectFetchAiAircraftXp->setEnabled(ui->radioButtonConnectDirectXp->isChecked());
}

void ConnectDialog::setConnected(bool connected)
{
  ui->buttonBoxConnect->button(QDialogButtonBox::Reset)->setEnabled(connected);
  updateButtonStates();
}

bool ConnectDialog::isAutoConnect() const
{
  return ui->checkBoxConnectOnStartup->isChecked();
}

bool ConnectDialog::isConnectDirect() const
{
  return ui->radioButtonConnectDirectFsx->isChecked() || ui->radioButtonConnectDirectXp->isChecked();
}

bool ConnectDialog::isFetchAiAircraft() const
{
  if(ui->radioButtonConnectDirectFsx->isChecked())
    return ui->checkBoxConnectFetchAiAircraftFsx->isChecked();
  else if(ui->radioButtonConnectDirectXp->isChecked())
    return ui->checkBoxConnectFetchAiAircraftFsx->isChecked();

  return false;
}

bool ConnectDialog::isFetchAiShip() const
{
  if(ui->radioButtonConnectDirectFsx->isChecked())
    return ui->checkBoxConnectFetchAiShipFsx->isChecked();

  return false;
}

bool ConnectDialog::isDirectFsx() const
{
  return ui->radioButtonConnectDirectFsx->isChecked();
}

bool ConnectDialog::isDirectXplane() const
{
  return ui->radioButtonConnectDirectXp->isChecked();
}

unsigned int ConnectDialog::getDirectUpdateRateMs()
{
  if(ui->radioButtonConnectDirectFsx->isChecked())
    return static_cast<unsigned int>(ui->spinBoxConnectUpdateRateFsx->value());
  else if(ui->radioButtonConnectDirectXp->isChecked())
    return static_cast<unsigned int>(ui->spinBoxConnectUpdateRateXp->value());

  return 500;
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
  widgetState.save({this, ui->comboBoxConnectHostname, ui->spinBoxConnectPort,
                    ui->spinBoxConnectUpdateRateFsx, ui->spinBoxConnectUpdateRateXp,
                    ui->checkBoxConnectOnStartup,
                    ui->radioButtonConnectRemote,
                    ui->radioButtonConnectDirectFsx, ui->radioButtonConnectDirectXp,
                    ui->checkBoxConnectFetchAiAircraftXp, ui->checkBoxConnectFetchAiAircraftFsx,
                    ui->checkBoxConnectFetchAiShipFsx});

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

  for(const QString& entry : entries)
    if(!entry.isEmpty())
      ui->comboBoxConnectHostname->addItem(entry);

  atools::gui::WidgetState widgetState(lnm::NAVCONNECT_REMOTE);

  widgetState.restore({this, ui->comboBoxConnectHostname, ui->spinBoxConnectPort,
                       ui->spinBoxConnectUpdateRateFsx, ui->spinBoxConnectUpdateRateXp,
                       ui->checkBoxConnectOnStartup,
                       ui->radioButtonConnectRemote,
                       ui->radioButtonConnectDirectFsx, ui->radioButtonConnectDirectXp,
                       ui->checkBoxConnectFetchAiAircraftXp, ui->checkBoxConnectFetchAiAircraftFsx,
                       ui->checkBoxConnectFetchAiShipFsx});

  if(!simConnect && ui->comboBoxConnectHostname->currentText().isEmpty())
    // Disable autoconnect if no host is given and this is not a windows client
    ui->checkBoxConnectOnStartup->setChecked(false);

  updateButtonStates();
}
