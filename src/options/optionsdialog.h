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

#ifndef LITTLENAVMAP_OPTIONSDIALOG_H
#define LITTLENAVMAP_OPTIONSDIALOG_H

#include "options/optiondata.h"

#include <QDialog>

namespace Ui {
class Options;
}

class QAbstractButton;
class MainWindow;
class OptionData;
class QCheckBox;
class QRadioButton;
class RangeRingValidator;
class QTreeWidgetItem;

/* Takes care about loading, changing and saving of global options.
 * All default options are defined in the widgets in the options.ui file.
 * OptionData will be populated by the OptionsDialog which loads widget data from the settings
 * and transfers this data into the OptionData class. */
class OptionsDialog :
  public QDialog
{
  Q_OBJECT

public:
  OptionsDialog(MainWindow *parentWindow);
  virtual ~OptionsDialog();

  /* Saves the state of all widgets */
  void saveState();

  /* Restores state of all widgets. Has to be called before getting the OptionData instance. */
  void restoreState();

  /* Show the dialog */
  virtual int exec() override;

signals:
  /* Emitted whenever OK or Apply is pressed on the dialog window */
  void optionsChanged();

private:
  void buttonBoxClicked(QAbstractButton *button);
  void widgetsToOptionData();
  void optionDataToWidgets();
  void toFlags(QCheckBox *checkBox, opts::Flags flag);
  void toFlags(QRadioButton *radioButton, opts::Flags flag);

  void fromFlags(QCheckBox *checkBox, opts::Flags flag);
  void fromFlags(QRadioButton *radioButton, opts::Flags flag);

  void selectActiveSkyPathClicked();
  void clearMemCachedClicked();
  void clearDiskCachedClicked();
  void updateWeatherButtonState();
  void updateActiveSkyPathStatus();

  void addDatabaseExcludePathClicked();
  void removeDatabaseExcludePathClicked();
  void addDatabaseAddOnExcludePathClicked();
  void removeDatabaseAddOnExcludePathClicked();

  void showDiskCacheClicked();
  void updateDatabaseButtonState();

  void testWeatherNoaaUrlClicked();
  void testWeatherVatsimUrlClicked();
  void testWeatherUrl(const QString& url);
  void updateWidgetUnits();
  void simUpdatesConstantClicked(bool state);
  void flightplanColorClicked();
  void trailColorClicked();

  // Add items to the tree widget and to the  displayOptItemIndex
  QTreeWidgetItem *addTopItem(QTreeWidgetItem *root, QString text);
  QTreeWidgetItem *addItem(QTreeWidgetItem *root, QString text, opts::DisplayOption type,
                           bool checked = false);

  // Copy tree widget states forth and back
  void saveDisplayOptItemStates();
  void restoreDisplayOptItemStates();
  void displayOptWidgetToOptionData();
  void displayOptDataToWidget();

  QVector<int> ringStrToVector(const QString& string) const;

  QColor flightplanColor, trailColor;

  Ui::Options *ui;
  MainWindow *mainWindow;
  QList<QObject *> widgets;

  // Validates the space separated list of ring sizes
  RangeRingValidator *rangeRingValidator;

  // Maps options flags to items in the tree widget
  QHash<opts::DisplayOptions, QTreeWidgetItem *> displayOptItemIndex;

  QString doubleSpinBoxOptionsMapZoomShowMapSuffix, doubleSpinBoxOptionsMapZoomShowMapMenuSuffix,
          spinBoxOptionsRouteGroundBufferSuffix, labelOptionsMapRangeRingsText,
          doubleSpinBoxOptionsRouteTodRuleSuffix;

};

#endif // LITTLENAVMAP_OPTIONSDIALOG_H
