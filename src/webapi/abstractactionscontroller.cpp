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

#include "abstractactionscontroller.h"

#include "common/abstractinfobuilder.h"
#include "webapi/webapirequest.h"
#include "webapi/webapiresponse.h"

#include <QDebug>

AbstractActionsController::AbstractActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder* infoBuilder) : QObject(parent), verbose(verboseParam), infoBuilder(infoBuilder)
{
    // qDebug() << Q_FUNC_INFO;
}
AbstractActionsController::~AbstractActionsController(){
    // qDebug() << Q_FUNC_INFO;
}
WebApiResponse AbstractActionsController::defaultAction(WebApiRequest request){
Q_UNUSED(request)
    WebApiResponse response = getResponse();
    // Example
    response.body = "Default action";
    response.status = 200;

    return response;

}

WebApiResponse AbstractActionsController::notFoundAction(WebApiRequest request){
Q_UNUSED(request)

    WebApiResponse response = this->getResponse();

    response.body = "Not found";
    response.status = 404;

    return response;

}

WebApiResponse AbstractActionsController::getResponse(){

    WebApiResponse response = WebApiResponse();

    response.headers.insert("Content-Type", infoBuilder->contentTypeHeader);

    return response;
}
