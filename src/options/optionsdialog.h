/*****************************************************************************
* Copyright 2015-2018 Alexander Barthel albar965@mailbox.org
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
class QMainWindow;
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
  OptionsDialog(QMainWindow *parentWindow);
  virtual ~OptionsDialog();

  /* Saves the state of all widgets */
  void saveState();

  /* Restores state of all widgets. Has to be called before getting the OptionData instance. */
  void restoreState();

  /* Show the dialog */
  virtual int exec() override;

  static bool isOverrideLanguage();
  static bool isOverrideLocale();

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
  void toFlags2(QCheckBox *checkBox, opts::Flags2 flag);
  void toFlags2(QRadioButton *radioButton, opts::Flags2 flag);
  void fromFlags2(QCheckBox *checkBox, opts::Flags2 flag);
  void fromFlags2(QRadioButton *radioButton, opts::Flags2 flag);

  void selectActiveSkyPathClicked();
  void clearMemCachedClicked();
  void clearDiskCachedClicked();
  void updateWeatherButtonState();
  void updateActiveSkyPathStatus();

  void addDatabaseExcludePathClicked();
  void removeDatabaseExcludePathClicked();
  void addDatabaseAddOnExcludePathClicked();
  void removeDatabaseAddOnExcludePathClicked();
  void simNoFollowAircraftOnScrollClicked(bool state);

  void showDiskCacheClicked();
  void updateDatabaseButtonState();

  void testWeatherNoaaUrlClicked();
  void testWeatherVatsimUrlClicked();
  void testWeatherIvaoUrlClicked();
  void testWeatherUrl(const QString& url);
  void updateWidgetUnits();
  void simUpdatesConstantClicked(bool state);
  void flightplanColorClicked();
  void flightplanActiveColorClicked();
  void flightplanPassedColorClicked();
  void flightplanProcedureColorClicked();
  void trailColorClicked();

  // Add items to the tree widget and to the  displayOptItemIndex
  QTreeWidgetItem *addTopItem(QTreeWidgetItem *root, const QString& text, const QString& tooltip);
  QTreeWidgetItem *addItem(QTreeWidgetItem *root, const QString& text, const QString& tooltip, opts::DisplayOption type,
                           bool checked = false);

  // Copy tree widget states forth and back
  void saveDisplayOptItemStates();
  void restoreDisplayOptItemStates();
  void displayOptWidgetToOptionData();
  void displayOptDataToWidget();
  void updateGuiStyleSpinboxState();
  void applyStyle();
  void updateButtonColors();
  void updateCacheElevationStates();
  void offlineDataSelectClicked();
  void checkUpdateClicked();
  void mapEmptyAirportsClicked(bool state);
  void updateOnlineWidgetStatus();
  void onlineTestStatusUrlClicked();
  void onlineTestWhazzupUrlClicked();
  void onlineTestUrl(const QString& url);

  QVector<int> ringStrToVector(const QString& string) const;

  QColor flightplanColor, flightplanProcedureColor, flightplanActiveColor, trailColor, flightplanPassedColor;

  Ui::Options *ui;
  QMainWindow *mainWindow;
  QList<QObject *> widgets;

  // Validates the space separated list of ring sizes
  RangeRingValidator *rangeRingValidator;

  // Maps options flags to items in the tree widget
  QHash<opts::DisplayOptions, QTreeWidgetItem *> displayOptItemIndex;

  QString doubleSpinBoxOptionsMapZoomShowMapSuffix, doubleSpinBoxOptionsMapZoomShowMapMenuSuffix,
          spinBoxOptionsRouteGroundBufferSuffix, labelOptionsMapRangeRingsText,
          doubleSpinBoxOptionsRouteTodRuleSuffix;

  // Collect data for all available styles
  QVector<QPalette> stylePalettes;
  QStringList stylesheets;

  int lastStyleIndex = 0;

};

#endif // LITTLENAVMAP_OPTIONSDIALOG_H
