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

#ifndef LNM_DATABASEPROGRESSDIALOG_H
#define LNM_DATABASEPROGRESSDIALOG_H

#include <QDialog>

namespace Ui {
class DatabaseProgressDialog;
}

class QAbstractButton;

/*
 * Progress dialog for database loading. Workaround for format and layout limitations in QProgressDialog.
 */
class DatabaseProgressDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit DatabaseProgressDialog(QWidget *parent, const QString& simulatorName);
  virtual ~DatabaseProgressDialog() override;

  /* Set text for framed label */
  void setLabelText(const QString& text);

  /* Set current, minimum and maximum value for progress */
  void setValue(int value);
  void setMinimum(int value);
  void setMaximum(int value);

  /* true if user hit Esc or clicked Cancel */
  bool wasCanceled()
  {
    return canceled;
  }

  /* Replace cancel button with use button after loading */
  void setFinishedState();

private:
  /* Save and restore dialog size. Called in constructor and destructor. */
  void saveState();
  void restoreState();

  /* Overload and call manually to avoid closing on cancel. Sets canceled to true */
  virtual void reject() override;
  void buttonBoxClicked(QAbstractButton *button);

  Ui::DatabaseProgressDialog *ui;

  /* Use clicked cancel button */
  bool canceled = false, finishedState = false;
};

#endif // LNM_DATABASEPROGRESSDIALOG_H
