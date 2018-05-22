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

#include "common/constants.h"

#include "gui/helphandler.h"

#include "options/optiondata.h"

#include <QDir>
#include <QRegularExpression>

static QString supportedLanguageOnline;

namespace lnm {

const QString helpLanguageOnline()
{
  if(supportedLanguageOnline.isEmpty())
  {
    if(OptionData::instance().getFlags() & opts::GUI_OVERRIDE_LANGUAGE)
      // Stick to English as forced in options
      supportedLanguageOnline = "en";
    else
    {
      // Get the online indicator file
      QString onlineFlagFile = atools::gui::HelpHandler::getHelpFile(
        QString("help") + QDir::separator() + "little-navmap-user-manual-${LANG}.online",
        OptionData::instance().getFlags() & opts::GUI_OVERRIDE_LANGUAGE);

      // Extract language from the file
      QRegularExpression regexp("little-navmap-user-manual-(.+)\\.online", QRegularExpression::CaseInsensitiveOption);
      QRegularExpressionMatch match = regexp.match(onlineFlagFile);
      if(match.hasMatch() && !match.captured(1).isEmpty())
        supportedLanguageOnline = match.captured(1);

      // Fall back to English
      if(supportedLanguageOnline.isEmpty())
        supportedLanguageOnline = "en";
    }
  }

  return supportedLanguageOnline;
}

} // namespace lnm
