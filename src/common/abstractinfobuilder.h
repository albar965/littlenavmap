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

#ifndef ABSTRACTINFOBUILDER_H
#define ABSTRACTINFOBUILDER_H

#include <QObject>
#include <QLocale>

namespace InfoBuilderTypes {
    struct AirportInfoData;
    struct SimConnectInfoData;
    struct UiInfoData;
    struct MapFeaturesData;
}
namespace atools {
    namespace sql {
        class SqlRecord;
    }
    namespace geo {
        class Pos;
    }
}

using atools::geo::Pos;
using InfoBuilderTypes::AirportInfoData;
using InfoBuilderTypes::SimConnectInfoData;
using InfoBuilderTypes::UiInfoData;
using InfoBuilderTypes::MapFeaturesData;

/**
 * Generic interface for LNM-specific views.
 * Base class for info builders
 * creating representations of supplied data.
 */
class AbstractInfoBuilder : public QObject
{
  Q_OBJECT

public:
  explicit AbstractInfoBuilder(QObject *parent);
  virtual ~AbstractInfoBuilder();
  AbstractInfoBuilder(const AbstractInfoBuilder& other) = delete;
  AbstractInfoBuilder& operator=(const AbstractInfoBuilder& other) = delete;

  /**
   * @brief Content MIME type header this builder is for
   * @example e.g. "text/plain"
   */
  QByteArray contentTypeHeader;

  /**
   * Creates a description for the provided airport.
   *
   * @param airportInfoData
   */
  virtual QByteArray airport(AirportInfoData airportInfoData) const;

  /**
   * Creates a description for the provided feature list.
   *
   * @param airportInfoData
   */
  virtual QByteArray features(MapFeaturesData mapFeaturesData) const;

  /**
   * Creates a description for the provided map object.
   *
   * @param airportInfoData
   */
  virtual QByteArray feature(MapFeaturesData mapFeaturesData) const;

  /**
   * Creates a description for the provided simconnect data.
   *
   * @param simConnectInfoData
   */
  virtual QByteArray siminfo(SimConnectInfoData simConnectInfoData) const;


  /**
   * Creates a description for the provided UI data.
   *
   * @param uiInfoData
   */
  virtual QByteArray uiinfo(UiInfoData uiInfoData) const;
protected:
  /**
   * @brief Get heading and opposed heading corrected by magnetic variation
   * @param heading
   * @param magvar
   * @return both headings as formatted string
   */
  virtual QString getHeadingsStringByMagVar(float heading, float magvar) const;
  /**
   * @brief Pretty print-format frequency
   * @param frequency
   * @return
   */
  virtual QString formatComFrequency(int frequency) const;
  /**
   * @brief Get pretty printed coords from suitable sql record
   * @param rec
   * @return printable string
   */
  virtual QString getCoordinatesString(const atools::sql::SqlRecord *rec) const;
  /**
   * @brief get pretty printed coords from Pos
   * @param rec
   * @return printable string
   */
  virtual QString getCoordinatesString(const Pos& pos) const;
  /**
   * @brief get pretty printed coords from Pos
   * @param rec
   * @return printable string
   */
  virtual QString getLocalizedCoordinatesString(const Pos& pos) const;
  /**
   * @brief get a map of coordinate data from suitable sql record
   * @param pos
   * @return map of coordinate data
   */
  virtual QMap<QString,float> getCoordinates(const atools::sql::SqlRecord *rec) const;
  /**
   * @brief get a map of coordinate data from Pos
   * @param pos
   * @return map of coordinate data
   */
  virtual QMap<QString,float> getCoordinates(const Pos& pos) const;
private:
  QLocale locale;
};

#endif // ABSTRACTINFOBUILDER_H
