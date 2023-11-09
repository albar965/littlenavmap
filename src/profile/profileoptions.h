/*****************************************************************************
* Copyright 2015-2023 Alexander Barthel alex@littlenavmap.org
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

#include <QCoreApplication>

namespace optsp {

/* Display options for elevation profile affecting line as well as top and left labels. Saved to settings. */
enum DisplayOptionProfile
{
  PROFILE_NONE = 0,

  /* General options */
  PROFILE_TOOLTIP = 1 << 1,
  PROFILE_HIGHLIGHT = 1 << 2,

  /* Labels */
  PROFILE_LABELS_ALT = 1 << 3,
  PROFILE_LABELS_DISTANCE = 1 << 4,
  PROFILE_LABELS_MAG_COURSE = 1 << 5,
  PROFILE_LABELS_TRUE_COURSE = 1 << 6,
  PROFILE_LABELS_RELATED = 1 << 7,

  /* Elevation Profile */
  PROFILE_GROUND = 1 << 8,
  PROFILE_SAFE_ALTITUDE = 1 << 9,
  PROFILE_LEG_SAFE_ALTITUDE = 1 << 10,

  /* Flight plan line options */
  PROFILE_FP_DIST = 1 << 11,
  PROFILE_FP_MAG_COURSE = 1 << 12,
  PROFILE_FP_TRUE_COURSE = 1 << 13,
  PROFILE_FP_VERTICAL_ANGLE = 1 << 14,
  PROFILE_FP_ALT_RESTRICTION = 1 << 15,
  PROFILE_FP_SPEED_RESTRICTION = 1 << 16,
  PROFILE_FP_ALT_RESTRICTION_BLOCK = 1 << 17,

  /* User aircraft labels */
  PROFILE_AIRCRAFT_ALTITUDE = 1 << 18,
  PROFILE_AIRCRAFT_VERT_SPEED = 1 << 19,
  PROFILE_AIRCRAFT_VERT_ANGLE_NEXT = 1 << 20,

  /* ui->labelProfileInfo in ProfileWidget::updateHeaderLabel() */
  PROFILE_HEADER_DIST_TIME_TO_DEST = 1 << 21, /* Destination: 82 NM (0 h 15 m). */
  PROFILE_HEADER_DIST_TIME_TO_TOD = 1 << 22, /* Top of Descent: 69 NM (0 h 11 m). */
  PROFILE_HEADER_DESCENT_PATH_DEVIATION = 1 << 23, /* Descent Path: Deviation: 1,153 ft, above */
  PROFILE_HEADER_DESCENT_PATH_ANGLE = 1 << 24, /* ... Angle and Speed: -3.4°, -2,102 fpm */

  // Next is 1 << 25

  /* All drawn in the top label */
  PROFILE_TOP_ANY = PROFILE_LABELS_DISTANCE | PROFILE_LABELS_MAG_COURSE | PROFILE_LABELS_TRUE_COURSE | PROFILE_LABELS_RELATED,

  /* All drawn in the header label */
  PROFILE_HEADER_ANY = PROFILE_HEADER_DIST_TIME_TO_DEST | PROFILE_HEADER_DIST_TIME_TO_TOD | PROFILE_HEADER_DESCENT_PATH_DEVIATION |
                       PROFILE_HEADER_DESCENT_PATH_ANGLE,

  /* All drawn along the flight plan line */
  PROFILE_FP_ANY = PROFILE_FP_DIST | PROFILE_FP_MAG_COURSE | PROFILE_FP_TRUE_COURSE | PROFILE_FP_VERTICAL_ANGLE,
};

Q_DECLARE_FLAGS(DisplayOptionsProfile, optsp::DisplayOptionProfile);
Q_DECLARE_OPERATORS_FOR_FLAGS(optsp::DisplayOptionsProfile);

/* All available options for loops */
static const QVector<optsp::DisplayOptionProfile> ALL_OPTIONS({optsp::PROFILE_LABELS_DISTANCE, optsp::PROFILE_LABELS_MAG_COURSE,
                                                               optsp::PROFILE_LABELS_TRUE_COURSE, optsp::PROFILE_LABELS_RELATED,
                                                               optsp::PROFILE_FP_DIST, optsp::PROFILE_FP_MAG_COURSE,
                                                               optsp::PROFILE_FP_TRUE_COURSE, optsp::PROFILE_FP_VERTICAL_ANGLE,
                                                               optsp::PROFILE_FP_ALT_RESTRICTION, optsp::PROFILE_FP_SPEED_RESTRICTION,
                                                               optsp::PROFILE_FP_ALT_RESTRICTION_BLOCK, optsp::PROFILE_GROUND,
                                                               optsp::PROFILE_SAFE_ALTITUDE, optsp::PROFILE_LEG_SAFE_ALTITUDE,
                                                               optsp::PROFILE_LABELS_ALT, optsp::PROFILE_TOOLTIP,
                                                               optsp::PROFILE_HIGHLIGHT, optsp::PROFILE_AIRCRAFT_ALTITUDE,
                                                               optsp::PROFILE_AIRCRAFT_VERT_SPEED, optsp::PROFILE_AIRCRAFT_VERT_ANGLE_NEXT,
                                                               optsp::PROFILE_HEADER_DIST_TIME_TO_DEST,
                                                               optsp::PROFILE_HEADER_DIST_TIME_TO_TOD,
                                                               optsp::PROFILE_HEADER_DESCENT_PATH_DEVIATION,
                                                               optsp::PROFILE_HEADER_DESCENT_PATH_ANGLE

                                                              });

static const optsp::DisplayOptionsProfile DEFAULT_OPTIONS = optsp::PROFILE_LABELS_DISTANCE | optsp::PROFILE_LABELS_RELATED |
                                                            optsp::PROFILE_FP_MAG_COURSE | optsp::PROFILE_FP_VERTICAL_ANGLE |
                                                            optsp::PROFILE_FP_ALT_RESTRICTION | optsp::PROFILE_FP_SPEED_RESTRICTION |
                                                            optsp::PROFILE_FP_ALT_RESTRICTION_BLOCK | optsp::PROFILE_GROUND |
                                                            optsp::PROFILE_SAFE_ALTITUDE | optsp::PROFILE_LEG_SAFE_ALTITUDE |
                                                            optsp::PROFILE_LABELS_ALT | optsp::PROFILE_TOOLTIP | optsp::PROFILE_HIGHLIGHT |
                                                            optsp::PROFILE_AIRCRAFT_ALTITUDE | optsp::PROFILE_AIRCRAFT_VERT_SPEED |
                                                            optsp::PROFILE_AIRCRAFT_VERT_ANGLE_NEXT |
                                                            optsp::PROFILE_HEADER_DIST_TIME_TO_DEST |
                                                            optsp::PROFILE_HEADER_DIST_TIME_TO_TOD |
                                                            optsp::PROFILE_HEADER_DESCENT_PATH_DEVIATION |
                                                            optsp::PROFILE_HEADER_DESCENT_PATH_ANGLE;
}

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
  void saveState();
  void restoreState();

private:
  /* Initialized with the default options for new installation */
  optsp::DisplayOptionsProfile displayOptions = optsp::DEFAULT_OPTIONS;
  QWidget *parentWidget;
};

#endif // LNM_PROFILEOPTIONS_H
