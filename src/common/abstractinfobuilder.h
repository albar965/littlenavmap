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

#include "sql/sqlrecord.h"
#include "common/unit.h"
#include "common/infobuildertypes.h"

#include <QObject>

using InfoBuilderTypes::AirportInfoData;

/**
 * Base class for all info builders
 * creating representations of supplied data.
 */
class AbstractInfoBuilder : public QObject
{
  Q_OBJECT

public:
  AbstractInfoBuilder(QObject *parent);
  virtual ~AbstractInfoBuilder();
  AbstractInfoBuilder(const AbstractInfoBuilder& other) = delete;
  AbstractInfoBuilder& operator=(const AbstractInfoBuilder& other) = delete;

  /**
   * @brief Content type header this builder is for
   * @example e.g. "text/plain"
   */
  QByteArray contentTypeHeader;

  /**
   * Creates a description for the provided airport 
   * and weather context.
   * 
   * @param airport
   * @param route
   * @param weatherContext
   */
  virtual QByteArray airport(AirportInfoData airportInfoData) const;

};

#endif // ABSTRACTINFOBUILDER_H
