/*****************************************************************************
* Copyright 2015-2025 Alexander Barthel alex@littlenavmap.org
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
#include <QSize>

class QLabel;
namespace Ui {
class OptionsDialog;
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
class LinkTooltipHandler;
class ListWidgetIndex;
class GridDelegate;
class WidgetZoomHandler;
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

  /* Load and select best language option and fill combo box. */
  void initLanguage();

  /* Copy main menu actions to allow using shortcuts in the non-modal dialog too */
  void initActions();

  /* Saves the state of all widgets */
  void saveState();

  /* Restores state of all widgets. Has to be called before getting the OptionData instance. */
  void restoreState();

  /* Assign default size and center window */
  void resetWindowLayout();

  /* Get override region settings options directly from settings file*/
  static bool isOverrideRegion();

  /* Test if a public network is used with a too low update rate */
  void checkOfficialOnlineUrls();

  /* Enable or disable tooltips changed */
  void updateTooltipOption();

  void styleChanged();

  /* Set by DirTool if line edit is empty and dir is valid */
  void setCacheMapThemeDir(const QString& mapThemesDir);
  void setCacheOfflineDataPath(const QString& globeDir);

  void fontChanged(const QFont& font);

  /* Called by signal from WebController */
  void webserverStatusChanged(bool);

signals:
  /* Emitted whenever OK or Apply is pressed on the dialog window */
  void optionsChanged();

  /* QGuiApplication::fontChanged is emitted for font changes */

private:
  /* Catch close button too since dialog is kept alive */
  virtual void reject() override;

  /* Dialog shown */
  virtual void showEvent(QShowEvent *) override;

  /* Dialog hidden. Save state. */
  virtual void hideEvent(QHideEvent *) override;

  void updateWidgetStates();
  void updateTrailStates();

  void buttonBoxClicked(QAbstractButton *button);

  /* Copy widget states to OptionData object */
  void widgetsToOptionData();

  /* Copy OptionData object to widget */
  void optionDataToWidgets(const OptionData& data);

  void widgetToMapThemeKeys(OptionData& data);
  void mapThemeKeysToWidget(const OptionData& data);

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
  void updateFlightplanColorWidgets();
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
  void updateNavOptions();

  /* Online networks */
  void updateOnlineWidgetStatus();
  void onlineTestStatusUrlClicked();
  void onlineTestWhazzupUrlClicked();
  void onlineTestUrl(const QString& url, bool statusFile);

  /* Add a dialog page */
  void addPageListItem(const QString& id, const QString& text, const QString& tooltip, const QString& iconPath);
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

  void mapboxUserMapClicked();

  /* Fill combo box with available languages and select best match. English, otherwise.*/
  void udpdateLanguageComboBox(const QString& lang);
  void languageChanged(int);

  /* Font selection for map and GUI */
  void selectGuiFontClicked();
  void resetGuiFontClicked();
  void selectMapFontClicked();
  void resetMapFontClicked();
  void buildFontDialog(const QFont& initialFont);
  void toolbarSizeClicked();
  void adjustGuiFont();

  void flightplanPatterShortClicked();
  void flightplanPatterLongClicked();
  void updateFlightplanExample();
  void updateLinks();
  void colorButtonClicked(QColor& color);
  void updateGuiWidgets();

  void hintLinkActivated(const QString& link);
  void updateLinkTooltipHandler();

  /* Converts range ring string to vector of floats. Falls back to 100 units single ring if nothing is valid.
   * Uses current locale to convert numbers and check min and max. */
  QList<float> rangeStringToFloat(const QString& rangeStr) const;
  QString rangeFloatToString(const QList<float>& ranges) const;
  void mapThemeKeyEdited(QTableWidgetItem *item);

  void searchTextEdited(const QString& text);

  /* Save and reload window position and size */
  void restoreDialogState();
  void saveDialogState();

  QString guiLanguage, guiFont, mapFont;
  QColor flightplanColor, flightplanOutlineColor, flightplanProcedureColor, flightplanActiveColor, trailColor, measurementColor,
         flightplanPassedColor, highlightFlightplanColor, highlightSearchColor, highlightProfileColor;

  Ui::OptionsDialog *ui;
  QMainWindow *mainWindow;

  /* All widgets having their state saved */
  QList<QObject *> widgets;

  /* Labels showing a hint and having a internal link to a page */
  QList<QLabel *> hintLabels;

  /* All labels having a http link */
  QList<QLabel *> linkLabels;

  QHash<QString, QListWidgetItem *> listWidgetItemIndex;

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

  atools::gui::WidgetZoomHandler *zoomHandlerLabelTree = nullptr, *zoomHandlerMapThemeKeysTable = nullptr,
                                 *zoomHandlerDatabaseInclude = nullptr, *zoomHandlerDatabaseExclude = nullptr,
                                 *zoomHandlerDatabaseAddonExclude = nullptr;

  atools::gui::GridDelegate *gridDelegate = nullptr;

  atools::gui::ListWidgetIndex *listWidgetIndex = nullptr;

  atools::gui::LinkTooltipHandler *linkTooltipHandler = nullptr;

  /* Size as given in UI */
  QSize defaultSize;
};

#endif // LITTLENAVMAP_OPTIONSDIALOG_H
