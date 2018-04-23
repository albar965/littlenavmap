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

#include "options/optionsdialog.h"

#include "navapp.h"
#include "common/constants.h"
#include "common/elevationprovider.h"
#include "common/unit.h"
#include "atools.h"
#include "ui_options.h"
#include "weather/weatherreporter.h"
#include "gui/widgetstate.h"
#include "gui/dialog.h"
#include "gui/widgetutil.h"
#include "settings/settings.h"
#include "mapgui/mapwidget.h"
#include "gui/helphandler.h"
#include "gui/palettesettings.h"
#include "util/updatecheck.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QDesktopServices>
#include <QColorDialog>
#include <QStyleFactory>
#include <QGuiApplication>
#include <QMainWindow>
#include <QUrl>
#include <QSettings>

#include <marble/MarbleModel.h>
#include <marble/MarbleDirs.h>

const int MAX_RANGE_RING_SIZE = 4000;
const int MAX_RANGE_RINGS = 10;

using atools::settings::Settings;
using atools::gui::HelpHandler;

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

OptionsDialog::OptionsDialog(QMainWindow *parentWindow)
  : QDialog(parentWindow), ui(new Ui::Options), mainWindow(parentWindow)
{
  qDebug() << Q_FUNC_INFO;
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowModality(Qt::ApplicationModal);

  ui->setupUi(this);

  QTreeWidgetItem *root = ui->treeWidgetOptionsDisplayTextOptions->invisibleRootItem();
  QTreeWidgetItem *ap = addTopItem(root, tr("Airport"), tr("Select airport labels to display on the map."));
  addItem(ap, tr("Name (Ident)"), QString(), opts::ITEM_AIRPORT_NAME, true);
  addItem(ap, tr("Tower Frequency"), QString(), opts::ITEM_AIRPORT_TOWER, true);
  addItem(ap, tr("ATIS / ASOS / AWOS Frequency"), QString(), opts::ITEM_AIRPORT_ATIS, true);
  addItem(ap, tr("Runway Information"),
          tr("Show runway length, width and light inidcator text."), opts::ITEM_AIRPORT_RUNWAY, true);
  // addItem(ap, tr("Wind Pointer"), opts::ITEM_AIRPORT_WIND_POINTER, false);

  QTreeWidgetItem *ua = addTopItem(root, tr("User Aircraft"),
                                   tr("Select text labels and other options for the user aircraft."));
  addItem(ua, tr("Registration"), QString(), opts::ITEM_USER_AIRCRAFT_REGISTRATION);
  addItem(ua, tr("Type"),
          tr("Show the aircraft type, like B738, B350 or M20T."),
          opts::ITEM_USER_AIRCRAFT_TYPE);
  addItem(ua, tr("Airline"), QString(), opts::ITEM_USER_AIRCRAFT_AIRLINE);
  addItem(ua, tr("Flight Number"), QString(), opts::ITEM_USER_AIRCRAFT_FLIGHT_NUMBER);
  addItem(ua, tr("Indicated Airspeed"), QString(), opts::ITEM_USER_AIRCRAFT_IAS);
  addItem(ua, tr("Ground Speed"), QString(), opts::ITEM_USER_AIRCRAFT_GS, true);
  addItem(ua, tr("Climb- and Sinkrate"), QString(), opts::ITEM_USER_AIRCRAFT_CLIMB_SINK);
  addItem(ua, tr("Heading"), QString(), opts::ITEM_USER_AIRCRAFT_HEADING);
  addItem(ua, tr("Altitude"), QString(), opts::ITEM_USER_AIRCRAFT_ALTITUDE, true);
  addItem(ua, tr("Track Line"),
          tr("Show the aircraft track as a black needle protruding from the aircraft nose."),
          opts::ITEM_USER_AIRCRAFT_TRACK_LINE, true);
  addItem(ua, tr("Wind Direction and Speed"),
          tr("Show wind direction and speed on the top center of the map."),
          opts::ITEM_USER_AIRCRAFT_WIND, true);
  addItem(ua, tr("Wind Pointer"),
          tr("Show wind direction pointer on the top center of the map."),
          opts::ITEM_USER_AIRCRAFT_WIND_POINTER, true);

  QTreeWidgetItem *ai =
    addTopItem(root, tr("AI, Multiplayer and Online Client Aircraft"),
               tr("Select text labels for the AI, multiplayer and online client aircraft."));
  addItem(ai, tr("Registration, Number or Callsign"), QString(), opts::ITEM_AI_AIRCRAFT_REGISTRATION, true);
  addItem(ai, tr("Type"), QString(), opts::ITEM_AI_AIRCRAFT_TYPE, true);
  addItem(ai, tr("Airline"), QString(), opts::ITEM_AI_AIRCRAFT_AIRLINE, true);
  addItem(ai, tr("Flight Number"), QString(), opts::ITEM_AI_AIRCRAFT_FLIGHT_NUMBER);
  addItem(ai, tr("Indicated Airspeed"), QString(), opts::ITEM_AI_AIRCRAFT_IAS);
  addItem(ai, tr("Ground Speed"), QString(), opts::ITEM_AI_AIRCRAFT_GS, true);
  addItem(ai, tr("Climb- and Sinkrate"), QString(), opts::ITEM_AI_AIRCRAFT_CLIMB_SINK);
  addItem(ai, tr("Heading"), QString(), opts::ITEM_AI_AIRCRAFT_HEADING);
  addItem(ai, tr("Altitude"), QString(), opts::ITEM_AI_AIRCRAFT_ALTITUDE, true);

  // Collect names and palettes from all styles
  for(const QString& styleName : QStyleFactory::keys())
  {
    ui->comboBoxOptionsGuiTheme->addItem(styleName, styleName);
    QStyle *style = QStyleFactory::create(styleName);

    QPalette palette = style->standardPalette();
    if(styleName == "Fusion")
    {
      // Store fusion palette settings a in a separate ini file
      QString filename = Settings::instance().getConfigFilename("_fusionstyle.ini");
      atools::gui::PaletteSettings paletteSettings(filename, "StyleColors");
      paletteSettings.syncPalette(palette);
    }
    stylePalettes.append(palette);
    stylesheets.append(QString());
    delete style;
  }

  QPalette darkPalette(QGuiApplication::palette());
  darkPalette.setColor(QPalette::Window, QColor(15, 15, 15));
  darkPalette.setColor(QPalette::WindowText, QColor(200, 200, 200));
  darkPalette.setColor(QPalette::Base, QColor(20, 20, 20));
  darkPalette.setColor(QPalette::AlternateBase, QColor(35, 35, 35));
  darkPalette.setColor(QPalette::ToolTipBase, QColor(35, 35, 35));
  darkPalette.setColor(QPalette::ToolTipText, QColor(200, 200, 200));
  darkPalette.setColor(QPalette::Text, QColor(200, 200, 200));
  darkPalette.setColor(QPalette::Button, QColor(35, 35, 35));
  darkPalette.setColor(QPalette::ButtonText, QColor(200, 200, 200));
  darkPalette.setColor(QPalette::BrightText, QColor(250, 250, 250));
  darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));

  darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
  darkPalette.setColor(QPalette::HighlightedText, Qt::black);

  darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(100, 100, 100));
  darkPalette.setColor(QPalette::Disabled, QPalette::Button, QColor(65, 65, 65));
  darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(100, 100, 100));

  // Add additional night mode
  ui->comboBoxOptionsGuiTheme->addItem("Night", "Fusion");

  // Add stylesheet for better checkbox radio button and toolbutton visibility
  stylesheets.append(
    QLatin1Literal("QCheckBox::indicator:checked "
                   "{ image: url(:/littlenavmap/resources/icons/checkbox_dark_checked.png); }") +
    QLatin1Literal("QCheckBox::indicator:unchecked "
                   "{ image: url(:/littlenavmap/resources/icons/checkbox_dark_unchecked.png); }") +
    QLatin1Literal("QRadioButton::indicator:checked "
                   "{ image: url(:/littlenavmap/resources/icons/radiobutton_dark_checked.png); }") +
    QLatin1Literal("QRadioButton::indicator:unchecked "
                   "{ image: url(:/littlenavmap/resources/icons/radiobutton_dark_unchecked.png); }") +
    QString("QToolButton:checked { background-color: %1;}").arg(darkPalette.color(QPalette::Window).lighter(600).name())
    );

  // Store dark palette settings a in a separate ini file
  QString filename = Settings::instance().getConfigFilename("_nightstyle.ini");
  atools::gui::PaletteSettings paletteSettings(filename, "StyleColors");
  paletteSettings.syncPalette(darkPalette);

  // darkPalette.setColor(QPalette::Active, QPalette::Text, QColor(100, 100, 100));
  // darkPalette.setColor(QPalette::Active, QPalette::ButtonText, QColor(100, 100, 100));
  // darkPalette.setColor(QPalette::Inactive, QPalette::Text, QColor(100, 100, 100));
  // darkPalette.setColor(QPalette::Inactive, QPalette::ButtonText, QColor(100, 100, 100));
  stylePalettes.append(darkPalette);

  rangeRingValidator = new RangeRingValidator;

  // Create widget list for state saver
  widgets.append(ui->tabWidgetOptions);
  widgets.append(ui->checkBoxOptionsGuiCenterKml);
  widgets.append(ui->checkBoxOptionsGuiCenterRoute);
  widgets.append(ui->checkBoxOptionsGuiAvoidOverwrite);
  widgets.append(ui->checkBoxOptionsGuiOverrideLanguage);
  widgets.append(ui->checkBoxOptionsGuiOverrideLocale);
  widgets.append(ui->checkBoxOptionsMapEmptyAirports);
  widgets.append(ui->checkBoxOptionsMapEmptyAirports3D);
  widgets.append(ui->checkBoxOptionsRouteShortName);
  widgets.append(ui->checkBoxOptionsMapTooltipAirport);
  widgets.append(ui->checkBoxOptionsMapTooltipNavaid);
  widgets.append(ui->checkBoxOptionsMapTooltipAirspace);
  widgets.append(ui->checkBoxOptionsMapClickAirport);
  widgets.append(ui->checkBoxOptionsMapClickNavaid);
  widgets.append(ui->checkBoxOptionsMapClickAirspace);
  widgets.append(ui->checkBoxOptionsRouteEastWestRule);
  widgets.append(ui->comboBoxOptionsRouteAltitudeRuleType);
  widgets.append(ui->checkBoxOptionsRoutePreferNdb);
  widgets.append(ui->checkBoxOptionsRoutePreferVor);
  widgets.append(ui->checkBoxOptionsRouteExportUserWpt);
  widgets.append(ui->checkBoxOptionsStartupLoadKml);
  widgets.append(ui->checkBoxOptionsStartupLoadMapSettings);
  widgets.append(ui->checkBoxOptionsStartupLoadRoute);
  widgets.append(ui->checkBoxOptionsStartupLoadTrail);
  widgets.append(ui->checkBoxOptionsStartupLoadSearch);
  widgets.append(ui->checkBoxOptionsStartupLoadInfoContent);
  widgets.append(ui->checkBoxOptionsWeatherInfoAsn);
  widgets.append(ui->checkBoxOptionsWeatherInfoNoaa);
  widgets.append(ui->checkBoxOptionsWeatherInfoVatsim);
  widgets.append(ui->checkBoxOptionsWeatherInfoIvao);
  widgets.append(ui->checkBoxOptionsWeatherInfoFs);
  widgets.append(ui->checkBoxOptionsWeatherTooltipAsn);
  widgets.append(ui->checkBoxOptionsWeatherTooltipNoaa);
  widgets.append(ui->checkBoxOptionsWeatherTooltipVatsim);
  widgets.append(ui->checkBoxOptionsWeatherTooltipIvao);
  widgets.append(ui->checkBoxOptionsWeatherTooltipFs);
  widgets.append(ui->lineEditOptionsMapRangeRings);
  widgets.append(ui->lineEditOptionsWeatherAsnPath);
  widgets.append(ui->lineEditOptionsWeatherNoaaUrl);
  widgets.append(ui->lineEditOptionsWeatherVatsimUrl);
  widgets.append(ui->lineEditOptionsWeatherIvaoUrl);
  widgets.append(ui->listWidgetOptionsDatabaseAddon);
  widgets.append(ui->listWidgetOptionsDatabaseExclude);
  widgets.append(ui->comboBoxMapScrollDetails);
  widgets.append(ui->radioButtonOptionsSimUpdateFast);
  widgets.append(ui->radioButtonOptionsSimUpdateLow);
  widgets.append(ui->radioButtonOptionsSimUpdateMedium);
  widgets.append(ui->checkBoxOptionsSimUpdatesConstant);
  widgets.append(ui->spinBoxOptionsSimUpdateBox);
  widgets.append(ui->spinBoxSimMaxTrackPoints);
  widgets.append(ui->radioButtonOptionsStartupShowHome);
  widgets.append(ui->radioButtonOptionsStartupShowLast);
  widgets.append(ui->radioButtonOptionsStartupShowFlightplan);

  widgets.append(ui->spinBoxOptionsCacheDiskSize);
  widgets.append(ui->spinBoxOptionsCacheMemorySize);
  widgets.append(ui->radioButtonCacheUseOffineElevation);
  widgets.append(ui->radioButtonCacheUseOnlineElevation);
  widgets.append(ui->lineEditCacheOfflineDataPath);

  widgets.append(ui->spinBoxOptionsGuiInfoText);
  widgets.append(ui->spinBoxOptionsGuiRouteText);
  widgets.append(ui->spinBoxOptionsGuiSearchText);
  widgets.append(ui->spinBoxOptionsGuiSimInfoText);
  // widgets.append(ui->comboBoxOptionsGuiTheme);
  widgets.append(ui->spinBoxOptionsGuiThemeMapDimming);
  widgets.append(ui->spinBoxOptionsMapClickRect);
  widgets.append(ui->spinBoxOptionsMapTooltipRect);
  widgets.append(ui->doubleSpinBoxOptionsMapZoomShowMap);
  widgets.append(ui->doubleSpinBoxOptionsMapZoomShowMapMenu);
  widgets.append(ui->spinBoxOptionsRouteGroundBuffer);
  widgets.append(ui->doubleSpinBoxOptionsRouteTodRule);

  widgets.append(ui->spinBoxOptionsDisplayTextSizeAircraftAi);
  widgets.append(ui->spinBoxOptionsDisplaySymbolSizeNavaid);
  widgets.append(ui->spinBoxOptionsDisplayTextSizeNavaid);
  widgets.append(ui->spinBoxOptionsDisplayThicknessFlightplan);
  widgets.append(ui->spinBoxOptionsDisplaySymbolSizeAirport);
  widgets.append(ui->spinBoxOptionsDisplaySymbolSizeAircraftAi);
  widgets.append(ui->spinBoxOptionsDisplayTextSizeFlightplan);
  widgets.append(ui->spinBoxOptionsDisplayTextSizeAircraftUser);
  widgets.append(ui->spinBoxOptionsDisplaySymbolSizeAircraftUser);
  widgets.append(ui->spinBoxOptionsDisplayTextSizeAirport);
  widgets.append(ui->spinBoxOptionsDisplayThicknessTrail);
  widgets.append(ui->spinBoxOptionsDisplayThicknessRangeDistance);
  widgets.append(ui->spinBoxOptionsDisplayThicknessCompassRose);
  widgets.append(ui->comboBoxOptionsDisplayTrailType);

  widgets.append(ui->comboBoxOptionsStartupUpdateChannels);
  widgets.append(ui->comboBoxOptionsStartupUpdateRate);

  widgets.append(ui->comboBoxOptionsUnitDistance);
  widgets.append(ui->comboBoxOptionsUnitAlt);
  widgets.append(ui->comboBoxOptionsUnitSpeed);
  widgets.append(ui->comboBoxOptionsUnitVertSpeed);
  widgets.append(ui->comboBoxOptionsUnitShortDistance);
  widgets.append(ui->comboBoxOptionsUnitCoords);
  widgets.append(ui->comboBoxOptionsUnitFuelWeight);

  widgets.append(ui->checkBoxOptionsShowTod);

  widgets.append(ui->checkBoxOptionsMapAirportText);
  widgets.append(ui->checkBoxOptionsMapNavaidText);
  widgets.append(ui->checkBoxOptionsMapFlightplanText);
  widgets.append(ui->checkBoxOptionsMapAirportBoundary);
  widgets.append(ui->checkBoxOptionsMapAirportDiagram);
  widgets.append(ui->checkBoxOptionsMapFlightplanDimPassed);
  widgets.append(ui->checkBoxOptionsSimDoNotFollowOnScroll);
  widgets.append(ui->checkBoxOptionsSimCenterLeg);
  widgets.append(ui->checkBoxOptionsSimCenterLegTable);
  widgets.append(ui->spinBoxSimDoNotFollowOnScrollTime);

  widgets.append(ui->radioButtonOptionsOnlineNone);
  widgets.append(ui->radioButtonOptionsOnlineVatsim);
  widgets.append(ui->radioButtonOptionsOnlineIvao);
  widgets.append(ui->radioButtonOptionsOnlineCustomStatus);
  widgets.append(ui->radioButtonOptionsOnlineCustom);

  widgets.append(ui->lineEditOptionsOnlineStatusUrl);
  widgets.append(ui->lineEditOptionsOnlineWhazzupUrl);
  widgets.append(ui->spinBoxOptionsOnlineUpdate);
  widgets.append(ui->comboBoxOptionsOnlineFormat);

  doubleSpinBoxOptionsMapZoomShowMapSuffix = ui->doubleSpinBoxOptionsMapZoomShowMap->suffix();
  doubleSpinBoxOptionsMapZoomShowMapMenuSuffix = ui->doubleSpinBoxOptionsMapZoomShowMapMenu->suffix();

  spinBoxOptionsRouteGroundBufferSuffix = ui->spinBoxOptionsRouteGroundBuffer->suffix();
  doubleSpinBoxOptionsRouteTodRuleSuffix = ui->doubleSpinBoxOptionsRouteTodRule->suffix();
  labelOptionsMapRangeRingsText = ui->labelOptionsMapRangeRings->text();

  ui->lineEditOptionsMapRangeRings->setValidator(rangeRingValidator);

  connect(ui->buttonBoxOptions, &QDialogButtonBox::clicked, this, &OptionsDialog::buttonBoxClicked);

  // GUI widgets
  connect(ui->comboBoxOptionsGuiTheme, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &OptionsDialog::updateGuiStyleSpinboxState);

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
  connect(ui->lineEditOptionsWeatherIvaoUrl, &QLineEdit::textEdited,
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
  connect(ui->pushButtonOptionsWeatherIvaoTest, &QPushButton::clicked,
          this, &OptionsDialog::testWeatherIvaoUrlClicked);

  connect(ui->checkBoxOptionsSimUpdatesConstant, &QCheckBox::toggled,
          this, &OptionsDialog::simUpdatesConstantClicked);

  connect(ui->pushButtonOptionsDisplayFlightplanColor, &QPushButton::clicked,
          this, &OptionsDialog::flightplanColorClicked);
  connect(ui->pushButtonOptionsDisplayFlightplanProcedureColor, &QPushButton::clicked,
          this, &OptionsDialog::flightplanProcedureColorClicked);
  connect(ui->pushButtonOptionsDisplayFlightplanActiveColor, &QPushButton::clicked,
          this, &OptionsDialog::flightplanActiveColorClicked);
  connect(ui->pushButtonOptionsDisplayTrailColor, &QPushButton::clicked,
          this, &OptionsDialog::trailColorClicked);
  connect(ui->pushButtonOptionsDisplayFlightplanPassedColor, &QPushButton::clicked,
          this, &OptionsDialog::flightplanPassedColorClicked);

  connect(ui->radioButtonCacheUseOffineElevation, &QRadioButton::clicked,
          this, &OptionsDialog::updateCacheElevationStates);
  connect(ui->radioButtonCacheUseOnlineElevation, &QRadioButton::clicked,
          this, &OptionsDialog::updateCacheElevationStates);
  connect(ui->lineEditCacheOfflineDataPath, &QLineEdit::textEdited,
          this, &OptionsDialog::updateCacheElevationStates);
  connect(ui->pushButtonCacheOfflineDataSelect, &QPushButton::clicked,
          this, &OptionsDialog::offlineDataSelectClicked);

  connect(ui->pushButtonOptionsStartupCheckUpdate, &QPushButton::clicked,
          this, &OptionsDialog::checkUpdateClicked);

  connect(ui->checkBoxOptionsMapEmptyAirports, &QCheckBox::toggled,
          this, &OptionsDialog::mapEmptyAirportsClicked);

  connect(ui->checkBoxOptionsSimDoNotFollowOnScroll, &QCheckBox::toggled, this,
          &OptionsDialog::simNoFollowAircraftOnScrollClicked);

  // Online tab =======================================================================
  connect(ui->radioButtonOptionsOnlineNone, &QRadioButton::clicked,
          this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->radioButtonOptionsOnlineVatsim, &QRadioButton::clicked,
          this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->radioButtonOptionsOnlineIvao, &QRadioButton::clicked,
          this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->radioButtonOptionsOnlineCustomStatus, &QRadioButton::clicked,
          this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->radioButtonOptionsOnlineCustom, &QRadioButton::clicked,
          this, &OptionsDialog::updateOnlineWidgetStatus);

  connect(ui->lineEditOptionsOnlineStatusUrl, &QLineEdit::textEdited,
          this, &OptionsDialog::updateOnlineWidgetStatus);
  connect(ui->lineEditOptionsOnlineWhazzupUrl, &QLineEdit::textEdited,
          this, &OptionsDialog::updateOnlineWidgetStatus);

  connect(ui->pushButtonOptionsOnlineTestStatusUrl, &QPushButton::clicked,
          this, &OptionsDialog::onlineTestStatusUrlClicked);
  connect(ui->pushButtonOptionsOnlineTestWhazzupUrl, &QPushButton::clicked,
          this, &OptionsDialog::onlineTestWhazzupUrlClicked);
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
  updateCacheElevationStates();
  updateWidgetUnits();
  updateDatabaseButtonState();
  updateGuiStyleSpinboxState();
  updateOnlineWidgetStatus();

  return QDialog::exec();
}

void OptionsDialog::onlineTestStatusUrlClicked()
{
  onlineTestUrl(ui->lineEditOptionsOnlineStatusUrl->text());
}

void OptionsDialog::onlineTestWhazzupUrlClicked()
{
  onlineTestUrl(ui->lineEditOptionsOnlineWhazzupUrl->text());
}

void OptionsDialog::onlineTestUrl(const QString& url)
{
  qDebug() << Q_FUNC_INFO << url;
  QString result;
  if(WeatherReporter::testUrl(url, QString(), result))
    QMessageBox::information(this, QApplication::applicationName(),
                             tr("Success. First lines in file:\n%1").arg(result));
  else
    QMessageBox::warning(this, QApplication::applicationName(), tr("Failed. Reason:\n%1").arg(result));
}

void OptionsDialog::updateOnlineWidgetStatus()
{
  qDebug() << Q_FUNC_INFO;

  if(ui->radioButtonOptionsOnlineNone->isChecked() || ui->radioButtonOptionsOnlineVatsim->isChecked() ||
     ui->radioButtonOptionsOnlineIvao->isChecked())
  {
    ui->lineEditOptionsOnlineStatusUrl->setEnabled(false);
    ui->lineEditOptionsOnlineWhazzupUrl->setEnabled(false);
    ui->pushButtonOptionsOnlineTestStatusUrl->setEnabled(false);
    ui->pushButtonOptionsOnlineTestWhazzupUrl->setEnabled(false);
    ui->spinBoxOptionsOnlineUpdate->setEnabled(false);
    ui->comboBoxOptionsOnlineFormat->setEnabled(false);
  }
  else
  {
    if(ui->radioButtonOptionsOnlineCustomStatus->isChecked())
    {
      ui->lineEditOptionsOnlineStatusUrl->setEnabled(true);
      ui->lineEditOptionsOnlineWhazzupUrl->setEnabled(false);
      ui->pushButtonOptionsOnlineTestStatusUrl->setEnabled(QUrl(ui->lineEditOptionsOnlineStatusUrl->text()).isValid());
      ui->pushButtonOptionsOnlineTestWhazzupUrl->setEnabled(false);
    }
    else if(ui->radioButtonOptionsOnlineCustom->isChecked())
    {
      ui->lineEditOptionsOnlineStatusUrl->setEnabled(false);
      ui->lineEditOptionsOnlineWhazzupUrl->setEnabled(true);
      ui->pushButtonOptionsOnlineTestStatusUrl->setEnabled(false);
      ui->pushButtonOptionsOnlineTestWhazzupUrl->setEnabled(QUrl(ui->lineEditOptionsOnlineWhazzupUrl->text()).isValid());
    }
    ui->spinBoxOptionsOnlineUpdate->setEnabled(true);
    ui->comboBoxOptionsOnlineFormat->setEnabled(true);
  }
}

void OptionsDialog::updateWidgetUnits()
{
  ui->doubleSpinBoxOptionsMapZoomShowMap->setSuffix(
    Unit::replacePlaceholders(doubleSpinBoxOptionsMapZoomShowMapSuffix));
  ui->doubleSpinBoxOptionsMapZoomShowMapMenu->setSuffix(
    Unit::replacePlaceholders(doubleSpinBoxOptionsMapZoomShowMapMenuSuffix));
  ui->spinBoxOptionsRouteGroundBuffer->setSuffix(
    Unit::replacePlaceholders(spinBoxOptionsRouteGroundBufferSuffix));

  ui->doubleSpinBoxOptionsRouteTodRule->setSuffix(
    Unit::replacePlaceholders(doubleSpinBoxOptionsRouteTodRuleSuffix));

  ui->labelOptionsMapRangeRings->setText(
    Unit::replacePlaceholders(labelOptionsMapRangeRingsText));
}

void OptionsDialog::applyStyle()
{
  int idx = ui->comboBoxOptionsGuiTheme->currentIndex();
  qDebug() << Q_FUNC_INFO << "index" << idx;

  QApplication::setStyle(QStyleFactory::create(ui->comboBoxOptionsGuiTheme->currentData().toString()));

  if(idx >= 0 && idx < stylePalettes.size())
  {
    qApp->setPalette(stylePalettes.at(idx));
    qApp->setStyleSheet(stylesheets.at(idx));
  }
  else
    qWarning() << "Invalid style index" << idx;
}

bool OptionsDialog::isOverrideLanguage()
{
  return Settings::instance().valueBool(lnm::OPTIONS_GUI_OVERRIDE_LANGUAGE, false);
}

bool OptionsDialog::isOverrideLocale()
{
  return Settings::instance().valueBool(lnm::OPTIONS_GUI_OVERRIDE_LOCALE, false);
}

void OptionsDialog::buttonBoxClicked(QAbstractButton *button)
{
  qDebug() << "Clicked" << button->text();

  if(button == ui->buttonBoxOptions->button(QDialogButtonBox::Apply))
  {
    widgetsToOptionData();
    saveState();
    applyStyle();
    emit optionsChanged();

    // Update dialog internal stuff
    updateWidgetUnits();
    updateActiveSkyPathStatus();
    updateWeatherButtonState();
    updateDatabaseButtonState();
  }
  else if(button == ui->buttonBoxOptions->button(QDialogButtonBox::Ok))
  {
    widgetsToOptionData();
    saveState();
    updateWidgetUnits();
    applyStyle();
    emit optionsChanged();
    accept();
  }
  else if(button == ui->buttonBoxOptions->button(QDialogButtonBox::Help))
    HelpHandler::openHelpUrl(this, lnm::HELP_ONLINE_URL + "OPTIONS.html", lnm::helpLanguagesOnline());
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
      int guiStyleIndex = OptionData::instanceInternal().guiStyleIndex;
      OptionData::instanceInternal() = OptionData();
      OptionData::instanceInternal().valid = true;

      // Do not reset style
      OptionData::instanceInternal().guiStyleIndex = guiStyleIndex;

      optionDataToWidgets();
      saveState();
      emit optionsChanged();

      updateWidgetUnits();
      applyStyle();
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
  saveDisplayOptItemStates();

  Settings& settings = Settings::instance();

  // Save the path lists
  QStringList paths;
  for(int i = 0; i < ui->listWidgetOptionsDatabaseExclude->count(); i++)
    paths.append(ui->listWidgetOptionsDatabaseExclude->item(i)->text());
  settings.setValue(lnm::OPTIONS_DIALOG_DB_EXCLUDE, paths);

  paths.clear();
  for(int i = 0; i < ui->listWidgetOptionsDatabaseAddon->count(); i++)
    paths.append(ui->listWidgetOptionsDatabaseAddon->item(i)->text());
  settings.setValue(lnm::OPTIONS_DIALOG_DB_ADDON_EXCLUDE, paths);

  settings.setValueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_COLOR, flightplanColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_PROCEDURE_COLOR, flightplanProcedureColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_ACTIVE_COLOR, flightplanActiveColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_PASSED_COLOR, flightplanPassedColor);
  settings.setValueVar(lnm::OPTIONS_DIALOG_TRAIL_COLOR, trailColor);

  settings.setValue(lnm::OPTIONS_DIALOG_GUI_STYLE_INDEX, ui->comboBoxOptionsGuiTheme->currentIndex());

  settings.syncSettings();
}

void OptionsDialog::restoreState()
{
  Settings& settings = Settings::instance();

  // Reload online network settings from configuration file which can be overloaded by placing a copy
  // in the settings file
  QString networksPath = settings.getOverloadedPath(":/littlenavmap/resources/config/networks.cfg");
  QSettings networkSettings(networksPath, QSettings::IniFormat);
  OptionData& od = OptionData::instanceInternal();
  od.onlineIvaoStatusUrl = networkSettings.value("ivao/statusurl").toString();
  od.onlineVatsimStatusUrl = networkSettings.value("vatsim/statusurl").toString();

  int update;
  if(networkSettings.value("options/update").toString().trimmed().toLower() == "auto")
    update = -1;
  else
    update = networkSettings.value("options/update").toInt();

  if(update < 60 && update != -1)
    update = 60;
  od.onlineReloadSecondsConfig = update;

  // Disable selection based on what was found in the file
  ui->radioButtonOptionsOnlineIvao->setVisible(!od.onlineIvaoStatusUrl.isEmpty());
  ui->radioButtonOptionsOnlineVatsim->setVisible(!od.onlineVatsimStatusUrl.isEmpty());

  atools::gui::WidgetState(lnm::OPTIONS_DIALOG_WIDGET,
                           false /*save visibility*/, true /*block signals*/).restore(widgets);
  restoreDisplayOptItemStates();

  if(settings.contains(lnm::OPTIONS_DIALOG_DB_EXCLUDE))
    ui->listWidgetOptionsDatabaseExclude->addItems(settings.valueStrList(lnm::OPTIONS_DIALOG_DB_EXCLUDE));
  if(settings.contains(lnm::OPTIONS_DIALOG_DB_ADDON_EXCLUDE))
    ui->listWidgetOptionsDatabaseAddon->addItems(settings.valueStrList(lnm::OPTIONS_DIALOG_DB_ADDON_EXCLUDE));

  flightplanColor =
    settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_COLOR, QColor(Qt::yellow)).value<QColor>();
  flightplanProcedureColor =
    settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_PROCEDURE_COLOR, QColor(255, 150, 0)).value<QColor>();
  flightplanActiveColor =
    settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_ACTIVE_COLOR, QColor(Qt::magenta)).value<QColor>();
  flightplanPassedColor =
    settings.valueVar(lnm::OPTIONS_DIALOG_FLIGHTPLAN_PASSED_COLOR, QColor(Qt::gray)).value<QColor>();
  trailColor =
    settings.valueVar(lnm::OPTIONS_DIALOG_TRAIL_COLOR, QColor(Qt::black)).value<QColor>();

  if(settings.contains(lnm::OPTIONS_DIALOG_GUI_STYLE_INDEX))
  {
    int idx = settings.valueInt(lnm::OPTIONS_DIALOG_GUI_STYLE_INDEX);
    if(idx >= 0 && idx < ui->comboBoxOptionsGuiTheme->count())
      ui->comboBoxOptionsGuiTheme->setCurrentIndex(idx);
    else
      qWarning() << "Invalid style index" << idx;
  }
  else
  {
    int index = 0;
    QStyle *currentStyle = QApplication::style();

    for(const QString& styleName : QStyleFactory::keys())
    {
      if(styleName.compare(currentStyle->objectName(), Qt::CaseInsensitive) == 0)
      {
        ui->comboBoxOptionsGuiTheme->setCurrentIndex(index);
        break;
      }
      index++;
    }
  }

  widgetsToOptionData();
  updateWidgetUnits();
  simUpdatesConstantClicked(false);
  mapEmptyAirportsClicked(false);
  simNoFollowAircraftOnScrollClicked(false);
  applyStyle();
  updateButtonColors();
}

void OptionsDialog::updateButtonColors()
{
  atools::gui::util::changeWidgetColor(ui->pushButtonOptionsDisplayFlightplanColor, flightplanColor);
  atools::gui::util::changeWidgetColor(ui->pushButtonOptionsDisplayFlightplanProcedureColor, flightplanProcedureColor);
  atools::gui::util::changeWidgetColor(ui->pushButtonOptionsDisplayFlightplanActiveColor, flightplanActiveColor);
  atools::gui::util::changeWidgetColor(ui->pushButtonOptionsDisplayFlightplanPassedColor, flightplanPassedColor);
  atools::gui::util::changeWidgetColor(ui->pushButtonOptionsDisplayTrailColor, trailColor);
}

void OptionsDialog::restoreDisplayOptItemStates()
{
  const Settings& settings = Settings::instance();
  for(const opts::DisplayOptions& dispOpt : displayOptItemIndex.keys())
  {
    QString optName = lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS + "_" + QString::number(dispOpt);
    if(settings.contains(optName))
      displayOptItemIndex.value(dispOpt)->
      setCheckState(0, settings.valueBool(optName, false) ? Qt::Checked : Qt::Unchecked);
  }
}

void OptionsDialog::saveDisplayOptItemStates()
{
  Settings& settings = Settings::instance();

  for(const opts::DisplayOptions& dispOpt : displayOptItemIndex.keys())
  {
    QTreeWidgetItem *item = displayOptItemIndex.value(dispOpt);

    QString optName = lnm::OPTIONS_DIALOG_DISPLAY_OPTIONS + "_" + QString::number(dispOpt);

    settings.setValue(optName, item->checkState(0) == Qt::Checked);
  }
}

void OptionsDialog::displayOptWidgetToOptionData()
{
  OptionData& od = OptionData::instanceInternal();
  od.displayOptions = opts::ITEM_NONE;
  for(const opts::DisplayOptions& dispOpt : displayOptItemIndex.keys())
  {
    if(displayOptItemIndex.value(dispOpt)->checkState(0) == Qt::Checked)
      od.displayOptions |= dispOpt;
  }
}

void OptionsDialog::displayOptDataToWidget()
{
  const OptionData& od = OptionData::instanceInternal();
  for(const opts::DisplayOptions& dispOpt : displayOptItemIndex.keys())
  {
    displayOptItemIndex.value(dispOpt)->setCheckState(
      0, od.displayOptions & dispOpt ? Qt::Checked : Qt::Unchecked);
  }
}

QTreeWidgetItem *OptionsDialog::addTopItem(QTreeWidgetItem *root, const QString& text, const QString& tooltip)
{
  QTreeWidgetItem *item = new QTreeWidgetItem(root, {text});
  item->setToolTip(0, tooltip);
  item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate | Qt::ItemIsEnabled);
  return item;
}

QTreeWidgetItem *OptionsDialog::addItem(QTreeWidgetItem *root, const QString& text, const QString& tooltip,
                                        opts::DisplayOption type, bool checked)
{
  QTreeWidgetItem *item = new QTreeWidgetItem(root, {text}, type);
  item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
  item->setToolTip(0, tooltip);
  item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
  displayOptItemIndex.insert(type, item);
  return item;
}

void OptionsDialog::checkUpdateClicked()
{
  qDebug() << Q_FUNC_INFO;

  // Trigger async check and get a dialog even if nothing was found
  NavApp::checkForUpdates(ui->comboBoxOptionsStartupUpdateChannels->currentIndex(), true /* manually triggered */);
}

void OptionsDialog::flightplanProcedureColorClicked()
{
  QColor col = QColorDialog::getColor(flightplanProcedureColor, mainWindow);
  if(col.isValid())
  {
    flightplanProcedureColor = col;
    updateButtonColors();
  }
}

void OptionsDialog::flightplanColorClicked()
{
  QColor col = QColorDialog::getColor(flightplanColor, mainWindow);
  if(col.isValid())
  {
    flightplanColor = col;
    updateButtonColors();
  }
}

void OptionsDialog::flightplanActiveColorClicked()
{
  QColor col = QColorDialog::getColor(flightplanActiveColor, mainWindow);
  if(col.isValid())
  {
    flightplanActiveColor = col;
    updateButtonColors();
  }
}

void OptionsDialog::flightplanPassedColorClicked()
{
  QColor col = QColorDialog::getColor(flightplanPassedColor, mainWindow);
  if(col.isValid())
  {
    flightplanPassedColor = col;
    updateButtonColors();
  }
}

void OptionsDialog::trailColorClicked()
{
  QColor col = QColorDialog::getColor(trailColor, mainWindow);
  if(col.isValid())
  {
    trailColor = col;
    updateButtonColors();
  }
}

/* Test NOAA weather URL and show a dialog with the result */
void OptionsDialog::testWeatherNoaaUrlClicked()
{
  qDebug() << Q_FUNC_INFO;
  testWeatherUrl(ui->lineEditOptionsWeatherNoaaUrl->text());
}

/* Test Vatsim weather URL and show a dialog with the result */
void OptionsDialog::testWeatherVatsimUrlClicked()
{
  qDebug() << Q_FUNC_INFO;
  testWeatherUrl(ui->lineEditOptionsWeatherVatsimUrl->text());
}

/* Test IVAO weather download URL and show a dialog of the first line */
void OptionsDialog::testWeatherIvaoUrlClicked()
{
  qDebug() << Q_FUNC_INFO;
  QString result;
  if(WeatherReporter::testUrl(ui->lineEditOptionsWeatherIvaoUrl->text(), QString(), result))
    QMessageBox::information(this, QApplication::applicationName(),
                             tr("Success. First METARs in file:\n%1").arg(result));
  else
    QMessageBox::warning(this, QApplication::applicationName(), tr("Failed. Reason:\n%1").arg(result));
}

void OptionsDialog::testWeatherUrl(const QString& url)
{
  QString result;
  if(WeatherReporter::testUrl(url, "KORD", result))
    QMessageBox::information(this, QApplication::applicationName(), tr("Success. Result:\n%1").arg(result));
  else
    QMessageBox::warning(this, QApplication::applicationName(), tr("Failed. Reason:\n%1").arg(result));
}

/* Show directory dialog to add exclude path */
void OptionsDialog::addDatabaseExcludePathClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString path = atools::gui::Dialog(this).openDirectoryDialog(
    tr("Open Directory to exclude from Scenery Loading"),
    lnm::OPTIONS_DIALOG_DB_FILE_DLG,
    atools::fs::FsPaths::getSceneryLibraryPath(NavApp::getCurrentSimulatorDb()));

  if(!path.isEmpty())
  {
    ui->listWidgetOptionsDatabaseExclude->addItem(QDir::toNativeSeparators(path));
    updateDatabaseButtonState();
  }
}

void OptionsDialog::removeDatabaseExcludePathClicked()
{
  qDebug() << Q_FUNC_INFO;

  // Item removes itself from the list when deleted
  delete ui->listWidgetOptionsDatabaseExclude->currentItem();
  updateDatabaseButtonState();
}

/* Show directory dialog to add add-on exclude path */
void OptionsDialog::addDatabaseAddOnExcludePathClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString path = atools::gui::Dialog(this).openDirectoryDialog(
    tr("Open Directory to exclude from Add-On Recognition"),
    lnm::OPTIONS_DIALOG_DB_FILE_DLG,
    atools::fs::FsPaths::getSceneryLibraryPath(NavApp::getCurrentSimulatorDb()));

  if(!path.isEmpty())
    ui->listWidgetOptionsDatabaseAddon->addItem(QDir::toNativeSeparators(path));
  updateDatabaseButtonState();
}

void OptionsDialog::removeDatabaseAddOnExcludePathClicked()
{
  qDebug() << Q_FUNC_INFO;

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

void OptionsDialog::updateGuiStyleSpinboxState()
{
  ui->spinBoxOptionsGuiThemeMapDimming->setEnabled(
    ui->comboBoxOptionsGuiTheme->currentIndex() == ui->comboBoxOptionsGuiTheme->count() - 1);
}

void OptionsDialog::simUpdatesConstantClicked(bool state)
{
  Q_UNUSED(state);
  ui->spinBoxOptionsSimUpdateBox->setDisabled(ui->checkBoxOptionsSimUpdatesConstant->isChecked());
}

void OptionsDialog::mapEmptyAirportsClicked(bool state)
{
  Q_UNUSED(state);
  ui->checkBoxOptionsMapEmptyAirports3D->setEnabled(ui->checkBoxOptionsMapEmptyAirports->isChecked());
}

void OptionsDialog::simNoFollowAircraftOnScrollClicked(bool state)
{
  Q_UNUSED(state);
  ui->spinBoxSimDoNotFollowOnScrollTime->setEnabled(ui->checkBoxOptionsSimDoNotFollowOnScroll->isChecked());
}

/* Convert the range ring string to an int vector */
QVector<int> OptionsDialog::ringStrToVector(const QString& string) const
{
  QVector<int> rings;
  for(const QString& str : string.split(" "))
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

  data.flightplanColor = flightplanColor;
  data.flightplanProcedureColor = flightplanProcedureColor;
  data.flightplanActiveColor = flightplanActiveColor;
  data.flightplanPassedColor = flightplanPassedColor;
  data.trailColor = trailColor;
  displayOptWidgetToOptionData();

  toFlags(ui->checkBoxOptionsStartupLoadKml, opts::STARTUP_LOAD_KML);
  toFlags(ui->checkBoxOptionsStartupLoadMapSettings, opts::STARTUP_LOAD_MAP_SETTINGS);
  toFlags(ui->checkBoxOptionsStartupLoadTrail, opts::STARTUP_LOAD_TRAIL);
  toFlags(ui->checkBoxOptionsStartupLoadRoute, opts::STARTUP_LOAD_ROUTE);
  toFlags(ui->checkBoxOptionsStartupLoadSearch, opts::STARTUP_LOAD_SEARCH);
  toFlags(ui->checkBoxOptionsStartupLoadInfoContent, opts::STARTUP_LOAD_INFO);
  toFlags(ui->radioButtonOptionsStartupShowHome, opts::STARTUP_SHOW_HOME);
  toFlags(ui->radioButtonOptionsStartupShowLast, opts::STARTUP_SHOW_LAST);
  toFlags(ui->radioButtonOptionsStartupShowFlightplan, opts::STARTUP_SHOW_ROUTE);
  toFlags(ui->checkBoxOptionsGuiCenterKml, opts::GUI_CENTER_KML);
  toFlags(ui->checkBoxOptionsGuiCenterRoute, opts::GUI_CENTER_ROUTE);
  toFlags(ui->checkBoxOptionsGuiAvoidOverwrite, opts::GUI_AVOID_OVERWRITE_FLIGHTPLAN);
  toFlags(ui->checkBoxOptionsGuiOverrideLanguage, opts::GUI_OVERRIDE_LANGUAGE);
  toFlags(ui->checkBoxOptionsGuiOverrideLocale, opts::GUI_OVERRIDE_LOCALE);
  toFlags(ui->checkBoxOptionsMapEmptyAirports, opts::MAP_EMPTY_AIRPORTS);
  toFlags(ui->checkBoxOptionsRouteEastWestRule, opts::ROUTE_ALTITUDE_RULE);
  toFlags(ui->checkBoxOptionsRoutePreferNdb, opts::ROUTE_PREFER_NDB);
  toFlags(ui->checkBoxOptionsRoutePreferVor, opts::ROUTE_PREFER_VOR);
  toFlags(ui->checkBoxOptionsRouteExportUserWpt, opts::ROUTE_GARMIN_USER_WPT);
  toFlags(ui->checkBoxOptionsWeatherInfoAsn, opts::WEATHER_INFO_ACTIVESKY);
  toFlags(ui->checkBoxOptionsWeatherInfoNoaa, opts::WEATHER_INFO_NOAA);
  toFlags(ui->checkBoxOptionsWeatherInfoVatsim, opts::WEATHER_INFO_VATSIM);
  toFlags2(ui->checkBoxOptionsWeatherInfoIvao, opts::WEATHER_INFO_IVAO);
  toFlags(ui->checkBoxOptionsWeatherInfoFs, opts::WEATHER_INFO_FS);
  toFlags(ui->checkBoxOptionsWeatherTooltipAsn, opts::WEATHER_TOOLTIP_ACTIVESKY);
  toFlags(ui->checkBoxOptionsWeatherTooltipNoaa, opts::WEATHER_TOOLTIP_NOAA);
  toFlags(ui->checkBoxOptionsWeatherTooltipVatsim, opts::WEATHER_TOOLTIP_VATSIM);
  toFlags2(ui->checkBoxOptionsWeatherTooltipIvao, opts::WEATHER_TOOLTIP_IVAO);
  toFlags(ui->checkBoxOptionsWeatherTooltipFs, opts::WEATHER_TOOLTIP_FS);
  toFlags(ui->checkBoxOptionsSimUpdatesConstant, opts::SIM_UPDATE_MAP_CONSTANTLY);
  toFlags(ui->checkBoxOptionsShowTod, opts::FLIGHT_PLAN_SHOW_TOD);

  toFlags(ui->radioButtonCacheUseOffineElevation, opts::CACHE_USE_OFFLINE_ELEVATION);
  toFlags(ui->radioButtonCacheUseOnlineElevation, opts::CACHE_USE_ONLINE_ELEVATION);

  toFlags2(ui->checkBoxOptionsMapEmptyAirports3D, opts::MAP_EMPTY_AIRPORTS_3D);
  toFlags2(ui->checkBoxOptionsRouteShortName, opts::ROUTE_SAVE_SHORT_NAME);

  toFlags2(ui->checkBoxOptionsMapAirportText, opts::MAP_AIRPORT_TEXT_BACKGROUND);
  toFlags2(ui->checkBoxOptionsMapNavaidText, opts::MAP_NAVAID_TEXT_BACKGROUND);
  toFlags2(ui->checkBoxOptionsMapFlightplanText, opts::MAP_ROUTE_TEXT_BACKGROUND);
  toFlags2(ui->checkBoxOptionsMapAirportBoundary, opts::MAP_AIRPORT_BOUNDARY);
  toFlags2(ui->checkBoxOptionsMapAirportDiagram, opts::MAP_AIRPORT_DIAGRAM);
  toFlags2(ui->checkBoxOptionsMapFlightplanDimPassed, opts::MAP_ROUTE_DIM_PASSED);
  toFlags2(ui->checkBoxOptionsSimDoNotFollowOnScroll, opts::ROUTE_NO_FOLLOW_ON_MOVE);
  toFlags2(ui->checkBoxOptionsSimCenterLeg, opts::ROUTE_AUTOZOOM);
  toFlags2(ui->checkBoxOptionsSimCenterLegTable, opts::ROUTE_CENTER_ACTIVE_LEG);

  data.cacheOfflineElevationPath = ui->lineEditCacheOfflineDataPath->text();

  data.displayTooltipOptions.setFlag(opts::TOOLTIP_AIRPORT, ui->checkBoxOptionsMapTooltipAirport->isChecked());
  data.displayTooltipOptions.setFlag(opts::TOOLTIP_NAVAID, ui->checkBoxOptionsMapTooltipNavaid->isChecked());
  data.displayTooltipOptions.setFlag(opts::TOOLTIP_AIRSPACE, ui->checkBoxOptionsMapTooltipAirspace->isChecked());

  data.displayClickOptions.setFlag(opts::CLICK_AIRPORT, ui->checkBoxOptionsMapClickAirport->isChecked());
  data.displayClickOptions.setFlag(opts::CLICK_NAVAID, ui->checkBoxOptionsMapClickNavaid->isChecked());
  data.displayClickOptions.setFlag(opts::CLICK_AIRSPACE, ui->checkBoxOptionsMapClickAirspace->isChecked());

  data.mapRangeRings = ringStrToVector(ui->lineEditOptionsMapRangeRings->text());

  data.weatherActiveSkyPath = QDir::toNativeSeparators(ui->lineEditOptionsWeatherAsnPath->text());
  data.weatherNoaaUrl = ui->lineEditOptionsWeatherNoaaUrl->text();
  data.weatherVatsimUrl = ui->lineEditOptionsWeatherVatsimUrl->text();
  data.weatherIvaoUrl = ui->lineEditOptionsWeatherIvaoUrl->text();

  data.databaseAddonExclude.clear();
  for(int i = 0; i < ui->listWidgetOptionsDatabaseAddon->count(); i++)
    data.databaseAddonExclude.append(ui->listWidgetOptionsDatabaseAddon->item(i)->text());

  data.databaseExclude.clear();
  for(int i = 0; i < ui->listWidgetOptionsDatabaseExclude->count(); i++)
    data.databaseExclude.append(ui->listWidgetOptionsDatabaseExclude->item(i)->text());

  data.mapScrollDetail = static_cast<opts::MapScrollDetail>(ui->comboBoxMapScrollDetails->currentIndex());

  if(ui->radioButtonOptionsSimUpdateFast->isChecked())
    data.simUpdateRate = opts::FAST;
  else if(ui->radioButtonOptionsSimUpdateLow->isChecked())
    data.simUpdateRate = opts::LOW;
  else if(ui->radioButtonOptionsSimUpdateMedium->isChecked())
    data.simUpdateRate = opts::MEDIUM;

  data.simNoFollowAircraftOnScroll = ui->spinBoxSimDoNotFollowOnScrollTime->value();
  data.simUpdateBox = ui->spinBoxOptionsSimUpdateBox->value();
  data.aircraftTrackMaxPoints = ui->spinBoxSimMaxTrackPoints->value();

  data.cacheSizeDisk = ui->spinBoxOptionsCacheDiskSize->value();
  data.cacheSizeMemory = ui->spinBoxOptionsCacheMemorySize->value();
  data.guiInfoTextSize = ui->spinBoxOptionsGuiInfoText->value();
  data.guiRouteTableTextSize = ui->spinBoxOptionsGuiRouteText->value();
  data.guiSearchTableTextSize = ui->spinBoxOptionsGuiSearchText->value();
  data.guiInfoSimSize = ui->spinBoxOptionsGuiSimInfoText->value();

  data.guiStyleIndex = ui->comboBoxOptionsGuiTheme->currentIndex();
  data.guiStyleMapDimming = ui->spinBoxOptionsGuiThemeMapDimming->value();
  data.guiStyleDark = data.guiStyleIndex == ui->comboBoxOptionsGuiTheme->count() - 1;

  data.mapClickSensitivity = ui->spinBoxOptionsMapClickRect->value();
  data.mapTooltipSensitivity = ui->spinBoxOptionsMapTooltipRect->value();

  data.mapZoomShowClick = static_cast<float>(ui->doubleSpinBoxOptionsMapZoomShowMap->value());
  data.mapZoomShowMenu = static_cast<float>(ui->doubleSpinBoxOptionsMapZoomShowMapMenu->value());

  data.routeGroundBuffer = ui->spinBoxOptionsRouteGroundBuffer->value();
  data.routeTodRule = ui->doubleSpinBoxOptionsRouteTodRule->value();

  data.displayTextSizeAircraftAi = ui->spinBoxOptionsDisplayTextSizeAircraftAi->value();
  data.displaySymbolSizeNavaid = ui->spinBoxOptionsDisplaySymbolSizeNavaid->value();
  data.displayTextSizeNavaid = ui->spinBoxOptionsDisplayTextSizeNavaid->value();
  data.displayThicknessFlightplan = ui->spinBoxOptionsDisplayThicknessFlightplan->value();
  data.displaySymbolSizeAirport = ui->spinBoxOptionsDisplaySymbolSizeAirport->value();
  data.displaySymbolSizeAircraftAi = ui->spinBoxOptionsDisplaySymbolSizeAircraftAi->value();
  data.displayTextSizeFlightplan = ui->spinBoxOptionsDisplayTextSizeFlightplan->value();
  data.displayTextSizeAircraftUser = ui->spinBoxOptionsDisplayTextSizeAircraftUser->value();
  data.displaySymbolSizeAircraftUser = ui->spinBoxOptionsDisplaySymbolSizeAircraftUser->value();
  data.displayTextSizeAirport = ui->spinBoxOptionsDisplayTextSizeAirport->value();
  data.displayThicknessTrail = ui->spinBoxOptionsDisplayThicknessTrail->value();
  data.displayThicknessRangeDistance = ui->spinBoxOptionsDisplayThicknessRangeDistance->value();
  data.displayThicknessCompassRose = ui->spinBoxOptionsDisplayThicknessCompassRose->value();
  data.displayTrailType = static_cast<opts::DisplayTrailType>(ui->comboBoxOptionsDisplayTrailType->currentIndex());

  data.updateRate = static_cast<opts::UpdateRate>(ui->comboBoxOptionsStartupUpdateRate->currentIndex());
  data.updateChannels = static_cast<opts::UpdateChannels>(ui->comboBoxOptionsStartupUpdateChannels->currentIndex());

  data.unitDist = static_cast<opts::UnitDist>(ui->comboBoxOptionsUnitDistance->currentIndex());
  data.unitShortDist = static_cast<opts::UnitShortDist>(ui->comboBoxOptionsUnitShortDistance->currentIndex());
  data.unitAlt = static_cast<opts::UnitAlt>(ui->comboBoxOptionsUnitAlt->currentIndex());
  data.unitSpeed = static_cast<opts::UnitSpeed>(ui->comboBoxOptionsUnitSpeed->currentIndex());
  data.unitVertSpeed = static_cast<opts::UnitVertSpeed>(ui->comboBoxOptionsUnitVertSpeed->currentIndex());
  data.unitCoords = static_cast<opts::UnitCoords>(ui->comboBoxOptionsUnitCoords->currentIndex());
  data.unitFuelWeight = static_cast<opts::UnitFuelAndWeight>(ui->comboBoxOptionsUnitFuelWeight->currentIndex());
  data.altitudeRuleType = static_cast<opts::AltitudeRule>(ui->comboBoxOptionsRouteAltitudeRuleType->currentIndex());

  if(ui->radioButtonOptionsOnlineNone->isChecked())
    data.onlineNetwork = opts::ONLINE_NONE;
  else if(ui->radioButtonOptionsOnlineVatsim->isChecked())
    data.onlineNetwork = opts::ONLINE_VATSIM;
  else if(ui->radioButtonOptionsOnlineIvao->isChecked())
    data.onlineNetwork = opts::ONLINE_IVAO;
  else if(ui->radioButtonOptionsOnlineCustomStatus->isChecked())
    data.onlineNetwork = opts::ONLINE_CUSTOM_STATUS;
  else if(ui->radioButtonOptionsOnlineCustom->isChecked())
    data.onlineNetwork = opts::ONLINE_CUSTOM;

  data.onlineStatusUrl = ui->lineEditOptionsOnlineStatusUrl->text();
  data.onlineWhazzupUrl = ui->lineEditOptionsOnlineWhazzupUrl->text();
  data.onlineReloadSeconds = ui->spinBoxOptionsOnlineUpdate->value();
  data.onlineFormat = static_cast<opts::OnlineFormat>(ui->comboBoxOptionsOnlineFormat->currentIndex());

  data.valid = true;
}

/* Copy OptionData object to widget */
void OptionsDialog::optionDataToWidgets()
{
  OptionData& data = OptionData::instanceInternal();

  flightplanColor = data.flightplanColor;
  flightplanProcedureColor = data.flightplanProcedureColor;
  flightplanActiveColor = data.flightplanActiveColor;
  flightplanPassedColor = data.flightplanPassedColor;
  trailColor = data.trailColor;
  displayOptDataToWidget();

  fromFlags(ui->checkBoxOptionsStartupLoadKml, opts::STARTUP_LOAD_KML);
  fromFlags(ui->checkBoxOptionsStartupLoadMapSettings, opts::STARTUP_LOAD_MAP_SETTINGS);
  fromFlags(ui->checkBoxOptionsStartupLoadRoute, opts::STARTUP_LOAD_ROUTE);
  fromFlags(ui->checkBoxOptionsStartupLoadTrail, opts::STARTUP_LOAD_TRAIL);
  fromFlags(ui->checkBoxOptionsStartupLoadInfoContent, opts::STARTUP_LOAD_INFO);
  fromFlags(ui->checkBoxOptionsStartupLoadSearch, opts::STARTUP_LOAD_SEARCH);
  fromFlags(ui->radioButtonOptionsStartupShowHome, opts::STARTUP_SHOW_HOME);
  fromFlags(ui->radioButtonOptionsStartupShowLast, opts::STARTUP_SHOW_LAST);
  fromFlags(ui->radioButtonOptionsStartupShowFlightplan, opts::STARTUP_SHOW_ROUTE);
  fromFlags(ui->checkBoxOptionsGuiCenterKml, opts::GUI_CENTER_KML);
  fromFlags(ui->checkBoxOptionsGuiCenterRoute, opts::GUI_CENTER_ROUTE);
  fromFlags(ui->checkBoxOptionsGuiAvoidOverwrite, opts::GUI_AVOID_OVERWRITE_FLIGHTPLAN);
  fromFlags(ui->checkBoxOptionsGuiOverrideLanguage, opts::GUI_OVERRIDE_LANGUAGE);
  fromFlags(ui->checkBoxOptionsGuiOverrideLocale, opts::GUI_OVERRIDE_LOCALE);
  fromFlags(ui->checkBoxOptionsMapEmptyAirports, opts::MAP_EMPTY_AIRPORTS);
  fromFlags(ui->checkBoxOptionsRouteEastWestRule, opts::ROUTE_ALTITUDE_RULE);
  fromFlags(ui->checkBoxOptionsRoutePreferNdb, opts::ROUTE_PREFER_NDB);
  fromFlags(ui->checkBoxOptionsRoutePreferVor, opts::ROUTE_PREFER_VOR);
  fromFlags(ui->checkBoxOptionsRouteExportUserWpt, opts::ROUTE_GARMIN_USER_WPT);
  fromFlags(ui->checkBoxOptionsWeatherInfoAsn, opts::WEATHER_INFO_ACTIVESKY);
  fromFlags(ui->checkBoxOptionsWeatherInfoNoaa, opts::WEATHER_INFO_NOAA);
  fromFlags(ui->checkBoxOptionsWeatherInfoVatsim, opts::WEATHER_INFO_VATSIM);
  fromFlags2(ui->checkBoxOptionsWeatherInfoIvao, opts::WEATHER_INFO_IVAO);
  fromFlags(ui->checkBoxOptionsWeatherInfoFs, opts::WEATHER_INFO_FS);
  fromFlags(ui->checkBoxOptionsWeatherTooltipAsn, opts::WEATHER_TOOLTIP_ACTIVESKY);
  fromFlags(ui->checkBoxOptionsWeatherTooltipNoaa, opts::WEATHER_TOOLTIP_NOAA);
  fromFlags(ui->checkBoxOptionsWeatherTooltipVatsim, opts::WEATHER_TOOLTIP_VATSIM);
  fromFlags2(ui->checkBoxOptionsWeatherTooltipIvao, opts::WEATHER_TOOLTIP_IVAO);
  fromFlags(ui->checkBoxOptionsWeatherTooltipFs, opts::WEATHER_TOOLTIP_FS);
  fromFlags(ui->checkBoxOptionsSimUpdatesConstant, opts::SIM_UPDATE_MAP_CONSTANTLY);
  fromFlags(ui->checkBoxOptionsShowTod, opts::FLIGHT_PLAN_SHOW_TOD);

  fromFlags(ui->radioButtonCacheUseOffineElevation, opts::CACHE_USE_OFFLINE_ELEVATION);
  fromFlags(ui->radioButtonCacheUseOnlineElevation, opts::CACHE_USE_ONLINE_ELEVATION);

  fromFlags2(ui->checkBoxOptionsMapEmptyAirports3D, opts::MAP_EMPTY_AIRPORTS_3D);
  fromFlags2(ui->checkBoxOptionsRouteShortName, opts::ROUTE_SAVE_SHORT_NAME);

  fromFlags2(ui->checkBoxOptionsMapAirportText, opts::MAP_AIRPORT_TEXT_BACKGROUND);
  fromFlags2(ui->checkBoxOptionsMapNavaidText, opts::MAP_NAVAID_TEXT_BACKGROUND);
  fromFlags2(ui->checkBoxOptionsMapFlightplanText, opts::MAP_ROUTE_TEXT_BACKGROUND);
  fromFlags2(ui->checkBoxOptionsMapAirportBoundary, opts::MAP_AIRPORT_BOUNDARY);
  fromFlags2(ui->checkBoxOptionsMapAirportDiagram, opts::MAP_AIRPORT_DIAGRAM);
  fromFlags2(ui->checkBoxOptionsMapFlightplanDimPassed, opts::MAP_ROUTE_DIM_PASSED);
  fromFlags2(ui->checkBoxOptionsSimDoNotFollowOnScroll, opts::ROUTE_NO_FOLLOW_ON_MOVE);
  fromFlags2(ui->checkBoxOptionsSimCenterLeg, opts::ROUTE_AUTOZOOM);
  fromFlags2(ui->checkBoxOptionsSimCenterLegTable, opts::ROUTE_CENTER_ACTIVE_LEG);

  ui->lineEditCacheOfflineDataPath->setText(data.cacheOfflineElevationPath);

  ui->checkBoxOptionsMapTooltipAirport->setChecked(data.displayTooltipOptions.testFlag(opts::TOOLTIP_AIRPORT));
  ui->checkBoxOptionsMapTooltipNavaid->setChecked(data.displayTooltipOptions.testFlag(opts::TOOLTIP_NAVAID));
  ui->checkBoxOptionsMapTooltipAirspace->setChecked(data.displayTooltipOptions.testFlag(opts::TOOLTIP_AIRSPACE));

  ui->checkBoxOptionsMapClickAirport->setChecked(data.displayClickOptions.testFlag(opts::CLICK_AIRPORT));
  ui->checkBoxOptionsMapClickNavaid->setChecked(data.displayClickOptions.testFlag(opts::CLICK_NAVAID));
  ui->checkBoxOptionsMapClickAirspace->setChecked(data.displayClickOptions.testFlag(opts::CLICK_AIRSPACE));

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
  ui->lineEditOptionsWeatherIvaoUrl->setText(data.weatherIvaoUrl);

  ui->listWidgetOptionsDatabaseAddon->clear();
  for(const QString& str : data.databaseAddonExclude)
    ui->listWidgetOptionsDatabaseAddon->addItem(str);

  ui->listWidgetOptionsDatabaseExclude->clear();
  for(const QString& str : data.databaseExclude)
    ui->listWidgetOptionsDatabaseExclude->addItem(str);

  ui->comboBoxMapScrollDetails->setCurrentIndex(data.mapScrollDetail);

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

  ui->spinBoxSimDoNotFollowOnScrollTime->setValue(data.simNoFollowAircraftOnScroll);
  ui->spinBoxOptionsSimUpdateBox->setValue(data.simUpdateBox);
  ui->spinBoxSimMaxTrackPoints->setValue(data.aircraftTrackMaxPoints);

  ui->spinBoxOptionsCacheDiskSize->setValue(data.cacheSizeDisk);
  ui->spinBoxOptionsCacheMemorySize->setValue(data.cacheSizeMemory);
  ui->spinBoxOptionsGuiInfoText->setValue(data.guiInfoTextSize);
  ui->spinBoxOptionsGuiRouteText->setValue(data.guiRouteTableTextSize);
  ui->spinBoxOptionsGuiSearchText->setValue(data.guiSearchTableTextSize);
  ui->spinBoxOptionsGuiSimInfoText->setValue(data.guiInfoSimSize);

  ui->comboBoxOptionsGuiTheme->setCurrentIndex(data.guiStyleIndex);
  ui->spinBoxOptionsGuiThemeMapDimming->setValue(data.guiStyleMapDimming);

  ui->spinBoxOptionsMapClickRect->setValue(data.mapClickSensitivity);
  ui->spinBoxOptionsMapTooltipRect->setValue(data.mapTooltipSensitivity);
  ui->doubleSpinBoxOptionsMapZoomShowMap->setValue(data.mapZoomShowClick);
  ui->doubleSpinBoxOptionsMapZoomShowMapMenu->setValue(data.mapZoomShowMenu);
  ui->spinBoxOptionsRouteGroundBuffer->setValue(data.routeGroundBuffer);
  ui->doubleSpinBoxOptionsRouteTodRule->setValue(data.routeTodRule);

  ui->spinBoxOptionsDisplayTextSizeAircraftAi->setValue(data.displayTextSizeAircraftAi);
  ui->spinBoxOptionsDisplaySymbolSizeNavaid->setValue(data.displaySymbolSizeNavaid);
  ui->spinBoxOptionsDisplayTextSizeNavaid->setValue(data.displayTextSizeNavaid);
  ui->spinBoxOptionsDisplayThicknessFlightplan->setValue(data.displayThicknessFlightplan);
  ui->spinBoxOptionsDisplaySymbolSizeAirport->setValue(data.displaySymbolSizeAirport);
  ui->spinBoxOptionsDisplaySymbolSizeAircraftAi->setValue(data.displaySymbolSizeAircraftAi);
  ui->spinBoxOptionsDisplayTextSizeFlightplan->setValue(data.displayTextSizeFlightplan);
  ui->spinBoxOptionsDisplayTextSizeAircraftUser->setValue(data.displayTextSizeAircraftUser);
  ui->spinBoxOptionsDisplaySymbolSizeAircraftUser->setValue(data.displaySymbolSizeAircraftUser);
  ui->spinBoxOptionsDisplayTextSizeAirport->setValue(data.displayTextSizeAirport);
  ui->spinBoxOptionsDisplayThicknessTrail->setValue(data.displayThicknessTrail);
  ui->spinBoxOptionsDisplayThicknessRangeDistance->setValue(data.displayThicknessRangeDistance);
  ui->spinBoxOptionsDisplayThicknessCompassRose->setValue(data.displayThicknessCompassRose);
  ui->comboBoxOptionsDisplayTrailType->setCurrentIndex(data.displayTrailType);

  ui->comboBoxOptionsStartupUpdateRate->setCurrentIndex(data.updateRate);
  ui->comboBoxOptionsStartupUpdateChannels->setCurrentIndex(data.updateChannels);

  ui->comboBoxOptionsUnitDistance->setCurrentIndex(data.unitDist);
  ui->comboBoxOptionsUnitShortDistance->setCurrentIndex(data.unitShortDist);
  ui->comboBoxOptionsUnitAlt->setCurrentIndex(data.unitAlt);
  ui->comboBoxOptionsUnitSpeed->setCurrentIndex(data.unitSpeed);
  ui->comboBoxOptionsUnitVertSpeed->setCurrentIndex(data.unitVertSpeed);
  ui->comboBoxOptionsUnitCoords->setCurrentIndex(data.unitCoords);
  ui->comboBoxOptionsUnitFuelWeight->setCurrentIndex(data.unitFuelWeight);
  ui->comboBoxOptionsRouteAltitudeRuleType->setCurrentIndex(data.altitudeRuleType);

  switch(data.onlineNetwork)
  {
    case opts::ONLINE_NONE:
      ui->radioButtonOptionsOnlineNone->setChecked(true);
      break;
    case opts::ONLINE_VATSIM:
      ui->radioButtonOptionsOnlineVatsim->setChecked(true);
      break;
    case opts::ONLINE_IVAO:
      ui->radioButtonOptionsOnlineIvao->setChecked(true);
      break;
    case opts::ONLINE_CUSTOM_STATUS:
      ui->radioButtonOptionsOnlineCustomStatus->setChecked(true);
      break;
    case opts::ONLINE_CUSTOM:
      ui->radioButtonOptionsOnlineCustom->setChecked(true);
      break;
  }
  ui->lineEditOptionsOnlineStatusUrl->setText(data.onlineStatusUrl);
  ui->lineEditOptionsOnlineWhazzupUrl->setText(data.onlineWhazzupUrl);
  ui->spinBoxOptionsOnlineUpdate->setValue(data.onlineReloadSeconds);
  ui->comboBoxOptionsOnlineFormat->setCurrentIndex(data.onlineFormat);
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

/* Add flag from checkbox to OptionData flags */
void OptionsDialog::toFlags2(QCheckBox *checkBox, opts::Flags2 flag)
{
  if(checkBox->isChecked())
    OptionData::instanceInternal().flags2 |= flag;
  else
    OptionData::instanceInternal().flags2 &= ~flag;
}

/* Add flag from radio button to OptionData flags */
void OptionsDialog::toFlags2(QRadioButton *radioButton, opts::Flags2 flag)
{
  if(radioButton->isChecked())
    OptionData::instanceInternal().flags2 |= flag;
  else
    OptionData::instanceInternal().flags2 &= ~flag;
}

void OptionsDialog::fromFlags2(QCheckBox *checkBox, opts::Flags2 flag)
{
  checkBox->setChecked(OptionData::instanceInternal().flags2 & flag);
}

void OptionsDialog::fromFlags2(QRadioButton *radioButton, opts::Flags2 flag)
{
  radioButton->setChecked(OptionData::instanceInternal().flags2 & flag);
}

void OptionsDialog::offlineDataSelectClicked()
{
  qDebug() << Q_FUNC_INFO;

  QString path = atools::gui::Dialog(this).
                 openDirectoryDialog(tr("Open GLOBE data directory"),
                                     lnm::OPTIONS_DIALOG_GLOBE_FILE_DLG, ui->lineEditCacheOfflineDataPath->text());

  if(!path.isEmpty())
    ui->lineEditCacheOfflineDataPath->setText(QDir::toNativeSeparators(path));

  updateCacheElevationStates();
}

void OptionsDialog::updateCacheElevationStates()
{
  ui->lineEditCacheOfflineDataPath->setDisabled(ui->radioButtonCacheUseOnlineElevation->isChecked());
  ui->pushButtonCacheOfflineDataSelect->setDisabled(ui->radioButtonCacheUseOnlineElevation->isChecked());

  if(ui->radioButtonCacheUseOffineElevation->isChecked())
  {
    const QString& path = ui->lineEditCacheOfflineDataPath->text();
    if(!path.isEmpty())
    {
      QFileInfo fileinfo(path);
      if(!fileinfo.exists())
        ui->labelCacheGlobePathState->setText(
          tr("<span style=\"font-weight: bold; color: red;\">Directory does not exist.</span>"));
      else if(!fileinfo.isDir())
        ui->labelCacheGlobePathState->setText(
          tr("<span style=\"font-weight: bold; color: red;\">Is not a directory.</span>"));
      else if(!NavApp::getElevationProvider()->isGlobeDirectoryValid(path))
        ui->labelCacheGlobePathState->setText(
          tr("<span style=\"font-weight: bold; color: red;\">No valid GLOBE data found.</span>"));
      else
        ui->labelCacheGlobePathState->setText(
          tr("Directory and files are valid."));
    }
    else
      ui->labelCacheGlobePathState->setText(tr("No directory selected."));
  }
  else
    ui->labelCacheGlobePathState->clear();
}

void OptionsDialog::updateWeatherButtonState()
{
  WeatherReporter *wr = NavApp::getWeatherReporter();
  bool hasAs = wr->getCurrentActiveSkyType() != WeatherReporter::NONE;
  ui->checkBoxOptionsWeatherInfoAsn->setEnabled(hasAs);
  ui->checkBoxOptionsWeatherTooltipAsn->setEnabled(hasAs);

  ui->pushButtonOptionsWeatherNoaaTest->setEnabled(!ui->lineEditOptionsWeatherNoaaUrl->text().isEmpty());
  ui->pushButtonOptionsWeatherVatsimTest->setEnabled(!ui->lineEditOptionsWeatherVatsimUrl->text().isEmpty());
  ui->pushButtonOptionsWeatherIvaoTest->setEnabled(!ui->lineEditOptionsWeatherIvaoUrl->text().isEmpty());
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
    WeatherReporter *wr = NavApp::getWeatherReporter();
    QString sim = atools::fs::FsPaths::typeToShortName(wr->getSimType());

    if(wr->getSimType() != atools::fs::FsPaths::XPLANE11)
    {
      switch(wr->getCurrentActiveSkyType())
      {
        case WeatherReporter::NONE:
          text = tr("No Active Sky weather snapshot found. Active Sky METARs are not available.");
          break;
        case WeatherReporter::MANUAL:
          text = tr("Will use default weather snapshot after confirming change.");
          break;
        case WeatherReporter::ASN:
          text = tr("No Active Sky weather snapshot file selected. "
                    "Using default for Active Sky Next for %1.").arg(sim);
          break;
        case WeatherReporter::AS16:
          text = tr("No Active Sky weather snapshot file selected. "
                    "Using default for AS16 for %1.").arg(sim);
          break;

        case WeatherReporter::ASP4:
          text = tr("No Active Sky weather snapshot file selected. "
                    "Using default for ASP4 for %1.").arg(sim);
          break;
      }
    }
    else
      text = tr("X-Plane is selected in the Scenery Library menu. Active Sky weather not available.");

    ui->labelOptionsWeatherAsnPathState->setText(text);
  }
}

void OptionsDialog::selectActiveSkyPathClicked()
{
  qDebug() << Q_FUNC_INFO;

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
  qDebug() << Q_FUNC_INFO;

  NavApp::getMapWidget()->clearVolatileTileCache();
  NavApp::setStatusMessage(tr("Memory cache cleared."));
}

void OptionsDialog::clearDiskCachedClicked()
{
  qDebug() << Q_FUNC_INFO;

  QMessageBox::StandardButton result =
    QMessageBox::question(this, QApplication::applicationName(),
                          tr("Clear the disk cache?\n"
                             "All files in the directory \"%1\" will be deleted.\n"
                             "This process will run in background and can take a while.").
                          arg(Marble::MarbleDirs::localPath()));

  if(result == QMessageBox::Yes)
  {
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    NavApp::getMapWidget()->model()->clearPersistentTileCache();
    NavApp::setStatusMessage(tr("Disk cache cleared."));
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
