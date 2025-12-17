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

#ifndef LNM_PROFILEOPTIONS_H
#define LNM_PROFILEOPTIONS_H

#include "profile/profileoptionflags.h"

#include <QCoreApplication>

/*
 * Wraps, loads and saves the elevation profile options.
 * Also has a method to display and edit options.
 */
class ProfileOptions
{
  Q_DECLARE_TR_FUNCTIONS(ProfileOptions)

public:
  explicit ProfileOptions(QWidget *parentWidgetParam);

  /* Show edit dialog and return true if user hit ok */
  bool showOptions();

  const optsp::DisplayOptionsProfile& getDisplayOptions() const
  {
    return displayOptions;
  }

  /* Save and restore options */
  void saveState() const;
  void restoreState();

private:
  /* Initialized with the default options for new installation */
  optsp::DisplayOptionsProfile displayOptions = optsp::DEFAULT_OPTIONS;
  QWidget *parentWidget;
};

#endif // LNM_PROFILEOPTIONS_H
