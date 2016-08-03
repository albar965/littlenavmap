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

#include "db/databasedialog.h"

#include "common/constants.h"
#include "db/databasemanager.h"
#include "fs/fspaths.h"
#include "gui/dialog.h"
#include "ui_databasedialog.h"

#include <QDebug>
#include <QDialog>

using atools::fs::FsPaths;

DatabaseDialog::DatabaseDialog(QWidget *parent, const SimulatorTypeMap& pathMap)
  : QDialog(parent), ui(new Ui::DatabaseDialog), paths(pathMap)
{
  ui->setupUi(this);

  ui->buttonBoxDatabase->button(QDialogButtonBox::Ok)->setText(tr("&Load"));

  bool hasInstalled = !paths.getAllInstalled().isEmpty();

  // Add an item to the combo box for each installed simulator
  for(atools::fs::FsPaths::SimulatorType type : paths.getAllInstalled())
    ui->comboBoxSimulator->addItem(FsPaths::typeToName(type),
                                   QVariant::fromValue<atools::fs::FsPaths::SimulatorType>(type));

  if(paths.isEmpty())
    ui->labelDatabaseInformation->setText(tr("<b>No Simulator Found and no database found.</b>"));

  // Disable everything if no installed simulators are found
  // (normally not needed since the action is already disabled)
  ui->comboBoxSimulator->setEnabled(hasInstalled);
  ui->pushButtonDatabaseBasePath->setEnabled(hasInstalled);
  ui->pushButtonDatabaseSceneryFile->setEnabled(hasInstalled);
  ui->pushButtonDatabaseResetPaths->setEnabled(hasInstalled);
  ui->lineEditDatabaseBasePath->setEnabled(hasInstalled);
  ui->lineEditDatabaseSceneryFile->setEnabled(hasInstalled);
  ui->buttonBoxDatabase->button(QDialogButtonBox::Ok)->setEnabled(hasInstalled);

  connect(ui->buttonBoxDatabase, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(ui->buttonBoxDatabase, &QDialogButtonBox::rejected, this, &QDialog::reject);

  connect(ui->comboBoxSimulator, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &DatabaseDialog::simComboChanged);
  connect(ui->pushButtonDatabaseBasePath, &QPushButton::clicked,
          this, &DatabaseDialog::selectBasePathClicked);
  connect(ui->pushButtonDatabaseSceneryFile, &QPushButton::clicked,
          this, &DatabaseDialog::selectSceneryConfigClicked);
  connect(ui->pushButtonDatabaseResetPaths, &QPushButton::clicked,
          this, &DatabaseDialog::resetPathsClicked);

  connect(ui->lineEditDatabaseBasePath, &QLineEdit::textEdited,
          this, &DatabaseDialog::basePathEdited);

  connect(ui->lineEditDatabaseSceneryFile, &QLineEdit::textEdited,
          this, &DatabaseDialog::sceneryConfigFileEdited);
}

DatabaseDialog::~DatabaseDialog()
{
  delete ui;
}

void DatabaseDialog::basePathEdited(const QString& text)
{
  paths[currentFsType].basePath = text;
}

void DatabaseDialog::sceneryConfigFileEdited(const QString& text)
{
  paths[currentFsType].sceneryCfg = text;
}

/* Reset paths of the current simulator back to default */
void DatabaseDialog::resetPathsClicked()
{
  paths[currentFsType].basePath = FsPaths::getBasePath(currentFsType);
  paths[currentFsType].sceneryCfg = FsPaths::getSceneryLibraryPath(currentFsType);
  updateLineEdits();
}

void DatabaseDialog::simComboChanged(int index)
{
  // Get paths for the selected simulators
  currentFsType = ui->comboBoxSimulator->itemData(index).value<atools::fs::FsPaths::SimulatorType>();

  qDebug() << currentFsType << "index" << index;

  updateLineEdits();

  emit simulatorChanged(currentFsType);
}

void DatabaseDialog::selectBasePathClicked()
{
  QString path = atools::gui::Dialog(this).openDirectoryDialog(
    tr("Select Flight Simulator Basepath"), lnm::DATABASE_BASEPATH, ui->lineEditDatabaseBasePath->text());

  if(!path.isEmpty())
  {
    paths[currentFsType].basePath = path;
    updateLineEdits();
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
    paths[currentFsType].sceneryCfg = path;
    updateLineEdits();
  }
}

void DatabaseDialog::setHeader(const QString& header)
{
  ui->labelDatabaseInformation->setText(header);
}

QString DatabaseDialog::getBasePath() const
{
  return paths.value(currentFsType).basePath;
}

QString DatabaseDialog::getSceneryConfigFile() const
{
  return paths.value(currentFsType).sceneryCfg;
}

void DatabaseDialog::setCurrentFsType(atools::fs::FsPaths::SimulatorType value)
{
  currentFsType = value;
  updateComboBox();
  updateLineEdits();
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

void DatabaseDialog::updateLineEdits()
{
  ui->lineEditDatabaseSceneryFile->blockSignals(true);
  ui->lineEditDatabaseSceneryFile->setText(paths.value(currentFsType).sceneryCfg);
  ui->lineEditDatabaseSceneryFile->blockSignals(false);

  ui->lineEditDatabaseBasePath->blockSignals(true);
  ui->lineEditDatabaseBasePath->setText(paths.value(currentFsType).basePath);
  ui->lineEditDatabaseBasePath->blockSignals(false);
}
