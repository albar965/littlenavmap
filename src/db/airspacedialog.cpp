/*****************************************************************************
* Copyright 2015-2022 Alexander Barthel alex@littlenavmap.org
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

#include "db/airspacedialog.h"
#include "ui_airspacedialog.h"

#include "atools.h"
#include "gui/dialog.h"
#include "gui/helphandler.h"
#include "util/htmlbuilder.h"
#include "common/constants.h"
#include "gui/widgetstate.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>

AirspaceDialog::AirspaceDialog(QWidget *parent)
  : QDialog(parent), parentWidget(parent), ui(new Ui::AirspaceDialog)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("&Load"));
  ui->buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);

  connect(ui->pushButtonAirspacePathSelect, &QPushButton::clicked, this, &AirspaceDialog::airspacePathSelectClicked);
  connect(ui->lineEditAirspacePath, &QLineEdit::textEdited, this, &AirspaceDialog::updateAirspaceStates);
  connect(ui->lineEditAirspaceExtensions, &QLineEdit::textEdited, this, &AirspaceDialog::updateAirspaceStates);
  connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &AirspaceDialog::buttonBoxClicked);

  restoreState();
  updateAirspaceStates();
}

AirspaceDialog::~AirspaceDialog()
{
  saveState();
  delete ui;
}

QString AirspaceDialog::getAirspacePath() const
{
  return ui->lineEditAirspacePath->text();
}

QString AirspaceDialog::getAirspaceFilePatterns() const
{
  return ui->lineEditAirspaceExtensions->text();
}

void AirspaceDialog::saveState()
{
  atools::gui::WidgetState widgetState(lnm::DATABASE_AIRSPACECONFIG, false);
  widgetState.save({this, ui->lineEditAirspacePath, ui->lineEditAirspaceExtensions});
}

void AirspaceDialog::restoreState()
{
  atools::gui::WidgetState widgetState(lnm::DATABASE_AIRSPACECONFIG, false);
  widgetState.restore({this, ui->lineEditAirspacePath, ui->lineEditAirspaceExtensions});
}

void AirspaceDialog::buttonBoxClicked(QAbstractButton *button)
{
  if(button == ui->buttonBox->button(QDialogButtonBox::Ok))
  {
    saveState();
    QDialog::accept();
  }
  else if(button == ui->buttonBox->button(QDialogButtonBox::Help))
    atools::gui::HelpHandler::openHelpUrlWeb(parentWidget, lnm::helpOnlineUrl + "AIRSPACELOAD.html", lnm::helpLanguageOnline());
  else if(button == ui->buttonBox->button(QDialogButtonBox::Close))
    QDialog::reject();
}

void AirspaceDialog::airspacePathSelectClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString defaultPath = ui->lineEditAirspacePath->text();

  if(defaultPath.isEmpty())
    defaultPath = atools::documentsDir();

  QString path = atools::gui::Dialog(parentWidget).openDirectoryDialog(
    tr("Select Directory for User Airspaces"), lnm::DATABASE_USER_AIRSPACE_PATH, defaultPath);

  if(!path.isEmpty())
    ui->lineEditAirspacePath->setText(QDir::toNativeSeparators(path));

  updateAirspaceStates();
}

void AirspaceDialog::updateAirspaceStates()
{
  const QString& path = ui->lineEditAirspacePath->text();
  bool disableOk = true;
  if(!path.isEmpty())
  {
    QFileInfo fileinfo(path);
    if(!fileinfo.exists())
      ui->labelAirspacePathStatus->setText(atools::util::HtmlBuilder::errorMessage(tr("Directory does not exist.")));
    else if(!fileinfo.isDir())
      ui->labelAirspacePathStatus->setText(atools::util::HtmlBuilder::errorMessage(tr(("Is not a directory."))));
    else
    {
      disableOk = false;
      ui->labelAirspacePathStatus->setText(tr("Directory is valid."));
    }
  }
  else
    ui->labelAirspacePathStatus->setText(tr("No directory selected."));

  if(ui->lineEditAirspaceExtensions->text().isEmpty())
    disableOk = true;

  ui->buttonBox->button(QDialogButtonBox::Ok)->setDisabled(disableOk);
}
