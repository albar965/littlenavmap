/*****************************************************************************
* Copyright 2015-2017 Alexander Barthel albar965@mailbox.org
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

#include "common/constants.h"

#include "gui/helphandler.h"

#include <options/optiondata.h>

static QStringList supportedLanguages;

namespace lnm {

const QStringList helpLanguages()
{
  if(supportedLanguages.isEmpty())
  {
    if(OptionData::instance().getFlags() & opts::GUI_OVERRIDE_LANGUAGE)
      // Stick to English as forced in options
      supportedLanguages.append("en");
    else
      // Otherwise determine manual language by installed PDF files
      supportedLanguages = atools::gui::HelpHandler::getInstalledLanguages(
        "help", "little-navmap-user-manual-([a-z]{2})\\.pdf");
  }

  return supportedLanguages;
}

} // namespace lnm
