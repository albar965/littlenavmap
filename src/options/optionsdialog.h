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
class QSpinBox;
class UnitStringTool;
class QListWidgetItem;
class QListWidget;

/* Takes care about loading, changing and saving of global options.
 * All default options are defined in the widgets in the options.ui file.
 * OptionData will be populated by the OptionsDialog which loads widget data from the settings
 * and transfers this data into the OptionData class.
 *
 * Dialog is kept alive during the whole program lifespan.
 */
class OptionsDialog :
  public QDialog
{
  Q_OBJECT

public:
  OptionsDialog(QMainWindow *parentWindow);
  virtual ~OptionsDialog() override;

  /* Saves the state of all widgets */
  void saveState();

  /* Restores state of all widgets. Has to be called before getting the OptionData instance. */
  void restoreState();

  /* Show the dialog */
  virtual void open() override;

  static bool isOverrideLanguage();
  static bool isOverrideLocale();

  QString selectCacheUserAirspace();

signals:
  /* Emitted whenever OK or Apply is pressed on the dialog window */
  void optionsChanged();

private:
  /* Catch close button too since dialog is kept alive */
  virtual void reject() override;

  void updateWidgetStates();

  void buttonBoxClicked(QAbstractButton *button);
  void widgetsToOptionData();
  void optionDataToWidgets(const OptionData& data);

  void toFlags(QCheckBox *checkBox, opts::Flags flag);
  void toFlags(QRadioButton *radioButton, opts::Flags flag);
  void fromFlags(const OptionData& data, QCheckBox *checkBox, opts::Flags flag);
  void fromFlags(const OptionData& data, QRadioButton *radioButton, opts::Flags flag);

  void toFlags2(QCheckBox *checkBox, opts2::Flags2 flag);
  void toFlags2(QRadioButton *radioButton, opts2::Flags2 flag);
  void fromFlags2(const OptionData& data, QCheckBox *checkBox, opts2::Flags2 flag);
  void fromFlags2(const OptionData& data, QRadioButton *radioButton, opts2::Flags2 flag);

  void toFlagsWeather(QCheckBox *checkBox, optsw::FlagsWeather flag);
  void fromFlagsWeather(const OptionData& data, QCheckBox *checkBox, optsw::FlagsWeather flag);

  void selectActiveSkyPathClicked();
  void selectXplanePathClicked();
  void weatherXplaneWindPathSelectClicked();
  void clearMemCachedClicked();
  void clearDiskCachedClicked();
  void updateWeatherButtonState();
  void updateActiveSkyPathStatus();
  void updateXplanePathStatus();
  void updateXplaneWindStatus();

  void addDatabaseExcludeDirClicked();
  void addDatabaseExcludeFileClicked();

  void removeDatabaseExcludePathClicked();
  void addDatabaseAddOnExcludePathClicked();
  void removeDatabaseAddOnExcludePathClicked();
  void simNoFollowAircraftOnScrollClicked(bool state);

  void showDiskCacheClicked();
  void updateDatabaseButtonState();

  void testWeatherNoaaUrlClicked();
  void testWeatherVatsimUrlClicked();
  void testWeatherIvaoUrlClicked();
  void testWeatherNoaaWindUrlClicked();
  void testWeatherUrl(const QString& url);
  void updateWidgetUnits();
  void simUpdatesConstantClicked(bool state);
  void flightplanColorClicked();
  void flightplanActiveColorClicked();
  void flightplanPassedColorClicked();
  void flightplanProcedureColorClicked();
  void trailColorClicked();
  void onlineDisplayRangeClicked();
  void eastWestRuleClicked();

  // Add items to the tree widget and to the  displayOptItemIndex
  QTreeWidgetItem *addTopItem(QTreeWidgetItem *root, const QString& text, const QString& tooltip);

  template<typename TYPE>
  QTreeWidgetItem *addItem(QTreeWidgetItem *root, QHash<TYPE, QTreeWidgetItem *>& index,
                           const QString& text, const QString& tooltip, TYPE type, bool checked = false) const;

  template<typename TYPE>
  void restoreOptionItemStates(const QHash<TYPE, QTreeWidgetItem *>& index, const QString& optionPrefix) const;

  template<typename TYPE>
  void saveDisplayOptItemStates(const QHash<TYPE, QTreeWidgetItem *>& index, const QString& optionPrefix) const;

  // Copy tree widget states forth and back
  template<typename TYPE>
  void displayOptDataToWidget(const TYPE& type, const QHash<TYPE, QTreeWidgetItem *>& index) const;

  template<typename TYPE>
  void displayOptWidgetToOptionData(TYPE& type, const QHash<TYPE, QTreeWidgetItem *>& index) const;

  void updateButtonColors();
  void updateCacheElevationStates();
  void updateCacheUserAirspaceStates();
  void offlineDataSelectClicked();
  void userAirspacePathSelectClicked();
  void checkUpdateClicked();
  void mapEmptyAirportsClicked(bool state);
  void updateOnlineWidgetStatus();
  void onlineTestStatusUrlClicked();
  void onlineTestWhazzupUrlClicked();
  void onlineTestUrl(const QString& url, bool statusFile);
  int displayOnlineRangeToData(const QSpinBox *spinBox, const QCheckBox *checkButton);
  void displayOnlineRangeFromData(QSpinBox *spinBox, QCheckBox *checkButton, int value);
  void updateNavOptions();

  QVector<int> ringStrToVector(const QString& string) const;

  QListWidgetItem *pageListItem(QListWidget *parent, const QString& text, const QString& tooltip = QString(),
                                const QString& iconPath = QString());
  void changePage(QListWidgetItem *current, QListWidgetItem *previous);

  /* Update label for docroot path */
  void updateWebDocrootStatus();
  void selectWebDocrootClicked();

  /* Show listening address(es)*/
  void updateWebServerStatus();
  void startStopWebServerClicked();

  /* copy values from widgets to server for instant check of parameters */
  void updateWebOptionsFromGui();

  /* Update web server with saved parameters */
  void updateWebOptionsFromData();

  QColor flightplanColor, flightplanProcedureColor, flightplanActiveColor, trailColor, flightplanPassedColor;

  Ui::Options *ui;
  QMainWindow *mainWindow;
  QList<QObject *> widgets;

  // Validates the space separated list of ring sizes
  RangeRingValidator *rangeRingValidator;

  // Maps options flags to items in the tree widget
  QHash<optsd::DisplayOptions, QTreeWidgetItem *> displayOptItemIndex;
  QHash<optsd::DisplayOptionsNavAid, QTreeWidgetItem *> displayOptItemIndexNavAid;
  QHash<optsd::DisplayOptionsRose, QTreeWidgetItem *> displayOptItemIndexRose;
  QHash<optsd::DisplayOptionsRoute, QTreeWidgetItem *> displayOptItemIndexRoute;

  UnitStringTool *units = nullptr;
};

#endif // LITTLENAVMAP_OPTIONSDIALOG_H
