/*****************************************************************************
* Copyright 2015-2024 Alexander Barthel alex@littlenavmap.org
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
#include "atools.h"
#include "common/dialogrecordhelper.h"
#include "common/constants.h"
#include "common/formatter.h"
#include "fs/util/coordinates.h"
#include "common/unit.h"
#include "gui/helphandler.h"
#include "userdata/userdataicons.h"
#include "gui/widgetstate.h"
#include "common/unitstringtool.h"
#include "common/maptypes.h"
#include "app/navapp.h"

#include <QPushButton>
#include <QDateTime>
#include <QLocale>

const float DEFAULT_VIEW_DISTANCE_NM = 250.f;
const float DEFAULT_VIEW_DISTANCE_KM = 450.f;
const float DEFAULT_VIEW_DISTANCE_MI = 300.f;

const QLatin1String UserdataDialog::DEFAULT_TYPE("Bookmark");

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
      setWindowTitle(QCoreApplication::applicationName() + tr(" - Edit Userpoint"));
    else if(mode == ud::ADD)
    {
      setWindowTitle(QCoreApplication::applicationName() + tr(" - Add Userpoint"));
      ui->comboBoxUserdataType->setCurrentText(DEFAULT_TYPE);
    }
  }
  else if(mode == ud::EDIT_MULTIPLE)
  {
    setWindowTitle(QCoreApplication::applicationName() + tr(" - Edit Userpoints"));
    showCheckbox = true;
    showLatLon = false;
  }

  // Change label depending on order
  if(Unit::getUnitCoords() == opts::COORDS_LONX_LATY)
    ui->labelUserdataLatLon->setText(tr("&Longitude and Latitude:"));
  else
    ui->labelUserdataLatLon->setText(tr("&Latitude and Longitude:"));

  // Update units
  units = new UnitStringTool();
  units->init({ui->spinBoxUserdataAltitude, ui->spinBoxUserdataVisible});

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
  ui->labelUserdataMagVar->setVisible(showLatLon);

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
  connect(ui->buttonBoxUserdata->button(QDialogButtonBox::Help), &QPushButton::clicked, this, &UserdataDialog::helpClicked);
  connect(ui->buttonBoxUserdata->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, &UserdataDialog::resetClicked);
  connect(ui->buttonBoxUserdata, &QDialogButtonBox::rejected, this, &QDialog::reject);

  atools::gui::WidgetState(lnm::TRAFFIC_PATTERN_DIALOG).restore(this);

  updateWidgets();
}

UserdataDialog::~UserdataDialog()
{
  atools::gui::WidgetState(lnm::USERDATA_EDIT_ADD_DIALOG).save(this);

  delete units;
  delete ui;
  delete record;
}

void UserdataDialog::coordsEdited(const QString& text)
{
  Q_UNUSED(text)

  QString message;
  atools::geo::Pos pos;
  bool valid = formatter::checkCoordinates(message, ui->lineEditUserdataLatLon->text(), &pos);
  ui->buttonBoxUserdata->button(QDialogButtonBox::Ok)->setEnabled(valid);
  ui->labelUserdataCoordStatus->setText(message);

  if(pos.isValid())
    ui->labelUserdataMagVar->setText(tr("Magnetic declination: %1").arg(map::magvarText(NavApp::getMagVar(pos))));
}

void UserdataDialog::helpClicked()
{
  if(editMode == ud::ADD)
    atools::gui::HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + "USERPOINT.html#userpoints-dialog-add", lnm::helpLanguageOnline());
  else
    atools::gui::HelpHandler::openHelpUrlWeb(this, lnm::helpOnlineUrl + "USERPOINT.html#userpoints-dialog-edit", lnm::helpLanguageOnline());
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
    ui->spinBoxUserdataVisible->setValue(atools::roundToInt(defaultDistValue()));
    ui->plainTextEditUserdataDescription->clear();
    ui->checkBoxUserdataTemp->setChecked(false);
  }
  else if(editMode == ud::EDIT_MULTIPLE || editMode == ud::EDIT_ONE)
    recordToDialog();
}

void UserdataDialog::acceptClicked()
{
  // Copy widget data to record
  dialogToRecord();
  qDebug() << Q_FUNC_INFO << *record;

  QDialog::accept();
}

void UserdataDialog::setRecord(const atools::sql::SqlRecord& sqlRecord)
{
  qDebug() << Q_FUNC_INFO << sqlRecord;

  *record = sqlRecord;

  // Data to widgets
  recordToDialog();
}

void UserdataDialog::saveState() const
{
  atools::gui::WidgetState(lnm::USERDATA_EDIT_ADD_DIALOG).save(this);
}

void UserdataDialog::restoreState()
{
  atools::gui::WidgetState(lnm::USERDATA_EDIT_ADD_DIALOG).restore(this);
  updateWidgets();
}

void UserdataDialog::updateWidgets()
{
  if(editMode == ud::EDIT_MULTIPLE)
  {
    // Enable or disable edit widgets depending in check box status
    ui->spinBoxUserdataAltitude->setEnabled(ui->checkBoxUserdataAltitude->isChecked());
    ui->plainTextEditUserdataDescription->setEnabled(ui->checkBoxUserdataDescription->isChecked());
    ui->lineEditUserdataIdent->setEnabled(ui->checkBoxUserdataIdent->isChecked());
    ui->lineEditUserdataRegion->setEnabled(ui->checkBoxUserdataRegion->isChecked());
    ui->lineEditUserdataName->setEnabled(ui->checkBoxUserdataName->isChecked());
    ui->lineEditUserdataTags->setEnabled(ui->checkBoxUserdataTags->isChecked());
    ui->comboBoxUserdataType->setEnabled(ui->checkBoxUserdataType->isChecked());
    ui->spinBoxUserdataVisible->setEnabled(ui->checkBoxUserdataVisible->isChecked());

    // Disable dialog OK button if nothing is checked
    ui->buttonBoxUserdata->button(QDialogButtonBox::Ok)->setEnabled(
      ui->checkBoxUserdataAltitude->isChecked() || ui->checkBoxUserdataDescription->isChecked() ||
      ui->checkBoxUserdataRegion->isChecked() || ui->checkBoxUserdataIdent->isChecked() || ui->checkBoxUserdataName->isChecked() ||
      ui->checkBoxUserdataTags->isChecked() || ui->checkBoxUserdataType->isChecked() || ui->checkBoxUserdataVisible->isChecked());
  }
}

void UserdataDialog::recordToDialog()
{
  qDebug() << Q_FUNC_INFO;

  fillTypeComboBox(record->valueStr("type"));

  ui->lineEditUserdataName->setText(record->valueStr("name"));
  ui->lineEditUserdataIdent->setText(record->valueStr("ident"));
  ui->lineEditUserdataRegion->setText(record->valueStr("region"));
  ui->plainTextEditUserdataDescription->setPlainText(record->valueStr("description"));
  ui->lineEditUserdataTags->setText(record->valueStr("tags"));

  if(record->contains("temp") && record->valueBool("temp"))
  {
    // temp point
    if(editMode == ud::ADD)
      // Checkbox in add mode if set in record
      ui->checkBoxUserdataTemp->setChecked(false);
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
  {
    ui->labelUserdataFile->setVisible(true);
    ui->labelUserdataFilepath->setVisible(true);
    ui->labelUserdataFilepath->setText(record->valueStr("import_file_path"));
  }
  else
  {
    ui->labelUserdataFile->setVisible(false);
    ui->labelUserdataFilepath->setVisible(false);
  }

  if(!record->isNull("visible_from"))
    ui->spinBoxUserdataVisible->setValue(atools::roundToInt(Unit::distNmF(record->valueFloat("visible_from"))));
  else
    ui->spinBoxUserdataVisible->setValue(atools::roundToInt(defaultDistValue()));

  ui->spinBoxUserdataAltitude->setValue(atools::roundToInt(Unit::altFeetF(record->valueInt("altitude"))));

  if(!record->isNull("lonx") && !record->isNull("laty"))
    ui->lineEditUserdataLatLon->setText(Unit::coords(atools::geo::Pos(record->valueFloat("lonx"), record->valueFloat("laty"))));
  coordsEdited(QString());
  updateWidgets();
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

  DialogRecordHelper helper(record, editMode == ud::EDIT_MULTIPLE);

  // If multiple set the values into all fields that have their checkbox checked

  if(editMode != ud::EDIT_MULTIPLE || ui->checkBoxUserdataType->isChecked())
    record->setValue("type", ui->comboBoxUserdataType->currentText());
  else if(editMode == ud::EDIT_MULTIPLE)
    // No checked in batch mode - remove
    record->remove("type");

  helper.dialogToRecordStr(ui->lineEditUserdataName, "name", ui->checkBoxUserdataName);
  helper.dialogToRecordStr(ui->lineEditUserdataIdent, "ident", ui->checkBoxUserdataIdent);
  helper.dialogToRecordStr(ui->lineEditUserdataRegion, "region", ui->checkBoxUserdataRegion);
  helper.dialogToRecordStr(ui->plainTextEditUserdataDescription, "description", ui->checkBoxUserdataDescription);
  helper.dialogToRecordStr(ui->lineEditUserdataTags, "tags", ui->checkBoxUserdataTags);

  record->setValue("last_edit_timestamp", atools::convertToIsoWithOffset(QDateTime::currentDateTime()));

  helper.dialogToRecordInt(ui->spinBoxUserdataVisible, "visible_from", ui->checkBoxUserdataVisible, Unit::distNmF);
  helper.dialogToRecordInt(ui->spinBoxUserdataAltitude, "altitude", ui->checkBoxUserdataAltitude, Unit::altFeetF);

  if(editMode != ud::EDIT_MULTIPLE)
  {
    bool hemisphere = false;
    atools::geo::Pos pos = atools::fs::util::fromAnyFormat(ui->lineEditUserdataLatLon->text(), &hemisphere);

    if(Unit::getUnitCoords() == opts::COORDS_LONX_LATY && !hemisphere)
      // Swap coordinates for lat lon formats if no hemisphere (N, S, E, W) is given
      atools::fs::util::maybeSwapOrdinates(pos, ui->lineEditUserdataLatLon->text());

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

float UserdataDialog::defaultDistValue()
{
  switch(Unit::getUnitDist())
  {
    case opts::DIST_NM:
      return DEFAULT_VIEW_DISTANCE_NM;

    case opts::DIST_KM:
      return DEFAULT_VIEW_DISTANCE_KM;

    case opts::DIST_MILES:
      return DEFAULT_VIEW_DISTANCE_MI;
  }
  return DEFAULT_VIEW_DISTANCE_NM;
}
