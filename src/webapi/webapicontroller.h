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

#include "geo/rect.h"

/**
 * @brief Generic WebApiRequest POD object
 */
class WebApiRequest {
public:
    QByteArray path;
    QByteArray method;
    QMultiMap<QByteArray, QByteArray> headers;
    QMultiMap<QByteArray, QByteArray> parameters;
    QByteArray body;
};

/**
 * @brief Generic WebApiResponse POD object
 */
class WebApiResponse {
public:
    int status;
    QMultiMap<QByteArray, QByteArray> headers;
    QByteArray body;
};

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
   * @brief Return API response for provided request
   * @param request
   * @return response
   */
  WebApiResponse service(WebApiRequest& request);

private:
  bool verbose = false;
};

#endif // LNM_WebApiController_H
