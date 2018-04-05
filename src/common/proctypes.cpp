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

#include "common/proctypes.h"

#include "atools.h"
#include "geo/calculations.h"
#include "common/unit.h"
#include "options/optiondata.h"

#include <QDataStream>
#include <QHash>
#include <QObject>

namespace proc  {

static QRegularExpression PARALLEL_REGEXP("^RW[0-9]{2}B$");

static QHash<QString, QString> approachFixTypeToStr;
static QHash<QString, QString> approachTypeToStr;
static QHash<ProcedureLegType, QString> approachLegTypeToStr;
static QHash<ProcedureLegType, QString> approachLegRemarkStr;

void initTranslateableTexts()
{
  approachFixTypeToStr = QHash<QString, QString>(
    {
      {"NONE", QObject::tr("NONE")},
      {"L", QObject::tr("Localizer")},
      {"V", QObject::tr("VOR")},
      {"N", QObject::tr("NDB")},
      {"TN", QObject::tr("Terminal NDB")},
      {"W", QObject::tr("Waypoint")},
      {"TW", QObject::tr("Terminal Waypoint")},
      {"R", QObject::tr("Runway")}
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
      {"GNSS", QObject::tr("GNSS")},
      {"TCN", QObject::tr("TACAN")},
      {"CTL", QObject::tr("Circle to Land")},
      {"MLS", QObject::tr("MLS")}

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
      {START_OF_PROCEDURE, QObject::tr("Start of procedure")}
    });

  approachLegRemarkStr = QHash<ProcedureLegType, QString>(
    {
      {ARC_TO_FIX, QObject::tr("")},
      {COURSE_TO_ALTITUDE, QObject::tr("")},
      {COURSE_TO_DME_DISTANCE, QObject::tr("")},
      {COURSE_TO_FIX, QObject::tr("")},
      {COURSE_TO_INTERCEPT, QObject::tr("")},
      {COURSE_TO_RADIAL_TERMINATION, QObject::tr("")},
      {DIRECT_TO_FIX, QObject::tr("")},
      {FIX_TO_ALTITUDE, QObject::tr("")},
      {TRACK_FROM_FIX_FROM_DISTANCE, QObject::tr("")},
      {TRACK_FROM_FIX_TO_DME_DISTANCE, QObject::tr("")},
      {FROM_FIX_TO_MANUAL_TERMINATION, QObject::tr("")},
      {HOLD_TO_ALTITUDE, QObject::tr("Mandatory hold")},
      {HOLD_TO_FIX, QObject::tr("Single circuit")},
      {HOLD_TO_MANUAL_TERMINATION, QObject::tr("Mandatory hold")},
      {INITIAL_FIX, QObject::tr("")},
      {PROCEDURE_TURN, QObject::tr("")},
      {CONSTANT_RADIUS_ARC, QObject::tr("")},
      {TRACK_TO_FIX, QObject::tr("")},
      {HEADING_TO_ALTITUDE_TERMINATION, QObject::tr("")},
      {HEADING_TO_DME_DISTANCE_TERMINATION, QObject::tr("")},
      {HEADING_TO_INTERCEPT, QObject::tr("")},
      {HEADING_TO_MANUAL_TERMINATION, QObject::tr("")},
      {HEADING_TO_RADIAL_TERMINATION, QObject::tr("")},

      {DIRECT_TO_RUNWAY, QObject::tr("")},
      {START_OF_PROCEDURE, QObject::tr("")}
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
    {"SX", START_OF_PROCEDURE}
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
    {START_OF_PROCEDURE, "SX"}
  });

QString procedureFixType(const QString& type)
{
  return approachFixTypeToStr.value(type, type);
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

QString altRestrictionText(const MapAltRestriction& restriction)
{
  switch(restriction.descriptor)
  {
    case proc::MapAltRestriction::NONE:
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
  return QString();
}

QString altRestrictionTextNarrow(const proc::MapAltRestriction& altRestriction)
{
  QString retval;
  switch(altRestriction.descriptor)
  {
    case proc::MapAltRestriction::NONE:
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
    case proc::MapAltRestriction::NONE:
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
    case proc::MapSpeedRestriction::NONE:
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
    case proc::MapSpeedRestriction::NONE:
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
    case proc::MapSpeedRestriction::NONE:
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

QDebug operator<<(QDebug out, const ProcedureLegType& type)
{
  out << proc::procedureLegTypeFullStr(type);
  return out;
}

QDebug operator<<(QDebug out, const MapProcedureLegs& legs)
{
  out << "ProcedureLeg =====" << endl;
  out << "maptype" << legs.mapType << endl;

  out << "approachDistance" << legs.approachDistance
      << "transitionDistance" << legs.transitionDistance
      << "missedDistance" << legs.missedDistance << endl;

  out << "approachType" << legs.approachType
      << "approachSuffix" << legs.approachSuffix
      << "approachFixIdent" << legs.approachFixIdent
      << "approachArincName" << legs.approachArincName
      << "transitionType" << legs.transitionType
      << "transitionFixIdent" << legs.transitionFixIdent
      << "procedureRunway" << legs.procedureRunway
      << "runwayEnd.name" << legs.runwayEnd.name << endl;

  out << "===== Legs =====" << endl;
  for(int i = 0; i < legs.size(); i++)
    out << "#" << i << legs.at(i);
  out << "==========================" << endl;
  return out;
}

QDebug operator<<(QDebug out, const MapProcedureLeg& leg)
{
  out << "ProcedureLeg =============" << endl;
  out << "approachId" << leg.approachId
      << "transitionId" << leg.transitionId
      << "legId" << leg.legId << endl
      << "type" << leg.type
      << "maptype" << leg.mapType
      << "missed" << leg.missed
      << "line" << leg.line << endl;

  out << "displayText" << leg.displayText
      << "remarks" << leg.remarks;

  out << "navId" << leg.navId << "fix" << leg.fixType << leg.fixIdent << leg.fixRegion << leg.fixPos << endl;

  out << "recNavId" << leg.recNavId << leg.recFixType << leg.recFixIdent << leg.recFixRegion << leg.recFixPos << endl;
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
      << "dist" << leg.distance
      << "time" << leg.time << endl;

  out << "altDescriptor" << leg.altRestriction.descriptor
      << "alt1" << leg.altRestriction.alt1
      << "alt2" << leg.altRestriction.alt2 << endl;
  return out;
}

MapProcedurePoint::MapProcedurePoint(const MapProcedureLeg& leg)
{
  calculatedDistance = leg.calculatedDistance;
  calculatedTrueCourse = leg.calculatedTrueCourse;
  time = leg.time;
  theta = leg.theta;
  rho = leg.rho;
  magvar = leg.magvar;
  fixType = leg.fixType;
  fixIdent = leg.fixIdent;
  recFixType = leg.recFixType;
  recFixIdent = leg.recFixIdent;
  turnDirection = leg.turnDirection;
  displayText = leg.displayText;
  remarks = leg.remarks;
  altRestriction = leg.altRestriction;
  speedRestriction = leg.speedRestriction;
  type = leg.type;
  missed = leg.missed;
  flyover = leg.flyover;
  mapType = leg.mapType;
  position = leg.line.getPos1();
}

bool MapProcedureLeg::hasInvalidRef() const
{
  return (!fixIdent.isEmpty() && !fixPos.isValid()) || (!recFixIdent.isEmpty() && !recFixPos.isValid());
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

float MapProcedureLeg::legTrueCourse() const
{
  return trueCourse ? course : atools::geo::normalizeCourse(course + magvar);
}

bool MapProcedureLeg::isHold() const
{
  return atools::contains(type,
                          {proc::HOLD_TO_ALTITUDE, proc::HOLD_TO_FIX, proc::HOLD_TO_MANUAL_TERMINATION});
}

bool MapProcedureLeg::isCircular() const
{
  return atools::contains(type, {proc::ARC_TO_FIX, proc::CONSTANT_RADIUS_ARC});
}

bool MapProcedureLeg::noDistanceDisplay() const
{
  return atools::contains(type,
                          {proc::COURSE_TO_ALTITUDE, proc::FIX_TO_ALTITUDE,
                           proc::FROM_FIX_TO_MANUAL_TERMINATION, proc::HEADING_TO_ALTITUDE_TERMINATION,
                           proc::HEADING_TO_MANUAL_TERMINATION, });
}

bool MapProcedureLeg::noCourseDisplay() const
{
  return type == /*proctypes::INITIAL_FIX ||*/ isCircular();
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

MapProcedureLeg& MapProcedureLegs::atInternal(int i)
{
  if(isDeparture())
  {
    if(i < approachLegs.size())
      return approachLegs[apprIdx(i)];
    else
      return transitionLegs[transIdx(i)];
  }
  else
  {
    if(i < transitionLegs.size())
      return transitionLegs[transIdx(i)];
    else
      return approachLegs[apprIdx(i)];
  }
}

const MapProcedureLeg& MapProcedureLegs::atInternalConst(int i) const
{
  if(isDeparture())
  {
    if(i < approachLegs.size())
      return approachLegs[apprIdx(i)];
    else
      return transitionLegs[transIdx(i)];
  }
  else
  {
    if(i < transitionLegs.size())
      return transitionLegs[transIdx(i)];
    else
      return approachLegs[apprIdx(i)];
  }
}

int MapProcedureLegs::apprIdx(int i) const
{
  return isDeparture() ? i : i - transitionLegs.size();
}

int MapProcedureLegs::transIdx(int i) const
{
  return isDeparture() ? i - approachLegs.size() : i;
}

void MapProcedureLegs::clearApproach()
{
  mapType &= ~proc::PROCEDURE_APPROACH;
  approachLegs.clear();
  approachDistance = missedDistance = 0.f;
  approachType.clear();
  approachSuffix.clear();
  approachArincName.clear();
  approachFixIdent.clear();
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

bool hasSidStarParallelRunways(const QString& approachArincName)
{
  return approachArincName.contains(PARALLEL_REGEXP);
}

bool hasSidStarAllRunways(const QString& approachArincName)
{
  return approachArincName == "ALL";
}

const MapProcedureLeg *proc::MapProcedureLegs::approachLegById(int legId) const
{
  for(const MapProcedureLeg& leg : approachLegs)
  {
    if(leg.legId == legId)
      return &leg;
  }
  return nullptr;
}

QString procedureLegCourse(const MapProcedureLeg& leg)
{
  if(!leg.noCourseDisplay() && leg.calculatedDistance > 0.f && leg.calculatedTrueCourse < map::INVALID_COURSE_VALUE)
    return QLocale().toString(atools::geo::normalizeCourse(leg.calculatedTrueCourse - leg.magvar), 'f', 0);
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
    if(leg.calculatedDistance > 0.f && leg.type != proc::INITIAL_FIX && leg.type != proc::START_OF_PROCEDURE)
      remainingDistance -= leg.calculatedDistance;

    if(remainingDistance < 0.f)
      remainingDistance = 0.f;

    retval += Unit::distNm(remainingDistance, false);
  }

  return retval;
}

QString procedureLegRemark(const MapProcedureLeg& leg)
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

  QString legremarks = proc::procedureLegRemarks(leg.type);
  if(!legremarks.isEmpty())
    remarks.append(legremarks);

  if(!leg.recFixIdent.isEmpty())
  {
    if(leg.rho > 0.f)
      remarks.append(QObject::tr("Related: %1 / %2 / %3").arg(leg.recFixIdent).
                     arg(Unit::distNm(leg.rho /*, true, 20, true*/)).
                     arg(QLocale().toString(leg.theta, 'f', 0) + QObject::tr("°M")));
    else
      remarks.append(QObject::tr("Related: %1").arg(leg.recFixIdent));
  }

  if(!leg.remarks.isEmpty())
    remarks.append(leg.remarks);

  if(!leg.fixIdent.isEmpty() && !leg.fixPos.isValid())
    remarks.append(QObject::tr("Error: Fix %1/%2 type %3 not found").
                   arg(leg.fixIdent).arg(leg.fixRegion).arg(leg.fixType));
  if(!leg.recFixIdent.isEmpty() && !leg.recFixPos.isValid())
    remarks.append(QObject::tr("Error: Recommended fix %1/%2 type %3 not found").
                   arg(leg.recFixIdent).arg(leg.recFixRegion).arg(leg.recFixType));

  return remarks.join(", ");
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

QDebug operator<<(QDebug out, const proc::MapProcedureTypes& type)
{
  QDebugStateSaver saver(out);
  Q_UNUSED(saver);

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

} // namespace types
