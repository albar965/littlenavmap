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

#ifndef LNM_LOGDATADIALOG_H
#define LNM_LOGDATADIALOG_H

#include <QDialog>
#include <functional>

namespace Ui {
class LogdataDialog;
}

namespace atools {
namespace sql {
class SqlRecord;
}
}

class UnitStringTool;
class QCheckBox;
class QLineEdit;
class QSpinBox;
class QTextEdit;
class QDateTimeEdit;
class QLabel;
class QComboBox;

namespace ld {

enum LogdataDialogMode
{
  /* Add new entry */
  ADD,

  /* Edit one entry. Do not show checkboxes. */
  EDIT_ONE,

  /* Edit more than one entry. Show checkboxes that allow to select the fields to change. */
  EDIT_MULTIPLE
};

}

/*
 * Edit/add dialog for logbook entries. Allows editing of multiple entries.
 *
 * Based on records off the logbook table.
 */
class LogdataDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit LogdataDialog(QWidget *parent, ld::LogdataDialogMode mode);
  virtual ~LogdataDialog() override;

  /* Get changed data. If mode is EDIT_MULTIPLE only a part of the columns are set. */
  const atools::sql::SqlRecord& getRecord() const
  {
    return *record;
  }

  /* Set data for editing and prepare/fill/hide/show widgets. */
  void setRecord(const atools::sql::SqlRecord& sqlRecord);

  /* Save and load dialog size */
  void saveState();
  void restoreState();

private:
  /* Button box click handlers */
  void acceptClicked();
  void helpClicked();
  void resetClicked();

  /* Update status for all widgets */
  void updateWidgets();
  void clearWidgets();
  void valueDateTime(QDateTimeEdit *edit, const QString& name) const;

  /* Copy record field data to widgets and vice versa */
  void recordToDialog();
  void dialogToRecord();

  /* Load airport from ident and update name field or status */
  void departureAirportUpdated();
  void destAirportUpdated();
  void airportUpdated(QLineEdit *lineEdit, QLabel *label);

  /* Update status for files */
  void flightplanFileUpdated();
  void perfFileUpdated();

  /* Validate files and update status */
  void fileUpdated(QLineEdit *lineEdit, QLabel *label, bool perf);

  /* Load files from logbook entry */
  void flightplanFileClicked();
  void perfFileClicked();

  /* Fuel units in combo box changed - update widgets */
  void fuelUnitsChanged();

  /* Add airport to record if ident is valid */
  void setAirport(const QString& ident, const QString& prefix, bool includeIdent);

  /* Remove airport including ident */
  void removeAirport(const QString& prefix);

  /* Remember fuel unit to detect changes in widget */
  bool volumeCurrent = false;

  Ui::LogdataDialog *ui;
  atools::sql::SqlRecord *record = nullptr;
  ld::LogdataDialogMode editMode;
  UnitStringTool *units = nullptr;

  /* List of all multi edit checkboxes */
  QVector<QCheckBox *> editCheckBoxList;

};

#endif // LNM_LOGDATADIALOG_H
