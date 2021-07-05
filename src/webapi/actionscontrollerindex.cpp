#include "actionscontrollerindex.h"
#include "airportactionscontroller.h"

void ActionsControllerIndex::registerQMetaTypes()
{
    /* Available action controllers must be registered here */
    qRegisterMetaType<AirportActionsController*>();
}
