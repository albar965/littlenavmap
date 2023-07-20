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

#include "common/settingsmigrate.h"

#include "common/constants.h"
#include "gui/messagesettings.h"
#include "options/optiondata.h"
#include "settings/settings.h"
#include "util/version.h"
#include "io/fileroller.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QRegularExpression>
#include <QSettings>

using atools::settings::Settings;
using  atools::util::Version;

namespace migrate {

static Version optionsVersion;

void removeAllAndLog()
{
  qInfo() << Q_FUNC_INFO << "Removing all keys";
  Settings::clearSettings();
}

void removeAndLog(QSettings *settings, const QString& key)
{
  if(settings != nullptr && settings->contains(key))
  {
    qInfo() << Q_FUNC_INFO << "Removing" << QFileInfo(settings->fileName()).fileName() << key;
    settings->remove(key);
  }
}

void removeAndLog(const QString& key)
{
  removeAndLog(Settings::getQSettings(), key);
}

void backupFileAndLog(const QString& filename)
{
  qDebug() << Q_FUNC_INFO << "Backing up" << filename;
  atools::io::FileRoller(3, "${base}.${ext}_update-backup.${num}", true /* keepOriginalFile */).rollFile(filename);
}

void checkAndMigrateSettings()
{
  Settings& settings = Settings::instance();

  optionsVersion = Version(settings.valueStr(lnm::OPTIONS_VERSION));
  Version programVersion;

  if(optionsVersion.isValid())
  {
    qInfo() << Q_FUNC_INFO << "Options" << optionsVersion << "program" << programVersion;

    // Migrate settings if settings version is different from program version ===========================
    if(optionsVersion != programVersion)
    {
      qInfo() << Q_FUNC_INFO << "Found settings version mismatch. Settings file version"
              << optionsVersion << "Program version" << programVersion << ".";

      // Get file names =====================================================================
      QString trackFile = atools::settings::Settings::getConfigFilename(lnm::AIRCRAFT_TRACK_SUFFIX);
      QString mapstyleFile = atools::settings::Settings::getConfigFilename(lnm::MAPSTYLE_INI_SUFFIX);
      QString nightstyleFile = atools::settings::Settings::getConfigFilename(lnm::NIGHTSTYLE_INI_SUFFIX);

      // Backup most important files with from/to version suffix ============================================
      backupFileAndLog(trackFile);
      backupFileAndLog(mapstyleFile);
      backupFileAndLog(nightstyleFile);
      backupFileAndLog(Settings::getFilename());

      QSettings mapstyleSettings(mapstyleFile, QSettings::IniFormat);
      QSettings nightstyleSettings(nightstyleFile, QSettings::IniFormat);

      // ===============================================================
      if(optionsVersion < Version("2.4.0"))
      {
        qInfo() << Q_FUNC_INFO << "Clearing all settings before 2.4.0";
        removeAllAndLog();
      }

      // ===============================================================
      if(optionsVersion < Version("2.4.2.beta"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before 2.4.2.beta";
        removeAndLog(lnm::ROUTE_STRING_DIALOG_OPTIONS);
        Settings::syncSettings();
      }

      // ===============================================================
      if(optionsVersion < Version("2.4.3.rc1"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before 2.4.3.rc1";
        removeAndLog("MainWindow/Widget_mapThemeComboBox");
        Settings::syncSettings();
      }

      // ===============================================================
      if(optionsVersion <= Version("2.4.5"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before or equal to 2.4.5";

        // Route view
        removeAndLog("Route/View_tableViewRoute");

        // Table columns dialog
        removeAndLog("Route/FlightPlanTableColumnsCheckBoxStates");

        // Route tabs
        removeAndLog("Route/WidgetTabsTabIds");
        removeAndLog("Route/WidgetTabsCurrentTabId");
        removeAndLog("Route/WidgetTabsLocked");

        // Reset all before flight
        removeAndLog("Route/ResetAllDialogCheckBoxStates");

        // Complete log search options
        qInfo() << Q_FUNC_INFO << "Removing" << QFileInfo(Settings::getFilename()).fileName() << "SearchPaneLogdata";
        settings.remove("SearchPaneLogdata");

        // Search views
        removeAndLog("SearchPaneAirport/WidgetView_tableViewAirportSearch");
        removeAndLog("SearchPaneAirport/WidgetDistView_tableViewAirportSearch");
        removeAndLog("SearchPaneNav/WidgetView_tableViewAirportSearch");
        removeAndLog("SearchPaneNav/WidgetDistView_tableViewAirportSearch");
        removeAndLog("SearchPaneUserdata/WidgetView_tableViewUserdata");

        // info tabs
        removeAndLog("InfoWindow/WidgetTabsTabIds");
        removeAndLog("InfoWindow/WidgetTabsCurrentTabId");
        removeAndLog("InfoWindow/WidgetTabsLocked");

        // Choice dialog import and export
        removeAndLog("UserdataExport/ChoiceDialogCheckBoxStates");
        removeAndLog("Logdata/CsvExportCheckBoxStates");

        // Range rings
        removeAndLog(lnm::MAP_RANGEMARKERS);

        // Marble plugins
        for(const QString& key : settings.childGroups())
          if(key.startsWith("plugin_"))
            removeAndLog(key);

        // and vatsim URL,
        settings.setValue("OptionsDialog/Widget_lineEditOptionsWeatherVatsimUrl",
                          QString("https://metar.vatsim.net/metar.php?id=ALL"));

        // clear allow undock map?

        Settings::syncSettings();
      }

      if(optionsVersion <= Version("2.6.0.beta"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before or equal to 2.6.0.beta";
        removeAndLog("SearchPaneLogdata/WidgetView_tableViewLogdata");
      }

      if(optionsVersion <= Version("2.6.1.beta"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before or equal to 2.6.1.beta";
        removeAndLog(lnm::ROUTE_EXPORT_FORMATS);
        removeAndLog("RouteExport/RouteExportDialog_tableViewRouteExport");
        removeAndLog("RouteExport/RouteExportDialog_RouteMultiExportDialog_size");
        removeAndLog("RouteExport/RouteExportDialog_tableViewRouteExport");
      }

      if(optionsVersion <= Version("2.6.6"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before or equal to 2.6.6";
        settings.setValue("MainWindow/Widget_statusBar", true);
      }

      // =====================================================================
      // Adapt update channels if not yet saved or previous version is stable and this one is not
      if(!settings.contains(lnm::OPTIONS_UPDATE_CHANNELS) || (optionsVersion.isStable() && !programVersion.isStable()))
      {
        // No channel assigned yet or user moved from a stable to beta or development version
        if(programVersion.isBeta() || programVersion.isReleaseCandidate())
        {
          qInfo() << Q_FUNC_INFO << "Adjusting update channel to beta";
          settings.setValue(lnm::OPTIONS_UPDATE_CHANNELS, opts::STABLE_BETA);
        }
        else if(programVersion.isDevelop())
        {
          qInfo() << Q_FUNC_INFO << "Adjusting update channel to develop";
          settings.setValue(lnm::OPTIONS_UPDATE_CHANNELS, opts::STABLE_BETA_DEVELOP);
        }
      }

      if(optionsVersion <= Version("2.6.13"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before or equal to 2.6.13";
        removeAndLog("SearchPaneOnlineCenter/WidgetView_tableViewOnlineCenterSearch");
        removeAndLog("SearchPaneOnlineClient/WidgetView_tableViewOnlineClientSearch");
        removeAndLog("SearchPaneOnlineServer/WidgetView_tableViewOnlineServerSearch");
      }

      if(optionsVersion <= Version("2.6.14"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before or equal to 2.6.14";
        removeAndLog("SearchPaneAirport/WidgetDistView_tableViewAirportSearch");
        removeAndLog("SearchPaneAirport/WidgetView_tableViewAirportSearch");
        removeAndLog("OptionsDialog/Widget_lineEditOptionsWeatherVatsimUrl");
      }

      if(optionsVersion <= Version("2.6.16"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before or equal to 2.6.16";
        removeAndLog("OptionsDialog/DisplayOptionsuserAircraft_2097152"); // ITEM_USER_AIRCRAFT_COORDINATES
        removeAndLog("OptionsDialog/DisplayOptionsAiAircraft_2"); // ITEM_AI_AIRCRAFT_COORDINATES
        removeAndLog("Route/View_tableViewRoute");
        removeAndLog("OptionsDialog/Widget_lineEditOptionsWeatherIvaoUrl");
        removeAndLog("Map/DetailFactor");
      }

      if(optionsVersion <= Version("2.6.17"))
        removeAndLog("Map/MarkDisplay"); // MAP_MARK_DISPLAY

      if(optionsVersion <= Version("2.7.8.develop"))
      {
        removeAndLog("OptionsDialog/Widget_lineEditOptionsWeatherIvaoUrl");
        removeAndLog("Map/Airports");
      }

      if(optionsVersion <= Version("2.8.0.beta"))
      {
        removeAndLog("SearchPaneLogdata/WidgetView_tableViewLogdata");
        removeAndLog("Route/View_tableViewRoute");
        removeAndLog("SearchPaneAirport/WidgetDistView_tableViewAirportSearch");
        removeAndLog("SearchPaneAirport/WidgetView_tableViewAirportSearch");
        removeAndLog("SearchPaneNav/WidgetDistView_tableViewNavSearch");
        removeAndLog("SearchPaneNav/WidgetView_tableViewNavSearch");
      }

#ifdef Q_OS_MACOS
      if(optionsVersion <= Version("2.8.1.beta"))
        removeAndLog("OptionsDialog/GuiStyleIndex");
#endif

      if(optionsVersion <= Version("2.8.1.beta"))
      {
        removeAndLog(&mapstyleSettings, "Marker/TurnPathPen");
        removeAndLog(&mapstyleSettings, "Marker/SelectedAltitudeRangePen");
        removeAndLog("Map/WindSource");
        removeAndLog("Actions/DeleteTrail");
      }

      if(optionsVersion <= Version("2.8.4.beta"))
        removeAndLog(&mapstyleSettings, "MainWindow/Widget_actionAircraftPerformanceWarnMismatch");

      if(optionsVersion <= Version("2.8.7"))
      {
        removeAndLog(lnm::OPTIONS_TRACK_NAT_URL);
        removeAndLog(lnm::OPTIONS_TRACK_NAT_PARAM);
        removeAndLog(lnm::OPTIONS_TRACK_PACOTS_URL);
        removeAndLog(lnm::OPTIONS_TRACK_PACOTS_PARAM);
        removeAndLog(lnm::OPTIONS_TRACK_AUSOTS_URL);
        removeAndLog(lnm::OPTIONS_TRACK_AUSOTS_PARAM);
        removeAndLog(lnm::OPTIONS_ONLINE_NETWORK_MAX_SHADOW_DIST_NM);
        removeAndLog(lnm::OPTIONS_ONLINE_NETWORK_MAX_SHADOW_ALT_DIFF_FT);
        removeAndLog(lnm::OPTIONS_ONLINE_NETWORK_MAX_SHADOW_GS_DIFF_KTS);
        removeAndLog(lnm::OPTIONS_ONLINE_NETWORK_MAX_SHADOW_HDG_DIFF_DEG);
      }

      qInfo() << Q_FUNC_INFO << "Clearing all essential messages since version differs";
      messages::resetEssentialMessages();

      // Set program version to options and save ===================
      settings.setValue(lnm::OPTIONS_VERSION, programVersion.getVersionString());
      Settings::syncSettings();

      mapstyleSettings.sync();
      nightstyleSettings.sync();
    } // if(optionsVersion != programVersion)
  } // if(optionsVersion.isValid())
  else
  {
    qWarning() << Q_FUNC_INFO << "No version information found in settings file. Updating to" << programVersion;
    settings.setValue(lnm::OPTIONS_VERSION, programVersion.getVersionString());
    Settings::syncSettings();
  }

  // Always correct map font if missing
  if(!settings.contains(lnm::OPTIONS_DIALOG_MAP_FONT))
  {
    qInfo() << Q_FUNC_INFO << "Adjusting map font";
    // Make map font a bold copy of system font if no setting present
    QFont font(QGuiApplication::font());
    font.setBold(true);
    settings.setValueVar(lnm::OPTIONS_DIALOG_MAP_FONT, font);
    Settings::syncSettings();
  }
}

const Version& getOptionsVersion()
{
  return optionsVersion;
}

} // namespace migrate
