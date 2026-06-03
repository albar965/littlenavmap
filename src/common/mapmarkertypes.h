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

#ifndef LITTLENAVMAP_MARKERTYPES_H
#define LITTLENAVMAP_MARKERTYPES_H

#include "maptypes.h"

namespace atools {
namespace util {

class XmlStreamWriter;
class XmlStreamReader;
}
}
namespace map {

/* =====================================================================
 * All markers in the user interface.
 * Can be converted to QVariant as well as loaded and saved from/to XML
 * ===================================================================== */

// =====================================================================
/* All information for complete traffic pattern structure */
/* Threshold position (end of final) and runway altitude MSL */
struct PatternMarker
  : public MapBase
{
  PatternMarker() :
    MapBase(staticType())
  {
  }

  static map::MapType staticType()
  {
    return map::MARK_PATTERNS;
  }

  float magCourse() const;

  /* Save and load from XML */
  void save(atools::util::XmlStreamWriter& stream) const;
  void restore(atools::util::XmlStreamReader& stream);

  QString displayText() const;

  QString airportIdent, runwayName;
  MapNav nav; /* Reference to related navaid. type is NONE if not attached to navaid. */
  QColor color;
  bool turnRight,
       base45Degree /* calculate base turn from 45 deg after threshold */,
       showEntryExit /* Entry and exit indicators */;
  int runwayLength; /* ft Does not include displaced threshold */

  float downwindParallelDistance, finalDistance, departureDistance; /* NM */
  float courseTrue; /* degree true final course*/

  const QString& getIdent() const
  {
    return airportIdent;
  }

  float getMagVar() const
  {
    return nav.magvar;
  }

  QColor& getColor()
  {
    return color;
  }

  MapNav& getNav()
  {
    return nav;
  }

};

QDataStream& operator>>(QDataStream& dataStream, map::PatternMarker& obj);
QDataStream& operator<<(QDataStream& dataStream, const map::PatternMarker& obj);

// =====================================================================
/* Holding marker.
 * Uses its own type distinct from holdings. */
struct HoldingMarker
  : public MapBase
{
  HoldingMarker() :
    MapBase(staticType())
  {
  }

  static map::MapType staticType()
  {
    return map::MARK_HOLDING;
  }

  /* Save and load from XML */
  void save(atools::util::XmlStreamWriter& stream) const;
  void restore(atools::util::XmlStreamReader& stream);

  /* "User Holding ..." */
  QString displayText() const;

  /* Label. Can contain navaid ident and frequency if applicable */
  QString text;

  /* Aggregate the database holding structure */
  map::MapHolding holding;

  const QString& getIdent() const
  {
    return holding.getIdent();
  }

  float getMagVar() const
  {
    return holding.getMagVar();
  }

  QColor& getColor()
  {
    return holding.color;
  }

  MapNav& getNav()
  {
    return holding.nav;
  }

};

/* Save only information for user defined holds */
QDataStream& operator>>(QDataStream& dataStream, map::HoldingMarker& obj);
QDataStream& operator<<(QDataStream& dataStream, const map::HoldingMarker& obj);

// =====================================================================
/* MSA marker as placed by user.
 * Uses its own type distinct from database MSA. */
struct MsaMarker
  : public MapBase
{
  MsaMarker() :
    MapBase(staticType())
  {
  }

  static map::MapType staticType()
  {
    return map::MARK_MSA;
  }

  /* Save and load from XML */
  void save(atools::util::XmlStreamWriter& stream) const;
  void restore(atools::util::XmlStreamReader& stream);

  QString displayText() const;

  const QString& getIdent() const
  {
    return msa.getIdent();
  }

  float getMagVar() const
  {
    return msa.getMagVar();
  }

  /*   Aggregate the database MSA structure */
  map::MapAirportMsa msa;
};

/* Save only information for user defined holds */
QDataStream& operator>>(QDataStream& dataStream, map::MsaMarker& obj);
QDataStream& operator<<(QDataStream& dataStream, const map::MsaMarker& obj);

// =====================================================================
/* Range rings marker. Covers user defined rings as well as radio-navaid ranges */
struct RangeMarker
  : public MapBase
{
  RangeMarker() :
    MapBase(staticType())
  {
  }

  static map::MapType staticType()
  {
    return map::MARK_RANGE;
  }

  /* Save and load from XML */
  void save(atools::util::XmlStreamWriter& stream) const;
  void restore(atools::util::XmlStreamReader& stream);

  QString displayText() const;

  QString text; /* Text to display like VOR name and frequency */
  QList<double> ranges; /* Range ring list (NM) */
  MapNav nav; /* Reference to related navaid. type is NONE if not attached to navaid. */
  bool attachedToNavaid = false; /* true if range ring at navaid which cannot be moved*/
  bool manualLabel = true; /* true if label was manually set by user and should not be changed when attaching to a navaid */
  bool aircraftRange = false; /* Shows aircraft range instead of one or more range rings */
  QColor color;

  const QString& getIdent() const
  {
    return nav.ident;
  }

  QColor& getColor()
  {
    return color;
  }

  MapNav& getNav()
  {
    return nav;
  }

};

QDataStream& operator>>(QDataStream& dataStream, map::RangeMarker& obj);
QDataStream& operator<<(QDataStream& dataStream, const map::RangeMarker& obj);

// =====================================================================
/* Distance measurement line. */
struct DistanceMarker
  : public MapBase
{
  DistanceMarker()
    : MapBase(staticType())
  {
  }

  static map::MapType staticType()
  {
    return map::MARK_DISTANCE;
  }

  bool isValid() const
  {
    return from.isValid();
  }

  const atools::geo::Pos& getPositionTo() const
  {
    return to;
  }

  const atools::geo::Pos& getPositionFrom() const
  {
    return from;
  }

  float getDistanceMeter() const
  {
    return from.distanceMeterTo(to);
  }

  float getDistanceNm() const;

  /* Save and load from XML */
  void save(atools::util::XmlStreamWriter& stream) const;
  void restore(atools::util::XmlStreamReader& stream);

  QString displayText() const;

  QString text; /* Text to display like VOR name and frequency */
  MapNav nav; /* Reference to related navaid. type is NONE if not attached to navaid. */
  QColor color; /* Line color depends on origin (airport or navaid type */
  atools::geo::Pos from, to;
  bool manualLabel = true; /* true if label was manually set by user and should not be changed when attaching to a navaid */

  float getMagVar() const
  {
    return nav.magvar;
  }

  const QString& getIdent() const
  {
    return nav.ident;
  }

  QColor& getColor()
  {
    return color;
  }

  MapNav& getNav()
  {
    return nav;
  }

};

QDataStream& operator>>(QDataStream& dataStream, map::DistanceMarker& obj);
QDataStream& operator<<(QDataStream& dataStream, const map::DistanceMarker& obj);

/* Register serializable objects */
void registerMarkerMetaTypes();

/* Get labels for markers depending on clicked type */
QString markerLabel(const map::MapUserpoint& userpoint);
QString markerLabel(const map::MapVor& vor);
QString markerLabel(const map::MapNdb& ndb);

} // namespace map

/* Type information =============================================== */
Q_DECLARE_TYPEINFO(map::RangeMarker, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(map::RangeMarker)
Q_DECLARE_METATYPE(QList<map::RangeMarker>)

Q_DECLARE_TYPEINFO(map::DistanceMarker, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(map::DistanceMarker)
Q_DECLARE_METATYPE(QList<map::DistanceMarker>)

Q_DECLARE_TYPEINFO(map::PatternMarker, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(map::PatternMarker)
Q_DECLARE_METATYPE(QList<map::PatternMarker>)

Q_DECLARE_TYPEINFO(map::HoldingMarker, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(map::HoldingMarker)
Q_DECLARE_METATYPE(QList<map::HoldingMarker>)

Q_DECLARE_TYPEINFO(map::MsaMarker, Q_RELOCATABLE_TYPE);
Q_DECLARE_METATYPE(map::MsaMarker)
Q_DECLARE_METATYPE(QList<map::MsaMarker>)

#endif // LITTLENAVMAP_MARKERTYPES_H
