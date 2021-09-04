#ifndef UIACTIONSCONTROLLER_H
#define UIACTIONSCONTROLLER_H

#include "abstractlnmactionscontroller.h"
#include <QObject>

class UiActionsController : public AbstractLnmActionsController
{
    Q_OBJECT
public:
    Q_INVOKABLE UiActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder* infoBuilder);
    /**
     * @brief get ui info
     */
    Q_INVOKABLE WebApiResponse infoAction(WebApiRequest request);
};

#endif // UIACTIONSCONTROLLER_H
