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

#include "connect/connectdialog.h"

#include "common/constants.h"
#include "gui/widgetstate.h"
#include "gui/helphandler.h"
#include "settings/settings.h"
#include "ui_connectdialog.h"
#include "fs/sc/datareaderthread.h"
#include "util/htmlbuilder.h"
#include "navapp.h"

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
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

#ifdef Q_OS_WIN32
  if(!simConnect)
  {
    // Disable the FSX and P3D controls if no simconnect
    ui->tabConnectFsx->setDisabled(true);
    ui->labelConnectFsx->setText(atools::util::HtmlBuilder::warningMessage(
                                   tr("SimConnect not found. Your <i>Little Navmap</i> "
                                      "installation is missing the file \"SimConnect.dll\".<br/>"
                                      "Reinstall <i>Little Navmap</i> or "
                                      "install a FSX SP2 compatible version of SimConnect on your computer.<br/>")) +
                                 ui->labelConnectFsx->text());
  }
#else
  // Remove FSX/P3D tab for non Windows systems
  int idx = ui->tabWidgetConnect->indexOf(ui->tabConnectFsx);
  if(idx != -1)
    ui->tabWidgetConnect->removeTab(idx);
#endif

  ui->comboBoxConnectHostname->setAutoCompletion(true);
  ui->comboBoxConnectHostname->setAutoCompletionCaseSensitivity(Qt::CaseInsensitive);

  // Change button texts
  QPushButton *okButton = ui->buttonBoxConnect->button(QDialogButtonBox::Ok);
  okButton->setText(tr("&Connect"));
  okButton->setToolTip(tr("Connect to a local or remote simulator.\n"
                          "Will retry to connect if \"Connect automatically\" "
                          "is checked."));
  okButton->setDefault(true);

  QPushButton *resetButton = ui->buttonBoxConnect->button(QDialogButtonBox::Reset);
  resetButton->setText(tr("&Disconnect"));
  resetButton->setToolTip(tr("Disconnect from a local or remote simulator and stop all reconnect attempts."));

  ui->buttonBoxConnect->button(QDialogButtonBox::Close)->
  setToolTip(tr("Close the dialog without changing the current connection status."));

  // Get a signal for any button
  connect(ui->buttonBoxConnect, &QDialogButtonBox::clicked, this, &ConnectDialog::buttonBoxClicked);
  connect(ui->checkBoxConnectOnStartup, &QRadioButton::toggled, this, &ConnectDialog::autoConnectToggled);
  connect(ui->checkBoxConnectOnStartup, &QRadioButton::toggled, this, &ConnectDialog::updateButtonStates);
  connect(ui->pushButtonConnectDeleteHostname, &QPushButton::clicked, this, &ConnectDialog::deleteClicked);

  connect(ui->checkBoxConnectFetchAiAircraftXp, &QCheckBox::toggled, this, &ConnectDialog::fetchOptionsClicked);
  connect(ui->checkBoxConnectFetchAiAircraftFsx, &QCheckBox::toggled, this, &ConnectDialog::fetchOptionsClicked);
  connect(ui->checkBoxConnectFetchAiShipFsx, &QCheckBox::toggled, this, &ConnectDialog::fetchOptionsClicked);
  connect(ui->checkBoxConnectFetchAiShipXp, &QCheckBox::toggled, this, &ConnectDialog::fetchOptionsClicked);

  connect(ui->tabWidgetConnect, &QTabWidget::currentChanged, this, &ConnectDialog::updateButtonStates);

  connect(ui->comboBoxConnectHostname, &QComboBox::editTextChanged, this, &ConnectDialog::updateButtonStates);

  connect(ui->spinBoxConnectUpdateRateFsx, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &ConnectDialog::directUpdateRateClicked);
  connect(ui->spinBoxConnectUpdateRateXp, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
          this, &ConnectDialog::directUpdateRateClicked);
}

ConnectDialog::~ConnectDialog()
{
  atools::gui::WidgetState widgetState(lnm::NAVCONNECT_REMOTE);
  widgetState.save(this);

  delete ui;
}

/* A button box button was clicked */
void ConnectDialog::buttonBoxClicked(QAbstractButton *button)
{
  qDebug() << Q_FUNC_INFO << "host" << ui->comboBoxConnectHostname->currentText();

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
    HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + "CONNECT.html", lnm::helpLanguageOnline());
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
  QPushButton *okButton = ui->buttonBoxConnect->button(QDialogButtonBox::Ok);
  if(getCurrentSimType() == cd::REMOTE)
    // Disable connect if hostname is empty
    okButton->setDisabled(ui->comboBoxConnectHostname->currentText().isEmpty());
  else
    // Disable connect button for disabled tabs (no SimConnect in windows)
    okButton->setEnabled(ui->tabWidgetConnect->currentWidget()->isEnabled());
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

bool ConnectDialog::isAnyConnectDirect() const
{
  return getCurrentSimType() == cd::FSX_P3D_MSFS || getCurrentSimType() == cd::XPLANE;
}

bool ConnectDialog::isFetchAiAircraft(cd::ConnectSimType type) const
{
  if(type == cd::FSX_P3D_MSFS)
    return ui->checkBoxConnectFetchAiAircraftFsx->isChecked();
  else if(type == cd::XPLANE)
    return ui->checkBoxConnectFetchAiAircraftXp->isChecked();

  // Not relevant for remote connections since Little Navconnect decides
  return true;
}

bool ConnectDialog::isFetchAiShip(cd::ConnectSimType type) const
{
  if(type == cd::FSX_P3D_MSFS)
    return ui->checkBoxConnectFetchAiShipFsx->isChecked();
  else if(type == cd::XPLANE)
    return ui->checkBoxConnectFetchAiShipXp->isChecked();

  // Not relevant for remote connections since Little Navconnect decides
  return true;
}

cd::ConnectSimType ConnectDialog::getCurrentSimType() const
{
  const QWidget *widget = ui->tabWidgetConnect->currentWidget();
  if(widget == ui->tabConnectXp)
    return cd::XPLANE;
  else if(widget == ui->tabConnectFsx)
    return cd::FSX_P3D_MSFS;
  else if(widget == ui->tabConnectRemote)
    return cd::REMOTE;
  else
    return cd::UNKNOWN;
}

unsigned int ConnectDialog::getDirectUpdateRateMs(cd::ConnectSimType type)
{
  if(type == cd::FSX_P3D_MSFS)
    return static_cast<unsigned int>(ui->spinBoxConnectUpdateRateFsx->value());
  else if(type == cd::XPLANE)
    return static_cast<unsigned int>(ui->spinBoxConnectUpdateRateXp->value());

  return 500;
}

void ConnectDialog::directUpdateRateClicked()
{
  QSpinBox *spinBox = dynamic_cast<QSpinBox *>(sender());

  if(spinBox == ui->spinBoxConnectUpdateRateFsx)
    emit directUpdateRateChanged(cd::FSX_P3D_MSFS);
  else if(spinBox == ui->spinBoxConnectUpdateRateXp)
    emit directUpdateRateChanged(cd::XPLANE);
}

void ConnectDialog::fetchOptionsClicked()
{
  QCheckBox *checkBox = dynamic_cast<QCheckBox *>(sender());

  if(checkBox == ui->checkBoxConnectFetchAiAircraftFsx || checkBox == ui->checkBoxConnectFetchAiShipFsx)
    emit fetchOptionsChanged(cd::FSX_P3D_MSFS);
  else if(checkBox == ui->checkBoxConnectFetchAiAircraftXp || checkBox == ui->checkBoxConnectFetchAiShipXp)
    emit fetchOptionsChanged(cd::XPLANE);
}

QString ConnectDialog::getRemoteHostname() const
{
  return ui->comboBoxConnectHostname->currentText();
}

quint16 ConnectDialog::getRemotePort() const
{
  return static_cast<quint16>(ui->spinBoxConnectPort->value());
}

void ConnectDialog::saveState()
{
  atools::gui::WidgetState widgetState(lnm::NAVCONNECT_REMOTE);
  widgetState.save({this, ui->comboBoxConnectHostname, ui->spinBoxConnectPort, ui->spinBoxConnectUpdateRateFsx,
                    ui->spinBoxConnectUpdateRateXp, ui->checkBoxConnectOnStartup, ui->tabWidgetConnect,
                    ui->checkBoxConnectFetchAiAircraftXp, ui->checkBoxConnectFetchAiAircraftFsx,
                    ui->checkBoxConnectFetchAiShipFsx, ui->checkBoxConnectFetchAiShipXp});

  // Save combo entries separately
  QStringList entries;
  for(int i = 0; i < ui->comboBoxConnectHostname->count(); i++)
    entries.append(ui->comboBoxConnectHostname->itemText(i));

  Settings::instance().setValue(lnm::NAVCONNECT_REMOTEHOSTS, entries);
}

void ConnectDialog::activateTab(QWidget *tabWidget)
{
  int idx = ui->tabWidgetConnect->indexOf(tabWidget);
  if(idx != -1)
    ui->tabWidgetConnect->setCurrentIndex(idx);
}

void ConnectDialog::restoreState()
{
  QStringList entries = Settings::instance().valueStrList(lnm::NAVCONNECT_REMOTEHOSTS);
  entries.removeDuplicates();

  for(const QString& entry : entries)
    if(!entry.isEmpty())
      ui->comboBoxConnectHostname->addItem(entry);

  atools::gui::WidgetState widgetState(lnm::NAVCONNECT_REMOTE);
  if(!widgetState.contains(ui->tabWidgetConnect))
  {
    // Tab state not saved - set reasonable initial value
#ifdef Q_OS_WIN32
    if(NavApp::hasAnyMsSimulator())
      // Activate FSX/P3D tab for Windows systems if any MS simulator was found
      activateTab(ui->tabConnectFsx);
    else
#endif
    if(NavApp::hasXplaneSimulator())
      // X - Plane
      activateTab(ui->tabConnectXp);
    else
      activateTab(ui->tabConnectRemote);
  }

  widgetState.restore({this, ui->comboBoxConnectHostname, ui->spinBoxConnectPort, ui->spinBoxConnectUpdateRateFsx,
                       ui->spinBoxConnectUpdateRateXp, ui->checkBoxConnectOnStartup, ui->tabWidgetConnect,
                       ui->checkBoxConnectFetchAiAircraftXp, ui->checkBoxConnectFetchAiAircraftFsx,
                       ui->checkBoxConnectFetchAiShipFsx, ui->checkBoxConnectFetchAiShipXp});

  updateButtonStates();
}
