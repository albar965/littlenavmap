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

#ifndef LITTLENAVMAP_PROCTYPES_H
#define LITTLENAVMAP_PROCTYPES_H

#include "common/maptypes.h"
#include "geo/pos.h"
#include "geo/line.h"
#include "geo/linestring.h"

#include <QColor>
#include <QRegularExpression>
#include <QString>

/*
 * Procedure types for approach, transition, SID and STAR.
 */
namespace proc {

/* Initialize all text that are translateable after loading the translation files */
void initTranslateableTexts();

/* Type covering all objects that are passed around in the program. Also use to determine what should be drawn. */
enum MapProcedureType
{
  PROCEDURE_NONE = 0,
  PROCEDURE_APPROACH = 1 << 0,
  PROCEDURE_MISSED = 1 << 1,
  PROCEDURE_TRANSITION = 1 << 2,
  PROCEDURE_SID = 1 << 3,
  PROCEDURE_SID_TRANSITION = 1 << 4,
  PROCEDURE_STAR = 1 << 5,
  PROCEDURE_STAR_TRANSITION = 1 << 6,

  /* Approach */
  PROCEDURE_ARRIVAL = PROCEDURE_TRANSITION | PROCEDURE_APPROACH | PROCEDURE_MISSED,

  PROCEDURE_STAR_ALL = PROCEDURE_STAR | PROCEDURE_STAR_TRANSITION,
  PROCEDURE_SID_ALL = PROCEDURE_SID | PROCEDURE_SID_TRANSITION,

  PROCEDURE_SID_STAR_ALL = PROCEDURE_STAR_ALL | PROCEDURE_SID_ALL,

  PROCEDURE_ARRIVAL_ALL = PROCEDURE_ARRIVAL | PROCEDURE_STAR_ALL,

  PROCEDURE_DEPARTURE = PROCEDURE_SID_ALL,

  PROCEDURE_ALL = PROCEDURE_ARRIVAL_ALL | PROCEDURE_DEPARTURE,
  PROCEDURE_ALL_BUT_MISSED = PROCEDURE_ALL & ~PROCEDURE_MISSED,
};

Q_DECLARE_FLAGS(MapProcedureTypes, MapProcedureType);
Q_DECLARE_OPERATORS_FOR_FLAGS(proc::MapProcedureTypes);

QDebug operator<<(QDebug out, const proc::MapProcedureTypes& type);

/* Altitude restriction for approaches or transitions */
struct MapAltRestriction
{
  enum Descriptor
  {
    NONE,
    AT,
    AT_OR_ABOVE,
    AT_OR_BELOW,
    BETWEEN
  };

  Descriptor descriptor = NONE;
  float alt1, alt2;

  bool isValid() const
  {
    return descriptor != NONE;
  }

};

struct MapSpeedRestriction
{
  enum Descriptor
  {
    NONE,
    AT,
    MIN,
    MAX
  };

  Descriptor descriptor = NONE;
  float speed = 0.f;

  bool isValid() const
  {
    return descriptor != NONE && speed > 0.f;
  }

};

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

  DIRECT_TO_RUNWAY, /* Artifical last segment inserted if approach does not contain a runway end */
  START_OF_PROCEDURE /* Artifical first point if procedures do not start with an initial fix
                      *  or with a track, heading or course to fix having length 0 */
};

QDebug operator<<(QDebug out, const proc::ProcedureLegType& type);

struct MapProcedureLeg;

/* Reduced procedure leg type for map index, tooltips and similar */
struct MapProcedurePoint
{
  MapProcedurePoint(const MapProcedureLeg& leg);

  float calculatedDistance, calculatedTrueCourse, time, theta, rho, magvar;

  QString fixType, fixIdent, recFixType, recFixIdent, turnDirection;

  proc::MapProcedureTypes mapType = PROCEDURE_NONE;

  QStringList displayText, remarks;
  MapAltRestriction altRestriction;
  MapSpeedRestriction speedRestriction;

  proc::ProcedureLegType type;

  bool missed, flyover, transition;

  atools::geo::Pos position;

  bool isValid() const
  {
    return position.isValid();
  }

  const atools::geo::Pos& getPosition() const
  {
    return position;
  }

  int getId() const
  {
    return -1;
  }

};

struct MapProcedureRef
{
  MapProcedureRef(int airport, int runwayEnd, int approach, int transition, int leg, proc::MapProcedureTypes type)
    : airportId(airport), runwayEndId(runwayEnd), approachId(approach), transitionId(transition), legId(leg),
    mapType(type)
  {
  }

  MapProcedureRef()
    : airportId(-1), runwayEndId(-1), approachId(-1), transitionId(-1), legId(-1), mapType(PROCEDURE_NONE)
  {
  }

  int airportId /* always from navdatabase*/,
      runwayEndId /* always from navdatabase*/,
      approachId, transitionId, legId;
  proc::MapProcedureTypes mapType;

  bool isLeg() const
  {
    return legId != -1;
  }

  bool hasApproachOnlyIds() const
  {
    return approachId != -1 && transitionId == -1;
  }

  bool hasTransitionId() const
  {
    return transitionId != -1;
  }

  bool hasApproachId() const
  {
    return approachId != -1;
  }

  bool hasApproachAndTransitionIds() const
  {
    return approachId != -1 && transitionId != -1;
  }

  bool hasApproachOrTransitionIds() const
  {
    return approachId != -1 || transitionId != -1;
  }

  bool isEmpty() const
  {
    return approachId == -1 && transitionId == -1;
  }

};

struct MapProcedureLeg
{

  QString fixType, fixIdent, fixRegion,
          recFixType, recFixIdent, recFixRegion, /* Recommended fix also used by rho and theta */
          turnDirection /* Turn to this fix*/;

  QStringList displayText /* Fix label for map - filled in approach query */,
              remarks /* Additional remarks for tree - filled in approach query */;
  atools::geo::Pos fixPos, recFixPos,
                   interceptPos, /* Position of an intercept leg for grey circle */
                   procedureTurnPos /* Extended position of a procedure turn */;
  atools::geo::Line line, /* Line with flying direction from pos1 to pos2 */
                    holdLine; /* Helping line to find out if aircraft leaves the hold */

  atools::geo::LineString geometry; /* Same as line or geometry approximation for intercept or arcs for distance to leg calculation */

  /* Navaids resolved by approach query class */
  map::MapSearchResult navaids;

  MapAltRestriction altRestriction;
  MapSpeedRestriction speedRestriction;

  proc::ProcedureLegType type = INVALID_LEG_TYPE;
  proc::MapProcedureTypes mapType = PROCEDURE_NONE; /* Any of the PROCEDURE_* types*/

  int approachId = -1, transitionId = -1, legId = -1, navId = -1, recNavId = -1;

  float course,
        distance /* Distance from source in nm */,
        calculatedDistance /* Calculated distance closer to the real one in nm */,
        calculatedTrueCourse /* Calculated distance closer to the real one */,
        time /* Only for holds in minutes */,
        theta /* magnetic course to recommended navaid */,
        rho /* distance to recommended navaid */,
        magvar /* from navaid or airport */;

  bool missed, flyover, trueCourse,
       intercept, /* Leg was modfied by a previous intercept */
       disabled /* Neither line nor fix should be painted - currently for IF legs after a CI or similar */;

  bool isValid() const
  {
    return type != INVALID_LEG_TYPE;
  }

  /* Draw red if there is an error in the leg (navaid could not be resolved */
  bool hasInvalidRef() const;

  /* true if leg is unusable because a required navaid could not be resolved */
  bool hasErrorRef() const;

  float legTrueCourse() const;

  bool isApproach() const
  {
    return mapType & proc::PROCEDURE_APPROACH;
  }

  bool isArrival() const
  {
    return mapType & proc::PROCEDURE_ARRIVAL;
  }

  bool isAnyArrival() const
  {
    return mapType & proc::PROCEDURE_ARRIVAL_ALL;
  }

  bool isAnyDeparture() const
  {
    return mapType & proc::PROCEDURE_DEPARTURE;
  }

  bool isTransition() const
  {
    return mapType & proc::PROCEDURE_TRANSITION;
  }

  bool isSid() const
  {
    return mapType & proc::PROCEDURE_SID;
  }

  bool isSidTransition() const
  {
    return mapType & proc::PROCEDURE_SID_TRANSITION;
  }

  bool isAnyStar() const
  {
    return mapType & proc::PROCEDURE_STAR_ALL;
  }

  bool isStar() const
  {
    return mapType & proc::PROCEDURE_STAR;
  }

  bool isStarTransition() const
  {
    return mapType & proc::PROCEDURE_STAR_TRANSITION;
  }

  bool isMissed() const
  {
    return mapType & proc::PROCEDURE_MISSED;
  }

  bool isHold() const;
  bool isCircular() const;

  /* Do not display distance e.g. for course to altitude */
  bool noDistanceDisplay() const;

  /* No course display for e.g. arc legs */
  bool noCourseDisplay() const;

};

QDebug operator<<(QDebug out, const proc::MapProcedureLeg& leg);

/* True if e.g. "RW10B" for a SID or STAR which means that 10L, 10C and 10R can be used. */
bool hasSidStarParallelRunways(const QString& approachArincName);

/* True if "ALL" for a SID or STAR. Means SID/STAR can be used for all runways of an airport. */
bool hasSidStarAllRunways(const QString& approachArincName);

/* All legs for a arrival or departure including STAR, transition and approach in order or
 * legs for SID only.
 * SID contains all in approach and legs transition fields.
 * STAR contins all in approach fields */
struct MapProcedureLegs
{
  QVector<MapProcedureLeg> transitionLegs, approachLegs;

  MapProcedureRef ref;
  atools::geo::Rect bounding;

  QString approachType, approachSuffix, approachFixIdent, approachArincName, transitionType, transitionFixIdent,
          procedureRunway; /* Runway from the procedure does not have to match the airport runway but is saved */

  /* Only for approaches - the found runway end at the airport - can be different due to fuzzy search */
  map::MapRunwayEnd runwayEnd;
  proc::MapProcedureTypes mapType = PROCEDURE_NONE;

  float approachDistance = 0.f,
        transitionDistance = 0.f,
        missedDistance = 0.f;

  bool gpsOverlay, hasError /* Unusable due to missing navaid */;

  void clearApproach();

  void clearTransition();

  bool isEmpty() const
  {
    return size() == 0;
  }

  int size() const
  {
    return transitionLegs.size() + approachLegs.size();
  }

  /* If it has arinc names like "ALL" or "RW10B" */
  bool hasSidOrStarMultipleRunways() const
  {
    return hasSidOrStarAllRunways() || hasSidOrStarParallelRunways();
  }

  bool hasSidOrStarParallelRunways() const
  {
    return proc::hasSidStarParallelRunways(approachArincName);
  }

  bool hasSidOrStarAllRunways() const
  {
    return proc::hasSidStarAllRunways(approachArincName);
  }

  /* first in list is transition and then approach  or STAR only.
   *  Order is reversed for departure - first approach and then transition */
  const MapProcedureLeg& at(int i) const
  {
    return atInternalConst(i);
  }

  const MapProcedureLeg& first() const
  {
    return atInternalConst(0);
  }

  const MapProcedureLeg& last() const
  {
    return atInternalConst(size() - 1);
  }

  const MapProcedureLeg *approachLegById(int legId) const;
  const MapProcedureLeg *transitionLegById(int legId) const;

  MapProcedureLeg& operator[](int i)
  {
    return atInternal(i);
  }

private:
  MapProcedureLeg& atInternal(int i);
  const MapProcedureLeg& atInternalConst(int i) const;
  int apprIdx(int i) const;
  int transIdx(int i) const;

  bool isDeparture() const
  {
    return mapType & proc::PROCEDURE_DEPARTURE;
  }

};

QDebug operator<<(QDebug out, const MapProcedureLegs& legs);

QString procedureTypeText(proc::MapProcedureTypes mapType);
QString procedureTypeText(const proc::MapProcedureLeg& leg);
QString procedureFixType(const QString& type);
QString procedureType(const QString& type);
proc::ProcedureLegType procedureLegEnum(const QString& type);
QString procedureLegTypeStr(ProcedureLegType type);
QString procedureLegTypeShortStr(ProcedureLegType type);
QString procedureLegTypeFullStr(ProcedureLegType type);
QString procedureLegRemarks(proc::ProcedureLegType);
QString altRestrictionText(const MapAltRestriction& restriction);

QString procedureLegRemark(const MapProcedureLeg& leg);
QString procedureLegRemDistance(const MapProcedureLeg& leg, float& remainingDistance);
QString procedureLegDistance(const MapProcedureLeg& leg);
QString procedureLegCourse(const MapProcedureLeg& leg);

proc::MapProcedureTypes procedureType(bool hasSidStar, const QString& type, const QString& suffix, bool gpsOverlay);

/* Put altitude restriction texts into list */
QString altRestrictionTextNarrow(const MapAltRestriction& altRestriction);
QString altRestrictionTextShort(const proc::MapAltRestriction& altRestriction);

QString speedRestrictionTextNarrow(const proc::MapSpeedRestriction& speedRestriction);
QString speedRestrictionText(const proc::MapSpeedRestriction& speedRestriction);
QString speedRestrictionTextShort(const proc::MapSpeedRestriction& speedRestriction);

} // namespace types

Q_DECLARE_TYPEINFO(proc::MapProcedureRef, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(proc::MapProcedureLeg, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(proc::MapAltRestriction, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(proc::MapProcedureLegs, Q_MOVABLE_TYPE);

#endif // LITTLENAVMAP_PROCTYPES_H
