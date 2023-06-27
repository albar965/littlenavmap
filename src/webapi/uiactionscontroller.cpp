#include "uiactionscontroller.h"
#include "gui/mainwindow.h"
#include "mapgui/mapwidget.h"
#include "common/infobuildertypes.h"
#include "common/abstractinfobuilder.h"
#include "app/navapp.h"

using InfoBuilderTypes::UiInfoData;

#include <QDebug>

UiActionsController::UiActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder* infoBuilder) :
    AbstractLnmActionsController(parent, verboseParam, infoBuilder)
{
    if(verbose)
        qDebug() << Q_FUNC_INFO;
}

WebApiResponse UiActionsController::infoAction(WebApiRequest request){
Q_UNUSED(request)
    if(verbose)
        qDebug() << Q_FUNC_INFO;

    // Get a new response object
    WebApiResponse response = getResponse();

    UiInfoData data = {
        getMainWindow()->getMapWidget()->zoom(),
        NavApp::getMapPaintWidgetWeb()->zoom(),
        getMainWindow()->getMapWidget()->distance(),
        NavApp::getMapPaintWidgetWeb()->distance()
    };

    response.body = infoBuilder->uiinfo(data);
    response.status = 200;

    return response;

}
