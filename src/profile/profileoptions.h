/*****************************************************************************
* Copyright 2015-2022 Alexander Barthel alex@littlenavmap.org
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

#include <QApplication>

namespace optsp {

/* Display options for elevation profile affecting line as well as top and left labels. */
enum DisplayOptionProfile
{
  PROFILE_NONE = 0,

  /* Top label options */
  PROFILE_TOP_DISTANCE = 1 << 0,
  PROFILE_TOP_MAG_COURSE = 1 << 1,
  PROFILE_TOP_TRUE_COURSE = 1 << 2,
  PROFILE_TOP_RELATED = 1 << 3,

  /* Flight plan line options */
  PROFILE_FP_DIST = 1 << 4,
  PROFILE_FP_MAG_COURSE = 1 << 5,
  PROFILE_FP_TRUE_COURSE = 1 << 6,
  PROFILE_FP_VERTICAL_ANGLE = 1 << 7,

  /* Shown at navaid label */
  PROFILE_FP_ALT_RESTRICTION = 1 << 8,
  PROFILE_FP_SPEED_RESTRICTION = 1 << 9,

  PROFILE_ALT_LABELS = 1 << 10,
  PROFILE_TOOLTIP = 1 << 11,

  /* All drawn in the top label */
  PROFILE_TOP_ANY = PROFILE_TOP_DISTANCE | PROFILE_TOP_MAG_COURSE | PROFILE_TOP_TRUE_COURSE | PROFILE_TOP_RELATED,

  /* All drawn along the flight plan line */
  PROFILE_FP_ANY = PROFILE_FP_DIST | PROFILE_FP_MAG_COURSE | PROFILE_FP_TRUE_COURSE | PROFILE_FP_VERTICAL_ANGLE,
};

Q_DECLARE_FLAGS(DisplayOptionsProfile, optsp::DisplayOptionProfile);
Q_DECLARE_OPERATORS_FOR_FLAGS(optsp::DisplayOptionsProfile);

/* All available options for loops */
static const QVector<optsp::DisplayOptionProfile> ALL_OPTIONS({optsp::PROFILE_TOP_DISTANCE, optsp::PROFILE_TOP_MAG_COURSE,
                                                               optsp::PROFILE_TOP_TRUE_COURSE, optsp::PROFILE_TOP_RELATED,
                                                               optsp::PROFILE_FP_DIST, optsp::PROFILE_FP_MAG_COURSE,
                                                               optsp::PROFILE_FP_TRUE_COURSE, optsp::PROFILE_FP_VERTICAL_ANGLE,
                                                               optsp::PROFILE_FP_ALT_RESTRICTION, optsp::PROFILE_FP_SPEED_RESTRICTION,
                                                               optsp::PROFILE_ALT_LABELS, optsp::PROFILE_TOOLTIP});
}

/*
 * Wraps, loads and saves the elevation profile options.
 * Also has a method to display and edit options.
 */
class ProfileOptions
{
  Q_DECLARE_TR_FUNCTIONS(ProfileOptions)

public:
  ProfileOptions(QWidget *parentWidgetParam);

  /* Show edit dialog and return true if user hit ok */
  bool showOptions();

  const optsp::DisplayOptionsProfile& getDisplayOptions() const
  {
    return displayOptions;
  }

  /* Save and restore options */
  void saveState();
  void restoreState();

private:
  /* Initialized with the default options for new installation */
  optsp::DisplayOptionsProfile displayOptions = optsp::PROFILE_TOP_DISTANCE | optsp::PROFILE_TOP_RELATED |
                                                optsp::PROFILE_FP_MAG_COURSE | optsp::PROFILE_FP_VERTICAL_ANGLE |
                                                optsp::PROFILE_ALT_LABELS | optsp::PROFILE_TOOLTIP;
  QWidget *parentWidget;
};

#endif // LNM_PROFILEOPTIONS_H
