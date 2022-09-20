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

#ifndef ABSTRACTACTIONSCONTROLLER_H
#define ABSTRACTACTIONSCONTROLLER_H

#include "webapi/webapirequest.h"
#include "webapi/webapiresponse.h"
class AbstractInfoBuilder;

#include <QObject>

/**
 * @brief Base class for all invokable API action controllers
 */
class AbstractActionsController :
        public QObject
{
    Q_OBJECT
public:
    AbstractActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder* infoBuilder);
    virtual ~AbstractActionsController() override;
    /**
     * @brief example for inheritable and invokable method/action
     */
    Q_INVOKABLE virtual WebApiResponse defaultAction(WebApiRequest request);
    /**
     * @brief return a "404 not found" response
     */
    Q_INVOKABLE virtual WebApiResponse notFoundAction(WebApiRequest request);
protected:
    /**
     * @brief get new response object
     * @return
     */
    WebApiResponse getResponse();
    /**
     * @brief verbose
     */
    bool verbose = false;
    /**
     * @brief the response body builder
     */
    AbstractInfoBuilder* infoBuilder;
};

#endif // ABSTRACTACTIONSCONTROLLER_H
