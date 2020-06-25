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

#include <QDebug>
#include <QApplication>
#include <QRegularExpression>
#include <QFile>
#include <QDir>
#include <QSettings>

using atools::settings::Settings;
using  atools::util::Version;

namespace migrate {

void checkAndMigrateSettings()
{
  Settings& settings = Settings::instance();

  Version optionsVersion(settings.valueStr(lnm::OPTIONS_VERSION));
  Version appVersion;

  if(optionsVersion.isValid())
  {
    // Migrate map style file =======================================================================
    QFile nightstyleFile(Settings::getPath() + QDir::separator() + "little_navmap_nightstyle.ini");
    QFile mapstyleFile(Settings::getPath() + QDir::separator() + "little_navmap_mapstyle.ini");
    QSettings mapstyleSettings(mapstyleFile.fileName(), QSettings::IniFormat);
    QSettings nightstyleSettings(nightstyleFile.fileName(), QSettings::IniFormat);
    Version mapstyleVersion(mapstyleSettings.value("Options/Version").toString());

    // No version or old
    if(!mapstyleVersion.isValid() || mapstyleVersion < Version("2.0.1.beta"))
    {
      qInfo() << Q_FUNC_INFO << "Moving little_navmap_mapstyle.ini to backup";

      // Backup with version name
      QString newName = Settings::getPath() + QDir::separator() + "little_navmap_mapstyle_backup" +
                        (mapstyleVersion.isValid() ? "_" + mapstyleVersion.getVersionString() : "") +
                        ".ini";

      // Rename so LNM can create a new one later
      if(mapstyleFile.rename(newName))
        qInfo() << "Renamed" << mapstyleFile.fileName() << "to" << newName;
      else
        qWarning() << "Renaming" << mapstyleFile.fileName() << "to" << newName << "failed";
    }

    // Migrate settings =======================================================================
    if(optionsVersion != appVersion)
    {
      qInfo().nospace().noquote() << "Found settings version mismatch. Settings file: " <<
        optionsVersion << ". Program " << appVersion << ".";

      // http://tgftp.nws.noaa.gov/data/observations/metar/stations/%1.TXT to
      // https://tgftp.nws.noaa.gov/data/observations/metar/stations/%1.TXT
      // in widget Widget_lineEditOptionsWeatherNoaaUrl
      if(optionsVersion < Version("2.2.4"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting NOAA URL";

        // Widget_lineEditOptionsWeatherNoaaUrl=http://tgftp.nws.noaa.gov/data/observations/metar/stations/%1.TXT
        QString noaaUrl = settings.valueStr("OptionsDialog/Widget_lineEditOptionsWeatherNoaaUrl");

        if(noaaUrl.isEmpty() ||
           noaaUrl.toLower() == "http://tgftp.nws.noaa.gov/data/observations/metar/stations/%1.txt")
        {
          qInfo() << Q_FUNC_INFO << "Changing NOAA URL to HTTPS";
          settings.setValue("OptionsDialog/Widget_lineEditOptionsWeatherNoaaUrl",
                            QString("https://tgftp.nws.noaa.gov/data/observations/metar/stations/%1.TXT"));
          settings.syncSettings();
        }
      }

      if(optionsVersion < Version("2.4.2.beta"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before 2.4.2.beta";
        settings.remove(lnm::ROUTE_STRING_DIALOG_OPTIONS);
        settings.syncSettings();

        nightstyleSettings.remove("StyleColors/Disabled_WindowText");
        nightstyleSettings.sync();
      }

      if(optionsVersion < Version("2.4.3.rc1"))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting settings for versions before 2.4.3.rc1";
        settings.remove("MainWindow/Widget_mapThemeComboBox");
        settings.syncSettings();
      }

      // CenterRadiusACC=60 and CenterRadiusFIR=60
      if(optionsVersion <= Version(2, 0, 2))
      {
        qInfo() << Q_FUNC_INFO << "Adjusting Online/CenterRadiusACC and Online/CenterRadiusFIR";
        if(settings.valueInt("Online/CenterRadiusACC", -1) == -1)
          settings.setValue("Online/CenterRadiusACC", 100);
        if(settings.valueInt("Online/CenterRadiusFIR", -1) == -1)
          settings.setValue("Online/CenterRadiusFIR", 100);

        settings.syncSettings();
      }

      // ------------------------------------------------------
      // Migrate/delete any settings that are not compatible right here
      // ------------------------------------------------------

      // Test the update channels
      if(!settings.contains(lnm::OPTIONS_UPDATE_CHANNELS) || (optionsVersion.isStable() && !appVersion.isStable()))
      {
        // No channel assigned yet or user moved from a stable to beta or development version
        if(appVersion.isBeta() || appVersion.isReleaseCandidate())
        {
          qInfo() << "Adjusting update channel to beta";
          settings.setValue(lnm::OPTIONS_UPDATE_CHANNELS, opts::STABLE_BETA);
        }
        else if(appVersion.isDevelop())
        {
          qInfo() << "Adjusting update channel to develop";
          settings.setValue(lnm::OPTIONS_UPDATE_CHANNELS, opts::STABLE_BETA_DEVELOP);
        }
      }

      settings.setValue(lnm::OPTIONS_VERSION, appVersion.getVersionString());
      settings.syncSettings();
    }
  }
  else
  {
    qWarning() << "No version information found in settings file. Updating to" << appVersion;
    settings.setValue(lnm::OPTIONS_VERSION, appVersion.getVersionString());
    settings.syncSettings();
  }
}

} // namespace migrate
