#include "simactionscontroller.h"
#include "common/maptypes.h"
#include "common/infobuildertypes.h"
#include "common/abstractinfobuilder.h"
#include "navapp.h"
#include "atools.h"
#include "fs/sc/simconnectdata.h"

using InfoBuilderTypes::SimConnectInfoData;

SimActionsController::SimActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder* infoBuilder) :
    AbstractLnmActionsController(parent, verboseParam, infoBuilder)
{
    if(verbose)
        qDebug() << Q_FUNC_INFO;
}


WebApiResponse SimActionsController::infoAction(WebApiRequest request){
    if(verbose)
        qDebug() << Q_FUNC_INFO;

    // Get a new response object
    WebApiResponse response = getResponse();

    SimConnectData simConnectData = getSimConnectData();

    SimConnectInfoData data = {
      &simConnectData
    };

    response.body = infoBuilder->siminfo(data);

    return response;

}
