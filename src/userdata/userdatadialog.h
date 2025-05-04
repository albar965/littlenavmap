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

#ifndef USERDATADIALOG_H
#define USERDATADIALOG_H

#include <QDialog>

namespace Ui {
class UserdataDialog;
}

namespace atools {
namespace sql {
class SqlRecord;
}
}

namespace ud {

enum UserdataDialogMode
{
  /* Add new entry */
  ADD,

  /* Edit one entry. Do not show checkboxes. */
  EDIT_ONE,

  /* Edit more than one entry. Show checkboxes that allow to select the fields to change. */
  EDIT_MULTIPLE
};

}

class UserdataIcons;
class UnitStringTool;

/*
 * Dialog allows to edit one or more userdata records or add one.
 */
class UserdataDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit UserdataDialog(QWidget *parent, ud::UserdataDialogMode mode, UserdataIcons *userdataIcons);
  virtual ~UserdataDialog() override;

  UserdataDialog(const UserdataDialog& other) = delete;
  UserdataDialog& operator=(const UserdataDialog& other) = delete;

  /* Get changed data. If mode is EDIT_MULTIPLE only a part of the columns are set */
  const atools::sql::SqlRecord& getRecord() const
  {
    return *record;
  }

  /* Set data for editing. */
  void setRecord(const atools::sql::SqlRecord& sqlRecord);

  const static QLatin1String DEFAULT_TYPE;

  void saveState() const;
  void restoreState();

private:
  void updateWidgets();
  void acceptClicked();
  void helpClicked();
  void resetClicked();

  /* Move widget data to the SQL record */
  void dialogToRecord();

  /* Move SQL record data to widgets */
  void recordToDialog();

  /* Update the coordinate status message */
  void coordsEdited(const QString& text);

  void fillTypeComboBox(const QString& type);

  float defaultDistValue();

  atools::sql::SqlRecord *record;
  ud::UserdataDialogMode editMode;
  Ui::UserdataDialog *ui;
  UserdataIcons *icons;
  UnitStringTool *units = nullptr;

};

#endif // USERDATADIALOG_H
