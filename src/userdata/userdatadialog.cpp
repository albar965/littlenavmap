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
#include "common/formatter.h"
#include "gui/mainwindow.h"
#include "fs/util/coordinates.h"
#include "common/unit.h"
#include "gui/helphandler.h"
#include "navapp.h"
#include "userdata/userdataicons.h"
#include "gui/widgetstate.h"

#include <QPushButton>
#include <QDateTime>
#include <QLocale>

const QLatin1Literal UserdataDialog::DEFAULT_TYPE("Bookmark");

UserdataDialog::UserdataDialog(QWidget *parent, ud::UserdataDialogMode mode, UserdataIcons *userdataIcons) :
  QDialog(parent), editMode(mode), ui(new Ui::UserdataDialog), icons(userdataIcons)
{
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  record = new atools::sql::SqlRecord();

  bool showCheckbox = true, showLatLon = true;
  if(mode == ud::EDIT_ONE || mode == ud::ADD)
  {
    showCheckbox = false;
    showLatLon = true;

    if(mode == ud::EDIT_ONE)
      setWindowTitle(QApplication::applicationName() + tr(" - Edit Userpoint"));
    else if(mode == ud::ADD)
    {
      setWindowTitle(QApplication::applicationName() + tr(" - Add Userpoint"));
      ui->comboBoxUserdataType->setCurrentText(DEFAULT_TYPE);
    }
  }
  else if(mode == ud::EDIT_MULTIPLE)
  {
    setWindowTitle(QApplication::applicationName() + tr(" - Edit Userpoints"));
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
  ui->checkBoxUserdataRegion->setVisible(showCheckbox);
  ui->checkBoxUserdataName->setVisible(showCheckbox);
  ui->checkBoxUserdataTags->setVisible(showCheckbox);
  ui->checkBoxUserdataType->setVisible(showCheckbox);
  ui->checkBoxUserdataVisible->setVisible(showCheckbox);

  // No lat/lon editing for more than one entry
  ui->line->setVisible(showLatLon);
  ui->labelUserdataLatLon->setVisible(showLatLon);
  ui->lineEditUserdataLatLon->setVisible(showLatLon);
  ui->labelUserdataCoordStatus->setVisible(showLatLon);

  bool hideMeta = editMode != ud::ADD && editMode != ud::EDIT_MULTIPLE;
  ui->labelUserdataLastChange->setVisible(hideMeta);
  ui->labelUserdataLastChangeDateTime->setVisible(hideMeta);
  ui->labelUserdataFile->setVisible(hideMeta);
  ui->labelUserdataFilepath->setVisible(hideMeta);
  ui->labelUserdataTemp->setVisible(hideMeta);
  ui->lineUserdataMeta->setVisible(hideMeta);

  ui->checkBoxUserdataTemp->setVisible(editMode == ud::ADD);

  // Connect checkboxes
  connect(ui->checkBoxUserdataAltitude, &QCheckBox::toggled, this, &UserdataDialog::updateWidgets);
  connect(ui->checkBoxUserdataDescription, &QCheckBox::toggled, this, &UserdataDialog::updateWidgets);
  connect(ui->checkBoxUserdataIdent, &QCheckBox::toggled, this, &UserdataDialog::updateWidgets);
  connect(ui->checkBoxUserdataRegion, &QCheckBox::toggled, this, &UserdataDialog::updateWidgets);
  connect(ui->checkBoxUserdataName, &QCheckBox::toggled, this, &UserdataDialog::updateWidgets);
  connect(ui->checkBoxUserdataTags, &QCheckBox::toggled, this, &UserdataDialog::updateWidgets);
  connect(ui->checkBoxUserdataType, &QCheckBox::toggled, this, &UserdataDialog::updateWidgets);
  connect(ui->checkBoxUserdataVisible, &QCheckBox::toggled, this, &UserdataDialog::updateWidgets);
  connect(ui->lineEditUserdataLatLon, &QLineEdit::textChanged, this, &UserdataDialog::coordsEdited);

  // Connect button box
  connect(ui->buttonBoxUserdata, &QDialogButtonBox::accepted, this, &UserdataDialog::acceptClicked);
  connect(ui->buttonBoxUserdata->button(QDialogButtonBox::Help), &QPushButton::clicked,
          this, &UserdataDialog::helpClicked);
  connect(ui->buttonBoxUserdata->button(QDialogButtonBox::Reset), &QPushButton::clicked,
          this, &UserdataDialog::resetClicked);
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
  Q_UNUSED(text);

  QString message;
  bool valid = formatter::checkCoordinates(message, ui->lineEditUserdataLatLon->text());
  ui->buttonBoxUserdata->button(QDialogButtonBox::Ok)->setEnabled(valid);
  ui->labelUserdataCoordStatus->setText(message);
}

void UserdataDialog::helpClicked()
{
  atools::gui::HelpHandler::openHelpUrl(
    NavApp::getMainWindow(), lnm::HELP_ONLINE_URL + "EDITUSERDATA.html", lnm::helpLanguagesOnline());
}

void UserdataDialog::resetClicked()
{
  qDebug() << Q_FUNC_INFO;

  if(editMode == ud::EDIT_MULTIPLE)
  {
    // Reset checkboxes to unchecked
    ui->checkBoxUserdataAltitude->setChecked(false);
    ui->checkBoxUserdataDescription->setChecked(false);
    ui->checkBoxUserdataIdent->setChecked(false);
    ui->checkBoxUserdataRegion->setChecked(false);
    ui->checkBoxUserdataName->setChecked(false);
    ui->checkBoxUserdataTags->setChecked(false);
    ui->checkBoxUserdataType->setChecked(false);
    ui->checkBoxUserdataVisible->setChecked(false);
  }

  if(editMode == ud::ADD)
  {
    // Clear all except coordinates
    ui->comboBoxUserdataType->setCurrentIndex(ui->comboBoxUserdataType->findText(DEFAULT_TYPE));
    ui->lineEditUserdataIdent->clear();
    ui->lineEditUserdataRegion->clear();
    ui->lineEditUserdataName->clear();
    ui->lineEditUserdataTags->clear();
    ui->spinBoxUserdataAltitude->setValue(0.);
    ui->spinBoxUserdataVisible->setValue(250);
    ui->textEditUserdataDescription->clear();
    ui->checkBoxUserdataTemp->setChecked(false);
  }
  else if(editMode == ud::EDIT_MULTIPLE || editMode == ud::EDIT_ONE)
    recordToDialog();
}

void UserdataDialog::acceptClicked()
{
  // Copy widget data to record
  dialogToRecord();
  qDebug() << Q_FUNC_INFO << record;

  QDialog::accept();
}

void UserdataDialog::setRecord(const atools::sql::SqlRecord& sqlRecord)
{
  qDebug() << Q_FUNC_INFO << sqlRecord;

  *record = sqlRecord;

  // Data to widgets
  recordToDialog();
}

void UserdataDialog::saveState()
{
  atools::gui::WidgetState(lnm::USERDATA_EDIT_ADD_DIALOG).save(this);
}

void UserdataDialog::restoreState()
{
  atools::gui::WidgetState(lnm::USERDATA_EDIT_ADD_DIALOG).restore(this);
}

void UserdataDialog::updateWidgets()
{
  if(editMode == ud::EDIT_MULTIPLE)
  {
    // Enable or disable edit widgets depending in check box status
    ui->spinBoxUserdataAltitude->setEnabled(ui->checkBoxUserdataAltitude->isChecked());
    ui->textEditUserdataDescription->setEnabled(ui->checkBoxUserdataDescription->isChecked());
    ui->lineEditUserdataIdent->setEnabled(ui->checkBoxUserdataIdent->isChecked());
    ui->lineEditUserdataRegion->setEnabled(ui->checkBoxUserdataRegion->isChecked());
    ui->lineEditUserdataName->setEnabled(ui->checkBoxUserdataName->isChecked());
    ui->lineEditUserdataTags->setEnabled(ui->checkBoxUserdataTags->isChecked());
    ui->comboBoxUserdataType->setEnabled(ui->checkBoxUserdataType->isChecked());
    ui->spinBoxUserdataVisible->setEnabled(ui->checkBoxUserdataVisible->isChecked());

    // Disable dialog OK button if nothing is checked
    ui->buttonBoxUserdata->button(QDialogButtonBox::Ok)->setEnabled(
      ui->checkBoxUserdataAltitude->isChecked() |
      ui->checkBoxUserdataDescription->isChecked() |
      ui->checkBoxUserdataRegion->isChecked() |
      ui->checkBoxUserdataName->isChecked() |
      ui->checkBoxUserdataTags->isChecked() |
      ui->checkBoxUserdataType->isChecked() |
      ui->checkBoxUserdataVisible->isChecked()
      );
  }
}

void UserdataDialog::recordToDialog()
{
  qDebug() << Q_FUNC_INFO;

  fillTypeComboBox(record->valueStr("type"));

  ui->lineEditUserdataName->setText(record->valueStr("name"));
  ui->lineEditUserdataIdent->setText(record->valueStr("ident"));
  ui->lineEditUserdataRegion->setText(record->valueStr("region"));
  ui->textEditUserdataDescription->setText(record->valueStr("description"));
  ui->lineEditUserdataTags->setText(record->valueStr("tags"));

  if(record->contains("temp") && !record->value("temp").isNull())
  {
    // temp point
    if(editMode == ud::ADD)
      // Checkbox in add mode if set in record
      ui->checkBoxUserdataTemp->setChecked(record->valueBool("temp"));
    else if(editMode == ud::EDIT_ONE)
      // Add label in edit mode
      ui->labelUserdataTemp->setText(tr("Temporary userpoint - will be deleted on next startup"));
  }
  else
    ui->labelUserdataTemp->setVisible(false);

  if(!record->value("last_edit_timestamp").isNull())
    ui->labelUserdataLastChangeDateTime->setText(QLocale().toString(record->value("last_edit_timestamp").toDateTime()));
  else
    ui->labelUserdataFilepath->setText(tr("-"));

  if(!record->value("import_file_path").isNull())
    ui->labelUserdataFilepath->setText(record->valueStr("import_file_path"));
  else
    ui->labelUserdataFilepath->setText(tr("Not imported"));

  if(!record->isNull("visible_from"))
    ui->spinBoxUserdataVisible->setValue(Unit::distNmF(record->valueInt("visible_from")));
  else
    ui->spinBoxUserdataVisible->setValue(Unit::distNmF(250.f));

  ui->spinBoxUserdataAltitude->setValue(Unit::altFeetF(record->valueInt("altitude")));

  if(!record->isNull("lonx") && !record->isNull("laty"))
    ui->lineEditUserdataLatLon->setText(Unit::coords(atools::geo::Pos(record->valueFloat("lonx"),
                                                                      record->valueFloat("laty"))));
  coordsEdited(QString());
}

void UserdataDialog::dialogToRecord()
{
  qDebug() << Q_FUNC_INFO;

  if(editMode == ud::ADD)
    record->setValue("temp", ui->checkBoxUserdataTemp->isChecked());

  if(editMode == ud::EDIT_MULTIPLE || editMode == ud::EDIT_ONE)
  {
    if(editMode == ud::EDIT_MULTIPLE)
      // Set all to null
      record->clearValues();

    // Fields that will never be updated when editing
    if(record->contains("import_file_path"))
      record->remove("import_file_path");
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

  if(editMode != ud::EDIT_MULTIPLE || ui->checkBoxUserdataRegion->isChecked())
    record->setValue("region", ui->lineEditUserdataRegion->text());
  else if(editMode == ud::EDIT_MULTIPLE)
    record->remove("region");

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
    record->setValue("visible_from", Unit::rev(ui->spinBoxUserdataVisible->value(), Unit::distNmF));
  else if(editMode == ud::EDIT_MULTIPLE)
    record->remove("visible_from");

  if(editMode != ud::EDIT_MULTIPLE || ui->checkBoxUserdataAltitude->isChecked())
    record->setValue("altitude", Unit::rev(ui->spinBoxUserdataAltitude->value(), Unit::altFeetF));
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
  else if(type.isEmpty())
  {
    // Current type does not match - add to list and set default icon
    ui->comboBoxUserdataType->setCurrentIndex(ui->comboBoxUserdataType->findText(DEFAULT_TYPE));
  }
  else
  {
    // Current type does not match - add to list and set default icon
    ui->comboBoxUserdataType->insertItem(0, QIcon(*icons->getIconPixmap(type, size)), type);
    ui->comboBoxUserdataType->setCurrentIndex(0);
  }
}
