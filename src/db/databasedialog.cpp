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

DatabaseDialog::DatabaseDialog(QWidget *parent, const FsPathTypeMap& pathMap)
  : QDialog(parent), ui(new Ui::DatabaseDialog), paths(pathMap)
{
  ui->setupUi(this);

  ui->buttonBoxDatabase->button(QDialogButtonBox::Ok)->setText(tr("&Load"));

  // Add an action to the toolbutton for each simulator
  for(atools::fs::FsPaths::SimulatorType type : paths.getAllRegistryPaths())
    ui->comboBoxSimulator->addItem(FsPaths::typeToName(type),
                                   QVariant::fromValue<atools::fs::FsPaths::SimulatorType>(type));

  if(paths.isEmpty())
    ui->labelDatabaseInformation->setText(tr("<b>No Simulator Found</b>"));
  ui->comboBoxSimulator->setDisabled(paths.isEmpty());
  ui->pushButtonDatabaseBasePath->setDisabled(paths.isEmpty());
  ui->pushButtonDatabaseSceneryFile->setDisabled(paths.isEmpty());
  ui->pushButtonDatabaseResetPaths->setDisabled(paths.isEmpty());
  ui->lineEditDatabaseBasePath->setDisabled(paths.isEmpty());
  ui->lineEditDatabaseSceneryFile->setDisabled(paths.isEmpty());
  ui->buttonBoxDatabase->button(QDialogButtonBox::Ok)->setDisabled(paths.isEmpty());

  connect(ui->buttonBoxDatabase, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(ui->buttonBoxDatabase, &QDialogButtonBox::rejected, this, &QDialog::reject);

  connect(ui->comboBoxSimulator, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &DatabaseDialog::simComboChanged);
  connect(ui->pushButtonDatabaseBasePath, &QPushButton::clicked, this, &DatabaseDialog::selectBasePath);
  connect(ui->pushButtonDatabaseSceneryFile, &QPushButton::clicked, this,
          &DatabaseDialog::selectSceneryConfig);
  connect(ui->pushButtonDatabaseResetPaths, &QPushButton::clicked, this, &DatabaseDialog::resetPaths);

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
  qDebug() << text;
  paths[currentFsType].basePath = text;
}

void DatabaseDialog::sceneryConfigFileEdited(const QString& text)
{
  qDebug() << text;
  paths[currentFsType].sceneryCfg = text;
}

void DatabaseDialog::resetPaths()
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

void DatabaseDialog::selectBasePath()
{
  QString path = atools::gui::Dialog(this).openDirectoryDialog(
    tr("Select Flight Simulator Basepath"), lnm::DATABASE_BASEPATH, ui->lineEditDatabaseBasePath->text());

  if(!path.isEmpty())
    paths[currentFsType].basePath = path;
  updateLineEdits();
}

void DatabaseDialog::selectSceneryConfig()
{
  QString path = atools::gui::Dialog(this).openFileDialog(
    tr("Open Scenery Configuration File"),
    tr("Scenery Configuration Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_SCENERYCONFIG),
    lnm::DATABASE_SCENERYCONFIG, ui->lineEditDatabaseSceneryFile->text());

  if(!path.isEmpty())
    paths[currentFsType].sceneryCfg = path;
  updateLineEdits();
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
