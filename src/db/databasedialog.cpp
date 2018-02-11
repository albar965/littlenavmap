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

#include "db/databasedialog.h"

#include "common/constants.h"
#include "db/databasemanager.h"
#include "fs/fspaths.h"
#include "gui/dialog.h"
#include "gui/helphandler.h"
#include "ui_databasedialog.h"

#include <QDebug>
#include <QDialog>
#include <QDir>
#include <QUrl>

using atools::fs::FsPaths;
using atools::gui::HelpHandler;

DatabaseDialog::DatabaseDialog(QWidget *parent, const SimulatorTypeMap& pathMap)
  : QDialog(parent), ui(new Ui::DatabaseDialog), simulators(pathMap)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  ui->buttonBoxDatabase->button(QDialogButtonBox::Ok)->setText(tr("&Load"));

  // Sort keys to avoid random order
  QList<FsPaths::SimulatorType> keys = simulators.getAllInstalled();

  // No registry key for X-Plane - add always
  if(!keys.contains(FsPaths::XPLANE11))
    keys.append(FsPaths::XPLANE11);

  std::sort(keys.begin(), keys.end());

  // Add an item to the combo box for each installed simulator
  for(atools::fs::FsPaths::SimulatorType type : keys)
  {
    if(simulators.value(type).isInstalled || type == FsPaths::XPLANE11)
      ui->comboBoxSimulator->addItem(FsPaths::typeToName(type),
                                     QVariant::fromValue<atools::fs::FsPaths::SimulatorType>(type));
  }

  if(simulators.isEmpty())
    ui->labelDatabaseInformation->setText(tr("<b>No Simulator Found and no database found.</b>"));

  connect(ui->buttonBoxDatabase, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(ui->buttonBoxDatabase, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(ui->buttonBoxDatabase, &QDialogButtonBox::helpRequested, this, &DatabaseDialog::helpClicked);

  connect(ui->comboBoxSimulator, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &DatabaseDialog::simComboChanged);
  connect(ui->pushButtonDatabaseBasePath, &QPushButton::clicked, this, &DatabaseDialog::selectBasePathClicked);
  connect(ui->pushButtonDatabaseSceneryFile, &QPushButton::clicked, this, &DatabaseDialog::selectSceneryConfigClicked);
  connect(ui->pushButtonDatabaseResetPaths, &QPushButton::clicked, this, &DatabaseDialog::resetPathsClicked);

  connect(ui->lineEditDatabaseBasePath, &QLineEdit::textEdited, this, &DatabaseDialog::basePathEdited);

  connect(ui->lineEditDatabaseSceneryFile, &QLineEdit::textEdited, this, &DatabaseDialog::sceneryConfigFileEdited);
}

DatabaseDialog::~DatabaseDialog()
{
  delete ui;
}

void DatabaseDialog::basePathEdited(const QString& text)
{
  simulators[currentFsType].basePath = QDir::toNativeSeparators(text);
}

void DatabaseDialog::sceneryConfigFileEdited(const QString& text)
{
  simulators[currentFsType].sceneryCfg = QDir::toNativeSeparators(text);
}

/* Show help in browser */
void DatabaseDialog::helpClicked()
{
  HelpHandler::openHelpUrl(this, lnm::HELP_ONLINE_URL + "SCENERY.html", lnm::helpLanguagesOnline());
}

/* Reset paths of the current simulator back to default */
void DatabaseDialog::resetPathsClicked()
{
  simulators[currentFsType].basePath = FsPaths::getBasePath(currentFsType);
  simulators[currentFsType].sceneryCfg = FsPaths::getSceneryLibraryPath(currentFsType);
  updateWidgets();
}

void DatabaseDialog::simComboChanged(int index)
{
  // Get paths for the selected simulators
  currentFsType = ui->comboBoxSimulator->itemData(index).value<atools::fs::FsPaths::SimulatorType>();

  qDebug() << currentFsType << "index" << index;

  updateWidgets();

  emit simulatorChanged(currentFsType);
}

void DatabaseDialog::selectBasePathClicked()
{
  QString path = atools::gui::Dialog(this).openDirectoryDialog(
    tr("Select Flight Simulator Basepath"), lnm::DATABASE_BASEPATH, ui->lineEditDatabaseBasePath->text());

  if(!path.isEmpty())
  {
    simulators[currentFsType].basePath = QDir::toNativeSeparators(path);
    updateWidgets();
  }
}

void DatabaseDialog::selectSceneryConfigClicked()
{
  QString path = atools::gui::Dialog(this).openFileDialog(
    tr("Open Scenery Configuration File"),
    tr("Scenery Configuration Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_SCENERYCONFIG),
    lnm::DATABASE_SCENERYCONFIG, ui->lineEditDatabaseSceneryFile->text());

  if(!path.isEmpty())
  {
    simulators[currentFsType].sceneryCfg = QDir::toNativeSeparators(path);
    updateWidgets();
  }
}

void DatabaseDialog::setHeader(const QString& header)
{
  ui->labelDatabaseInformation->setText(header);
}

QString DatabaseDialog::getBasePath() const
{
  return simulators.value(currentFsType).basePath;
}

bool DatabaseDialog::isReadInactive() const
{
  return ui->checkBoxReadInactive->isChecked();
}

void DatabaseDialog::setReadInactive(bool value)
{
  return ui->checkBoxReadInactive->setChecked(value);
}

bool DatabaseDialog::isReadAddOnXml() const
{
  return ui->checkBoxReadAddOnXml->isChecked();
}

void DatabaseDialog::setReadAddOnXml(bool value)
{
  return ui->checkBoxReadAddOnXml->setChecked(value);
}

QString DatabaseDialog::getSceneryConfigFile() const
{
  return simulators.value(currentFsType).sceneryCfg;
}

void DatabaseDialog::setCurrentFsType(atools::fs::FsPaths::SimulatorType value)
{
  if(value == FsPaths::UNKNOWN)
    currentFsType = FsPaths::XPLANE11;
  else
    currentFsType = value;
  updateComboBox();
  updateWidgets();
}

void DatabaseDialog::updateComboBox()
{
  for(int i = 0; i < ui->comboBoxSimulator->count(); i++)
  {
    if(ui->comboBoxSimulator->itemData(i) == currentFsType)
    {
      ui->comboBoxSimulator->blockSignals(true);
      ui->comboBoxSimulator->setCurrentIndex(i);
      ui->comboBoxSimulator->blockSignals(false);
      break;
    }
  }
}

void DatabaseDialog::updateWidgets()
{
  bool showXplane = currentFsType == atools::fs::FsPaths::XPLANE11 || currentFsType == atools::fs::FsPaths::UNKNOWN;

  ui->lineEditDatabaseSceneryFile->setDisabled(showXplane);
  ui->checkBoxReadInactive->setDisabled(showXplane);
  ui->labelDatabaseSceneryFile->setDisabled(showXplane);

  ui->lineEditDatabaseSceneryFile->blockSignals(true);
  ui->lineEditDatabaseSceneryFile->setText(simulators.value(currentFsType).sceneryCfg);
  ui->lineEditDatabaseSceneryFile->blockSignals(false);

  ui->lineEditDatabaseBasePath->blockSignals(true);
  ui->lineEditDatabaseBasePath->setText(simulators.value(currentFsType).basePath);
  ui->lineEditDatabaseBasePath->blockSignals(false);

  ui->checkBoxReadAddOnXml->setEnabled(currentFsType == atools::fs::FsPaths::P3D_V3 ||
                                       currentFsType == atools::fs::FsPaths::P3D_V4);

  // Disable everything if no installed simulators are found
  // (normally not needed since the action is already disabled)
  ui->pushButtonDatabaseSceneryFile->setEnabled(!showXplane);
  ui->lineEditDatabaseSceneryFile->setEnabled(!showXplane);
  ui->pushButtonDatabaseResetPaths->setEnabled(!showXplane);
}
