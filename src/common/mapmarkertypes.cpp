/*****************************************************************************
* Copyright 2015-2026 Alexander Barthel alex@littlenavmap.org
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

#include "common/mapmarkertypes.h"

#include "common/unit.h"
#include "fs/common/binarymsageometry.h"
#include "util/xmlstreamreader.h"
#include "util/xmlstreamwriter.h"

namespace map {

// PatternMarker ######################################################################################
// .   <AirportICAO>EDDL</AirportICAO>
// .   <RunwayName>R10L</RunwayName>
// .   <Color>#101010</Color>
// .   <TurnRight>true</TurnRight>
// .   <Base45Degree>true</Base45Degree>
// .   <ShowEntryExit>true</ShowEntryExit>
// .   <RunwayLengthFt>10000</RunwayLengthFt>
// .   <DownwindParallelDistanceNM>2</DownwindParallelDistanceNM>
// .   <FinalDistanceNM>3.5</FinalDistanceNM>
// .   <DepartureDistanceNM>1.5</DepartureDistanceNM>
// .   <CourseTrueDeg>10.8</CourseTrueDeg>
// .   <MagVarDeg>1.5</MagVarDeg>
// .   <Pos Lon="13.368729" Lat="55.534626" Alt="231.00"/>
void PatternMarker::save(atools::util::XmlStreamWriter& stream) const
{
  stream.writeTextElement(QStringLiteral("AirportIdent"), airportIdent);
  stream.writeTextElement(QStringLiteral("RunwayName"), runwayName);
  stream.writeTextElement(QStringLiteral("Color"), color);
  stream.writeTextElement(QStringLiteral("TurnRight"), turnRight);
  stream.writeTextElement(QStringLiteral("Base45Degree"), base45Degree);
  stream.writeTextElement(QStringLiteral("ShowEntryExit"), showEntryExit);
  stream.writeTextElement(QStringLiteral("RunwayLengthFt"), runwayLength);
  stream.writeTextElement(QStringLiteral("DownwindParallelDistanceNM"), downwindParallelDistance);
  stream.writeTextElement(QStringLiteral("FinalDistanceNM"), finalDistance);
  stream.writeTextElement(QStringLiteral("DepartureDistanceNM"), departureDistance);
  stream.writeTextElement(QStringLiteral("CourseTrueDeg"), courseTrue);
  stream.writeTextElement(QStringLiteral("Pos"), position, true /* writeAltitude */);
  stream.writeStartElement(QStringLiteral("Nav"));
  nav.save(stream);
  stream.writeEndElement();
}

void PatternMarker::restore(atools::util::XmlStreamReader& stream)
{
  nav.reset();

  while(stream.readNextStartElement())
  {
    const QStringView name = stream.name();
    if(name == QStringLiteral("AirportIdent"))
      airportIdent = stream.readElementTextStr();
    else if(name == QStringLiteral("RunwayName"))
      runwayName = stream.readElementTextStr();
    else if(name == QStringLiteral("Color"))
      color = stream.readElementTextColor();
    else if(name == QStringLiteral("TurnRight"))
      turnRight = stream.readElementTextBool();
    else if(name == QStringLiteral("Base45Degree"))
      base45Degree = stream.readElementTextBool();
    else if(name == QStringLiteral("ShowEntryExit"))
      showEntryExit = stream.readElementTextBool();
    else if(name == QStringLiteral("RunwayLengthFt"))
      runwayLength = stream.readElementTextInt();
    else if(name == QStringLiteral("DownwindParallelDistanceNM"))
      downwindParallelDistance = stream.readElementTextFloat();
    else if(name == QStringLiteral("FinalDistanceNM"))
      finalDistance = stream.readElementTextFloat();
    else if(name == QStringLiteral("DepartureDistanceNM"))
      departureDistance = stream.readElementTextFloat();
    else if(name == QStringLiteral("CourseTrueDeg"))
      courseTrue = stream.readElementTextFloat();
    else if(name == QStringLiteral("Pos"))
      position = stream.readElementPos();
    else if(name == QStringLiteral("Nav"))
      nav.restore(stream);
    else
      stream.skipCurrentElement(true /* warning */);
  }

  type = map::MARK_PATTERNS;
}

QString PatternMarker::displayText() const
{
  if(airportIdent.isEmpty())
    return QObject::tr("Traffic Pattern %1 RW %2").arg(turnRight ?
                                                       QObject::tr("R", "Pattern direction") :
                                                       QObject::tr("L", "Pattern direction")).arg(runwayName);
  else
    return QObject::tr("Traffic Pattern %1 %2 RW %3").
           arg(airportIdent).arg(turnRight ?
                                 QObject::tr("R", "Pattern direction") :
                                 QObject::tr("L", "Pattern direction")).arg(runwayName);
}

// HoldingMarker ######################################################################################
// .   <NavIdent>BOMBI</NavIdent>
// .   <NavType>VOR</NavType> AIRPORT, VOR, NDB or WAYPOINT
// .   <VorDmeOnly>true</VorDmeOnly>
// .   <VorHasDme>false</VorHasDme>
// .   <VorTacan>false</VorTacan>
// .   <VorVortac>false</VorVortac>
// .   <Color>#101010</Color>
// .   <TurnLeft>true</TurnLeft>
// .   <TimeMin>4.5</TimeMin>
// .   <SpeedKts>200</SpeedKts>
// .   <CourseTrueDeg>145</CourseTrueDeg>
// .   <MagvarDeg>1.5</MagvarDeg>
// .   <Pos Lon="13.368729" Lat="55.534626" Alt="231.00"/>
void HoldingMarker::save(atools::util::XmlStreamWriter& stream) const
{
  stream.writeTextElement(QStringLiteral("Name"), holding.name);
  stream.writeTextElement(QStringLiteral("Text"), text);
  stream.writeTextElement(QStringLiteral("Color"), holding.color);
  stream.writeTextElement(QStringLiteral("TurnLeft"), holding.turnLeft);
  stream.writeTextElement(QStringLiteral("TimeMin"), holding.time);
  stream.writeTextElement(QStringLiteral("SpeedKts"), holding.speedKts);
  stream.writeTextElement(QStringLiteral("CourseTrueDeg"), holding.courseTrue);
  stream.writeTextElement(QStringLiteral("Pos"), holding.position, true /* writeAltitude */);
  stream.writeStartElement(QStringLiteral("Nav"));
  holding.nav.save(stream);
  stream.writeEndElement();
}

void HoldingMarker::restore(atools::util::XmlStreamReader& stream)
{
  holding.nav.reset();
  while(stream.readNextStartElement())
  {
    const QStringView name = stream.name();
    if(name == QStringLiteral("Text"))
      text = stream.readElementTextStr();
    else if(name == QStringLiteral("Name"))
      holding.name = stream.readElementTextStr();
    else if(name == QStringLiteral("Color"))
      holding.color = stream.readElementTextColor();
    else if(name == QStringLiteral("TurnLeft"))
      holding.turnLeft = stream.readElementTextBool();
    else if(name == QStringLiteral("TimeMin"))
      holding.time = stream.readElementTextFloat();
    else if(name == QStringLiteral("SpeedKts"))
      holding.speedKts = stream.readElementTextFloat();
    else if(name == QStringLiteral("CourseTrueDeg"))
      holding.courseTrue = stream.readElementTextFloat();
    else if(name == QStringLiteral("Pos"))
      position = holding.position = stream.readElementPos();
    else if(name == QStringLiteral("Nav"))
      holding.nav.restore(stream);
    else
      stream.skipCurrentElement(true /* warning */);
  }

  // Not used in map marker
  holding.length = holding.speedLimit = holding.minAltititude = holding.maxAltititude = 0.f;
  holding.id = -1;
  type = map::MARK_HOLDING;
}

QString HoldingMarker::displayText() const
{
  if(holding.nav.ident.isEmpty())
    return QObject::tr("User Holding %1 %2").
           arg(holding.turnLeft ? QObject::tr("L", "Holding direction") : QObject::tr("R", "Holding direction")).
           arg(Unit::altFeet(holding.position.getAltitude()));
  else
    return QObject::tr("User Holding %1 %2 %3").
           arg(holding.nav.ident).
           arg(holding.turnLeft ? QObject::tr("L", "Holding direction") : QObject::tr("R", "Holding direction")).
           arg(Unit::altFeet(holding.position.getAltitude()));
}

// MsaMarker ######################################################################################
// .   <AirportIdent>EDDF</AirportIdent>
// .   <NavIdent>BOMBI</NavIdent>
// .   <Region>ED</Region>
// .   <MultipleCode></MultipleCode>
// .   <VorType>DME</VorType>
// .   <NavType>VOR</NavType> AIRPORT, VOR, NDB, ILS or WAYPOINT
// .   <VorDmeOnly>true</VorDmeOnly>
// .   <VorHasDme>true</VorHasDme>
// .   <VorTacan>false</VorTacan>
// .   <VorVortac>false</VorVortac>
// .   <RadiusNM>20</RadiusNM>
// .   <MagvarDeg>1.5</MagvarDeg>
// .   <Bearings>
// .     <BearingDeg>120</BearingDeg>
// .     <BearingDeg>180</BearingDeg>
// .   </Bearings>
// .   <Altitudes>
// .     <AltitudeFt>120</AltitudeFt>
// .     <AltitudeFt>180</AltitudeFt>
// .   </Altitudes>
// .   <TrueBearing>false</TrueBearing>
// .   <Pos Lon="13.368729" Lat="55.534626"/>
void MsaMarker::save(atools::util::XmlStreamWriter& stream) const
{
  stream.writeTextElement(QStringLiteral("AirportIdent"), msa.airportIdent);
  stream.writeTextElement(QStringLiteral("Region"), msa.region);
  stream.writeTextElement(QStringLiteral("MultipleCode"), msa.multipleCode);
  stream.writeTextElement(QStringLiteral("VorType"), msa.vorType);
  stream.writeTextElement(QStringLiteral("RadiusNM"), msa.radius);
  stream.writeTextElement(QStringLiteral("TrueBearing"), msa.trueBearing);
  stream.writeTextElement(QStringLiteral("Bearings"), QStringLiteral("BearingDeg"), msa.bearings);
  stream.writeTextElement(QStringLiteral("Altitudes"), QStringLiteral("AltitudeFt"), msa.altitudes);
  stream.writeTextElement(QStringLiteral("Pos"), msa.position);
  stream.writeStartElement(QStringLiteral("Nav"));
  msa.nav.save(stream);
  stream.writeEndElement();
}

void MsaMarker::restore(atools::util::XmlStreamReader& stream)
{
  msa.nav.reset();

  while(stream.readNextStartElement())
  {
    const QStringView name = stream.name();
    if(name == QStringLiteral("AirportIdent"))
      msa.airportIdent = stream.readElementTextStr();
    else if(name == QStringLiteral("Region"))
      msa.region = stream.readElementTextStr();
    else if(name == QStringLiteral("MultipleCode"))
      msa.multipleCode = stream.readElementTextStr();
    else if(name == QStringLiteral("VorType"))
      msa.vorType = stream.readElementTextStr();
    else if(name == QStringLiteral("RadiusNM"))
      msa.radius = stream.readElementTextFloat();
    else if(name == QStringLiteral("TrueBearing"))
      msa.trueBearing = stream.readElementTextBool();
    else if(name == QStringLiteral("Bearings"))
      msa.bearings = stream.readElementListFloat(QStringLiteral("BearingDeg"));
    else if(name == QStringLiteral("Altitudes"))
      msa.altitudes = stream.readElementListFloat(QStringLiteral("AltitudeFt"));
    else if(name == QStringLiteral("Pos"))
      position = msa.position = stream.readElementPos();
    else if(name == QStringLiteral("Nav"))
      msa.nav.restore(stream);
    else
      stream.skipCurrentElement(true /* warning */);
  }

  // Calculate geometry and labels ===================
  if(msa.bearings.size() == msa.altitudes.size())
  {
    atools::fs::common::BinaryMsaGeometry geo;
    for(int i = 0; i < msa.bearings.size(); i++)
      geo.addSector(msa.bearings.at(i), msa.altitudes.at(i));

    geo.calculate(msa.position, msa.radius, msa.nav.magvar, msa.trueBearing);
    msa.bearingEndPos = geo.getBearingEndPositions();
    msa.labelPositions = geo.getLabelPositions();
    msa.geometry = geo.getGeometry();
    msa.bounding = geo.getBoundingRect();
  }
  else
    qWarning() << Q_FUNC_INFO << msa.bearings.size() << "!=" << msa.altitudes.size();

  type = map::MARK_MSA;
}

QString MsaMarker::displayText() const
{
  return airportMsaText(msa, true);
}

// RangeMarker ######################################################################################
// .   <Text>EDDF</Text>
// .   <Ranges>
// .     <RangeNM>2</RangeNM>
// .     <RangeNM>4</RangeNM>
// .   </Ranges>
// .   <Pos Lon="13.368729" Lat="55.534626"/>
// .   <Color>#101010</Color>
void RangeMarker::save(atools::util::XmlStreamWriter& stream) const
{
  stream.writeTextElement(QStringLiteral("Text"), text);
  stream.writeTextElement(QStringLiteral("Color"), color);
  stream.writeTextElement(QStringLiteral("Ranges"), QStringLiteral("RangeNM"), ranges);
  stream.writeTextElement(QStringLiteral("AttachedToNavaid"), attachedToNavaid);
  stream.writeTextElement(QStringLiteral("ManualLabel"), manualLabel);
  stream.writeTextElement(QStringLiteral("AircraftRange"), aircraftRange);
  stream.writeTextElement(QStringLiteral("Pos"), position);
  stream.writeStartElement(QStringLiteral("Nav"));
  nav.save(stream);
  stream.writeEndElement();
}

void RangeMarker::restore(atools::util::XmlStreamReader& stream)
{
  nav.reset();

  while(stream.readNextStartElement())
  {
    const QStringView name = stream.name();
    if(name == QStringLiteral("Text"))
      text = stream.readElementTextStr();
    else if(name == QStringLiteral("Color"))
      color = stream.readElementTextColor();
    else if(name == QStringLiteral("Ranges"))
      ranges = stream.readElementListDouble(QStringLiteral("RangeNM"));
    else if(name == QStringLiteral("AttachedToNavaid"))
      attachedToNavaid = stream.readElementTextBool();
    else if(name == QStringLiteral("ManualLabel"))
      manualLabel = stream.readElementTextBool();
    else if(name == QStringLiteral("AircraftRange"))
      aircraftRange = stream.readElementTextBool();
    else if(name == QStringLiteral("Pos"))
      position = stream.readElementPos();
    else if(name == QStringLiteral("Nav"))
      nav.restore(stream);
    else
      stream.skipCurrentElement(true /* warning */);
  }

  type = map::MARK_RANGE;
}

QString RangeMarker::displayText() const
{
  if(text.isEmpty())
    return QObject::tr("Range Rings");
  else
    return QObject::tr("Range Rings %1").arg(text);
}

// DistanceMarker ######################################################################################
// .   <Text>EDDF</Text>
// .   <Color>#101010</Color>
// .   <MagVarDeg>1.5</MagVarDeg>
// .   <Flags>RADIAL|CALIBRATED</Flags>
// .   <PosFrom Lon="13.368729" Lat="55.534626"/>
// .   <PosTo Lon="13.368729" Lat="55.534626"/>
void DistanceMarker::save(atools::util::XmlStreamWriter& stream) const
{
  stream.writeTextElement(QStringLiteral("Text"), text);
  stream.writeTextElement(QStringLiteral("Color"), color);
  stream.writeTextElement(QStringLiteral("PosFrom"), from);
  stream.writeTextElement(QStringLiteral("PosTo"), to);
  stream.writeTextElement(QStringLiteral("ManualLabel"), manualLabel);
  stream.writeStartElement(QStringLiteral("Nav"));
  nav.save(stream);
  stream.writeEndElement();
}

void DistanceMarker::restore(atools::util::XmlStreamReader& stream)
{
  nav.reset();

  while(stream.readNextStartElement())
  {
    const QStringView name = stream.name();
    if(name == QStringLiteral("Text"))
      text = stream.readElementTextStr();
    else if(name == QStringLiteral("Color"))
      color = stream.readElementTextColor();
    else if(name == QStringLiteral("PosFrom"))
      from = stream.readElementPos();
    else if(name == QStringLiteral("PosTo"))
      to = stream.readElementPos();
    else if(name == QStringLiteral("ManualLabel"))
      manualLabel = stream.readElementTextBool();
    else if(name == QStringLiteral("Nav"))
      nav.restore(stream);
    else
      stream.skipCurrentElement(true /* warning */);
  }

  type = map::MARK_DISTANCE;
  position = to;
}

QString DistanceMarker::displayText() const
{
  float distanceMeter = getDistanceMeter();
  QString distStr(distanceMeter < map::INVALID_DISTANCE_VALUE ? Unit::distMeter(distanceMeter) : QStringLiteral());

  if(text.isEmpty())
    return QObject::tr("Measurement %1").arg(distStr);
  else
    return QObject::tr("Measurement %1 %2").arg(text).arg(distStr);
}

// Binary legacy functions ######################################################################################

QDataStream& operator>>(QDataStream& dataStream, PatternMarker& obj)
{
  obj.nav.reset();
  dataStream >> obj.airportIdent >> obj.runwayName >> obj.color >> obj.turnRight >> obj.base45Degree >> obj.showEntryExit
  >> obj.runwayLength >> obj.downwindParallelDistance >> obj.finalDistance >> obj.departureDistance
  >> obj.courseTrue >> obj.nav.magvar >> obj.position;
  obj.type = map::MARK_PATTERNS;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const PatternMarker& obj)
{
  dataStream << obj.airportIdent << obj.runwayName << obj.color << obj.turnRight << obj.base45Degree << obj.showEntryExit
             << obj.runwayLength << obj.downwindParallelDistance << obj.finalDistance << obj.departureDistance
             << obj.courseTrue << obj.nav.magvar << obj.position;

  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, HoldingMarker& obj)
{
  obj.holding.nav.reset();

  dataStream >> obj.holding.nav.ident >> obj.holding.nav.type >> obj.holding.nav.vorDmeOnly
  >> obj.holding.nav.vorDme
  >> obj.holding.nav.vorTacan >> obj.holding.nav.vorVortac >> obj.holding.color >> obj.holding.turnLeft >> obj.holding.time
  >> obj.holding.speedKts >> obj.holding.courseTrue >> obj.holding.nav.magvar >> obj.holding.position;

  obj.holding.length = obj.holding.speedLimit = obj.holding.minAltititude = obj.holding.maxAltititude = 0.f;
  obj.holding.airportIdent.clear();

  obj.position = obj.holding.position;
  obj.type = map::MARK_HOLDING;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const HoldingMarker& obj)
{
  dataStream << obj.holding.nav.ident << obj.holding.nav.type << obj.holding.nav.vorDmeOnly
             << obj.holding.nav.vorDme
             << obj.holding.nav.vorTacan << obj.holding.nav.vorVortac << obj.holding.color << obj.holding.turnLeft
             << obj.holding.time << obj.holding.speedKts << obj.holding.courseTrue << obj.holding.nav.magvar << obj.holding.position;
  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, map::MsaMarker& obj)
{
  dataStream >> obj.msa.airportIdent >> obj.msa.nav.ident >> obj.msa.region >> obj.msa.multipleCode >> obj.msa.vorType;
  dataStream >> obj.msa.nav.type >> obj.msa.nav.vorDmeOnly >> obj.msa.nav.vorDme >> obj.msa.nav.vorTacan
  >> obj.msa.nav.vorVortac;
  dataStream >> obj.msa.radius >> obj.msa.nav.magvar;
  dataStream >> obj.msa.bearings >> obj.msa.altitudes >> obj.msa.trueBearing
  >> obj.msa.geometry >> obj.msa.labelPositions >> obj.msa.bearingEndPos >> obj.msa.bounding >> obj.msa.position;

  obj.position = obj.msa.position;
  obj.type = map::MARK_MSA;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const map::MsaMarker& obj)
{
  dataStream << obj.msa.airportIdent << obj.msa.nav.ident << obj.msa.region << obj.msa.multipleCode << obj.msa.vorType;
  dataStream << obj.msa.nav.type << obj.msa.nav.vorDmeOnly << obj.msa.nav.vorDme << obj.msa.nav.vorTacan
             << obj.msa.nav.vorVortac;
  dataStream << obj.msa.radius << obj.msa.nav.magvar;
  dataStream << obj.msa.bearings << obj.msa.altitudes << obj.msa.trueBearing
             << obj.msa.geometry << obj.msa.labelPositions << obj.msa.bearingEndPos << obj.msa.bounding << obj.msa.position;
  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, map::RangeMarker& obj)
{
  QList<float> rangesFloat;
  dataStream >> obj.text >> rangesFloat >> obj.position >> obj.color;

  obj.ranges.clear();
  for(float range : std::as_const(rangesFloat))
    obj.ranges.append(static_cast<double>(range));

  obj.type = map::MARK_RANGE;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const map::RangeMarker& obj)
{
  QList<float> rangesFloat;
  for(double range : std::as_const(obj.ranges))
    rangesFloat.append(static_cast<float>(range));

  dataStream << obj.text << rangesFloat << obj.position << obj.color;
  return dataStream;
}

QDataStream& operator>>(QDataStream& dataStream, map::DistanceMarker& obj)
{
  obj.nav.reset();
  quint32 flags;
  dataStream >> obj.text >> obj.color >> obj.from >> obj.to >> obj.nav.magvar >> flags;
  obj.type = map::MARK_DISTANCE;
  obj.position = obj.to;
  return dataStream;
}

QDataStream& operator<<(QDataStream& dataStream, const map::DistanceMarker& obj)
{
  quint32 flags = 0;
  dataStream << obj.text << obj.color << obj.from << obj.to << obj.nav.magvar << flags;
  return dataStream;
}

float PatternMarker::magCourse() const
{
  return atools::geo::normalizeCourse(courseTrue - nav.magvar);
}

float DistanceMarker::getDistanceNm() const
{
  return atools::geo::meterToNm(getDistanceMeter());
}

void registerMarkerMetaTypes()
{
  qRegisterMetaType<map::RangeMarker>();
  qRegisterMetaType<QList<map::RangeMarker> >();
  qRegisterMetaType<map::DistanceMarker>();
  qRegisterMetaType<QList<map::DistanceMarker> >();
  qRegisterMetaType<map::PatternMarker>();
  qRegisterMetaType<QList<map::PatternMarker> >();
  qRegisterMetaType<map::HoldingMarker>();
  qRegisterMetaType<QList<map::HoldingMarker> >();
  qRegisterMetaType<map::MsaMarker>();
  qRegisterMetaType<QList<map::MsaMarker> >();
}

QString markerLabel(const MapUserpoint& userpoint)
{
  return map::userpointShortText(userpoint, 20);
}

QString markerLabel(const MapVor& vor)
{
  if(vor.frequency == 0 || vor.tacan)
    return QObject::tr("%1 %2").arg(vor.ident).arg(vor.channel);
  else
    return QObject::tr("%1 %2").arg(vor.ident).arg(QString::number(vor.frequency / 1000., 'f', 2));
}

QString markerLabel(const MapNdb& ndb)
{
  return QObject::tr("%1 %2").arg(ndb.ident).arg(QString::number(ndb.frequency / 100., 'f', 1));
}

} // namespace map
