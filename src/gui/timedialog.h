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

#ifndef LNM_TIMEDIALOG_H
#define LNM_TIMEDIALOG_H

#include <QDialog>

namespace Ui {
class TimeDialog;
}

class QAbstractButton;

/*
 * Dialog that allows to enter a UTC time and date. Used to define the user defined sun shading time.
 *
 * Sets time and shading in map widget.
 */
class TimeDialog :
  public QDialog
{
  Q_OBJECT

public:
  explicit TimeDialog(QWidget *parent, const QDateTime& datetime);
  virtual ~TimeDialog();

  QDateTime getDateTime() const;

private:
  Ui::TimeDialog *ui;
  void buttonBoxClicked(QAbstractButton *button);

};

#endif // LNM_TIMEDIALOG_H
