/*****************************************************************************
* Copyright 2015-2016 Alexander Barthel albar965@mailbox.org
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include "mappainterils.h"
#include "symbolpainter.h"

#include "mapgui/mapscale.h"
#include "mapgui/maplayer.h"
#include "mapgui/mapquery.h"
#include "geo/calculations.h"
#include "common/maptypes.h"
#include "common/mapcolors.h"

#include <QElapsedTimer>

#include <marble/GeoPainter.h>
#include <marble/ViewportParams.h>

using namespace Marble;
using namespace atools::geo;

MapPainterIls::MapPainterIls(Marble::MarbleWidget *widget, MapQuery *mapQuery, MapScale *mapScale,
                             bool verboseMsg)
  : MapPainter(widget, mapQuery, mapScale, verboseMsg)
{
  symbolPainter = new SymbolPainter();
}

MapPainterIls::~MapPainterIls()
{
  delete symbolPainter;
}

void MapPainterIls::paint(const MapLayer *mapLayer, Marble::GeoPainter *painter,
                          Marble::ViewportParams *viewport, maptypes::ObjectTypes objectTypes)
{
  if(mapLayer == nullptr || !objectTypes.testFlag(maptypes::ILS))
    return;

  if(mapLayer->isIls())
  {
    bool drawFast = widget->viewContext() == Marble::Animation;

    const GeoDataLatLonBox& curBox = viewport->viewLatLonAltBox();
    QElapsedTimer t;
    t.start();

    const QList<MapIls> *ilss = query->getIls(curBox, mapLayer, drawFast);
    if(ilss != nullptr)
    {
      setRenderHints(painter);
      if(widget->viewContext() == Marble::Still && verbose)
      {
        qDebug() << "Number of ils" << ilss->size();
        qDebug() << "Time for query" << t.elapsed() << " ms";
        qDebug() << curBox.toString();
        qDebug() << *mapLayer;
        t.restart();
      }

      for(const MapIls& ils : *ilss)
      {
        int x, y;
        bool visible = wToS(ils.position, x, y);

        if(!visible)
        {
          GeoDataLatLonBox ilsbox(ils.bounding.getNorth(), ils.bounding.getSouth(),
                                  ils.bounding.getEast(), ils.bounding.getWest(),
                                  GeoDataCoordinates::Degree);
          visible = ilsbox.intersects(curBox);
        }

        if(visible)
          drawIlsSymbol(painter, ils, x, y, mapLayer, drawFast);
      }
    }
    if(widget->viewContext() == Marble::Still && verbose)
      qDebug() << "Time for paint" << t.elapsed() << " ms";
  }
}

void MapPainterIls::drawIlsSymbol(GeoPainter *painter, const MapIls& ils, int x, int y,
                                  const MapLayer *mapLayer, bool fast)
{
  painter->save();

  painter->setBackgroundMode(Qt::TransparentMode);

  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(mapcolors::ilsSymbolColor, 2, Qt::SolidLine, Qt::FlatCap));

  bool visible;
  QPoint pmid = wToS(ils.posmid, &visible);
  QPoint origin(x, y);

  QPoint p1 = wToS(ils.pos1, &visible);
  QPoint p2 = wToS(ils.pos2, &visible);

  painter->drawLine(origin, p1);
  painter->drawLine(p1, pmid);
  painter->drawLine(pmid, p2);
  painter->drawLine(p2, origin);

  if(ils.slope > 0)
    painter->drawLine(p1, p2);

  QString text;
  if(mapLayer->isIlsInfo())
  {
    text = ils.ident + " / " +
           QString::number(ils.frequency / 1000., 'f', 2) + " / " +
           QString::number(ils.heading + ils.magvar, 'f', 0) + "°M";

    if(ils.slope > 0)
      text += " / GS " + QString::number(ils.slope, 'f', 1) + "°";
    if(ils.dme)
      text += " / DME";
  }
  else if(mapLayer->isIlsIdent())
    text = ils.ident;

  if(!text.isEmpty() && !fast)
  {
    painter->translate(x, y);

    float rotate;
    if(ils.heading > 180)
      rotate = ils.heading + 90.f - ils.width / 2.f;
    else
      rotate = atools::geo::opposedCourseDeg(ils.heading) + 90.f + ils.width / 2.f;

    int featherLen = static_cast<int>(std::roundf(scale->getPixelForMeter(nmToMeter(8.f), rotate)));
    int texth = painter->fontMetrics().descent();
    int textw = painter->fontMetrics().width(text);

    int textpos;
    if(ils.heading > 180)
      textpos = (featherLen - textw) / 2;
    else
      textpos = -(featherLen + textw) / 2;

    painter->rotate(rotate);
    painter->drawText(textpos, -texth, text);
    painter->resetTransform();
  }
  painter->restore();
}
