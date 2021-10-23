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

#ifndef LNM_CHOICEDIALOG_H
#define LNM_CHOICEDIALOG_H

#include <QDialog>
#include <QSet>

namespace Ui {
class ChoiceDialog;
}

class QAbstractButton;
class QCheckBox;

/*
 * A configurable dialog that shows the user a list of checkboxes.
 */
class ChoiceDialog :
  public QDialog
{
  Q_OBJECT

public:
  /* settingsPrefixParam is used to save the dialog and checkbox state.
   * helpBaseUrlParam is the base URL of the help system. Help button will be hidden if empty.*/
  ChoiceDialog(QWidget *parent, const QString& title, const QString& description, const QString& settingsPrefixParam,
               const QString& helpBaseUrlParam);
  virtual ~ChoiceDialog() override;

  ChoiceDialog(const ChoiceDialog& other) = delete;
  ChoiceDialog& operator=(const ChoiceDialog& other) = delete;

  /* Add a checkbox with the given id, text and tooltip */
  template<typename TYPE>
  void addCheckBox(TYPE id, const QString& text, const QString& tooltip = QString(), bool checked = false,
                   bool disabled = false, bool hidden = false)
  {
    addCheckBoxInt(static_cast<int>(id), text, tooltip, checked, disabled, hidden);
  }

  /* Shortcut to add a disabled widget */
  template<typename TYPE>
  void addCheckBoxDisabled(TYPE id, const QString& text, const QString& tooltip, bool checked)
  {
    addCheckBoxDisabledInt(static_cast<int>(id), text, tooltip, checked);
  }

  /* Shortcut to add a hidden, disabled and unchecked widget.
   * Useful if different configurations are saved in the same setting variable. */
  template<typename TYPE>
  void addCheckBoxHidden(TYPE id)
  {
    addCheckBoxHiddenInt(static_cast<int>(id));
  }

  /* true if box for id is checked and enabled */
  template<typename TYPE>
  bool isChecked(TYPE id) const
  {
    return isCheckedInt(static_cast<int>(id));
  }

  /* Get checkbox for given id */
  template<typename TYPE>
  QCheckBox *getCheckBox(TYPE id)
  {
    return getCheckBoxInt(static_cast<int>(id));
  }

  /* Add a separator line */
  void addLine();

  /* Call after adding all buttons to restore button state */
  void restoreState();

signals:
  /* Emitted when a checkbox is toggled */
  void checkBoxToggled(int id, bool checked);

private:
  QVector<std::pair<int, bool> > getCheckState() const;
  void buttonBoxClicked(QAbstractButton *button);
  void checkBoxToggledInternal(bool checked);
  void saveState();

  void addCheckBoxInt(int id, const QString& text, const QString& tooltip = QString(), bool checked = false,
                      bool disabled = false, bool hidden = false);
  void addCheckBoxDisabledInt(int id, const QString& text, const QString& tooltip, bool checked);
  void addCheckBoxHiddenInt(int id);
  bool isCheckedInt(int id) const;
  QCheckBox *getCheckBoxInt(int id);

  Ui::ChoiceDialog *ui;
  QString helpBaseUrl, settingsPrefix;

  /* Maps user given id to check box. */
  QHash<int, QCheckBox *> index;
};

#endif // LNM_CHOICEDIALOG_H
