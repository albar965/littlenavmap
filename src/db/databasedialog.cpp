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

#include "databasedialog.h"
#include "ui_databasedialog.h"
#include "fs/fspaths.h"

#include <QDialog>

#include <gui/dialog.h>

DatabaseDialog::DatabaseDialog(QWidget *parent)
  : QDialog(parent), ui(new Ui::DatabaseDialog)
{

  ui->setupUi(this);

  ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("&Load Navigation Data"));

  dialog = new atools::gui::Dialog(this);

  using atools::fs::fstype::SimulatorType;
  using atools::fs::FsPaths;

  // Add an action to the toolbutton for each simulator
  for(SimulatorType type : atools::fs::fstype::ALL_SIMULATOR_TYPES)
  {
    QString sceneryCfg = FsPaths::getSceneryLibraryPath(type);
    QString basePath = FsPaths::getBasePath(type);

    if(!basePath.isEmpty())
    {
      QAction *action = new QAction(QString("Get paths for %1").arg(FsPaths::typeToString(type)), this);
      action->setData(type);
      ui->toolButtonDatabaseMenu->addAction(action);
    }
  }

  connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

  connect(ui->toolButtonDatabaseMenu, &QToolButton::triggered, this, &DatabaseDialog::menuTriggered);
  connect(ui->pushButtonDatabaseBasePath, &QPushButton::clicked,
          this, &DatabaseDialog::selectBasePath);
  connect(ui->pushButtonDatabaseSceneryFile, &QPushButton::clicked,
          this, &DatabaseDialog::selectSceneryConfig);
}

DatabaseDialog::~DatabaseDialog()
{
  delete ui;
}

void DatabaseDialog::menuTriggered(QAction *action)
{
  using atools::fs::fstype::SimulatorType;
  using atools::fs::FsPaths;

  // Get paths for the selected simulators
  SimulatorType type = static_cast<SimulatorType>(action->data().toInt());
  setBasePath(FsPaths::getBasePath(type));
  setSceneryConfigFile(FsPaths::getSceneryLibraryPath(type));
}

QString DatabaseDialog::getBasePath() const
{
  return ui->lineEditDatabaseBasePath->text();
}

QString DatabaseDialog::getSceneryConfigFile() const
{
  return ui->lineEditDatabaseSceneryFile->text();
}

void DatabaseDialog::setBasePath(const QString& path)
{
  ui->lineEditDatabaseBasePath->setText(path);
}

void DatabaseDialog::setSceneryConfigFile(const QString& path)
{
  ui->lineEditDatabaseSceneryFile->setText(path);
}

void DatabaseDialog::selectBasePath()
{
  QString path = dialog->openDirectoryDialog(tr("Select Flight Simulator Basepath"),
                                             "Database/BasePath",
                                             getBasePath());

  if(!path.isEmpty())
    ui->lineEditDatabaseBasePath->setText(path);
}

void DatabaseDialog::selectSceneryConfig()
{
  QString path = dialog->openFileDialog(tr("Open Scenery Configuration File"),
                                        tr("Scenery Configuration Files (*.cfg *.CFG);;All Files (*)"),
                                        "Database/SceneryConfig",
                                        getSceneryConfigFile());

  if(!path.isEmpty())
    ui->lineEditDatabaseSceneryFile->setText(path);
}

void DatabaseDialog::setHeader(const QString& header)
{
  ui->labelDatabaseInformation->setText(header);
}
