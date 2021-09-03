#include "uiactionscontroller.h"
#include "gui/mainwindow.h"
#include "mapgui/mapwidget.h"
#include "common/infobuildertypes.h"
#include "common/abstractinfobuilder.h"

using InfoBuilderTypes::UiInfoData;

#include <QDebug>

UiActionsController::UiActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder* infoBuilder) :
    AbstractLnmActionsController(parent, verboseParam, infoBuilder)
{
    if(verbose)
        qDebug() << Q_FUNC_INFO;
}

#include <QDebug>

WebApiResponse UiActionsController::infoAction(WebApiRequest request){
    if(verbose)
        qDebug() << Q_FUNC_INFO;

    // Get a new response object
    WebApiResponse response = getResponse();

    UiInfoData data = {
        getMainWindow()->getMapWidget()->zoom()
    };

    response.body = infoBuilder->uiinfo(data);
    response.status = 200;

    return response;

}
