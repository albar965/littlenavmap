#include "actionscontrollerindex.h"
#include "airportactionscontroller.h"
#include "simactionscontroller.h"
#include "uiactionscontroller.h"

void ActionsControllerIndex::registerQMetaTypes()
{
    /* Available action controllers must be registered here */
    qRegisterMetaType<AirportActionsController*>();
    qRegisterMetaType<SimActionsController*>();
    qRegisterMetaType<UiActionsController*>();
}
