#include "navmapwidget.h"
#include <marble/MarbleModel.h>
#include <marble/GeoDataPlacemark.h>
#include <marble/GeoDataDocument.h>
#include <marble/GeoDataTreeModel.h>
#include <marble/MarbleWidgetPopupMenu.h>
#include <marble/MarbleWidgetInputHandler.h>
#include <marble/GeoDataStyle.h>
#include <marble/GeoDataIconStyle.h>
#include <marble/GeoDataCoordinates.h>
#include <marble/GeoPainter.h>
#include <marble/LayerInterface.h>
#include <marble/ViewportParams.h>
#include<marble/MarbleLocale.h>
#include <QContextMenuEvent>

using namespace Marble;

using namespace Marble;

class MyPaintLayer :
  public QObject, public LayerInterface
{
public:
  // Constructor
  MyPaintLayer(MarbleWidget *widget) :
    m_widget(widget)
  {

  }

  // Implemented from LayerInterface
  virtual QStringList renderPosition() const
  {
    return QStringList("ORBIT");
  }

  // Implemented from LayerInterface
  virtual bool render(GeoPainter *painter,
                      ViewportParams *viewport,
                      const QString& renderPos = "NONE",
                      GeoSceneLayer *layer = 0);

private:
  MarbleWidget *m_widget;
};

bool MyPaintLayer::render(GeoPainter *painter,
                          ViewportParams *viewport,
                          const QString& renderPos,
                          GeoSceneLayer *layer)
{
  // qDebug() << ;
  // Have window title reflect the current paint layer
  GeoDataCoordinates home(8.26589, 50.29824, 0.0, GeoDataCoordinates::Degree);
  painter->mapQuality();
  painter->setRenderHint(QPainter::Antialiasing, true);

  painter->setPen(QPen(QBrush(QColor::fromRgb(255, 0, 0, 200)), 3.0, Qt::SolidLine, Qt::RoundCap));

  if(m_widget->viewContext() == Marble::Still)
    painter->drawEllipse(home, 10, 5, true);

  GeoDataCoordinates France(2.2, 48.52, 0.0, GeoDataCoordinates::Degree);
  painter->setPen(QColor(0, 0, 0));
  painter->drawText(France, "France");

  GeoDataCoordinates Canada(-77.02, 48.52, 0.0, GeoDataCoordinates::Degree);
  painter->setPen(QColor(0, 0, 0));
  painter->drawText(Canada, "Canada");

  // A line from France to Canada without tessellation

  GeoDataLineString shapeNoTessellation(NoTessellation);
  shapeNoTessellation << France << Canada;

  painter->setPen(QColor(255, 0, 0));
  painter->drawPolyline(shapeNoTessellation);

  // The same line, but with tessellation

  GeoDataLineString shapeTessellate(Tessellate);
  shapeTessellate << France << Canada;

  painter->setPen(QColor(0, 255, 0));
  painter->drawPolyline(shapeTessellate);

  // Now following the latitude circles

  GeoDataLineString shapeLatitudeCircle(RespectLatitudeCircle | Tessellate);
  shapeLatitudeCircle << France << Canada;

  painter->setPen(QColor(0, 0, 255));
  painter->drawPolyline(shapeLatitudeCircle);

//  RespectLatitudeCircle = 0x2,
//  FollowGround = 0x4,
//  RotationIndicatesFill = 0x8,
//  SkipLatLonNormalization = 0x10
  return true;
}

NavMapWidget::NavMapWidget(QWidget *parent)
  : Marble::MarbleWidget(parent)
{
MarbleGlobal::getInstance()->locale()->setMeasurementSystem(MarbleLocale::NauticalSystem);

//  MarbleLocale::MeasurementSystem distanceUnit;
//        distanceUnit = MarbleGlobal::getInstance()->locale()->measurementSystem();

  MyPaintLayer *layer = new MyPaintLayer(this);
  addLayer(layer);

  // MarbleWidgetInputHandler *localInputHandler = inputHandler();
  // MarbleAbstractPresenter *pres = new MarbleAbstractPresenter;
  // setInputHandler(nullptr);
}
