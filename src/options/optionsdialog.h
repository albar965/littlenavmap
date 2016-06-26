/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
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

#ifndef LITTLENAVMAP_OPTIONS_H
#define LITTLENAVMAP_OPTIONS_H

#include "options/optiondata.h"

#include <QDialog>
#include <QRegularExpressionValidator>

namespace Ui {
class Options;
}

class QAbstractButton;
class MainWindow;
class OptionData;
class QCheckBox;
class QRadioButton;

class OptionsDialog :
  public QDialog
{
  Q_OBJECT

public:
  OptionsDialog(MainWindow *parentWindow, OptionData *optionDataParam);
  virtual ~OptionsDialog();

  void saveState();
  void restoreState();

  virtual int exec() override;

signals:
  void optionsChanged(const OptionData *optionData);

private:
  void buttonBoxClicked(QAbstractButton *button);
  void toOptionData();
  void fromOptionData();

  Ui::Options *ui;
  MainWindow *mainWindow;

  QList<QObject *> widgets;

  /* OptionData is owned by main window */
  OptionData *optionData;

  QRegularExpressionValidator rangeRingValidator;

  void toFlags(QCheckBox *checkBox, opts::Flags flag);
  void toFlags(QRadioButton *checkBox, opts::Flags flag);

  void fromFlags(QCheckBox *checkBox, opts::Flags flag);
  void fromFlags(QRadioButton *checkBox, opts::Flags flag);
  void resetDefaultClicked();

  void selectAsnPathClicked();
  void clearMemCachedClicked();
  void clearDiskCachedClicked();

};

#endif // LITTLENAVMAP_OPTIONS_H
