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

#include "options/optionsdialog.h"

#include "common/constants.h"
#include "gui/mainwindow.h"
#include "ui_options.h"
#include "common/weatherreporter.h"
#include "gui/widgetstate.h"
#include "gui/dialog.h"
#include "settings/settings.h"
#include "mapgui/mapwidget.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QDesktopServices>

#include <marble/MarbleModel.h>
#include <marble/MarbleDirs.h>

const int MAX_RANGE_RING_SIZE = 4000;
const int MAX_RANGE_RINGS = 10;

using atools::settings::Settings;

/* Validates the space separated list of range ring sizes */
class RangeRingValidator :
  public QValidator
{
public:
  RangeRingValidator();

private:
  virtual QValidator::State validate(QString& input, int& pos) const override;

  bool ringStrToVector(const QString& str) const;

  QRegularExpressionValidator regexpVal;
};

RangeRingValidator::RangeRingValidator()
{
  // Multiple numbers starting 1-9 max 4 digits separated by space
  regexpVal.setRegularExpression(QRegularExpression("^([1-9]\\d{0,3} )*[1-9]\\d{1,3}$"));
}

QValidator::State RangeRingValidator::validate(QString& input, int& pos) const
{
  State state = regexpVal.validate(input, pos);
  if(state == Invalid)
    return Invalid;
  else
  {
    // Half valid check number of rings and distance
    if(!ringStrToVector(input))
      return Invalid;
  }

  return state;
}

/* Check for maximum of 10 rings with a maximum size of 4000 nautical miles.
 * @return true if string is ok
 */
bool RangeRingValidator::ringStrToVector(const QString& input) const
{
  int numRing = 0;
  for(const QString& str : input.split(" "))
  {
    QString val = str.trimmed();
    if(!val.isEmpty())
    {
      bool ok;
      int num = val.toInt(&ok);
      if(!ok || num > MAX_RANGE_RING_SIZE)
        return false;

      numRing++;
    }
  }
  return numRing <= MAX_RANGE_RINGS;
}

// ------------------------------------------------------------------------

OptionsDialog::OptionsDialog(MainWindow *parentWindow)
  : QDialog(parentWindow), ui(new Ui::Options), mainWindow(parentWindow)
{
  ui->setupUi(this);

  rangeRingValidator = new RangeRingValidator;

  // Create widget list for state saver
  widgets.append(ui->tabWidgetOptions);
  widgets.append(ui->checkBoxOptionsGuiCenterKml);
  widgets.append(ui->checkBoxOptionsGuiCenterRoute);
  widgets.append(ui->checkBoxOptionsMapEmptyAirports);
  widgets.append(ui->checkBoxOptionsRouteEastWestRule);
  widgets.append(ui->checkBoxOptionsRoutePreferNdb);
  widgets.append(ui->checkBoxOptionsRoutePreferVor);
  widgets.append(ui->checkBoxOptionsStartupLoadKml);
  widgets.append(ui->checkBoxOptionsStartupLoadMapSettings);
  widgets.append(ui->checkBoxOptionsStartupLoadRoute);
  widgets.append(ui->checkBoxOptionsWeatherInfoAsn);
  widgets.append(ui->checkBoxOptionsWeatherInfoNoaa);
  widgets.append(ui->checkBoxOptionsWeatherInfoVatsim);
  widgets.append(ui->checkBoxOptionsWeatherTooltipAsn);
  widgets.append(ui->checkBoxOptionsWeatherTooltipNoaa);
  widgets.append(ui->checkBoxOptionsWeatherTooltipVatsim);
  widgets.append(ui->lineEditOptionsMapRangeRings);
  widgets.append(ui->lineEditOptionsWeatherAsnPath);
  widgets.append(ui->lineEditOptionsWeatherNoaaUrl);
  widgets.append(ui->lineEditOptionsWeatherVatsimUrl);
  widgets.append(ui->listWidgetOptionsDatabaseAddon);
  widgets.append(ui->listWidgetOptionsDatabaseExclude);
  widgets.append(ui->radioButtonOptionsMapScrollFull);
  widgets.append(ui->radioButtonOptionsMapScrollNone);
  widgets.append(ui->radioButtonOptionsMapScrollNormal);
  widgets.append(ui->radioButtonOptionsSimUpdateFast);
  widgets.append(ui->radioButtonOptionsSimUpdateLow);
  widgets.append(ui->radioButtonOptionsSimUpdateMedium);
  widgets.append(ui->spinBoxOptionsSimUpdateBox);
  widgets.append(ui->radioButtonOptionsStartupShowHome);
  widgets.append(ui->radioButtonOptionsStartupShowLast);
  widgets.append(ui->spinBoxOptionsCacheDiskSize);
  widgets.append(ui->spinBoxOptionsCacheMemorySize);
  widgets.append(ui->spinBoxOptionsGuiInfoText);
  widgets.append(ui->spinBoxOptionsGuiRouteText);
  widgets.append(ui->spinBoxOptionsGuiSearchText);
  widgets.append(ui->spinBoxOptionsGuiSimInfoText);
  widgets.append(ui->spinBoxOptionsMapClickRect);
  widgets.append(ui->spinBoxOptionsMapSymbolSize);
  widgets.append(ui->spinBoxOptionsMapTextSize);
  widgets.append(ui->spinBoxOptionsMapTooltipRect);
  widgets.append(ui->doubleSpinBoxOptionsMapZoomShowMap);
  widgets.append(ui->spinBoxOptionsRouteGroundBuffer);

  ui->lineEditOptionsMapRangeRings->setValidator(rangeRingValidator);

  connect(ui->buttonBoxOptions, &QDialogButtonBox::clicked, this, &OptionsDialog::buttonBoxClicked);

  // Weather widgets
  connect(ui->pushButtonOptionsWeatherAsnPathSelect, &QPushButton::clicked,
          this, &OptionsDialog::selectActiveSkyPathClicked);

  connect(ui->lineEditOptionsWeatherAsnPath, &QLineEdit::editingFinished,
          this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherAsnPath, &QLineEdit::textEdited,
          this, &OptionsDialog::updateActiveSkyPathStatus);

  connect(ui->lineEditOptionsWeatherNoaaUrl, &QLineEdit::textEdited,
          this, &OptionsDialog::updateWeatherButtonState);
  connect(ui->lineEditOptionsWeatherVatsimUrl, &QLineEdit::textEdited,
          this, &OptionsDialog::updateWeatherButtonState);

  // Database exclude path
  connect(ui->pushButtonOptionsDatabaseAddExclude, &QPushButton::clicked,
          this, &OptionsDialog::addDatabaseExcludePathClicked);
  connect(ui->pushButtonOptionsDatabaseRemoveExclude, &QPushButton::clicked,
          this, &OptionsDialog::removeDatabaseExcludePathClicked);
  connect(ui->listWidgetOptionsDatabaseExclude, &QListWidget::currentRowChanged,
          this, &OptionsDialog::updateDatabaseButtonState);

  // Database addon exclude path
  connect(ui->pushButtonOptionsDatabaseAddAddon, &QPushButton::clicked,
          this, &OptionsDialog::addDatabaseAddOnExcludePathClicked);
  connect(ui->pushButtonOptionsDatabaseRemoveAddon, &QPushButton::clicked,
          this, &OptionsDialog::removeDatabaseAddOnExcludePathClicked);
  connect(ui->listWidgetOptionsDatabaseAddon, &QListWidget::currentRowChanged,
          this, &OptionsDialog::updateDatabaseButtonState);

  // Cache
  connect(ui->pushButtonOptionsCacheClearMemory, &QPushButton::clicked,
          this, &OptionsDialog::clearMemCachedClicked);
  connect(ui->pushButtonOptionsCacheClearDisk, &QPushButton::clicked,
          this, &OptionsDialog::clearDiskCachedClicked);
  connect(ui->pushButtonOptionsCacheShow, &QPushButton::clicked,
          this, &OptionsDialog::showDiskCacheClicked);

  // Weather test buttons
  connect(ui->pushButtonOptionsWeatherNoaaTest, &QPushButton::clicked,
          this, &OptionsDialog::testWeatherNoaaUrlClicked);
  connect(ui->pushButtonOptionsWeatherVatsimTest, &QPushButton::clicked,
          this, &OptionsDialog::testWeatherVatsimUrlClicked);
}

OptionsDialog::~OptionsDialog()
{
  delete rangeRingValidator;
  delete ui;
}

int OptionsDialog::exec()
{
  optionDataToWidgets();
  updateWeatherButtonState();
  updateDatabaseButtonState();

  return QDialog::exec();
}

void OptionsDialog::buttonBoxClicked(QAbstractButton *button)
{
  qDebug() << "Clicked" << button->text();

  if(button == ui->buttonBoxOptions->button(QDialogButtonBox::Apply))
  {
    widgetsToOptionData();
    saveState();
    emit optionsChanged();
    updateActiveSkyPathStatus();
    updateWeatherButtonState();
    updateDatabaseButtonState();
  }
  else if(button == ui->buttonBoxOptions->button(QDialogButtonBox::Ok))
  {
    widgetsToOptionData();
    saveState();
    emit optionsChanged();
    accept();
  }
  else if(button == ui->buttonBoxOptions->button(QDialogButtonBox::Cancel))
    reject();

  else if(button == ui->buttonBoxOptions->button(QDialogButtonBox::RestoreDefaults))
  {
    qDebug() << "OptionsDialog::resetDefaultClicked";

    QMessageBox::StandardButton result = QMessageBox::question(this, QApplication::applicationName(),
                                                               tr("Reset all options to default?"));

    if(result == QMessageBox::Yes)
    {
      // Reset option instance and set it to valid
      OptionData::instanceInternal() = OptionData();
      OptionData::instanceInternal().valid = true;
      optionDataToWidgets();
      saveState();
      emit optionsChanged();
      updateActiveSkyPathStatus();
      updateWeatherButtonState();
      updateDatabaseButtonState();
    }
  }
}

void OptionsDialog::saveState()
{
  optionDataToWidgets();

  // Save widgets to settings
  atools::gui::WidgetState(lnm::OPTIONS_DIALOG_WIDGET,
                           false /* save visibility */, true /* block signals */).save(widgets);

  Settings& settings = Settings::instance();

  // Save the path lists
  QStringList paths;
  for(int i = 0; i < ui->listWidgetOptionsDatabaseExclude->count(); i++)
    paths.append(ui->listWidgetOptionsDatabaseExclude->item(i)->text());
  if(!paths.isEmpty())
    settings.setValue(lnm::OPTIONS_DIALOG_DB_EXCLUDE, paths);

  paths.clear();
  for(int i = 0; i < ui->listWidgetOptionsDatabaseAddon->count(); i++)
    paths.append(ui->listWidgetOptionsDatabaseAddon->item(i)->text());
  if(!paths.isEmpty())
    settings.setValue(lnm::OPTIONS_DIALOG_DB_ADDON_EXCLUDE, paths);
}

void OptionsDialog::restoreState()
{
  atools::gui::WidgetState(lnm::OPTIONS_DIALOG_WIDGET,
                           false /*save visibility*/, true /*block signals*/).restore(widgets);

  Settings& settings = Settings::instance();
  if(settings.contains(lnm::OPTIONS_DIALOG_DB_EXCLUDE))
    ui->listWidgetOptionsDatabaseExclude->addItems(settings.valueStrList(lnm::OPTIONS_DIALOG_DB_EXCLUDE));
  if(settings.contains(lnm::OPTIONS_DIALOG_DB_ADDON_EXCLUDE))
    ui->listWidgetOptionsDatabaseAddon->addItems(settings.valueStrList(lnm::OPTIONS_DIALOG_DB_ADDON_EXCLUDE));

  widgetsToOptionData();
}

/* Test NOAA weather URL and show a dialog with the result */
void OptionsDialog::testWeatherNoaaUrlClicked()
{
  qDebug() << "OptionsDialog::testWeatherNoaaUrlClicked";
  testWeatherUrl(ui->lineEditOptionsWeatherNoaaUrl->text());
}

/* Test Vatsim weather URL and show a dialog with the result */
void OptionsDialog::testWeatherVatsimUrlClicked()
{
  qDebug() << "OptionsDialog::testWeatherVatsimUrlClicked";
  testWeatherUrl(ui->lineEditOptionsWeatherVatsimUrl->text());
}

void OptionsDialog::testWeatherUrl(const QString& url)
{
  QString result;
  if(mainWindow->getWeatherReporter()->testUrl(url, "KORD", result))
    QMessageBox::information(this, QApplication::applicationName(), tr("Success. Result:\n%1").arg(result));
  else
    QMessageBox::warning(this, QApplication::applicationName(), tr("Failed. Reason:\n%1").arg(result));
}

/* Show directory dialog to add exclude path */
void OptionsDialog::addDatabaseExcludePathClicked()
{
  qDebug() << "OptionsDialog::addDatabaseExcludePathClicked";

  QString path = atools::gui::Dialog(this).openDirectoryDialog(
    tr("Open Directory to exclude from Scenery Loading"),
    lnm::OPTIONS_DIALOG_DB_FILE_DLG,
    atools::fs::FsPaths::getSceneryLibraryPath(mainWindow->getCurrentSimulator()));

  if(!path.isEmpty())
  {
    ui->listWidgetOptionsDatabaseExclude->addItem(QDir::toNativeSeparators(path));
    updateDatabaseButtonState();
  }
}

void OptionsDialog::removeDatabaseExcludePathClicked()
{
  qDebug() << "OptionsDialog::removeDatabaseExcludePathClicked";

  // Item removes itself from the list when deleted
  delete ui->listWidgetOptionsDatabaseExclude->currentItem();
  updateDatabaseButtonState();
}

/* Show directory dialog to add add-on exclude path */
void OptionsDialog::addDatabaseAddOnExcludePathClicked()
{
  qDebug() << "OptionsDialog::addDatabaseAddOnExcludePathClicked";

  QString path = atools::gui::Dialog(this).openDirectoryDialog(
    tr("Open Directory to exclude from Add-On Recognition"),
    lnm::OPTIONS_DIALOG_DB_FILE_DLG,
    atools::fs::FsPaths::getSceneryLibraryPath(mainWindow->getCurrentSimulator()));

  if(!path.isEmpty())
    ui->listWidgetOptionsDatabaseAddon->addItem(QDir::toNativeSeparators(path));
  updateDatabaseButtonState();
}

void OptionsDialog::removeDatabaseAddOnExcludePathClicked()
{
  qDebug() << "OptionsDialog::removeDatabaseAddOnExcludePathClicked";

  delete ui->listWidgetOptionsDatabaseAddon->currentItem();
  updateDatabaseButtonState();
}

void OptionsDialog::updateDatabaseButtonState()
{
  ui->pushButtonOptionsDatabaseRemoveExclude->setEnabled(
    ui->listWidgetOptionsDatabaseExclude->currentRow() != -1);
  ui->pushButtonOptionsDatabaseRemoveAddon->setEnabled(
    ui->listWidgetOptionsDatabaseAddon->currentRow() != -1);
}

/* Convert the range ring string to an int vector */
QVector<int> OptionsDialog::ringStrToVector(const QString& string) const
{
  QVector<int> rings;
  for(const QString& str :  string.split(" "))
  {
    QString val = str.trimmed();

    if(!val.isEmpty())
    {
      bool ok;
      int num = val.toInt(&ok);
      if(ok && num <= MAX_RANGE_RING_SIZE)
        rings.append(num);
    }
  }
  return rings;
}

/* Copy widget states to OptionData object */
void OptionsDialog::widgetsToOptionData()
{
  OptionData& data = OptionData::instanceInternal();

  toFlags(ui->checkBoxOptionsStartupLoadKml, opts::STARTUP_LOAD_KML);
  toFlags(ui->checkBoxOptionsStartupLoadMapSettings, opts::STARTUP_LOAD_MAP_SETTINGS);
  toFlags(ui->checkBoxOptionsStartupLoadRoute, opts::STARTUP_LOAD_ROUTE);
  toFlags(ui->radioButtonOptionsStartupShowHome, opts::STARTUP_SHOW_HOME);
  toFlags(ui->radioButtonOptionsStartupShowLast, opts::STARTUP_SHOW_LAST);
  toFlags(ui->checkBoxOptionsGuiCenterKml, opts::GUI_CENTER_KML);
  toFlags(ui->checkBoxOptionsGuiCenterRoute, opts::GUI_CENTER_ROUTE);
  toFlags(ui->checkBoxOptionsMapEmptyAirports, opts::MAP_EMPTY_AIRPORTS);
  toFlags(ui->checkBoxOptionsRouteEastWestRule, opts::ROUTE_EAST_WEST_RULE);
  toFlags(ui->checkBoxOptionsRoutePreferNdb, opts::ROUTE_PREFER_NDB);
  toFlags(ui->checkBoxOptionsRoutePreferVor, opts::ROUTE_PREFER_VOR);
  toFlags(ui->checkBoxOptionsWeatherInfoAsn, opts::WEATHER_INFO_ACTIVESKY);
  toFlags(ui->checkBoxOptionsWeatherInfoNoaa, opts::WEATHER_INFO_NOAA);
  toFlags(ui->checkBoxOptionsWeatherInfoVatsim, opts::WEATHER_INFO_VATSIM);
  toFlags(ui->checkBoxOptionsWeatherTooltipAsn, opts::WEATHER_TOOLTIP_ACTIVESKY);
  toFlags(ui->checkBoxOptionsWeatherTooltipNoaa, opts::WEATHER_TOOLTIP_NOAA);
  toFlags(ui->checkBoxOptionsWeatherTooltipVatsim, opts::WEATHER_TOOLTIP_VATSIM);

  data.mapRangeRings = ringStrToVector(ui->lineEditOptionsMapRangeRings->text());

  data.weatherActiveSkyPath = QDir::toNativeSeparators(ui->lineEditOptionsWeatherAsnPath->text());
  data.weatherNoaaUrl = ui->lineEditOptionsWeatherNoaaUrl->text();
  data.weatherVatsimUrl = ui->lineEditOptionsWeatherVatsimUrl->text();

  data.databaseAddonExclude.clear();
  for(int i = 0; i < ui->listWidgetOptionsDatabaseAddon->count(); i++)
    data.databaseAddonExclude.append(ui->listWidgetOptionsDatabaseAddon->item(i)->text());

  data.databaseExclude.clear();
  for(int i = 0; i < ui->listWidgetOptionsDatabaseExclude->count(); i++)
    data.databaseExclude.append(ui->listWidgetOptionsDatabaseExclude->item(i)->text());

  if(ui->radioButtonOptionsMapScrollFull->isChecked())
    data.mapScrollDetail = opts::FULL;
  else if(ui->radioButtonOptionsMapScrollNone->isChecked())
    data.mapScrollDetail = opts::NONE;
  else if(ui->radioButtonOptionsMapScrollNormal->isChecked())
    data.mapScrollDetail = opts::NORMAL;

  if(ui->radioButtonOptionsSimUpdateFast->isChecked())
    data.simUpdateRate = opts::FAST;
  else if(ui->radioButtonOptionsSimUpdateLow->isChecked())
    data.simUpdateRate = opts::LOW;
  else if(ui->radioButtonOptionsSimUpdateMedium->isChecked())
    data.simUpdateRate = opts::MEDIUM;
  data.simUpdateBox = ui->spinBoxOptionsSimUpdateBox->value();

  data.cacheSizeDisk = ui->spinBoxOptionsCacheDiskSize->value();
  data.cacheSizeMemory = ui->spinBoxOptionsCacheMemorySize->value();
  data.guiInfoTextSize = ui->spinBoxOptionsGuiInfoText->value();
  data.guiRouteTableTextSize = ui->spinBoxOptionsGuiRouteText->value();
  data.guiSearchTableTextSize = ui->spinBoxOptionsGuiSearchText->value();
  data.guiInfoSimSize = ui->spinBoxOptionsGuiSimInfoText->value();
  data.mapClickSensitivity = ui->spinBoxOptionsMapClickRect->value();
  data.mapTooltipSensitivity = ui->spinBoxOptionsMapTooltipRect->value();
  data.mapSymbolSize = ui->spinBoxOptionsMapSymbolSize->value();
  data.mapTextSize = ui->spinBoxOptionsMapTextSize->value();
  data.mapZoomShow = static_cast<float>(ui->doubleSpinBoxOptionsMapZoomShowMap->value());
  data.routeGroundBuffer = ui->spinBoxOptionsRouteGroundBuffer->value();
  data.valid = true;
}

/* Copy OptionData object to widget */
void OptionsDialog::optionDataToWidgets()
{
  fromFlags(ui->checkBoxOptionsStartupLoadKml, opts::STARTUP_LOAD_KML);
  fromFlags(ui->checkBoxOptionsStartupLoadMapSettings, opts::STARTUP_LOAD_MAP_SETTINGS);
  fromFlags(ui->checkBoxOptionsStartupLoadRoute, opts::STARTUP_LOAD_ROUTE);
  fromFlags(ui->radioButtonOptionsStartupShowHome, opts::STARTUP_SHOW_HOME);
  fromFlags(ui->radioButtonOptionsStartupShowLast, opts::STARTUP_SHOW_LAST);
  fromFlags(ui->checkBoxOptionsGuiCenterKml, opts::GUI_CENTER_KML);
  fromFlags(ui->checkBoxOptionsGuiCenterRoute, opts::GUI_CENTER_ROUTE);
  fromFlags(ui->checkBoxOptionsMapEmptyAirports, opts::MAP_EMPTY_AIRPORTS);
  fromFlags(ui->checkBoxOptionsRouteEastWestRule, opts::ROUTE_EAST_WEST_RULE);
  fromFlags(ui->checkBoxOptionsRoutePreferNdb, opts::ROUTE_PREFER_NDB);
  fromFlags(ui->checkBoxOptionsRoutePreferVor, opts::ROUTE_PREFER_VOR);
  fromFlags(ui->checkBoxOptionsWeatherInfoAsn, opts::WEATHER_INFO_ACTIVESKY);
  fromFlags(ui->checkBoxOptionsWeatherInfoNoaa, opts::WEATHER_INFO_NOAA);
  fromFlags(ui->checkBoxOptionsWeatherInfoVatsim, opts::WEATHER_INFO_VATSIM);
  fromFlags(ui->checkBoxOptionsWeatherTooltipAsn, opts::WEATHER_TOOLTIP_ACTIVESKY);
  fromFlags(ui->checkBoxOptionsWeatherTooltipNoaa, opts::WEATHER_TOOLTIP_NOAA);
  fromFlags(ui->checkBoxOptionsWeatherTooltipVatsim, opts::WEATHER_TOOLTIP_VATSIM);

  OptionData& data = OptionData::instanceInternal();

  QString txt;
  for(int val : data.mapRangeRings)
  {
    if(val > 0)
    {
      if(!txt.isEmpty())
        txt += " ";
      txt += QString::number(val);
    }
  }
  ui->lineEditOptionsMapRangeRings->setText(txt);
  ui->lineEditOptionsWeatherAsnPath->setText(QDir::toNativeSeparators(data.weatherActiveSkyPath));
  ui->lineEditOptionsWeatherNoaaUrl->setText(data.weatherNoaaUrl);
  ui->lineEditOptionsWeatherVatsimUrl->setText(data.weatherVatsimUrl);

  ui->listWidgetOptionsDatabaseAddon->clear();
  for(const QString& str : data.databaseAddonExclude)
    ui->listWidgetOptionsDatabaseAddon->addItem(str);

  ui->listWidgetOptionsDatabaseExclude->clear();
  for(const QString& str : data.databaseExclude)
    ui->listWidgetOptionsDatabaseExclude->addItem(str);

  switch(data.mapScrollDetail)
  {
    case opts::FULL:
      ui->radioButtonOptionsMapScrollFull->setChecked(true);
      break;
    case opts::NORMAL:
      ui->radioButtonOptionsMapScrollNormal->setChecked(true);
      break;
    case opts::NONE:
      ui->radioButtonOptionsMapScrollNone->setChecked(true);
      break;
  }

  switch(data.simUpdateRate)
  {
    case opts::FAST:
      ui->radioButtonOptionsSimUpdateFast->setChecked(true);
      break;
    case opts::MEDIUM:
      ui->radioButtonOptionsSimUpdateMedium->setChecked(true);
      break;
    case opts::LOW:
      ui->radioButtonOptionsSimUpdateLow->setChecked(true);
      break;
  }

  ui->spinBoxOptionsSimUpdateBox->setValue(data.simUpdateBox);

  ui->spinBoxOptionsCacheDiskSize->setValue(data.cacheSizeDisk);
  ui->spinBoxOptionsCacheMemorySize->setValue(data.cacheSizeMemory);
  ui->spinBoxOptionsGuiInfoText->setValue(data.guiInfoTextSize);
  ui->spinBoxOptionsGuiRouteText->setValue(data.guiRouteTableTextSize);
  ui->spinBoxOptionsGuiSearchText->setValue(data.guiSearchTableTextSize);
  ui->spinBoxOptionsGuiSimInfoText->setValue(data.guiInfoSimSize);
  ui->spinBoxOptionsMapClickRect->setValue(data.mapClickSensitivity);
  ui->spinBoxOptionsMapTooltipRect->setValue(data.mapTooltipSensitivity);
  ui->spinBoxOptionsMapSymbolSize->setValue(data.mapSymbolSize);
  ui->spinBoxOptionsMapTextSize->setValue(data.mapTextSize);
  ui->doubleSpinBoxOptionsMapZoomShowMap->setValue(data.mapZoomShow);
  ui->spinBoxOptionsRouteGroundBuffer->setValue(data.routeGroundBuffer);
}

/* Add flag from checkbox to OptionData flags */
void OptionsDialog::toFlags(QCheckBox *checkBox, opts::Flags flag)
{
  if(checkBox->isChecked())
    OptionData::instanceInternal().flags |= flag;
  else
    OptionData::instanceInternal().flags &= ~flag;
}

/* Add flag from radio button to OptionData flags */
void OptionsDialog::toFlags(QRadioButton *radioButton, opts::Flags flag)
{
  if(radioButton->isChecked())
    OptionData::instanceInternal().flags |= flag;
  else
    OptionData::instanceInternal().flags &= ~flag;
}

void OptionsDialog::fromFlags(QCheckBox *checkBox, opts::Flags flag)
{
  checkBox->setChecked(OptionData::instanceInternal().flags & flag);
}

void OptionsDialog::fromFlags(QRadioButton *radioButton, opts::Flags flag)
{
  radioButton->setChecked(OptionData::instanceInternal().flags & flag);
}

void OptionsDialog::updateWeatherButtonState()
{
  WeatherReporter *wr = mainWindow->getWeatherReporter();
  bool hasAs = wr->getCurrentActiveSkyType() != WeatherReporter::NONE;
  ui->checkBoxOptionsWeatherInfoAsn->setEnabled(hasAs);
  ui->checkBoxOptionsWeatherTooltipAsn->setEnabled(hasAs);

  ui->pushButtonOptionsWeatherNoaaTest->setEnabled(!ui->lineEditOptionsWeatherNoaaUrl->text().isEmpty());
  ui->pushButtonOptionsWeatherVatsimTest->setEnabled(!ui->lineEditOptionsWeatherVatsimUrl->text().isEmpty());
  updateActiveSkyPathStatus();
}

/* Checks the path to the ASN weather file and its contents. Display an error message in the label */
void OptionsDialog::updateActiveSkyPathStatus()
{
  const QString& path = ui->lineEditOptionsWeatherAsnPath->text();

  if(!path.isEmpty())
  {
    QFileInfo fileinfo(path);
    if(!fileinfo.exists())
      ui->labelOptionsWeatherAsnPathState->setText(
        tr("<span style=\"font-weight: bold; color: red;\">File does not exist.</span>"));
    else if(!fileinfo.isFile())
      ui->labelOptionsWeatherAsnPathState->setText(
        tr("<span style=\"font-weight: bold; color: red;\">Is not a file.</span>"));
    else if(!WeatherReporter::validateActiveSkyFile(path))
      ui->labelOptionsWeatherAsnPathState->setText(
        tr(
          "<span style=\"font-weight: bold; color: red;\">Is not an Active Sky weather snapshot file.</span>"));
    else
      ui->labelOptionsWeatherAsnPathState->setText(
        tr("Weather snapshot file is valid. Using this one for all simulators"));
  }
  else
  {
    // No manual path set
    QString text;
    WeatherReporter *wr = mainWindow->getWeatherReporter();
    QString sim = atools::fs::FsPaths::typeToShortName(wr->getSimType());

    switch(wr->getCurrentActiveSkyType())
    {
      case WeatherReporter::NONE:
        text = tr("No Active Sky weather snapshot found. Active Sky metars are not available.");
        break;
      case WeatherReporter::MANUAL:
        text = tr("Will use default weather snapshot after confirming change.");
        break;
      case WeatherReporter::ASN:
        text =
          tr("No Active Sky weather snapshot file selected. "
             "Using default for Active Sky Next for %1.").arg(sim);
        break;
      case WeatherReporter::AS16:
        text = tr("No Active Sky weather snapshot file selected. "
                  "Using default for AS16 for %1.").arg(sim);
        break;
    }

    ui->labelOptionsWeatherAsnPathState->setText(text);
  }
}

void OptionsDialog::selectActiveSkyPathClicked()
{
  qDebug() << "OptionsDialog::selectAsnPathClicked";

  QString path = atools::gui::Dialog(this).openFileDialog(
    tr("Open Active Sky Weather Snapshot File"),
    tr("Active Sky Weather Snapshot Files %1;;All Files (*)").arg(lnm::FILE_PATTERN_AS_SNAPSHOT),
    lnm::OPTIONS_DIALOG_AS_FILE_DLG, ui->lineEditOptionsWeatherAsnPath->text());

  if(!path.isEmpty())
    ui->lineEditOptionsWeatherAsnPath->setText(QDir::toNativeSeparators(path));

  updateWeatherButtonState();
}

void OptionsDialog::clearMemCachedClicked()
{
  qDebug() << "OptionsDialog::clearMemCachedClicked";
  mainWindow->getMapWidget()->clearVolatileTileCache();
  mainWindow->setStatusMessage(tr("Memory cache cleared."));
}

void OptionsDialog::clearDiskCachedClicked()
{
  qDebug() << "OptionsDialog::clearDiskCachedClicked";
  QMessageBox::StandardButton result =
    QMessageBox::question(this, QApplication::applicationName(),
                          tr("Clear the disk cache?\n"
                             "All files in the directory \"%1\" will be deleted.\n"
                             "This process will run in background and can take a while.").
                          arg(Marble::MarbleDirs::localPath()));

  if(result == QMessageBox::Yes)
  {
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    mainWindow->getMapWidget()->model()->clearPersistentTileCache();
    mainWindow->setStatusMessage(tr("Disk cache cleared."));
    QGuiApplication::restoreOverrideCursor();
  }
}

/* Opens the disk cache in explorer, finder, whatever */
void OptionsDialog::showDiskCacheClicked()
{
  const QString& localPath = Marble::MarbleDirs::localPath();

  QUrl url = QUrl::fromLocalFile(localPath);

  if(!QDesktopServices::openUrl(url))
    QMessageBox::warning(this, QApplication::applicationName(), QString(
                           tr("Error opening help URL \"%1\"")).arg(url.toDisplayString()));
}
