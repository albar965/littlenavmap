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

#include "options/optionchangeflags.h"

#include <QDebug>

namespace optc {

QStringList optionChangeFlagsToString(const optc::OptionChangeFlags& flags)
{
  ATOOLS_FLAGS_TO_STR_BEGIN(optc::OPTION_CHANGE_NONE);
  ATOOLS_FLAGS_TO_STR(optc::OPTION_CHANGE_OTHER);
  ATOOLS_FLAGS_TO_STR(optc::OPTION_CHANGE_UI_FONT);
  ATOOLS_FLAGS_TO_STR(optc::OPTION_CHANGE_LANGUAGE);
  ATOOLS_FLAGS_TO_STR(optc::OPTION_CHANGE_UNITS);
  ATOOLS_FLAGS_TO_STR(optc::OPTION_CHANGE_UNDOCKMAP);
  ATOOLS_FLAGS_TO_STR(optc::OPTION_CHANGE_MAPTHEMES);
  ATOOLS_FLAGS_TO_STR(optc::OPTION_CHANGE_SCENERY);
  ATOOLS_FLAGS_TO_STR(optc::OPTION_CHANGE_WEBSERVER);
  ATOOLS_FLAGS_TO_STR_END;
}

QDebug operator<<(QDebug out, OptionChangeFlag flags)
{
  out << optc::OptionChangeFlags(flags);
  return out;
}

QDebug operator<<(QDebug out, const optc::OptionChangeFlags& flags)
{
  QDebugStateSaver saver(out);
  out.nospace().noquote() << optionChangeFlagsToString(flags).join("|");
  return out;
}

} // namespace optc
