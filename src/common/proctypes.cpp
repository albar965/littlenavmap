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

#include "common/proctypes.h"

#include "atools.h"
#include "common/unit.h"
#include "fs/util/fsutil.h"
#include "geo/calculations.h"
#include "app/navapp.h"
#include "query/mapquery.h"
#include "route/route.h"

#include <QDataStream>
#include <QStringBuilder>
#include <QHash>
#include <QObject>

namespace proc  {

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
using Qt::endl;
#endif

static QHash<QString, QString> approachFixTypeToStr;
static QHash<QString, QString> approachTypeToStr;
static QHash<ProcedureLegType, QString> approachLegTypeToStr;
static QHash<ProcedureLegType, QString> approachLegRemarkStr;

static QHash<LegSpecialType, QString> specialTypeShortStr;
static QHash<LegSpecialType, QString> specialTypeLongStr;

void initTranslateableTexts()
{
  specialTypeShortStr = QHash<LegSpecialType, QString>(
    {
      {proc::IAF, QObject::tr("IAF")},
      {proc::FAF, QObject::tr("FAF")},
      {proc::FACF, QObject::tr("FACF")},
      {proc::MAP, QObject::tr("MAP")},
      {proc::FEP, QObject::tr("FEP")},
      {proc::NONE, QString()}
    });

  specialTypeLongStr = QHash<LegSpecialType, QString>(
    {
      {proc::IAF, QObject::tr("Initial Approach Fix")},
      {proc::FAF, QObject::tr("Final Approach Fix")},
      {proc::FACF, QObject::tr("Final Approach Course Fix")},
      {proc::MAP, QObject::tr("Missed Approach Point")},
      {proc::FEP, QObject::tr("Final Endpoint Fix")},
      {proc::NONE, QString()}
    });

  approachFixTypeToStr = QHash<QString, QString>(
    {
      {"NONE", QObject::tr("NONE")},
      {"L", QObject::tr("Localizer")},
      {"V", QObject::tr("VOR")},
      {"N", QObject::tr("NDB")},
      {"TN", QObject::tr("Terminal NDB")},
      {"W", QObject::tr("Waypoint")},
      {"TW", QObject::tr("Terminal Waypoint")},
      {"R", QObject::tr("Runway")},
      {"CST", QObject::tr("Custom Fix")}
    });

  approachTypeToStr = QHash<QString, QString>(
    {
      {"GPS", QObject::tr("GPS")},
      {"VOR", QObject::tr("VOR")},
      {"NDB", QObject::tr("NDB")},
      {"ILS", QObject::tr("ILS")},
      {"LOC", QObject::tr("Localizer")},
      {"SDF", QObject::tr("SDF")},
      {"LDA", QObject::tr("LDA")},
      {"VORDME", QObject::tr("VORDME")},
      {"NDBDME", QObject::tr("NDBDME")},
      {"RNAV", QObject::tr("RNAV")},
      {"LOCB", QObject::tr("Localizer Backcourse")},

      // Additional types from X-Plane
      {"FMS", QObject::tr("FMS")},
      {"IGS", QObject::tr("IGS")},
      {"GNSS", QObject::tr("GLS")},
      {"TCN", QObject::tr("TACAN")},
      {"CTL", QObject::tr("Circle to Land")},
      {"MLS", QObject::tr("MLS")},

      /* User defined approach procedure */
      {atools::fs::pln::APPROACH_TYPE_CUSTOM, QObject::tr("Approach")},
      {atools::fs::pln::SID_TYPE_CUSTOM, QObject::tr("Departure")}
    });

  approachLegTypeToStr = QHash<ProcedureLegType, QString>(
    {
      {ARC_TO_FIX, QObject::tr("Arc to fix")},
      {COURSE_TO_ALTITUDE, QObject::tr("Course to altitude")},
      {COURSE_TO_DME_DISTANCE, QObject::tr("Course to DME distance")},
      {COURSE_TO_FIX, QObject::tr("Course to fix")},
      {COURSE_TO_INTERCEPT, QObject::tr("Course to intercept")},
      {COURSE_TO_RADIAL_TERMINATION, QObject::tr("Course to radial termination")},
      {DIRECT_TO_FIX, QObject::tr("Direct to fix")},
      {FIX_TO_ALTITUDE, QObject::tr("Fix to altitude")},
      {TRACK_FROM_FIX_FROM_DISTANCE, QObject::tr("Track from fix from distance")},
      {TRACK_FROM_FIX_TO_DME_DISTANCE, QObject::tr("Track from fix to DME distance")},
      {FROM_FIX_TO_MANUAL_TERMINATION, QObject::tr("From fix to manual termination")},
      {HOLD_TO_ALTITUDE, QObject::tr("Hold to altitude")},
      {HOLD_TO_FIX, QObject::tr("Hold to fix")},
      {HOLD_TO_MANUAL_TERMINATION, QObject::tr("Hold to manual termination")},
      {INITIAL_FIX, QObject::tr("Initial fix")},
      {PROCEDURE_TURN, QObject::tr("Procedure turn")},
      {CONSTANT_RADIUS_ARC, QObject::tr("Constant radius arc")},
      {TRACK_TO_FIX, QObject::tr("Track to fix")},
      {HEADING_TO_ALTITUDE_TERMINATION, QObject::tr("Heading to altitude termination")},
      {HEADING_TO_DME_DISTANCE_TERMINATION, QObject::tr("Heading to DME distance termination")},
      {HEADING_TO_INTERCEPT, QObject::tr("Heading to intercept")},
      {HEADING_TO_MANUAL_TERMINATION, QObject::tr("Heading to manual termination")},
      {HEADING_TO_RADIAL_TERMINATION, QObject::tr("Heading to radial termination")},

      {DIRECT_TO_RUNWAY, QObject::tr("Proceed to runway")},
      {CIRCLE_TO_LAND, QObject::tr("Circle to land")},
      {STRAIGHT_IN, QObject::tr("Straight in")},
      {START_OF_PROCEDURE, QObject::tr("Start of procedure")},
      {VECTORS, QObject::tr("Vectors")},

      {CUSTOM_APP_START, QObject::tr("Start of final")},
      {CUSTOM_APP_RUNWAY, QObject::tr("Final leg")},

      {CUSTOM_DEP_END, QObject::tr("Departure leg")},
      {CUSTOM_DEP_RUNWAY, QObject::tr("Proceed to runway")},
    });

  approachLegRemarkStr = QHash<ProcedureLegType, QString>(
    {
      {ARC_TO_FIX, QString()},
      {COURSE_TO_ALTITUDE, QString()},
      {COURSE_TO_DME_DISTANCE, QString()},
      {COURSE_TO_FIX, QString()},
      {COURSE_TO_INTERCEPT, QString()},
      {COURSE_TO_RADIAL_TERMINATION, QString()},
      {DIRECT_TO_FIX, QString()},
      {FIX_TO_ALTITUDE, QString()},
      {TRACK_FROM_FIX_FROM_DISTANCE, QString()},
      {TRACK_FROM_FIX_TO_DME_DISTANCE, QString()},
      {FROM_FIX_TO_MANUAL_TERMINATION, QString()},
      {HOLD_TO_ALTITUDE, QObject::tr("Mandatory hold")},
      {HOLD_TO_FIX, QObject::tr("Single circuit")},
      {HOLD_TO_MANUAL_TERMINATION, QObject::tr("Mandatory hold")},
      {INITIAL_FIX, QString()},
      {PROCEDURE_TURN, QString()},
      {CONSTANT_RADIUS_ARC, QString()},
      {TRACK_TO_FIX, QString()},
      {HEADING_TO_ALTITUDE_TERMINATION, QString()},
      {HEADING_TO_DME_DISTANCE_TERMINATION, QString()},
      {HEADING_TO_INTERCEPT, QString()},
      {HEADING_TO_MANUAL_TERMINATION, QString()},
      {HEADING_TO_RADIAL_TERMINATION, QString()},

      {DIRECT_TO_RUNWAY, QString()},
      {CIRCLE_TO_LAND, QString()},
      {STRAIGHT_IN, QString()},
      {START_OF_PROCEDURE, QString()},
      {VECTORS, QString()},

      {CUSTOM_APP_START, QString()},
      {CUSTOM_APP_RUNWAY, QString()},

      {CUSTOM_DEP_END, QString()},
      {CUSTOM_DEP_RUNWAY, QString()},
    });
}

const static QHash<QString, ProcedureLegType> approachLegTypeToEnum(
  {
    {"AF", ARC_TO_FIX},
    {"CA", COURSE_TO_ALTITUDE},
    {"CD", COURSE_TO_DME_DISTANCE},
    {"CF", COURSE_TO_FIX},
    {"CI", COURSE_TO_INTERCEPT},
    {"CR", COURSE_TO_RADIAL_TERMINATION},
    {"DF", DIRECT_TO_FIX},
    {"FA", FIX_TO_ALTITUDE},
    {"FC", TRACK_FROM_FIX_FROM_DISTANCE},
    {"FD", TRACK_FROM_FIX_TO_DME_DISTANCE},
    {"FM", FROM_FIX_TO_MANUAL_TERMINATION},
    {"HA", HOLD_TO_ALTITUDE},
    {"HF", HOLD_TO_FIX},
    {"HM", HOLD_TO_MANUAL_TERMINATION},
    {"IF", INITIAL_FIX},
    {"PI", PROCEDURE_TURN},
    {"RF", CONSTANT_RADIUS_ARC},
    {"TF", TRACK_TO_FIX},
    {"VA", HEADING_TO_ALTITUDE_TERMINATION},
    {"VD", HEADING_TO_DME_DISTANCE_TERMINATION},
    {"VI", HEADING_TO_INTERCEPT},
    {"VM", HEADING_TO_MANUAL_TERMINATION},
    {"VR", HEADING_TO_RADIAL_TERMINATION},

    {"RX", DIRECT_TO_RUNWAY},
    {"CX", CIRCLE_TO_LAND},
    {"TX", STRAIGHT_IN},
    {"SX", START_OF_PROCEDURE},
    {"VX", VECTORS},

    {"CFX", CUSTOM_APP_START},
    {"CRX", CUSTOM_APP_RUNWAY},

    {"CDX", CUSTOM_DEP_END},
    {"CDR", CUSTOM_DEP_RUNWAY}
  });

const static QHash<ProcedureLegType, QString> approachLegTypeToShortStr(
  {
    {ARC_TO_FIX, "AF"},
    {COURSE_TO_ALTITUDE, "CA"},
    {COURSE_TO_DME_DISTANCE, "CD"},
    {COURSE_TO_FIX, "CF"},
    {COURSE_TO_INTERCEPT, "CI"},
    {COURSE_TO_RADIAL_TERMINATION, "CR"},
    {DIRECT_TO_FIX, "DF"},
    {FIX_TO_ALTITUDE, "FA"},
    {TRACK_FROM_FIX_FROM_DISTANCE, "FC"},
    {TRACK_FROM_FIX_TO_DME_DISTANCE, "FD"},
    {FROM_FIX_TO_MANUAL_TERMINATION, "FM"},
    {HOLD_TO_ALTITUDE, "HA"},
    {HOLD_TO_FIX, "HF"},
    {HOLD_TO_MANUAL_TERMINATION, "HM"},
    {INITIAL_FIX, "IF"},
    {PROCEDURE_TURN, "PI"},
    {CONSTANT_RADIUS_ARC, "RF"},
    {TRACK_TO_FIX, "TF"},
    {HEADING_TO_ALTITUDE_TERMINATION, "VA"},
    {HEADING_TO_DME_DISTANCE_TERMINATION, "VD"},
    {HEADING_TO_INTERCEPT, "VI"},
    {HEADING_TO_MANUAL_TERMINATION, "VM"},
    {HEADING_TO_RADIAL_TERMINATION, "VR"},

    {DIRECT_TO_RUNWAY, "RX"},
    {CIRCLE_TO_LAND, "CX"},
    {STRAIGHT_IN, "TX"},
    {START_OF_PROCEDURE, "SX"},
    {VECTORS, "VX"},

    {CUSTOM_APP_START, "CFX"},
    {CUSTOM_APP_RUNWAY, "CRX"},

    {CUSTOM_DEP_END, "CDX"},
    {CUSTOM_DEP_RUNWAY, "CDR"}
  });

QString procedureFixType(const QString& type)
{
  return approachFixTypeToStr.value(type, type);
}

QString procedureLegFixStr(const MapProcedureLeg& leg)
{
  QString fix;

  if(atools::contains(leg.type, {proc::TRACK_FROM_FIX_TO_DME_DISTANCE, proc::HEADING_TO_DME_DISTANCE_TERMINATION,
                                 proc::COURSE_TO_DME_DISTANCE}))
    // DME distance always uses recommended fix (VOR, etc.)
    fix = QObject::tr("%1+%2").arg(leg.recFixIdent).arg(atools::roundToInt(leg.distance));
  else if(atools::contains(leg.type, {proc::TRACK_FROM_FIX_FROM_DISTANCE}))
    // Track from fix uses fix ident
    fix = QObject::tr("%1+%2").arg(leg.fixIdent).arg(atools::roundToInt(leg.calculatedDistance));
  else
    fix = leg.fixIdent;

  QString specialType(proc::proceduresLegSecialTypeShortStr(proc::specialType(leg.arincDescrCode)));
  if(!specialType.isEmpty())
    return QObject::tr("%1 (%2)").arg(fix).arg(specialType);
  else
    return fix;
}

QString procedureType(const QString& type)
{
  return approachTypeToStr.value(type, type);
}

QString edgeLights(const QString& type)
{
  if(type == "L")
    return QObject::tr("Low");
  else if(type == "M")
    return QObject::tr("Medium");
  else if(type == "H")
    return QObject::tr("High");
  else
    return QString();
}

QString patternDirection(const QString& type)
{
  if(type == "L")
    return QObject::tr("Left");
  else if(type == "R")
    return QObject::tr("Right");
  else
    return QString();
}

proc::ProcedureLegType procedureLegEnum(const QString& type)
{
  return approachLegTypeToEnum.value(type);
}

QString proceduresLegSecialTypeShortStr(proc::LegSpecialType type)
{
  return specialTypeShortStr.value(type);
}

QString proceduresLegSecialTypeLongStr(proc::LegSpecialType type)
{
  return specialTypeLongStr.value(type);
}

QString procedureLegTypeStr(proc::ProcedureLegType type)
{
  return approachLegTypeToStr.value(type);
}

QString procedureLegTypeShortStr(ProcedureLegType type)
{
  return approachLegTypeToShortStr.value(type);
}

QString procedureLegTypeFullStr(ProcedureLegType type)
{
  return QObject::tr("%1 (%2)").arg(approachLegTypeToStr.value(type)).arg(approachLegTypeToShortStr.value(type));
}

QString procedureLegRemarks(proc::ProcedureLegType type)
{
  return approachLegRemarkStr.value(type);
}

QStringList restrictionText(const MapProcedureLeg& procedureLeg)
{
  QStringList restrictions;
  if(procedureLeg.altRestriction.isValid() && !procedureLeg.isCustomDeparture())
    restrictions.append(proc::altRestrictionTextShort(procedureLeg.altRestriction));

  if(procedureLeg.speedRestriction.isValid())
    restrictions.append(proc::speedRestrictionTextShort(procedureLeg.speedRestriction));

  if(procedureLeg.isVerticalAngleValid())
    restrictions.append(QObject::tr("%L1°").arg(procedureLeg.verticalAngle, 0, 'g', 3));

  return restrictions;
}

QString altRestrictionText(const MapAltRestriction& restriction)
{
  if(restriction.verticalAngleAlt < map::INVALID_ALTITUDE_VALUE)
    return QObject::tr("At %1 (vertical path)").arg(Unit::altFeet(restriction.verticalAngleAlt));
  else
  {
    switch(restriction.descriptor)
    {
      case proc::MapAltRestriction::ILS_AT:
      case proc::MapAltRestriction::ILS_AT_OR_ABOVE:
        return QObject::tr("ILS GS %1").arg(Unit::altFeet(restriction.alt1));

      case proc::MapAltRestriction::NO_ALT_RESTR:
        return QString();

      case proc::MapAltRestriction::AT:
        return QObject::tr("At %1").arg(Unit::altFeet(restriction.alt1));

      case proc::MapAltRestriction::AT_OR_ABOVE:
        return QObject::tr("At or above %1").arg(Unit::altFeet(restriction.alt1));

      case proc::MapAltRestriction::AT_OR_BELOW:
        return QObject::tr("At or below %1").arg(Unit::altFeet(restriction.alt1));

      case proc::MapAltRestriction::BETWEEN:
        return QObject::tr("At or above %1 and at or below %2").
               arg(Unit::altFeet(restriction.alt2)).
               arg(Unit::altFeet(restriction.alt1));
    }
  }
  return QString();
}

QString altRestrictionTextNarrow(const proc::MapAltRestriction& altRestriction)
{
  QString retval;
  switch(altRestriction.descriptor)
  {
    case proc::MapAltRestriction::ILS_AT:
    case proc::MapAltRestriction::ILS_AT_OR_ABOVE:
      retval = QObject::tr("GS") + Unit::altFeet(altRestriction.alt1, true, true);
      break;

    case proc::MapAltRestriction::NO_ALT_RESTR:
      break;

    case proc::MapAltRestriction::AT:
      retval = Unit::altFeet(altRestriction.alt1, true, true);
      break;

    case proc::MapAltRestriction::AT_OR_ABOVE:
      retval = QObject::tr("A") + Unit::altFeet(altRestriction.alt1, true, true);
      break;

    case proc::MapAltRestriction::AT_OR_BELOW:
      retval = QObject::tr("B") + Unit::altFeet(altRestriction.alt1, true, true);
      break;

    case proc::MapAltRestriction::BETWEEN:
      retval = QObject::tr("A") + Unit::altFeet(altRestriction.alt2, false, true) +
               QObject::tr("B") + Unit::altFeet(altRestriction.alt1, true, true);
      break;
  }
  return retval;
}

QString altRestrictionTextShort(const proc::MapAltRestriction& altRestriction)
{
  QString retval;
  switch(altRestriction.descriptor)
  {
    case proc::MapAltRestriction::ILS_AT:
    case proc::MapAltRestriction::ILS_AT_OR_ABOVE:
      retval = QObject::tr("GS ") + Unit::altFeet(altRestriction.alt1, false, false);
      break;
    case proc::MapAltRestriction::NO_ALT_RESTR:
      break;
    case proc::MapAltRestriction::AT:
      retval = Unit::altFeet(altRestriction.alt1, false, false);
      break;
    case proc::MapAltRestriction::AT_OR_ABOVE:
      retval = QObject::tr("A ") + Unit::altFeet(altRestriction.alt1, false, false);
      break;
    case proc::MapAltRestriction::AT_OR_BELOW:
      retval = QObject::tr("B ") + Unit::altFeet(altRestriction.alt1, false, false);
      break;
    case proc::MapAltRestriction::BETWEEN:
      retval = QObject::tr("A ") + Unit::altFeet(altRestriction.alt2, false, false) + QObject::tr(", B ") +
               Unit::altFeet(altRestriction.alt1, false, false);
      break;
  }
  return retval;
}

QString speedRestrictionTextShort(const proc::MapSpeedRestriction& speedRestriction)
{
  switch(speedRestriction.descriptor)
  {
    case proc::MapSpeedRestriction::NO_SPD_RESTR:
      break;

    case proc::MapSpeedRestriction::AT:
      return Unit::speedKts(speedRestriction.speed, false);

    case proc::MapSpeedRestriction::MIN:
      return QObject::tr("A ") + Unit::speedKts(speedRestriction.speed, false);

    case proc::MapSpeedRestriction::MAX:
      return QObject::tr("B ") + Unit::speedKts(speedRestriction.speed, false);
  }
  return QString();
}

QString speedRestrictionText(const proc::MapSpeedRestriction& speedRestriction)
{
  switch(speedRestriction.descriptor)
  {
    case proc::MapSpeedRestriction::NO_SPD_RESTR:
      break;

    case proc::MapSpeedRestriction::AT:
      return QObject::tr("At ") + Unit::speedKts(speedRestriction.speed);

    case proc::MapSpeedRestriction::MIN:
      return QObject::tr("Above ") + Unit::speedKts(speedRestriction.speed);

    case proc::MapSpeedRestriction::MAX:
      return QObject::tr("Below ") + Unit::speedKts(speedRestriction.speed);
  }
  return QString();
}

QString speedRestrictionTextNarrow(const proc::MapSpeedRestriction& speedRestriction)
{
  switch(speedRestriction.descriptor)
  {
    case proc::MapSpeedRestriction::NO_SPD_RESTR:
      break;

    case proc::MapSpeedRestriction::AT:
      return Unit::speedKts(speedRestriction.speed, false) + Unit::getUnitSpeedStr();

    case proc::MapSpeedRestriction::MIN:
      return QObject::tr("A") + Unit::speedKts(speedRestriction.speed, false) + Unit::getUnitSpeedStr();

    case proc::MapSpeedRestriction::MAX:
      return QObject::tr("B") + Unit::speedKts(speedRestriction.speed, false) + Unit::getUnitSpeedStr();
  }
  return QString();
}

QDebug operator<<(QDebug out, const proc::MapProcedureRef& ref)
{
  QDebugStateSaver saver(out);
  out.noquote().nospace() << "MapProcedureRef("
                          << "airportId " << ref.airportId
                          << ", runwayEndId " << ref.runwayEndId
                          << ", approachId " << ref.procedureId
                          << ", transitionId " << ref.transitionId
                          << ", legId " << ref.legId
                          << ", mapType " << ref.mapType << ")";
  return out;
}

QDebug operator<<(QDebug out, const ProcedureLegType& type)
{
  QDebugStateSaver saver(out);
  out << proc::procedureLegTypeFullStr(type);
  return out;
}

QDebug operator<<(QDebug out, const MapProcedureLegs& legs)
{
  QDebugStateSaver saver(out);
  out << "ProcedureLeg =====" << endl;
  out << "maptype" << legs.mapType << endl;

  out << "approachDistance" << legs.procedureDistance
      << "transitionDistance" << legs.transitionDistance
      << "missedDistance" << legs.missedDistance << endl;

  out << "approachType" << legs.type
      << "approachSuffix" << legs.suffix
      << "approachFixIdent" << legs.procedureFixIdent
      << "approachArincName" << legs.arincName
      << "transitionType" << legs.transitionType
      << "transitionFixIdent" << legs.transitionFixIdent
      << "procedureRunway" << legs.runway
      << "runwayEnd.name" << legs.runwayEnd.name << endl;

  out << "===== Legs =====" << endl;
  for(int i = 0; i < legs.size(); i++)
    out << "#" << i << legs.at(i);
  out << "==========================" << endl;
  return out;
}

QDebug operator<<(QDebug out, const MapProcedureLeg& leg)
{
  QDebugStateSaver saver(out);
  out << "ProcedureLeg =============" << endl;
  out << "approachId" << leg.procedureId
      << "transitionId" << leg.transitionId
      << "legId" << leg.legId << endl
      << "type" << leg.type
      << "maptype" << leg.mapType
      << "missed" << leg.missed
      << "line" << leg.line << endl;

  out << "displayText" << leg.displayText
      << "remarks" << leg.remarks;

  out << "fix" << leg.fixType << leg.fixIdent << leg.fixRegion << leg.fixPos << endl;

  out << leg.recFixType << leg.recFixIdent << leg.recFixRegion << leg.recFixPos << endl;
  out << "intercept" << leg.interceptPos << leg.intercept << endl;
  out << "pc pos" << leg.procedureTurnPos << endl;
  out << "geometry" << leg.geometry << endl;

  out << "turnDirection" << leg.turnDirection
      << "flyover" << leg.flyover
      << "trueCourse" << leg.trueCourse
      << "disabled" << leg.disabled
      << "course" << leg.course << endl;

  out << "calculatedDistance" << leg.calculatedDistance
      << "calculatedTrueCourse" << leg.calculatedTrueCourse << endl;

  out << "magvar" << leg.magvar
      << "theta" << leg.theta
      << "rho" << leg.rho
      << "distance" << leg.distance
      << "time" << leg.time << endl;

  out << "altDescriptor" << leg.altRestriction.descriptor
      << "alt1" << leg.altRestriction.alt1
      << "alt2" << leg.altRestriction.alt2 << endl;

  out << "speedDescriptor" << leg.speedRestriction.descriptor
      << "speed" << leg.speedRestriction.speed << endl;
  return out;
}

float MapProcedureLeg::calculatedMagCourse() const
{
  return calculatedTrueCourse < map::INVALID_COURSE_VALUE ?
         atools::geo::normalizeCourse(calculatedTrueCourse - magvar) : map::INVALID_COURSE_VALUE;
}

bool MapProcedureLeg::hasErrorRef() const
{
  // Check for required recommended fix - required as it is used here, not by ARINC definition
  if(atools::contains(type, {proc::ARC_TO_FIX, CONSTANT_RADIUS_ARC,
                             proc::COURSE_TO_RADIAL_TERMINATION, HEADING_TO_RADIAL_TERMINATION,
                             proc::COURSE_TO_DME_DISTANCE, proc::HEADING_TO_DME_DISTANCE_TERMINATION}) &&
     (recFixIdent.isEmpty() || !recFixPos.isValid()))
    return true;

  // If there is a fix it should be resolved
  if(!fixIdent.isEmpty() && !fixPos.isValid())
    return true;

  return false;
}

bool MapProcedureLeg::hasHardErrorRef() const
{
  return !line.isValid();
}

float MapProcedureLeg::legTrueCourse() const
{
  if(course < map::INVALID_COURSE_VALUE / 2.f)
    return trueCourse ? course : atools::geo::normalizeCourse(course + magvar);

  return map::INVALID_COURSE_VALUE;
}

bool MapProcedureLeg::isFinalApproachFix() const
{
  return proc::specialType(arincDescrCode) == proc::FAF;
}

bool MapProcedureLeg::isFinalApproachCourseFix() const
{
  return proc::specialType(arincDescrCode) == proc::FACF;
}

bool MapProcedureLeg::isFinalEndpointFix() const
{
  return proc::specialType(arincDescrCode) == proc::FEP;
}

bool MapProcedureLeg::isMissedApproachPoint() const
{
  return proc::specialType(arincDescrCode) == proc::MAP;
}

bool MapProcedureLeg::isHold() const
{
  return atools::contains(type, {proc::HOLD_TO_ALTITUDE, proc::HOLD_TO_FIX, proc::HOLD_TO_MANUAL_TERMINATION});
}

bool MapProcedureLeg::isCircular() const
{
  return atools::contains(type, {proc::ARC_TO_FIX, proc::CONSTANT_RADIUS_ARC});
}

bool MapProcedureLeg::noDistanceDisplay() const
{
  return atools::contains(type, {proc::COURSE_TO_ALTITUDE, proc::FIX_TO_ALTITUDE, proc::HEADING_TO_ALTITUDE_TERMINATION
                                 /*proc::FROM_FIX_TO_MANUAL_TERMINATION, proc::HEADING_TO_MANUAL_TERMINATION*/});
}

proc::LegSpecialType specialType(const QString& arincDescrCode)
{
  QChar idx3(atools::charAt(arincDescrCode, 3));
  if(idx3 == 'A' /* IAF */ || idx3 == 'C' /* IAF and hold */ || idx3 == 'D' /* IAF with final approach course fix */)
    return proc::IAF;

  if(idx3 == 'E')
    // Final endpoint fix
    return proc::FEP;

  if(idx3 == 'M')
    // Missed approach point
    return proc::MAP;

  if(idx3 == 'F' /* FAF published or database */)
    return proc::FAF;

  if(idx3 == 'I' /* Final approach course fix */)
    return proc::FACF;

  return proc::NONE;
}

bool MapProcedureLeg::noCalcCourseDisplay() const
{
  return isCircular() || type == proc::DIRECT_TO_FIX;
}

bool MapProcedureLeg::noIdentDisplay() const
{
  return type == proc::FROM_FIX_TO_MANUAL_TERMINATION || type == proc::HEADING_TO_MANUAL_TERMINATION;
}

const MapProcedureLeg *MapProcedureLegs::transitionLegById(int legId) const
{
  for(const MapProcedureLeg& leg : transitionLegs)
  {
    if(leg.legId == legId)
      return &leg;
  }
  return nullptr;
}

atools::geo::LineString MapProcedureLegs::buildGeometry() const
{
  atools::geo::LineString retval;
  for(int i = 0; i < size(); i++)
  {
    if(!at(i).isMissed())
      retval.append(at(i).line.getPos2());
  }
  retval.removeInvalid();
  retval.removeDuplicates();
  return retval;
}

void MapProcedureLegs::clearProcedure()
{
  mapType &= ~proc::PROCEDURE_APPROACH;
  procedureLegs.clear();
  procedureDistance = missedDistance = 0.f;
  type.clear();
  suffix.clear();
  arincName.clear();
  procedureFixIdent.clear();
  runwayEnd = map::MapRunwayEnd();
}

void MapProcedureLegs::clearTransition()
{
  mapType &= ~proc::PROCEDURE_TRANSITION;
  transitionLegs.clear();
  transitionDistance = 0.f;
  transitionType.clear();
  transitionFixIdent.clear();
}

int MapProcedureLegs::indexForLeg(const proc::MapProcedureLeg& leg) const
{
  for(int i = 0; i < size(); i++)
  {
    if(at(i).legId == leg.legId && at(i).mapType == leg.mapType && at(i).type == leg.type)
      return i;
  }
  return -1;
}

bool MapProcedureLegs::hasSidOrStarParallelRunways() const
{
  return atools::fs::util::hasSidStarParallelRunways(arincName);
}

bool MapProcedureLegs::hasSidOrStarAllRunways() const
{
  return atools::fs::util::hasSidStarAllRunways(arincName);
}

const MapProcedureLeg *proc::MapProcedureLegs::procedureLegById(int legId) const
{
  for(const MapProcedureLeg& leg : procedureLegs)
  {
    if(leg.legId == legId)
      return &leg;
  }
  return nullptr;
}

QString procedureLegCourse(const MapProcedureLeg& leg)
{
  if(!leg.noCalcCourseDisplay() && leg.calculatedDistance > 0.f && leg.calculatedTrueCourse < map::INVALID_COURSE_VALUE)
    return QLocale().toString(leg.calculatedMagCourse(), 'f', 0);
  else
    return QString();
}

QString procedureLegDistance(const MapProcedureLeg& leg)
{
  QString retval;

  if(!leg.noDistanceDisplay())
  {
    if(leg.calculatedDistance > 0.f)
      retval += Unit::distNm(leg.calculatedDistance, false);

    if(leg.time > 0.f)
    {
      if(!retval.isEmpty())
        retval += ", ";
      retval += QString::number(leg.time, 'g', 2) + QObject::tr(" min");
    }
  }
  return retval;
}

QString procedureLegRemDistance(const MapProcedureLeg& leg, float& remainingDistance)
{
  QString retval;

  if(!leg.missed)
  {
    if(leg.calculatedDistance > 0.f && leg.isInitialFix() && leg.type != proc::START_OF_PROCEDURE)
      remainingDistance -= leg.calculatedDistance;

    if(remainingDistance < 0.f)
      remainingDistance = 0.f;

    retval += Unit::distNm(remainingDistance, false);
  }

  return retval;
}

QStringList procedureLegRecommended(const MapProcedureLeg& leg)
{
  QStringList related;
  if(!leg.recFixIdent.isEmpty())
  {
    related.append(leg.recFixIdent);

    if(leg.recNavaids.hasIls())
    {
      if(!leg.recNavaids.ils.constFirst().isAnyGlsRnp())
        related.append(leg.recNavaids.ils.constFirst().freqMHzLocale() % QObject::tr(" MHz"));
    }

    if(leg.recNavaids.hasVor())
    {
      const map::MapVor& vor = leg.recNavaids.vors.constFirst();
      if(vor.tacan)
        related.append(vor.channel);
      else
        related.append(QLocale().toString(vor.frequency / 1000., 'f', 2) % QObject::tr(" MHz"));
    }

    if(leg.recNavaids.hasNdb())
      related.append(QLocale().toString(leg.recNavaids.ndbs.constFirst().frequency / 100., 'f', 1) % QObject::tr(" kHz"));

    if(leg.rho > 0.f && leg.rho < map::INVALID_DISTANCE_VALUE)
    {
      related.append(Unit::distNm(leg.rho));

      if(leg.theta < map::INVALID_COURSE_VALUE)
        related.append(radialText(leg.theta));
    }
  }
  return related;
}

QStringList procedureLegRemark(const MapProcedureLeg& leg)
{
  QStringList remarks;
  if(leg.flyover)
    remarks.append(QObject::tr("Fly over"));

  if(leg.turnDirection == "R")
    remarks.append(QObject::tr("Turn right"));
  else if(leg.turnDirection == "L")
    remarks.append(QObject::tr("Turn left"));
  else if(leg.turnDirection == "B")
    remarks.append(QObject::tr("Turn left or right"));

  if(leg.rnp > 0.f && leg.rnp < map::INVALID_DISTANCE_VALUE)
    remarks.append(QObject::tr("RNP %L1").arg(leg.rnp, 0, 'g', 3));

  QString legremarks = proc::procedureLegRemarks(leg.type);
  if(!legremarks.isEmpty())
    remarks.append(legremarks);

  if(!leg.remarks.isEmpty())
    remarks.append(leg.remarks);

  if(!leg.fixIdent.isEmpty() && !leg.fixPos.isValid())
    remarks.append(QObject::tr("Error: Fix %1/%2 type %3 not found").
                   arg(leg.fixIdent).arg(leg.fixRegion).arg(leg.fixType));

  if(!leg.recFixIdent.isEmpty() && !leg.recFixPos.isValid())
    remarks.append(QObject::tr("Error: Recommended fix %1/%2 type %3 not found").
                   arg(leg.recFixIdent).arg(leg.recFixRegion).arg(leg.recFixType));

  remarks.removeDuplicates();
  remarks.removeAll(QString());

  return remarks;
}

proc::MapProcedureTypes procedureType(bool hasSidStar, const QString& type,
                                      const QString& suffix, bool gpsOverlay)
{
  // STARS use the suffix="A" while SIDS use the suffix="D".
  if(hasSidStar && type == "GPS" && (suffix == "A" || suffix == "D") && gpsOverlay)
  {
    if(suffix == "A")
      return proc::PROCEDURE_STAR;
    else if(suffix == "D")
      return proc::PROCEDURE_SID;
  }
  return proc::PROCEDURE_APPROACH;
}

QString procedureTypeText(proc::MapProcedureTypes mapType)
{
  QString suffix;
  if(mapType & proc::PROCEDURE_APPROACH)
    suffix = QObject::tr("Approach");
  else if(mapType & proc::PROCEDURE_MISSED)
    suffix = QObject::tr("Missed");
  else if(mapType & proc::PROCEDURE_TRANSITION)
    suffix = QObject::tr("Transition");
  else if(mapType & proc::PROCEDURE_STAR)
    suffix = QObject::tr("STAR");
  else if(mapType & proc::PROCEDURE_SID)
    suffix = QObject::tr("SID");
  else if(mapType & proc::PROCEDURE_SID_TRANSITION)
    suffix = QObject::tr("SID Transition");
  else if(mapType & proc::PROCEDURE_STAR_TRANSITION)
    suffix = QObject::tr("STAR Transition");
  return suffix;
}

QString procedureTypeText(const proc::MapProcedureLeg& leg)
{
  return procedureTypeText(leg.mapType);
}

QString procedureLegsText(const proc::MapProcedureLegs& legs, proc::MapProcedureTypes procType, bool narrow, bool includeRunway,
                          bool missedAsApproach, bool transitionAsProcedure)
{
  QString procText;

  // Custom departure or arrival runway ===================================
  if(legs.isCustomApproach() && legs.mapType.testFlag(proc::PROCEDURE_APPROACH))
    procText = QObject::tr("Approach");
  else if(legs.isCustomDeparture() && legs.mapType.testFlag(proc::PROCEDURE_SID))
    procText = QObject::tr("Departure");
  else
  {
    if(legs.mapType.testFlag(proc::PROCEDURE_APPROACH) || procType.testFlag(proc::PROCEDURE_MISSED))
    {
      if(transitionAsProcedure && procType.testFlag(proc::PROCEDURE_TRANSITION))
        // This leg is a transition leg and text "APPR via TRANS" is not desired
        procText = QObject::tr("Transition %1").arg(legs.transitionFixIdent);
      else
      {
        // Approach procedure or transition =================================================
        procText = QObject::tr("%1%2%3 %4").
                   // Add missed text if leg ist missed
                   arg(narrow ? QString() :
                       (procType.testFlag(proc::PROCEDURE_MISSED) && !missedAsApproach ?
                        QObject::tr("Missed ") : QObject::tr("Approach "))).
                   arg(legs.displayType()).
                   arg(legs.suffix.isEmpty() ? QString() : (QObject::tr("-") + legs.suffix)).
                   arg(legs.procedureFixIdent);

        // Add transition text if type from related leg is a transitionn
        if(procType.testFlag(proc::PROCEDURE_TRANSITION) && legs.mapType.testFlag(proc::PROCEDURE_TRANSITION))
          procText.append(QObject::tr(" via %1").arg(legs.transitionFixIdent));
      }
    }
    else
    {
      // SID or STAR with respective transitions =================================
      if(legs.mapType.testFlag(proc::PROCEDURE_STAR_TRANSITION) && procType.testFlag(proc::PROCEDURE_STAR_TRANSITION))
        procText = QObject::tr("STAR %1.%2").arg(legs.procedureFixIdent).arg(legs.transitionFixIdent);
      else if(legs.mapType.testFlag(proc::PROCEDURE_SID_TRANSITION) && procType.testFlag(proc::PROCEDURE_SID_TRANSITION))
        procText = QObject::tr("SID %1.%2").arg(legs.procedureFixIdent).arg(legs.transitionFixIdent);
      else if(legs.mapType.testFlag(proc::PROCEDURE_STAR))
        procText = QObject::tr("STAR %1").arg(legs.procedureFixIdent);
      else if(legs.mapType.testFlag(proc::PROCEDURE_SID))
        procText = QObject::tr("SID %1").arg(legs.procedureFixIdent);
    }
  }

  // Add runway if available ================================================
  if(includeRunway)
  {
    if(!legs.runwayEnd.name.isEmpty())
      procText.append((narrow ? QObject::tr(" %1") : QObject::tr(" runway %1")).arg(legs.runwayEnd.name));
    else
    {
      // Multi runway procedure =============
      if(atools::fs::util::hasSidStarAllRunways(legs.arincName))
        procText.append(narrow ? QObject::tr(" all") : QObject::tr(" all runways"));
      else if(atools::fs::util::hasSidStarParallelRunways(legs.arincName))
        procText.append((narrow ? QObject::tr(" all %1") : QObject::tr(" all runways %1")).arg(legs.arincName.mid(2, 2)));
    }
  }
  return procText;
}

QDebug operator<<(QDebug out, const proc::MapProcedureTypes& type)
{
  QDebugStateSaver saver(out);

  QStringList flags;
  if(type == PROCEDURE_NONE)
    flags.append("PROC_NONE");
  else
  {
    if(type & PROCEDURE_APPROACH)
      flags.append("PROC_APPROACH");
    if(type & PROCEDURE_MISSED)
      flags.append("PROC_MISSED");
    if(type & PROCEDURE_TRANSITION)
      flags.append("PROC_TRANSITION");
    if(type & PROCEDURE_SID)
      flags.append("PROC_SID");
    if(type & PROCEDURE_SID_TRANSITION)
      flags.append("PROC_SID_TRANSITION");
    if(type & PROCEDURE_STAR)
      flags.append("PROC_STAR");
    if(type & PROCEDURE_STAR_TRANSITION)
      flags.append("PROC_STAR_TRANSITION");
  }

  out.nospace().noquote() << flags.join("|");

  return out;

}

bool procedureLegFixAtStart(ProcedureLegType type)
{
  return atools::contains(type, {
      proc::START_OF_PROCEDURE,
      // ARC_TO_FIX,
      // COURSE_TO_ALTITUDE,
      // COURSE_TO_DME_DISTANCE,
      // COURSE_TO_FIX,
      // COURSE_TO_INTERCEPT,
      // COURSE_TO_RADIAL_TERMINATION,
      // DIRECT_TO_FIX,
      FIX_TO_ALTITUDE,
      TRACK_FROM_FIX_FROM_DISTANCE,
      TRACK_FROM_FIX_TO_DME_DISTANCE,
      FROM_FIX_TO_MANUAL_TERMINATION,
      HOLD_TO_ALTITUDE,
      HOLD_TO_FIX,
      HOLD_TO_MANUAL_TERMINATION,
      INITIAL_FIX,
      // PROCEDURE_TURN,
      // CONSTANT_RADIUS_ARC,
      // TRACK_TO_FIX,
      // HEADING_TO_ALTITUDE_TERMINATION,
      // HEADING_TO_DME_DISTANCE_TERMINATION,
      // HEADING_TO_INTERCEPT,
      // HEADING_TO_MANUAL_TERMINATION,
      // HEADING_TO_RADIAL_TERMINATION,
      CUSTOM_APP_START, /* Start: INITIAL_FIX */
      // CUSTOM_APP_RUNWAY, /* End: COURSE_TO_FIX */
      // CUSTOM_DEP_END, /* End: COURSE_TO_FIX */
      // CUSTOM_DEP_RUNWAY /* Start: DIRECT_TO_RUNWAY */,
    });
}

bool procedureLegFixAtEnd(ProcedureLegType type)
{
  return atools::contains(type, {
      ARC_TO_FIX,
      // COURSE_TO_ALTITUDE,
      // COURSE_TO_DME_DISTANCE,
      COURSE_TO_FIX,
      // COURSE_TO_INTERCEPT,
      // COURSE_TO_RADIAL_TERMINATION,
      DIRECT_TO_FIX,
      // FIX_TO_ALTITUDE,
      // TRACK_FROM_FIX_FROM_DISTANCE,
      // TRACK_FROM_FIX_TO_DME_DISTANCE,
      // FROM_FIX_TO_MANUAL_TERMINATION,
      HOLD_TO_ALTITUDE,
      HOLD_TO_FIX,
      HOLD_TO_MANUAL_TERMINATION,
      INITIAL_FIX,
      // PROCEDURE_TURN,
      CONSTANT_RADIUS_ARC,
      TRACK_TO_FIX,
      // HEADING_TO_ALTITUDE_TERMINATION,
      // HEADING_TO_DME_DISTANCE_TERMINATION,
      // HEADING_TO_INTERCEPT,
      // HEADING_TO_MANUAL_TERMINATION,
      // HEADING_TO_RADIAL_TERMINATION,
      CUSTOM_APP_START, /* Start: INITIAL_FIX */
      CUSTOM_APP_RUNWAY, /* End: COURSE_TO_FIX */
      CUSTOM_DEP_END, /* End: COURSE_TO_FIX */
      // CUSTOM_DEP_RUNWAY /* Start: DIRECT_TO_RUNWAY */,
    });
}

bool procedureLegDrawIdent(ProcedureLegType type)
{
  return atools::contains(type, {
      ARC_TO_FIX,
      // COURSE_TO_ALTITUDE,
      // COURSE_TO_DME_DISTANCE,
      COURSE_TO_FIX,
      // COURSE_TO_INTERCEPT,
      // COURSE_TO_RADIAL_TERMINATION,
      DIRECT_TO_FIX,
      // FIX_TO_ALTITUDE,
      // TRACK_FROM_FIX_FROM_DISTANCE,
      // TRACK_FROM_FIX_TO_DME_DISTANCE,
      // FROM_FIX_TO_MANUAL_TERMINATION,
      HOLD_TO_ALTITUDE,
      HOLD_TO_FIX,
      HOLD_TO_MANUAL_TERMINATION,
      INITIAL_FIX,
      // PROCEDURE_TURN,
      CONSTANT_RADIUS_ARC,
      TRACK_TO_FIX,
      // HEADING_TO_ALTITUDE_TERMINATION,
      // HEADING_TO_DME_DISTANCE_TERMINATION,
      // HEADING_TO_INTERCEPT,
      // HEADING_TO_MANUAL_TERMINATION,
      // HEADING_TO_RADIAL_TERMINATION,

      // DIRECT_TO_RUNWAY,
      // CIRCLE_TO_LAND,
      // STRAIGHT_IN,
      START_OF_PROCEDURE,
      // VECTORS
      CUSTOM_APP_START, /* Start: INITIAL_FIX */
      CUSTOM_APP_RUNWAY, /* End: COURSE_TO_FIX */
      CUSTOM_DEP_END, /* End: COURSE_TO_FIX */
      CUSTOM_DEP_RUNWAY /* Start: DIRECT_TO_RUNWAY */,
    });
}

bool procedureLegFrom(ProcedureLegType type)
{
  return atools::contains(type, {
      // ARC_TO_FIX,
      // COURSE_TO_ALTITUDE,
      COURSE_TO_DME_DISTANCE,
      // COURSE_TO_FIX,
      COURSE_TO_INTERCEPT,
      COURSE_TO_RADIAL_TERMINATION,
      // DIRECT_TO_FIX,
      // FIX_TO_ALTITUDE,
      TRACK_FROM_FIX_FROM_DISTANCE,
      TRACK_FROM_FIX_TO_DME_DISTANCE,
      FROM_FIX_TO_MANUAL_TERMINATION,
      // HOLD_TO_ALTITUDE,
      // HOLD_TO_FIX,
      // HOLD_TO_MANUAL_TERMINATION,
      // INITIAL_FIX,
      // PROCEDURE_TURN,
      // CONSTANT_RADIUS_ARC,
      // TRACK_TO_FIX,
      // HEADING_TO_ALTITUDE_TERMINATION,
      HEADING_TO_DME_DISTANCE_TERMINATION,
      HEADING_TO_INTERCEPT,
      HEADING_TO_MANUAL_TERMINATION,
      HEADING_TO_RADIAL_TERMINATION,
      // DIRECT_TO_RUNWAY,
      // CIRCLE_TO_LAND,
      // STRAIGHT_IN,
      // START_OF_PROCEDURE,
      // VECTORS
      // CUSTOM_APP_START, /* Start: INITIAL_FIX */
      // CUSTOM_APP_RUNWAY, /* End: COURSE_TO_FIX */
      // CUSTOM_DEP_END, /* End: COURSE_TO_FIX */
      // CUSTOM_DEP_RUNWAY /* Start: DIRECT_TO_RUNWAY */,
    });
}

QString  procedureTextSuffixDepartDest(const Route& route, const map::MapAirport& airport, bool& disable)
{
  bool departure = false, destination = false, alternate = false;
  proc::procedureFlags(route, &airport, &departure, &destination, &alternate);

  QString text;
  if(airport.isValid())
  {
    if(departure)
      text = QObject::tr(" (is departure)");
    else if(destination)
      text = QObject::tr(" (is destination)");
    else if(alternate)
      text = QObject::tr(" (is alternate)");
    disable = false;
  }
  else
    disable = true;
  return text;
}

QString  procedureTextSuffixAlternate(const Route& route, const map::MapAirport& airport, bool& disable)
{
  bool departure = false, destination = false, alternate = false;
  proc::procedureFlags(route, &airport, &departure, &destination, &alternate);

  QString text;
  if(airport.isValid())
  {
    if(destination)
    {
      disable = true;
      text = QObject::tr(" (is destination)");
    }
    else if(departure)
      text = QObject::tr(" (is departure)");
    else if(alternate)
    {
      disable = true;
      text = QObject::tr(" (is alternate)");
    }
  }
  else
    disable = true;
  return text;
}

QString  procedureTextSuffixDirectTo(bool& disable, const Route& route, int legIndex, const map::MapAirport *airport)
{
  QString text;
  int activeLegIndex = route.getActiveLegIndex();
  if(!NavApp::isConnectedAndAircraft())
  {
    disable = true;
    text = QObject::tr(" (not connected to simulator)");
  }
  else if(activeLegIndex == -1 || activeLegIndex == map::INVALID_INDEX_VALUE)
  {
    // Active is required for PPOS
    disable = true;
    text = QObject::tr(" (no active flight plan segment)");
  }
  else if((legIndex != -1 && legIndex < activeLegIndex) || (route.getSizeWithoutAlternates() == 1 && legIndex == 0))
  {
    // Do not allow to direct to back if leg index is given
    // Or circling with single airport - allow only another airport or navaid
    disable = true;
    text = QObject::tr(" (past active)");
  }
  else if(legIndex != -1 && legIndex < map::INVALID_INDEX_VALUE)
  {
    // Index for route leg given
    bool containsProc, moveDownTouchesProc, moveUpTouchesProc;
    route.selectionFlagsProcedures(QList<int>({legIndex}), containsProc, moveDownTouchesProc, moveUpTouchesProc);

    bool procOk = (containsProc && !moveUpTouchesProc) || // First point of approach or arrival procedure
                  !containsProc; // Selected leg is no procedure
    bool departOk = legIndex > 0 || activeLegIndex > 0; // Back to departure only if active after direct to leg

    disable = !procOk || !departOk;
    if(disable)
      text = QObject::tr(" (is procedure)");
    else
    {
      // Airport given - try to find "is destination" and other texts
      if(airport != nullptr && airport->isValid())
      {
        bool disableDummy;
        text = procedureTextSuffixDepartDest(route, *airport, disableDummy);
      }

      if(text.isEmpty())
        // No specific text but it is still a part of the plan
        text = QObject::tr(" (flight plan)");
    }
  }

  return text;
}

void procedureFlags(const Route& route, const map::MapBase *base, bool *departure, bool *destination, bool *alternate,
                    bool *roundtrip, bool *arrivalProc, bool *departureProc)
{
  if(departure != nullptr)
    *departure = false;
  if(destination != nullptr)
    *destination = false;
  if(alternate != nullptr)
    *alternate = false;
  if(roundtrip != nullptr)
    *roundtrip = false;
  if(arrivalProc != nullptr)
    *arrivalProc = false;
  if(departureProc != nullptr)
    *departureProc = false;

  if(base != nullptr && base->getType() == map::AIRPORT)
  {
    const map::MapAirport *airport = base->asPtr<map::MapAirport>();

    if(departure != nullptr)
      *departure = route.isAirportDeparture(airport->ident);
    if(destination != nullptr)
      *destination = route.isAirportDestination(airport->ident);
    if(alternate != nullptr)
      *alternate = route.isAirportAlternate(airport->ident) || route.isAirportDestination(airport->ident);
    if(roundtrip != nullptr)
      *roundtrip = route.isAirportRoundTrip(airport->ident);

    if(arrivalProc != nullptr)
      *arrivalProc = NavApp::getMapQueryGui()->hasArrivalProcedures(*airport);
    if(departureProc != nullptr)
      *departureProc = NavApp::getMapQueryGui()->hasDepartureProcedures(*airport);
  }
}

QStringList procedureTextFirstAndLastFix(const MapProcedureLegs& legs, proc::MapProcedureTypes mapType)
{
  // First create a vector with either all legs or only approach legs depending on type related to leg
  QVector<const MapProcedureLeg *> tempLegs;
  if(mapType & proc::PROCEDURE_ANY_TRANSITION)
  {
    for(int i = 0; i < legs.size(); i++)
      tempLegs.append(&legs.at(i));
  }
  else
  {
    for(const proc::MapProcedureLeg& leg : legs.procedureLegs)
      tempLegs.append(&leg);
  }

  // Find first fix name from beginning
  QStringList fixes;
  for(const proc::MapProcedureLeg *leg : tempLegs)
  {
    if(leg->isMissed())
      break;

    if(!leg->fixIdent.isEmpty())
    {
      fixes.append(leg->fixIdent);
      break;
    }
  }

  // Look for last fix name from end
  for(int i = tempLegs.size() - 1; i >= 0; i--)
  {
    const MapProcedureLeg *leg = tempLegs.at(i);
    if(leg->isMissed())
      continue;

    if(!leg->fixIdent.isEmpty())
    {
      fixes.append(leg->fixIdent);
      break;
    }
  }
  return fixes;
}

QString aircraftCategoryText(const QString& cat)
{
  // Aircraft category
  if(cat.isEmpty())
    return QString();
  else if(cat == "A")
    return QObject::tr("All Aircraft");
  else if(cat == "C")
    return QObject::tr("All Aircraft, Cruise 250 kts or less");
  else if(cat == "D")
    return QObject::tr("Non-Jet and Turbo Prop");
  else if(cat == "E")
    return QObject::tr("Multi-Engine Props Only");
  else if(cat == "F")
    return QObject::tr("Jets and Turbo Props/Special, Cruise at least 190 kts");
  else if(cat == "H")
    return QObject::tr("Helicopter Only");
  else if(cat == "J")
    return QObject::tr("Jet Power");
  else if(cat == "M")
    return QObject::tr("Turbo-Prop/Special, Cruise at least 190 kts");
  else if(cat == "N")
    return QObject::tr("Non-Jet, Non-Turbo Prop");
  else if(cat == "P")
    return QObject::tr("Non-Jet, Cruise at least 190 kts");
  else if(cat == "Q")
    return QObject::tr("Non-Jet, Cruise below 190 kts");
  else if(cat == "R")
    // return QObject::tr("Aircraft as defined in a Notes Continuation Record");
    return QString();
  else if(cat == "S")
    return QObject::tr("Single Engine");
  else if(cat == "T")
    return QObject::tr("Twin Engine");

#ifdef DEBUG_INFORMATION
  return QString("[Aircraft %1]").arg(cat);

#else
  return QString();

#endif
}

QString radialText(float radial)
{
  return QObject::tr("R%1", "Radial").arg(radial, 3, 'f', 0, QChar('0'));
}

} // namespace proc
