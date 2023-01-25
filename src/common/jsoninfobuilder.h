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

#ifndef JSONINFOBUILDER_H
#define JSONINFOBUILDER_H

#include "common/abstractinfobuilder.h"

// Use JSON library
#include "json/nlohmann/json.hpp"
using JSON = nlohmann::json;

/**
 * Builder for JSON representations of supplied data. All
 * usable methods must be declared at AbstractInfoBuilder
 * in order to be callable through its interface.
 */
class JsonInfoBuilder : public AbstractInfoBuilder
{
  Q_OBJECT
public:
  explicit JsonInfoBuilder(QObject *parent);
  virtual ~JsonInfoBuilder() override;
  JsonInfoBuilder(const JsonInfoBuilder& other) = delete;
  JsonInfoBuilder& operator=(const JsonInfoBuilder& other) = delete;

  QByteArray airport(AirportInfoData airportInfoData) const override;
  QByteArray siminfo(SimConnectInfoData simConnectInfoData) const override;
  QByteArray uiinfo(UiInfoData uiInfoData) const override;
  QByteArray features(MapFeaturesData mapFeaturesData) const override;
  QByteArray feature(MapFeaturesData mapFeaturesData) const override;

private:
  JSON coordinatesToJSON(QMap<QString,float> map) const;
};

#endif // JSONINFOBUILDER_H
