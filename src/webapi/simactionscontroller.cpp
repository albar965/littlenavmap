#include "simactionscontroller.h"
#include "common/infobuildertypes.h"
#include "common/abstractinfobuilder.h"
#include "geo/calculations.h"
#include "fs/sc/simconnectdata.h"
#include "webapi/webapirequest.h"

using InfoBuilderTypes::SimConnectInfoData;
using atools::geo::normalizeCourse;

SimActionsController::SimActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder* infoBuilder) :
    AbstractLnmActionsController(parent, verboseParam, infoBuilder)
{
    if(verbose)
        qDebug() << Q_FUNC_INFO;
}


WebApiResponse SimActionsController::infoAction(WebApiRequest request){
Q_UNUSED(request)
    if(verbose)
        qDebug() << Q_FUNC_INFO;

    // Get a new response object
    WebApiResponse response = getResponse();

    SimConnectData simConnectData = getSimConnectData();

    float windSpeed = simConnectData.getUserAircraft().getWindSpeedKts();
    float windDir = normalizeCourse(simConnectData.getUserAircraft().getWindDirectionDegT() - simConnectData.getUserAircraft().getMagVarDeg());

    SimConnectInfoData data = {
      &simConnectData,
      windSpeed,
      windDir
    };

    response.body = infoBuilder->siminfo(data);
    response.status = 200;

    return response;

}
