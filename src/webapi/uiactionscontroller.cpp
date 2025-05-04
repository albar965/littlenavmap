#include "uiactionscontroller.h"
#include "gui/mainwindow.h"
#include "mapgui/mapwidget.h"
#include "common/infobuildertypes.h"
#include "common/abstractinfobuilder.h"
#include "app/navapp.h"

using InfoBuilderTypes::UiInfoData;

#include <QDebug>

UiActionsController::UiActionsController(QObject *parent, bool verboseParam, AbstractInfoBuilder *infoBuilder) :
  AbstractLnmActionsController(parent, verboseParam, infoBuilder)
{
  if(verbose)
    qDebug() << Q_FUNC_INFO;
}

WebApiResponse UiActionsController::infoAction(WebApiRequest request)
{
  Q_UNUSED(request)
  if(verbose)
    qDebug() << Q_FUNC_INFO;

  MapWidget *mw = getMainWindow()->getMapWidget();
  MapPaintWidget *mpw = NavApp::getMapPaintWidgetWeb();

  atools::geo::Rect rw = mpw->getViewRect();
  atools::geo::Rect ru = mw->getViewRect();

  UiInfoData data = {
    mw->zoom(),
    mpw->zoom(),
    mw->distance(),
    mpw->distance(),
    {ru.getNorth(), ru.getEast(), ru.getSouth(), ru.getWest()},
    {rw.getNorth(), rw.getEast(), rw.getSouth(), rw.getWest()}
  };

  // Get a new response object
  WebApiResponse response = getResponse();

  response.body = infoBuilder->uiinfo(data);
  response.status = 200;

  return response;

}
