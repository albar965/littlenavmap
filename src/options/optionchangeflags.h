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

#ifndef LITTLENAVMAP_OPTIONCHANGEFLAGS_H
#define LITTLENAVMAP_OPTIONCHANGEFLAGS_H

#include "util/flags.h"

namespace optc {

/*
 * Flag field that roughly defines what was changed in options and if a restart is required
 */
enum OptionChangeFlag : quint32
{
  OPTION_CHANGE_NONE = 0,
  OPTION_CHANGE_OTHER = 1 << 1,
  OPTION_CHANGE_UI_FONT = 1 << 2, /* User interface font */
  OPTION_CHANGE_LANGUAGE = 1 << 3, /* User interface language */
  OPTION_CHANGE_UNITS = 1 << 4, /* Units */
  OPTION_CHANGE_UNDOCKMAP = 1 << 5, /* Map window undock status */
  OPTION_CHANGE_MAPTHEMES = 1 << 6, /* Map theme directory or keys changed */
  OPTION_CHANGE_SCENERY = 1 << 7, /* Scenery library includes or excludes */
  OPTION_CHANGE_WEBSERVER = 1 << 8, /* Webserver root directory */
  OPTION_CHANGE_TEXT_SIZES = 1 << 9, /* Info or table text sizes */
  OPTION_CHANGE_ELEVATION = 1 << 10, /* Elevation source, path or cache change */

  // Also change optionChangeFlagsToString()

  /* All options that require a restart */
  OPTION_CHANGE_RESTART_NEEDED = OPTION_CHANGE_LANGUAGE | OPTION_CHANGE_UNDOCKMAP | OPTION_CHANGE_MAPTHEMES | OPTION_CHANGE_WEBSERVER,

  /* All options but not restart flag */
  OPTION_CHANGE_ALL = OPTION_CHANGE_OTHER | OPTION_CHANGE_UI_FONT | OPTION_CHANGE_LANGUAGE | OPTION_CHANGE_UNITS | OPTION_CHANGE_UNDOCKMAP |
                      OPTION_CHANGE_MAPTHEMES | OPTION_CHANGE_SCENERY | OPTION_CHANGE_WEBSERVER | OPTION_CHANGE_TEXT_SIZES |
                      OPTION_CHANGE_ELEVATION,
};

ATOOLS_DECLARE_FLAGS_32(OptionChangeFlags, optc::OptionChangeFlag)
ATOOLS_DECLARE_OPERATORS_FOR_FLAGS(optc::OptionChangeFlags)

QDebug operator<<(QDebug out, OptionChangeFlag flags);
QDebug operator<<(QDebug out, const OptionChangeFlags& flags);

} // namespace optc

Q_DECLARE_TYPEINFO(optc::OptionChangeFlags, Q_PRIMITIVE_TYPE);

#endif // LITTLENAVMAP_OPTIONCHANGEFLAGS_H
