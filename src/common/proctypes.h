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

#ifndef LITTLENAVMAP_PROCTYPES_H
#define LITTLENAVMAP_PROCTYPES_H

#include "common/mapresult.h"
#include "common/procflags.h"

#include <QColor>
#include <QRegularExpression>
#include <QString>

class Route;

/*
 * Procedure types for approach, transition, SID and STAR.
 */
namespace proc {

/* Initialize all text that are translateable after loading the translation files */
void initTranslateableTexts();

// =====================================================================
/* Altitude restriction for approaches or transitions */
struct MapAltRestriction
{
  enum Descriptor
  {
    NONE,
    AT,
    AT_OR_ABOVE,
    AT_OR_BELOW,
    BETWEEN, /* At or above alt2 and at or below alt1 */
    ILS_AT, /* ILS glideslope altitude at alt1 */
    ILS_AT_OR_ABOVE /* ILS glideslope altitude at alt1*/
  };

  Descriptor descriptor = NONE;
  float alt1, alt2,
        verticalAngleAlt = map::INVALID_ALTITUDE_VALUE; /* Forced since calculated from vertical angle */

  /* Indicator used to force lowest altitude on final FAF and FACF to avoid arriving above glide slope or VASI */
  bool forceFinal = false;

  bool isValid() const
  {
    return descriptor != NONE;
  }

  bool isIls() const
  {
    return descriptor == ILS_AT || descriptor == ILS_AT_OR_ABOVE;
  }

};

// =====================================================================
/* Procedure speed restriction */
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

// =====================================================================
/* Reference object containing all ids for procedures, transitions and legs
 * Hashable and compareable */
struct MapProcedureRef
{
  MapProcedureRef(int airportIdParam, int runwayEndIdParam, int approachIdParam, int transitionIdParam, int legIdParam,
                  proc::MapProcedureTypes type)
    : airportId(airportIdParam), runwayEndId(runwayEndIdParam), approachId(approachIdParam), transitionId(transitionIdParam),
    legId(legIdParam), mapType(type)
  {
  }

  MapProcedureRef()
    : airportId(-1), runwayEndId(-1), approachId(-1), transitionId(-1), legId(-1), mapType(PROCEDURE_NONE)
  {
  }

  bool operator==(const proc::MapProcedureRef& other) const
  {
    return airportId == other.airportId && approachId == other.approachId && runwayEndId == other.runwayEndId &&
           transitionId == other.transitionId && legId == other.legId && mapType == other.mapType;
  }

  bool operator!=(const proc::MapProcedureRef& other) const
  {
    return !operator==(other);
  }

  int airportId /* always from navdatabase - only simdatabase if this is a custom approach */,
      runwayEndId /* always from navdatabase - only simdatabase if this is a custom approach */,
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

QDebug operator<<(QDebug out, const proc::MapProcedureRef& ref);

inline uint qHash(const proc::MapProcedureRef& ref)
{
  return static_cast<unsigned int>(ref.airportId) ^ static_cast<unsigned int>(ref.approachId) ^ static_cast<unsigned int>(ref.runwayEndId) ^
         static_cast<unsigned int>(ref.transitionId) ^ static_cast<unsigned int>(ref.legId) ^ qHash(ref.mapType);
}

// =====================================================================
/* Procedure leg */
struct MapProcedureLeg
{
  QString fixType, fixIdent, fixRegion,
          recFixType, recFixIdent, recFixRegion, /* Recommended fix also used by rho and theta */
          turnDirection, /* Turn to this fix*/
          arincDescrCode /* 5.17 */;

  QStringList displayText /* Fix label for map - filled in approach query */,
              remarks /* Additional remarks for tree - filled in approach query */;
  atools::geo::Pos fixPos, recFixPos,
                   interceptPos, /* Position of an intercept leg for grey circle */
                   procedureTurnPos /* Extended position of a procedure turn */;
  atools::geo::Line line, /* Line with flying direction from pos1 to pos2 */
                    holdLine; /* Helping line to find out if aircraft leaves the hold */

  atools::geo::LineString geometry; /* Same as line or geometry approximation for intercept or arcs for distance to leg calculation */

  /* Navaids resolved by approach query class */
  map::MapResult navaids, recNavaids;

  MapAltRestriction altRestriction;
  MapSpeedRestriction speedRestriction;

  proc::ProcedureLegType type = INVALID_LEG_TYPE; /* Type of this leg */
  proc::MapProcedureTypes mapType = PROCEDURE_NONE; /* Type of the procedure this leg belongs to */

  int airportId = -1, approachId = -1, transitionId = -1, legId = -1;

  float course, /* magnetic from ARINC */
        distance /* Distance from source in NM */,
        calculatedDistance /* Calculated distance closer to the real one in NM */,
        calculatedTrueCourse /* Calculated distance closer to the real one - great circle line */,
        time /* Only for holds in minutes */,
        theta /* magnetic course to recommended navaid or INVALID_COURSE_VALUE if not available */,
        rho /* distance to recommended navaid in NM or INVALID_DISTANCE_VALUE if not available */,
        magvar /* from navaid or airport */,
        verticalAngle = map::INVALID_ANGLE_VALUE /* degrees or INVALID_ANGLE_VALUE if not available */;

  bool missed = false, flyover = false, trueCourse = false,
       intercept = false, /* Leg was modified by a previous intercept */
       disabled = false, /* Neither line nor fix should be painted - currently for IF legs after a CI or similar */

       correctedArc = false, /* Fix of previous leg does not match arc distance. Therefore, p1 is corrected for distance.
                              * P1 is entry, intercept is start of arc and P2 is end of arc.
                              * Geometry contains entry stub. */

       malteseCross = false; /* Draw Maltese cross for either FAF or FACF depending on ILS altitude restriction */

  bool isValid() const
  {
    return type != INVALID_LEG_TYPE;
  }

  /* true if leg is probably unusable because a required navaid could not be resolved */
  bool hasErrorRef() const;

  /* true if leg is totally unusable because a required navaid could not be resolved and it
   * contains no valid coordinates at all */
  bool hasHardErrorRef() const;

  float legTrueCourse() const;

  bool isAnyApproach() const
  {
    return isApproach() || isTransition() || isMissed();
  }

  bool isApproach() const
  {
    return mapType & proc::PROCEDURE_APPROACH;
  }

  bool isArrival() const
  {
    return mapType & proc::PROCEDURE_APPROACH_ALL_MISSED;
  }

  bool isAnyArrival() const
  {
    return mapType & proc::PROCEDURE_ARRIVAL_ALL;
  }

  bool isAnyDeparture() const
  {
    return mapType & proc::PROCEDURE_DEPARTURE;
  }

  bool isAnyTransition() const
  {
    return isTransition() || isSidTransition() || isStarTransition();
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

  bool isCircleToLand() const
  {
    return type == proc::CIRCLE_TO_LAND;
  }

  bool isStraightIn() const
  {
    return type == proc::STRAIGHT_IN;
  }

  bool isVectors() const
  {
    return type == proc::VECTORS;
  }

  bool isManual() const
  {
    return type == HEADING_TO_MANUAL_TERMINATION || type == FROM_FIX_TO_MANUAL_TERMINATION;
  }

  bool isAnyCustom() const
  {
    return isCustomDeparture() || isCustomApproach();
  }

  bool isCustomDeparture() const
  {
    return type == CUSTOM_DEP_END || type == CUSTOM_DEP_RUNWAY;

  }

  bool isCustomApproach() const
  {
    return type == CUSTOM_APP_START || type == CUSTOM_APP_RUNWAY;

  }

  bool isFinalApproachFix() const;
  bool isFinalApproachCourseFix() const;
  bool isFinalEndpointFix() const;

  bool isHold() const;

  bool isProcedureTurn() const
  {
    return type == proc::PROCEDURE_TURN;
  }

  bool isCircular() const;

  bool isInitialFix() const
  {
    return type == proc::INITIAL_FIX || type == proc::CUSTOM_APP_START;
  }

  /* Do not display distance e.g. for course to altitude */
  bool noDistanceDisplay() const;

  /* No course display for e.g. arc legs */
  bool noCourseDisplay() const;

  /* No ident at end of manual legs */
  bool noIdentDisplay() const;

};

QDebug operator<<(QDebug out, const proc::MapProcedureLeg& leg);

// =====================================================================
/* All legs for a arrival or departure including SID, STAR, transition and approach.
 * Approach (also partially synonym for procedure). Flying order in transitionLegs and then approachLegs.
 * SID contains all legs in approach and transition fields. Flying order in approachLegs and then transitionLegs.
 * STAR contains all legs in approach and transition fields. Flying order in transitionLegs and then approachLegs */
struct MapProcedureLegs
{
  QVector<MapProcedureLeg> transitionLegs, approachLegs;

  /* Reference with all database ids */
  MapProcedureRef ref;
  atools::geo::Rect bounding;

  QString approachType, /* GNSS (display GLS) GPS IGS ILS LDA LOC LOCB NDB NDBDME RNAV (RNV) SDF TCN VOR VORDME */
          approachSuffix, approachFixIdent /* Approach fix or SID/STAR name */,
          approachArincName, transitionType, transitionFixIdent,
          procedureRunway; /* Runway from the procedure does not have to match the airport runway but is saved */

  /* Only for approaches - the found runway end at the airport - can be different due to fuzzy search */
  map::MapRunwayEnd runwayEnd;
  proc::MapProcedureTypes mapType = PROCEDURE_NONE;

  /* Accumulated distances */
  float approachDistance = 0.f, transitionDistance = 0.f, missedDistance = 0.f;

  /* Assigned color for multi preview or default color for normal preview. */
  QColor previewColor;

  /* Parameters only for custom approaches.
   * Altitude in feet above airport elevation and distance to runway threshold in NM */
  float customAltitude = 0.f, customDistance = 0.f, customOffset = 0.f;

  bool gpsOverlay,
       hasError, /* Probably unusable due to missing navaid */
       hasHardError, /* Deny usage since geometry is not valid */
       circleToLand; /* Runway is not part of procedure and was added internally */

  /* Short display type name */
  QString displayApproachType() const
  {
    // Correct wrong designation of GLS approaches as GNSS for display
    return approachType == "GNSS" ? "GLS" : approachType;
  }

  static QString displayApproachType(const QString& type)
  {
    // Correct wrong designation of GLS approaches as GNSS for display
    return type == "GNSS" ? "GLS" : type;
  }

  /* Anything that needs to display an ILS frequency or GNSS channel */
  bool hasFrequencyOrChannel() const
  {
    return hasFrequencyOrChannel(approachType);
  }

  static bool hasFrequencyOrChannel(const QString& approachType)
  {
    // All: RNAV VORDME GPS ILS VOR NDB LOC NDBDME TCN LOCB GNSS LDA SDF IGS
    return hasFrequency(approachType) || hasChannel(approachType);
  }

  /* All ILS, LOC, LDA, etc. */
  bool hasFrequency() const
  {
    return hasFrequency(approachType);
  }

  /* RNP and GLS. */
  bool hasChannel() const
  {
    return hasChannel(approachType);
  }

  static bool hasFrequency(const QString& approachType)
  {
    return approachType == "ILS" || approachType == "LOC" || approachType == "LOCB" || approachType == "LDA" ||
           approachType == "IGS" || approachType == "SDF";
  }

  static bool hasChannel(const QString& approachType)
  {
    return approachType == "GNSS" || approachType == "GLS";
  }

  bool isAnyCustom() const
  {
    return isCustomApproach() || isCustomDeparture();
  }

  bool isCustomApproach() const
  {
    return approachType == "CUSTOM";
  }

  bool isCustomDeparture() const
  {
    return approachType == "CUSTOMDEPART";
  }

  bool isRnavGps() const
  {
    return approachType == "RNAV" || approachType == "GPS";
  }

  bool isPrecision() const
  {
    return approachType == "ILS" || approachType == "GNSS" /* GLS */;
  }

  bool isGls() const
  {
    return approachType == "GNSS" /* GLS */;
  }

  bool isIls() const
  {
    return approachType == "ILS";
  }

  bool isNonPrecision() const
  {
    return !isPrecision();
  }

  void clearApproach();

  void clearTransition();

  bool isEmpty() const
  {
    return size() == 0;
  }

  /* Get index number for given leg. Index related for all legs (approach and transition) */
  int indexForLeg(const MapProcedureLeg& leg) const;

  int size() const
  {
    return transitionLegs.size() + approachLegs.size();
  }

  /* If it has arinc names like "ALL" or "RW10B" */
  bool hasSidOrStarMultipleRunways() const
  {
    return hasSidOrStarAllRunways() || hasSidOrStarParallelRunways();
  }

  /* If it has arinc names like ""RW10B" */
  bool hasSidOrStarParallelRunways() const;

  /* If it has arinc names like "ALL" */
  bool hasSidOrStarAllRunways() const;

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

  /* Positions of all legs not including missed approach */
  atools::geo::LineString buildGeometry() const;

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

// =====================================================================
/* Functions */

/* Text describing procedure name and type. procType of related leg determines if
 * certain types like missed or transition are shown */
QString procedureLegsText(const proc::MapProcedureLegs& legs, proc::MapProcedureTypes procType, bool narrow, bool includeRunway,
                          bool missedAsApproach);
QString procedureTypeText(proc::MapProcedureTypes mapType);
QString procedureTypeText(const proc::MapProcedureLeg& leg);

/* Finds first and last fix name. Can be waypoint, VOR, NDB, airport or runway but not special like manual */
QStringList procedureTextFirstAndLastFix(const proc::MapProcedureLegs& legs, proc::MapProcedureTypes mapType);

/* VOR, NDB, etc. */
QString procedureFixType(const QString& type);

/* Ident name and FAF, MAP, IAF */
QString procedureLegFixStr(const proc::MapProcedureLeg& leg);

QString procedureType(const QString& type);
proc::ProcedureLegType procedureLegEnum(const QString& type);
QString procedureLegTypeStr(ProcedureLegType type);
QString procedureLegTypeShortStr(ProcedureLegType type);
QString procedureLegTypeFullStr(ProcedureLegType type);
QString procedureLegRemarks(proc::ProcedureLegType);
QString altRestrictionText(const MapAltRestriction& restriction);

/* Slash separated list of all restrictions, altitude, speed and angle */
QStringList restrictionText(const MapProcedureLeg& procedureLeg);

/* true if leg has fix at the start */
bool procedureLegFixAtStart(proc::ProcedureLegType type);

/* true if leg has fix at the end */
bool procedureLegFixAtEnd(proc::ProcedureLegType type);

/* true if the ident of the leg and a navaid symbol can be drawn - otherwise no ident and generic proc symbol */
bool procedureLegDrawIdent(ProcedureLegType type);

/* true if flying from waypoint and a "from" indication should be displayed */
bool procedureLegFrom(proc::ProcedureLegType type);

/* IAF, FAF, MAP */
QString proceduresLegSecialTypeShortStr(proc::LegSpecialType type);
QString proceduresLegSecialTypeLongStr(proc::LegSpecialType type);

/* Get special leg type from ARINC description code */
proc::LegSpecialType specialType(const QString& arincDescrCode);

/* Ident, frequency, distance and bearing of recommended/related */
QStringList procedureLegRecommended(const MapProcedureLeg& leg);

/* Fly over, turn direction and error messages */
QStringList procedureLegRemark(const MapProcedureLeg& leg);

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

/* Determine various route and procedure related states for the given map object.
 * Queries are omitted if the respective parameters are null */
void procedureFlags(const Route& route, const map::MapBase *base, bool *departure = nullptr,
                    bool *destination = nullptr,
                    bool *alternate = nullptr, bool *roundtrip = nullptr, bool *arrivalProc = nullptr,
                    bool *departureProc = nullptr);

/* Check if airport can be added as departure, destination or alternate and gives
 * information if menu items should be disabled.
 * Returns suffix string for menu items. */
QString  procedureTextSuffixDeparture(const Route& route, const map::MapAirport& airport, bool& disable);
QString  procedureTextSuffixDestination(const Route& route, const map::MapAirport& airport, bool& disable);
QString  procedureTextSuffixAlternate(const Route& route, const map::MapAirport& airport, bool& disable);

} // namespace types

Q_DECLARE_TYPEINFO(proc::MapProcedureRef, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(proc::MapProcedureLeg, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(proc::MapAltRestriction, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(proc::MapProcedureLegs, Q_MOVABLE_TYPE);

#endif // LITTLENAVMAP_PROCTYPES_H
