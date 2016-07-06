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

#include "common/settingsmigrate.h"

#include "settings/settings.h"
#include "common/constants.h"
#include "logging/loggingdefs.h"
#include "util/version.h"

#include <QApplication>
#include <QRegularExpression>

using atools::settings::Settings;
using  atools::util::Version;

namespace migrate {

void checkAndMigrateSettings()
{
  Settings& s = Settings::instance();

  Version optionsVersion(s.valueStr(lnm::OPTIONS_VERSION));
  Version appVersion(QCoreApplication::applicationVersion());

  if(optionsVersion.isValid())
  {
    if(optionsVersion != appVersion)
    {
      qWarning().nospace().noquote() << "Found settings version mismatch. Settings file: " <<
      optionsVersion << ". Program " << appVersion << ".";

      // ------------------------------------------------------
      // Migrate/delete any settings that are not compatible right here
      // ------------------------------------------------------

      s.setValue(lnm::OPTIONS_VERSION, appVersion.getVersionString());
      s.syncSettings();
    }
  }
  else
  {
    qWarning() << "No version information found in settings file. Updating to" << appVersion;
    s.setValue(lnm::OPTIONS_VERSION, appVersion.getVersionString());
    s.syncSettings();
  }
}

} // namespace migrate
