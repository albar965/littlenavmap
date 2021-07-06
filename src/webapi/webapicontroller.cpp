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

#include "webapi/webapicontroller.h"
#include "webapi/actionscontrollerindex.h"

#include <QDebug>
#include <QMetaMethod>


WebApiController::WebApiController(QObject *parent, bool verboseParam)
  : QObject(parent), verbose(verboseParam)
{
    qDebug() << Q_FUNC_INFO;
    webApiPathPrefix = "/api";
    registerControllers();
}

void WebApiController::registerControllers(){
    ActionsControllerIndex::registerQMetaTypes();
}

WebApiController::~WebApiController()
{
  qDebug() << Q_FUNC_INFO;
}

WebApiResponse WebApiController::service(WebApiRequest& request)
{

  // Create response object
  WebApiResponse response;

  // Set controller and action by string
  QByteArray controllerName = getControllerNameByPath(request.path);
  QByteArray actionName = getActionNameByPath(request.path);

  qDebug() << Q_FUNC_INFO << ":"
           << request.method << ":"
           << request.path << ":"
           << controllerName << ":"
           << actionName << ":"
           << request.body;


  if(request.method == "OPTIONS"){

      /* Pass through preflight request */
      response.status = 200;

  }else{

      /* Process REST controller/action request */

      // Get controller class id
      int id = QMetaType::type(controllerName+"*");

      if (id != 0) {

          // Create controller instance
          const QMetaObject* mo = QMetaType::metaObjectForType(id);
          QObject* controller = mo->newInstance(Q_ARG(QObject*, parent()),Q_ARG(bool, verbose));

          // Invoke action on instance
          bool actionExecuted = QMetaObject::invokeMethod(
                      controller,
                      actionName.data(),
                      Qt::DirectConnection,
                      Q_RETURN_ARG(WebApiResponse,response),
                      Q_ARG(WebApiRequest, request)
                      );

          if(!actionExecuted){
              response.status = 400; /* Bad request */
              response.body = "Action not found/failed";
          }

      }else{
          response.status = 400; /* Bad request */
          response.body = "Controller not found";
      }

  }

  addCommonResponseHeaders(response);

  return response;

}

QByteArray WebApiController::getControllerNameByPath(QByteArray path){
    QByteArray name = "AbstractActionsController"; /* Default fallback */
    QList<QByteArray> list = path.split('/');
    if(list.length() > 1 && list[1].length() > 0){
        name = list[1]+"ActionsController";
        /* upper case first letter to enable lowercase controller URL's */
        name[0] = toupper(name[0]);
        return name;
    }
    return name;
};
QByteArray WebApiController::getActionNameByPath(QByteArray path){
    QByteArray name = "notFoundAction"; /* Default fallback */
    QList<QByteArray> list = path.split('/');
    if(list.length() > 2 && list[2].length() > 0){
        name = list[2]+"Action";
    }
    return name;
};

void WebApiController::addCommonResponseHeaders(WebApiResponse &response){

    /* CORS: Enable cross-origin requests */
    response.headers.insert("Access-Control-Allow-Origin","*");
    response.headers.insert("Access-Control-Allow-Methods","GET, PUT, POST, DELETE");
    response.headers.insert("Access-Control-Allow-Headers","content-type");

}
