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

#ifndef LNM_WebApiController_H
#define LNM_WebApiController_H

#include <QMap>
#include <QObject>

class WebApiRequest;
class WebApiResponse;
class AbstractInfoBuilder;

/**
 * @brief The WebApiController class resolving WebApiRequests to WebApiResponses
 */
class WebApiController :
  public QObject
{
  Q_OBJECT

public:
  explicit WebApiController(QObject *parent, bool verboseParam);
  virtual ~WebApiController() override;

  WebApiController(const WebApiController& other) = delete;
  WebApiController& operator=(const WebApiController& other) = delete;

  /**
   * @brief webApiPathPrefix declaring the path prefix which should serve API requests
   * @example e.g. "/api"
   */
  QString webApiPathPrefix;

  /**
   * @brief Resolves request path to requested controller and action.
   * Instantiates requested controller dynamically and invokes method/action
   * to process the response to return.
   * @example e.g. /airport/default -> AirportActionsController::defaultAction(...)
   * @param request
   * @return response
   */
  WebApiResponse service(WebApiRequest& request);

private:

  /**
   * @brief already instanced controllers keyed
   * by controller name
   */
  QMap<QString,QObject *> controllerInstances;

  /**
   * @brief return stored action controller instance
   * or instantiate a new one for the given name
   * @param controllerName
   * @return the controller instance
   */
  QObject* getControllerInstance(QByteArray controllerName);

  /**
   * @brief register available controllers for dynamic invocation
   */
  void registerControllers();
  /**
   * @brief register available info builders
   */
  void registerInfoBuilders();
  bool verbose = false;

  /**
   * @brief requested content-type info builder
   */
  AbstractInfoBuilder* infoBuilder;
  /**
   * @brief create controller name from path string
   * @param path
   * @return the controller class name
   */
  QByteArray getControllerNameByPath(QByteArray path);
  /**
   * @brief create action name from path string
   * @param path
   * @return the action method name
   */
  QByteArray getActionNameByPath(QByteArray path);

  /**
   * @brief add headers common to all responses
   * @param the response to add headers to
   */
  void addCommonResponseHeaders(WebApiResponse& response);
};

#endif // LNM_WebApiController_H
