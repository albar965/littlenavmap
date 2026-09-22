/*****************************************************************************
* RNAV Web API controller.
*****************************************************************************/
#ifndef LNM_RNAVACTIONSCONTROLLER_H
#define LNM_RNAVACTIONSCONTROLLER_H

#include "webapi/abstractlnmactionscontroller.h"

class WebApiRequest;
class WebApiResponse;
class AbstractInfoBuilder;

class RnavActionsController : public AbstractLnmActionsController
{
  Q_OBJECT
public:
  Q_INVOKABLE RnavActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder *infoBuilder);
  Q_INVOKABLE WebApiResponse referenceAction(WebApiRequest request);
};

#endif // LNM_RNAVACTIONSCONTROLLER_H
