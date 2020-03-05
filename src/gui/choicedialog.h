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

namespace Ui {
class ChoiceDialog;
}

class QAbstractButton;
class QCheckBox;

/* A dialog helper that allows to show the user a list of checkboxes. */
class ChoiceDialog :
  public QDialog
{
  Q_OBJECT

public:
  /* settingsPrefixParam is used to save the dialog and checkbox state.
   * helpBaseUrlParam is the base URL of the help system. Help button will be hidden if empty.*/
  ChoiceDialog(QWidget *parent, const QString& title, const QString& header, const QString& settingsPrefixParam,
               const QString& helpBaseUrlParam);
  virtual ~ChoiceDialog() override;

  /* Add a checkbox with the given id, text and tooltip */
  void add(int id, const QString& text, const QString& tooltip = QString(), bool checked = false);

  /* Call after adding all buttons to restore button state */
  void restoreState();

  /* Get the ids of the checked checkboxes after calling exec */
  QVector<int> getCheckedIds() const;

  /* true if box for id is checked */
  bool isChecked(int id) const;

private:
  QVector<std::pair<int, bool> > getCheckState() const;
  void buttonBoxClicked(QAbstractButton *button);
  void saveState();

  Ui::ChoiceDialog *ui;
  QString helpBaseUrl, settingsPrefix;

  /* Maps user given id to check box. */
  QHash<int, QCheckBox *> index;
};

#endif // LNM_CHOICEDIALOG_H
