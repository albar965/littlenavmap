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

#include "common/settingsmigrate.h"

#include "settings/settings.h"
#include "common/constants.h"
#include "util/version.h"
#include "options/optiondata.h"

#include <QDebug>
#include <QApplication>
#include <QRegularExpression>

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
    if(optionsVersion != appVersion)
    {
      qInfo().nospace().noquote() << "Found settings version mismatch. Settings file: " <<
        optionsVersion << ". Program " << appVersion << ".";

      // ------------------------------------------------------
      // Migrate/delete any settings that are not compatible right here
      // ------------------------------------------------------
      if(optionsVersion <= Version(1, 2, 4)) // Reset main window state since a toolbar was added
      {
        qInfo() << Q_FUNC_INFO << "removing" << lnm::MAINWINDOW_WIDGET_STATE << lnm::MAINWINDOW_WIDGET_STATE_POS
                << lnm::MAINWINDOW_WIDGET_STATE_SIZE << lnm::MAINWINDOW_WIDGET_STATE_MAXIMIZED;
        settings.remove(lnm::MAINWINDOW_WIDGET_STATE);
        settings.remove(lnm::MAINWINDOW_WIDGET_STATE_POS);
        settings.remove(lnm::MAINWINDOW_WIDGET_STATE_SIZE);
        settings.remove(lnm::MAINWINDOW_WIDGET_STATE_MAXIMIZED);
      }

      // Test the update channels
      if(!settings.contains(lnm::OPTIONS_UPDATE_CHANNELS) || (optionsVersion.isStable() && !appVersion.isStable()))
      {
        // No channel assigned yet or user moved from a stable to beta or development version
        if(appVersion.isBeta())
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
