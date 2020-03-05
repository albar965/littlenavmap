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

#ifndef LNM_DIALOGRECORDHELPER_H
#define LNM_DIALOGRECORDHELPER_H

#include <functional>

namespace atools {
namespace sql {
class SqlRecord;
}
}

class QLineEdit;
class QSpinBox;
class QComboBox;
class QDateTimeEdit;
class QTextEdit;
class QCheckBox;
class QString;

/*
 * Contains methods to support editing dialogs with multi-edit functionality like the userdata or log edit dialogs.
 */
class DialogRecordHelper
{
public:
  DialogRecordHelper(atools::sql::SqlRecord *recordParam, bool multiEditParam);

  /* Copies value if no multi edit. For multi edit: Copies value if checkbox is not null and set */
  void dialogToRecordStr(QLineEdit *widget, const QString& name, QCheckBox *checkBox = nullptr, bool toUpper = false);

  /* Needs function like Unit::fuelLbsGallonF to convert fuel from GUI to internal value */
  void dialogToRecordFuel(QSpinBox *widget, const QString& name, QCheckBox *checkBox,
                          std::function<float(float value, bool fuelAsVolume)> func, bool fuelAsVolume);
  void dialogToRecordInt(QSpinBox *widget, const QString& name, QCheckBox *checkBox);
  void dialogToRecordInt(QSpinBox *widget, const QString& name, QCheckBox *checkBox,
                         std::function<float(float value)> func);

  void dialogToRecordInt(QComboBox *widget, const QString& name, QCheckBox *checkBox);

  void dialogToRecordStr(QTextEdit *widget, const QString& name, QCheckBox *checkBox);
  void dialogToRecordDateTime(QDateTimeEdit *widget, const QString& name, QCheckBox *checkBox = nullptr);

private:
  /* true if value should be set */
  bool isSetValue(QCheckBox *checkBox);

  /* true if value should be removed (not nulled) */
  bool isRemoveValue(QCheckBox *checkBox);

  atools::sql::SqlRecord *record = nullptr;
  bool multiEdit = false;

};

#endif // LNM_DIALOGRECORDHELPER_H
