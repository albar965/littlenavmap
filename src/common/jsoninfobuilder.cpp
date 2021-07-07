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

#include "common/jsoninfobuilder.h"
#include "common/maptypes.h"

// Use JSON library
#include "json/nlohmann/json.hpp"
using JSON = nlohmann::json;

JsonInfoBuilder::JsonInfoBuilder(QObject *parent)
  : AbstractInfoBuilder(parent)
{
  contentTypeHeader = "application/json";
}

JsonInfoBuilder::~JsonInfoBuilder()
{

}

QByteArray JsonInfoBuilder::airport(const map::MapAirport& airport, const map::WeatherContext& weatherContext,
                                    const Route *route) const
{
    JSON json = {
        {"airportName",qUtf8Printable(airport.name)}
    };

    return json.dump().data();
}

