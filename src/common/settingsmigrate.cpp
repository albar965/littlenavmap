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

#include "common/settingsmigrate.h"

#include "settings/settings.h"
#include "common/constants.h"
#include "util/version.h"
#include "options/optiondata.h"
#include "gui/messagesettings.h"

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

void removeAndLog(Settings& settings, const QString& key)
{
  if(settings.contains(key))
  {
    qInfo() << Q_FUNC_INFO << "Removing" << QFileInfo(Settings::getFilename()).fileName() << key;
    settings.remove(key);
  }
}

void checkAndMigrateSettings()
{
  Settings& settings = Settings::instance();

  optionsVersion = Version(settings.valueStr(lnm::OPTIONS_VERSION));
  Version programVersion;

  if(optionsVersion.isValid())
  {
    qInfo() << Q_FUNC_INFO << "Options" << optionsVersion << "program" << programVersion;

    // Migrate settings =======================================================================
    if(optionsVersion != programVersion)
    {
      qInfo() << Q_FUNC_INFO << "Found settings version mismatch. Settings file version" <<
        optionsVersion << "Program version" << programVersion << ".";

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
        removeAndLog(settings, lnm::ROUTE_STRING_DIALOG_OPTIONS);
        Settings::syncSettings();
      }

      // ===============================================================
      if(optionsVersion < Version("2.4.3.rc1"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before 2.4.3.rc1";
        removeAndLog(settings, "MainWindow/Widget_mapThemeComboBox");
        Settings::syncSettings();
      }

      // ===============================================================
      if(optionsVersion <= Version("2.4.5"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before or equal to 2.4.5";

        // Route view
        removeAndLog(settings, "Route/View_tableViewRoute");

        // Table columns dialog
        removeAndLog(settings, "Route/FlightPlanTableColumnsCheckBoxStates");

        // Route tabs
        removeAndLog(settings, "Route/WidgetTabsTabIds");
        removeAndLog(settings, "Route/WidgetTabsCurrentTabId");
        removeAndLog(settings, "Route/WidgetTabsLocked");

        // Reset all before flight
        removeAndLog(settings, "Route/ResetAllDialogCheckBoxStates");

        // Complete log search options
        qInfo() << Q_FUNC_INFO << "Removing" << QFileInfo(Settings::getFilename()).fileName() << "SearchPaneLogdata";
        settings.remove("SearchPaneLogdata");

        // Search views
        removeAndLog(settings, "SearchPaneAirport/WidgetView_tableViewAirportSearch");
        removeAndLog(settings, "SearchPaneAirport/WidgetDistView_tableViewAirportSearch");
        removeAndLog(settings, "SearchPaneNav/WidgetView_tableViewAirportSearch");
        removeAndLog(settings, "SearchPaneNav/WidgetDistView_tableViewAirportSearch");
        removeAndLog(settings, "SearchPaneUserdata/WidgetView_tableViewUserdata");

        // info tabs
        removeAndLog(settings, "InfoWindow/WidgetTabsTabIds");
        removeAndLog(settings, "InfoWindow/WidgetTabsCurrentTabId");
        removeAndLog(settings, "InfoWindow/WidgetTabsLocked");

        // Choice dialog import and export
        removeAndLog(settings, "UserdataExport/ChoiceDialogCheckBoxStates");
        removeAndLog(settings, "Logdata/CsvExportCheckBoxStates");

        // Range rings
        removeAndLog(settings, lnm::MAP_RANGEMARKERS);

        // Marble plugins
        for(const QString& key : settings.childGroups())
          if(key.startsWith("plugin_"))
            removeAndLog(settings, key);

        // and vatsim URL,
        settings.setValue("OptionsDialog/Widget_lineEditOptionsWeatherVatsimUrl",
                          QString("https://metar.vatsim.net/metar.php?id=ALL"));

        // clear allow undock map?

        Settings::syncSettings();
      }

      if(optionsVersion <= Version("2.6.0.beta"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before or equal to 2.6.0.beta";
        removeAndLog(settings, "SearchPaneLogdata/WidgetView_tableViewLogdata");
      }

      if(optionsVersion <= Version("2.6.1.beta"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before or equal to 2.6.1.beta";
        removeAndLog(settings, lnm::ROUTE_EXPORT_FORMATS);
        removeAndLog(settings, "RouteExport/RouteExportDialog_tableViewRouteExport");
        removeAndLog(settings, "RouteExport/RouteExportDialog_RouteMultiExportDialog_size");
        removeAndLog(settings, "RouteExport/RouteExportDialog_tableViewRouteExport");
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
        removeAndLog(settings, "SearchPaneOnlineCenter/WidgetView_tableViewOnlineCenterSearch");
        removeAndLog(settings, "SearchPaneOnlineClient/WidgetView_tableViewOnlineClientSearch");
        removeAndLog(settings, "SearchPaneOnlineServer/WidgetView_tableViewOnlineServerSearch");
      }

      if(optionsVersion <= Version("2.6.14"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before or equal to 2.6.14";
        removeAndLog(settings, "SearchPaneAirport/WidgetDistView_tableViewAirportSearch");
        removeAndLog(settings, "SearchPaneAirport/WidgetView_tableViewAirportSearch");
        removeAndLog(settings, "OptionsDialog/Widget_lineEditOptionsWeatherVatsimUrl");
      }

      if(optionsVersion <= Version("2.6.16"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before or equal to 2.6.16";
        removeAndLog(settings, "OptionsDialog/DisplayOptionsuserAircraft_2097152"); // ITEM_USER_AIRCRAFT_COORDINATES
        removeAndLog(settings, "OptionsDialog/DisplayOptionsAiAircraft_2"); // ITEM_AI_AIRCRAFT_COORDINATES
        removeAndLog(settings, "Route/View_tableViewRoute");
        removeAndLog(settings, "OptionsDialog/Widget_lineEditOptionsWeatherIvaoUrl");
        removeAndLog(settings, "Map/DetailFactor");
      }

      if(optionsVersion <= Version("2.6.17"))
        removeAndLog(settings, "Map/MarkDisplay"); // MAP_MARK_DISPLAY

      if(optionsVersion <= Version("2.7.8.develop"))
      {
        removeAndLog(settings, "OptionsDialog/Widget_lineEditOptionsWeatherIvaoUrl");
        removeAndLog(settings, "Map/Airports");
      }

      if(optionsVersion <= Version("2.8.0.beta"))
      {
        removeAndLog(settings, "SearchPaneLogdata/WidgetView_tableViewLogdata");
        removeAndLog(settings, "Route/View_tableViewRoute");
        removeAndLog(settings, "SearchPaneAirport/WidgetDistView_tableViewAirportSearch");
        removeAndLog(settings, "SearchPaneAirport/WidgetView_tableViewAirportSearch");
        removeAndLog(settings, "SearchPaneNav/WidgetDistView_tableViewNavSearch");
        removeAndLog(settings, "SearchPaneNav/WidgetView_tableViewNavSearch");
      }

      qInfo() << Q_FUNC_INFO << "Clearing all essential messages since version differs";
      messages::resetEssentialMessages();

      // Set program version to options and save ===================
      settings.setValue(lnm::OPTIONS_VERSION, programVersion.getVersionString());
      Settings::syncSettings();
    }
  }
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
