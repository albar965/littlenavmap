#ifndef SIMACTIONSCONTROLLER_H
#define SIMACTIONSCONTROLLER_H

#include "abstractlnmactionscontroller.h"

/**
 * @brief Simulation actions controller implementation.
 */
class SimActionsController :
        public AbstractLnmActionsController
{
    Q_OBJECT
public:
    Q_INVOKABLE SimActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder* infoBuilder);
    /**
     * @brief get simulation info
     */
    Q_INVOKABLE WebApiResponse infoAction(WebApiRequest request);
};

#endif // SIMACTIONSCONTROLLER_H
