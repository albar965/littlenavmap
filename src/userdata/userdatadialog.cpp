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

#include "userdata/userdatadialog.h"

#include "ui_userdatadialog.h"
#include "sql/sqlrecord.h"
#include "geo/pos.h"
#include "common/constants.h"
#include "gui/mainwindow.h"
#include "fs/util/coordinates.h"
#include "common/unit.h"
#include "gui/helphandler.h"
#include "navapp.h"
#include "userdata/userdataicons.h"

#include <QPushButton>
#include <QDateTime>

UserdataDialog::UserdataDialog(QWidget *parent, ud::UserdataDialogMode mode, UserdataIcons *userdataIcons) :
  QDialog(parent), editMode(mode), ui(new Ui::UserdataDialog), icons(userdataIcons)
{
  ui->setupUi(this);

  record = new atools::sql::SqlRecord();

  bool showCheckbox = true, showLatLon = true;
  if(mode == ud::EDIT_ONE || mode == ud::ADD)
  {
    showCheckbox = false;
    showLatLon = true;
  }
  else if(mode == ud::EDIT_MULTIPLE)
  {
    showCheckbox = true;
    showLatLon = false;
  }

  // Update units
  ui->spinBoxUserdataAltitude->setSuffix(
    Unit::replacePlaceholders(ui->spinBoxUserdataAltitude->suffix()));
  ui->spinBoxUserdataVisible->setSuffix(
    Unit::replacePlaceholders(ui->spinBoxUserdataVisible->suffix()));

  // Show checkboxes when editing more than one entry
  ui->checkBoxUserdataAltitude->setVisible(showCheckbox);
  ui->checkBoxUserdataDescription->setVisible(showCheckbox);
  ui->checkBoxUserdataIdent->setVisible(showCheckbox);
  ui->checkBoxUserdataName->setVisible(showCheckbox);
  ui->checkBoxUserdataTags->setVisible(showCheckbox);
  ui->checkBoxUserdataType->setVisible(showCheckbox);
  ui->checkBoxUserdataVisible->setVisible(showCheckbox);

  // No lat/lon editing for more than one entry
  ui->line->setVisible(showLatLon);
  ui->labelUserdataLatLon->setVisible(showLatLon);
  ui->lineEditUserdataLatLon->setVisible(showLatLon);
  ui->labelUserdataCoordStatus->setVisible(showLatLon);

  // Connect checkboxes
  connect(ui->checkBoxUserdataAltitude, &QCheckBox::toggled, this, &UserdataDialog::updateWidgets);
  connect(ui->checkBoxUserdataDescription, &QCheckBox::toggled, this, &UserdataDialog::updateWidgets);
  connect(ui->checkBoxUserdataIdent, &QCheckBox::toggled, this, &UserdataDialog::updateWidgets);
  connect(ui->checkBoxUserdataName, &QCheckBox::toggled, this, &UserdataDialog::updateWidgets);
  connect(ui->checkBoxUserdataTags, &QCheckBox::toggled, this, &UserdataDialog::updateWidgets);
  connect(ui->checkBoxUserdataType, &QCheckBox::toggled, this, &UserdataDialog::updateWidgets);
  connect(ui->checkBoxUserdataVisible, &QCheckBox::toggled, this, &UserdataDialog::updateWidgets);
  connect(ui->lineEditUserdataLatLon, &QLineEdit::textChanged, this, &UserdataDialog::coordsEdited);

  // Connect button box
  connect(ui->buttonBoxUserdata, &QDialogButtonBox::accepted, this, &UserdataDialog::acceptClicked);
  connect(ui->buttonBoxUserdata->button(QDialogButtonBox::Help), &QPushButton::clicked,
          this, &UserdataDialog::helpClicked);
  connect(ui->buttonBoxUserdata, &QDialogButtonBox::rejected, this, &QDialog::reject);

  updateWidgets();
}

UserdataDialog::~UserdataDialog()
{
  delete ui;
  delete record;
}

void UserdataDialog::coordsEdited(const QString& text)
{
  atools::geo::Pos pos = atools::fs::util::fromAnyFormat(text);

  if(pos.isValid())
  {
    QString coords = Unit::coords(pos);
    if(coords != text)
      ui->labelUserdataCoordStatus->setText(tr("Coordinates are valid: %1").arg(coords));
    else
      // Same as in line edit. No need to show again
      ui->labelUserdataCoordStatus->setText(tr("Coordinates are valid."));
  }
  else
    // Show red warning
    ui->labelUserdataCoordStatus->setText(tr(
                                            "<span style=\"font-weight: bold; color: red;\">Coordinates are not valid.</span>"));
}

void UserdataDialog::helpClicked()
{
  atools::gui::HelpHandler::openHelpUrl(
    NavApp::getMainWindow(), lnm::HELP_ONLINE_URL + "EDITUSERDATA.html", lnm::helpLanguagesOnline());
}

void UserdataDialog::acceptClicked()
{
  // Copy widget data to record
  dialogToRecord();
  qDebug() << *record;

  QDialog::accept();
}

void UserdataDialog::setRecord(const atools::sql::SqlRecord& sqlRecord)
{
  qDebug() << sqlRecord;

  *record = sqlRecord;

  // Data to widgets
  recordToDialog();
}

void UserdataDialog::updateWidgets()
{
  if(editMode == ud::EDIT_MULTIPLE)
  {
    // Enable or disable edit widgets depending in check box status
    ui->spinBoxUserdataAltitude->setEnabled(ui->checkBoxUserdataAltitude->isChecked());
    ui->textEditUserdataDescription->setEnabled(ui->checkBoxUserdataDescription->isChecked());
    ui->lineEditUserdataIdent->setEnabled(ui->checkBoxUserdataIdent->isChecked());
    ui->lineEditUserdataName->setEnabled(ui->checkBoxUserdataName->isChecked());
    ui->lineEditUserdataTags->setEnabled(ui->checkBoxUserdataTags->isChecked());
    ui->comboBoxUserdataType->setEnabled(ui->checkBoxUserdataType->isChecked());
    ui->spinBoxUserdataVisible->setEnabled(ui->checkBoxUserdataVisible->isChecked());

    // Disable dialog OK button if nothing is checked
    ui->buttonBoxUserdata->button(QDialogButtonBox::Ok)->setEnabled(
      ui->checkBoxUserdataAltitude->isChecked() |
      ui->checkBoxUserdataDescription->isChecked() |
      ui->checkBoxUserdataIdent->isChecked() |
      ui->checkBoxUserdataName->isChecked() |
      ui->checkBoxUserdataTags->isChecked() |
      ui->checkBoxUserdataType->isChecked() |
      ui->checkBoxUserdataVisible->isChecked()
      );
  }
}

void UserdataDialog::recordToDialog()
{
  fillTypeComboBox(record->valueStr("type"));
  ui->lineEditUserdataName->setText(record->valueStr("name"));
  ui->lineEditUserdataIdent->setText(record->valueStr("ident"));
  ui->textEditUserdataDescription->setText(record->valueStr("description"));
  ui->lineEditUserdataTags->setText(record->valueStr("tags"));
  ui->spinBoxUserdataVisible->setValue(record->valueInt("visible_from"));
  ui->spinBoxUserdataAltitude->setValue(record->valueInt("altitude"));
  ui->lineEditUserdataLatLon->setText(Unit::coords(atools::geo::Pos(record->valueFloat("lonx"),
                                                                    record->valueFloat("laty"))));

#ifdef DEBUG_INFORMATION
  if(editMode == ud::ADD)
  {
    ui->lineEditUserdataIdent->setText(QString("TEST%1").arg(qrand() / (RAND_MAX / 100)));
    ui->lineEditUserdataName->setText(QString("Test name %1").arg(qrand()));
    ui->textEditUserdataDescription->setText(QString("Test description %1").arg(qrand()));
    ui->comboBoxUserdataType->setCurrentText("TST");
    // ui->lineEditUserdataLatLon->setText("E9° 12' 5.49\" N49° 26' 41.57\"");
    ui->lineEditUserdataLatLon->setText("N49° 26' 41.57\" E9° 12' 5.49\"");
  }
#endif

}

void UserdataDialog::dialogToRecord()
{
  if(editMode == ud::EDIT_MULTIPLE || editMode == ud::EDIT_ONE)
  {
    if(editMode == ud::EDIT_MULTIPLE)
      // Set all to null
      record->clearValues();

    // Fields that will never be updated when editing
    if(record->contains("import_file_path"))
      record->remove("import_file_path");
    if(record->contains("import_timestamp"))
      record->remove("import_timestamp");
    if(record->contains("userdata_id"))
      record->remove("userdata_id");

    if(editMode == ud::EDIT_MULTIPLE)
    {
      record->remove("lonx");
      record->remove("laty");
    }
  }

  // If multiple set the values into all fields that have their checkbox checked
  if(editMode != ud::EDIT_MULTIPLE || ui->checkBoxUserdataType->isChecked())
    record->setValue("type", ui->comboBoxUserdataType->currentText());
  else if(editMode == ud::EDIT_MULTIPLE)
    // No checked in batch mode - remove
    record->remove("type");

  if(editMode != ud::EDIT_MULTIPLE || ui->checkBoxUserdataName->isChecked())
    record->setValue("name", ui->lineEditUserdataName->text());
  else if(editMode == ud::EDIT_MULTIPLE)
    record->remove("name");

  if(editMode != ud::EDIT_MULTIPLE || ui->checkBoxUserdataIdent->isChecked())
    record->setValue("ident", ui->lineEditUserdataIdent->text());
  else if(editMode == ud::EDIT_MULTIPLE)
    record->remove("ident");

  if(editMode != ud::EDIT_MULTIPLE || ui->checkBoxUserdataDescription->isChecked())
    record->setValue("description", ui->textEditUserdataDescription->toPlainText());
  else if(editMode == ud::EDIT_MULTIPLE)
    record->remove("description");

  if(editMode != ud::EDIT_MULTIPLE || ui->checkBoxUserdataTags->isChecked())
    record->setValue("tags", ui->lineEditUserdataTags->text());
  else if(editMode == ud::EDIT_MULTIPLE)
    record->remove("tags");

  record->setValue("last_edit_timestamp", QDateTime::currentDateTime());

  if(editMode != ud::EDIT_MULTIPLE || ui->checkBoxUserdataVisible->isChecked())
    record->setValue("visible_from", ui->spinBoxUserdataVisible->value());
  else if(editMode == ud::EDIT_MULTIPLE)
    record->remove("visible_from");

  if(editMode != ud::EDIT_MULTIPLE || ui->checkBoxUserdataAltitude->isChecked())
    record->setValue("altitude", ui->spinBoxUserdataAltitude->value());
  else if(editMode == ud::EDIT_MULTIPLE)
    record->remove("altitude");

  if(editMode != ud::EDIT_MULTIPLE)
  {
    atools::geo::Pos pos = atools::fs::util::fromAnyFormat(ui->lineEditUserdataLatLon->text());
    record->setValue("lonx", pos.getLonX());
    record->setValue("laty", pos.getLatY());
  }
}

void UserdataDialog::fillTypeComboBox(const QString& type)
{
  // Fill default types and icons
  ui->comboBoxUserdataType->clear();
  int size = ui->comboBoxUserdataType->iconSize().height();
  for(const QString& t : icons->getAllTypes())
    ui->comboBoxUserdataType->addItem(QIcon(*icons->getIconPixmap(t, size)), t);

  int index = ui->comboBoxUserdataType->findText(type);
  if(index != -1)
    // Current type does match - set index
    ui->comboBoxUserdataType->setCurrentIndex(index);
  else
  {
    // Current type does not match - add to list and set default icon
    ui->comboBoxUserdataType->insertItem(0, QIcon(*icons->getIconPixmap(type, size)), type);
    ui->comboBoxUserdataType->setCurrentIndex(0);
  }
}
