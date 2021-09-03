#include "actionscontrollerindex.h"
#include "airportactionscontroller.h"
#include "simactionscontroller.h"

void ActionsControllerIndex::registerQMetaTypes()
{
    /* Available action controllers must be registered here */
    qRegisterMetaType<AirportActionsController*>();
    qRegisterMetaType<SimActionsController*>();
}
