/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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
#include <QLocale>

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
class QFontDialog;
class QTableWidgetItem;
class QTableWidget;

namespace atools {
namespace gui {
class ListWidgetIndex;
class GridDelegate;
class ItemViewZoomHandler;
}
}
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
  explicit OptionsDialog(QMainWindow *parentWindow);
  virtual ~OptionsDialog() override;

  OptionsDialog(const OptionsDialog& other) = delete;
  OptionsDialog& operator=(const OptionsDialog& other) = delete;

  /* Saves the state of all widgets */
  void saveState();

  /* Restores state of all widgets. Has to be called before getting the OptionData instance. */
  void restoreState();

  /* Show the dialog */
  virtual void open() override;

  /* Get override region settings options directly from settings file*/
  static bool isOverrideRegion();

  /* Get locale name like "en_US" or "de" directly from settings file */
  static QString getLocale();

  /* Test if a public network is used with a too low update rate */
  void checkOfficialOnlineUrls();

  /* Enable or disable tooltips changed */
  void updateTooltipOption();

  void styleChanged();

  /* Set by DirTool if line edit is empty and dir is valid */
  void setCacheMapThemeDir(const QString& mapThemesDir);
  void setCacheOfflineDataPath(const QString& globeDir);

signals:
  /* Emitted whenever OK or Apply is pressed on the dialog window */
  void optionsChanged();

private:
  /* Catch close button too since dialog is kept alive */
  virtual void reject() override;

  void updateWidgetStates();
  void updateTrailStates();

  void buttonBoxClicked(QAbstractButton *button);
  void widgetsToOptionData();
  void optionDataToWidgets(const OptionData& data);

  void widgetToMapThemeKeys(OptionData& data);
  void mapThemeKeysToWidget(const OptionData& data);

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
  void selectXplane11PathClicked();
  void selectXplane12PathClicked();
  void weatherXplane11WindPathSelectClicked();
  void clearMemCachedClicked();
  void updateWeatherButtonState();
  void updateActiveSkyPathStatus();
  void updateXplane11PathStatus();
  void updateXplane12PathStatus();
  void updateXplaneWindStatus();
  void updateFlightPlanColorWidgets();
  void updateHighlightWidgets();

  void addDatabaseIncludeDirClicked();
  void removeDatabaseIncludePathClicked();

  void addDatabaseExcludeDirClicked();
  void addDatabaseExcludeFileClicked();

  void removeDatabaseExcludePathClicked();
  void addDatabaseAddOnExcludePathClicked();
  void removeDatabaseAddOnExcludePathClicked();

  void addDatabaseTableItem(QTableWidget *widget, const QString& path);
  void addDatabaseTableItems(QTableWidget *widget, const QStringList& strings);
  void removeSelectedDatabaseTableItems(QTableWidget *widget);

  void updateWhileFlyingWidgets(bool);

  void showDiskCacheClicked();
  void updateDatabaseButtonState();

  void testWeatherNoaaUrlClicked();
  void testWeatherVatsimUrlClicked();
  void testWeatherIvaoUrlClicked();
  void testWeatherNoaaWindUrlClicked();

  void resetWeatherNoaaUrlClicked();
  void resetWeatherVatsimUrlClicked();
  void resetWeatherIvaoUrlClicked();
  void resetWeatherNoaaWindUrlClicked();

  void updateWidgetUnits();

  void flightplanColorClicked();
  void flightplanOutlineColorClicked();
  void flightplanActiveColorClicked();
  void flightplanPassedColorClicked();
  void flightplanProcedureColorClicked();

  void mapHighlightFlightplanColorClicked();
  void mapHighlightSearchColorClicked();
  void mapHighlightProfileColorClicked();

  void trailColorClicked();
  void mapMeasurementColorClicked();
  void onlineDisplayRangeClicked();
  void eastWestRuleClicked();

  // Add items to the tree widget and to the  displayOptItemIndex
  QTreeWidgetItem *addTopItem(const QString& text, const QString& description);

  template<typename TYPE>
  QTreeWidgetItem *addItem(QTreeWidgetItem *root, QHash<TYPE, QTreeWidgetItem *>& index,
                           const QString& text, const QString& description, TYPE type, bool checked = false) const;

  template<typename TYPE>
  void restoreOptionItemStates(const QHash<TYPE, QTreeWidgetItem *>& index, const QString& optionPrefix) const;

  template<typename TYPE>
  void saveDisplayOptItemStates(const QHash<TYPE, QTreeWidgetItem *>& index, const QString& optionPrefix) const;

  // Copy tree widget states forth and back
  template<typename TYPE>
  void displayOptDataToWidget(const TYPE& type, const QHash<TYPE, QTreeWidgetItem *>& index) const;

  template<typename TYPE>
  void displayOptWidgetToOptionData(TYPE& type, const QHash<TYPE, QTreeWidgetItem *>& index) const;

  void updateFontFromData();
  void updateMapFontLabel();
  void updateGuiFontLabel();
  void updateButtonColors();
  void updateCacheElevationStates();
  void updateCacheMapThemeDir();
  void offlineDataSelectClicked();
  void mapThemeDirSelectClicked();
  void checkUpdateClicked();
  void mapEmptyAirportsClicked(bool state);
  int displayOnlineRangeToData(const QSpinBox *spinBox, const QCheckBox *checkButton);
  void displayOnlineRangeFromData(QSpinBox *spinBox, QCheckBox *checkButton, int value);
  void updateNavOptions();

  /* Online networks */
  void updateOnlineWidgetStatus();
  void onlineTestStatusUrlClicked();
  void onlineTestWhazzupUrlClicked();
  void onlineTestUrl(const QString& url, bool statusFile);

  /* Add a dialog page */
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

  void mapClickAirportProcsToggled();

  void mapboxUserMapClicked();

  /* Fill combo box with available languages and select best match. English, otherwise.*/
  void udpdateLanguageComboBox(const QString& guiLanguage);
  void languageChanged(int);

  /* Font selection for map and GUI */
  void selectGuiFontClicked();
  void resetGuiFontClicked();
  void selectMapFontClicked();
  void resetMapFontClicked();
  void buildFontDialog();
  void toolbarSizeClicked();

  void flightplanPatterShortClicked();
  void flightplanPatterLongClicked();
  void updateFlightplanExample();
  void updateLinks();
  void colorButtonClicked(QColor& color);
  void updateGuiWidgets();

  /* Converts range ring string to vector of floats. Falls back to 100 units single ring if nothing is valid.
   * Uses current locale to convert numbers and check min and max. */
  QVector<float> rangeStringToFloat(const QString& rangeStr) const;
  QString rangeFloatToString(const QVector<float>& ranges) const;
  void mapThemeKeyEdited(QTableWidgetItem *item);

  void searchTextEdited(const QString& text);

  QString guiLanguage, guiFont, mapFont;
  QColor flightplanColor, flightplanOutlineColor, flightplanProcedureColor, flightplanActiveColor, trailColor, measurementColor,
         flightplanPassedColor, highlightFlightplanColor, highlightSearchColor, highlightProfileColor;

  Ui::Options *ui;
  QMainWindow *mainWindow;
  QList<QObject *> widgets;

  // Maps options flags to items in the tree widget
  QHash<optsac::DisplayOptionsUserAircraft, QTreeWidgetItem *> displayOptItemIndexUser;
  QHash<optsac::DisplayOptionsAiAircraft, QTreeWidgetItem *> displayOptItemIndexAi;
  QHash<optsd::DisplayOptionsAirport, QTreeWidgetItem *> displayOptItemIndexAirport;
  QHash<optsd::DisplayOptionsNavAid, QTreeWidgetItem *> displayOptItemIndexNavAid;
  QHash<optsd::DisplayOptionsAirspace, QTreeWidgetItem *> displayOptItemIndexAirspace;
  QHash<optsd::DisplayOptionsRose, QTreeWidgetItem *> displayOptItemIndexRose;
  QHash<optsd::DisplayOptionsMeasurement, QTreeWidgetItem *> displayOptItemIndexMeasurement;
  QHash<optsd::DisplayOptionsRoute, QTreeWidgetItem *> displayOptItemIndexRoute;

  UnitStringTool *units = nullptr;

  QFontDialog *fontDialog = nullptr;

  atools::gui::ItemViewZoomHandler *zoomHandlerLabelTree = nullptr, *zoomHandlerMapThemeKeysTable = nullptr,
                                   *zoomHandlerDatabaseInclude = nullptr, *zoomHandlerDatabaseExclude = nullptr,
                                   *zoomHandlerDatabaseAddonExclude = nullptr;

  atools::gui::GridDelegate *gridDelegate = nullptr;

  atools::gui::ListWidgetIndex *listWidgetIndex = nullptr;
};

#endif // LITTLENAVMAP_OPTIONSDIALOG_H
