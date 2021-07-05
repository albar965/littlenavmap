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

#include "webapirequest.h"
#include "webapiresponse.h"

#include <QObject>

/**
 * @brief The base class for all API action controllers
 */
class AbstractActionsController :
        public QObject
{
    Q_OBJECT
public:
    AbstractActionsController(QObject *parent, bool verboseParam);
    virtual ~AbstractActionsController();
    /**
     * @brief example for inheritable and invokable method/action
     */
    Q_INVOKABLE virtual WebApiResponse defaultAction(WebApiRequest request);
protected:
    bool verbose = false;
};

#endif // ABSTRACTACTIONSCONTROLLER_H