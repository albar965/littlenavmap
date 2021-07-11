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

#ifndef AIRPORTACTIONSCONTROLLER_H
#define AIRPORTACTIONSCONTROLLER_H

#include "webapi/abstractlnmactionscontroller.h"

/**
 * @brief Airport actions controller implementation.
 */
class AirportActionsController :
        public AbstractLnmActionsController
{
    Q_OBJECT
public:
    Q_INVOKABLE AirportActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder* infoBuilder);
    /**
     * @brief get airport info
     */
    Q_INVOKABLE WebApiResponse infoAction(WebApiRequest request);
};

#endif // AIRPORTACTIONSCONTROLLER_H
