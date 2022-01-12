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

#ifndef LITTLENAVMAP_PROCFLAGS_H
#define LITTLENAVMAP_PROCFLAGS_H

#include <QDebug>

/*
 * Procedure types for approach, transition, SID and STAR.
 */
namespace proc {

// =====================================================================
/* Type covering all objects that are passed around in the program. Also use to determine what should be drawn. */
enum MapProcedureType
{
  PROCEDURE_NONE = 0,
  PROCEDURE_APPROACH = 1 << 0, /* Also custom */
  PROCEDURE_MISSED = 1 << 1,
  PROCEDURE_TRANSITION = 1 << 2,
  PROCEDURE_SID = 1 << 3, /* Also custom */
  PROCEDURE_SID_TRANSITION = 1 << 4,
  PROCEDURE_STAR = 1 << 5,
  PROCEDURE_STAR_TRANSITION = 1 << 6,

  /* SID, STAR and respective transitions */
  PROCEDURE_STAR_ALL = PROCEDURE_STAR | PROCEDURE_STAR_TRANSITION,
  PROCEDURE_SID_ALL = PROCEDURE_SID | PROCEDURE_SID_TRANSITION,

  /* Approach and transition but no missed */
  PROCEDURE_APPROACH_ALL = PROCEDURE_APPROACH | PROCEDURE_TRANSITION,

  /* Approach, transition and missed */
  PROCEDURE_APPROACH_ALL_MISSED = PROCEDURE_TRANSITION | PROCEDURE_APPROACH | PROCEDURE_MISSED,

  /* All SID, STAR and respective transitions */
  PROCEDURE_SID_STAR_ALL = PROCEDURE_STAR_ALL | PROCEDURE_SID_ALL,

  /* All leading towards destination */
  PROCEDURE_ARRIVAL_ALL = PROCEDURE_APPROACH_ALL_MISSED | PROCEDURE_STAR_ALL,

  /* All from departure */
  PROCEDURE_DEPARTURE = PROCEDURE_SID_ALL,

  /* Any procedure but not transitions */
  PROCEDURE_ANY_PROCEDURE = PROCEDURE_APPROACH | PROCEDURE_SID | PROCEDURE_STAR,

  /* Any transition */
  PROCEDURE_ANY_TRANSITION = PROCEDURE_TRANSITION | PROCEDURE_SID_TRANSITION | PROCEDURE_STAR_TRANSITION,

  PROCEDURE_ALL = PROCEDURE_ARRIVAL_ALL | PROCEDURE_DEPARTURE,
  PROCEDURE_ALL_BUT_MISSED = PROCEDURE_ALL & ~PROCEDURE_MISSED,
};

Q_DECLARE_FLAGS(MapProcedureTypes, MapProcedureType);
Q_DECLARE_OPERATORS_FOR_FLAGS(proc::MapProcedureTypes);

QDebug operator<<(QDebug out, const proc::MapProcedureTypes& type);

// =====================================================================
/* ARINC leg types */
enum ProcedureLegType
{
  INVALID_LEG_TYPE,
  ARC_TO_FIX,
  COURSE_TO_ALTITUDE,
  COURSE_TO_DME_DISTANCE,
  COURSE_TO_FIX,
  COURSE_TO_INTERCEPT,
  COURSE_TO_RADIAL_TERMINATION,
  DIRECT_TO_FIX,
  FIX_TO_ALTITUDE,
  TRACK_FROM_FIX_FROM_DISTANCE,
  TRACK_FROM_FIX_TO_DME_DISTANCE,
  FROM_FIX_TO_MANUAL_TERMINATION,
  HOLD_TO_ALTITUDE,
  HOLD_TO_FIX,
  HOLD_TO_MANUAL_TERMINATION,
  INITIAL_FIX,
  PROCEDURE_TURN,
  CONSTANT_RADIUS_ARC,
  TRACK_TO_FIX,
  HEADING_TO_ALTITUDE_TERMINATION,
  HEADING_TO_DME_DISTANCE_TERMINATION,
  HEADING_TO_INTERCEPT,
  HEADING_TO_MANUAL_TERMINATION,
  HEADING_TO_RADIAL_TERMINATION,

  DIRECT_TO_RUNWAY, /* Artifical last segment inserted for first leg of departure (similar to IF) */
  CIRCLE_TO_LAND, /* Artifical last segment inserted if approach does not contain a runway end and is a CTL
                   *  Points for airport center */
  STRAIGHT_IN, /* Artifical last segment inserted from MAP to runway */
  START_OF_PROCEDURE, /* Artifical first point if procedures do not start with an initial fix
                       *  or with a track, heading or course to fix having length 0 */
  VECTORS, /* Fills a gap between manual segments and an initial fix */

  /* Custom approach */
  CUSTOM_APP_START, /* Start: INITIAL_FIX */
  CUSTOM_APP_RUNWAY, /* End: COURSE_TO_FIX */

  /* Custom departure */
  CUSTOM_DEP_END, /* End: COURSE_TO_FIX */
  CUSTOM_DEP_RUNWAY /* Start: DIRECT_TO_RUNWAY */,
};

QDebug operator<<(QDebug out, const proc::ProcedureLegType& type);

// =====================================================================
/* Special leg types */
enum LegSpecialType
{
  NONE,
  IAF, /* The Initial Approach Fix (IAF) is the point where the initial approach segment of an instrument approach begins.  */
  FAF, /* Final approach fix. The final approach point on an instrument approach with vertical guidance is glide
        *  slope or glide path intercept at the lowest published altitude (ICAO definition). */
  FACF, /* Final approach course fix. */
  MAP, /* Miseed approach point.
        * This is the point prescribed in each instrument approach at which a missed approach procedure shall
        * be executed if the required visual reference does not exist.[ */
  FEP /* Final endpoint fix - only needed to ignore altitude restriction */
};

} // namespace types

#endif // LITTLENAVMAP_PROCFLAGS_H
