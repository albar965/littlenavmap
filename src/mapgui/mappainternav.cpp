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

#include "mappainternav.h"
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

MapPainterNav::MapPainterNav(Marble::MarbleWidget *widget, MapQuery *mapQuery, MapScale *mapScale)
  : MapPainter(widget, mapQuery, mapScale)
{
  symbolPainter = new SymbolPainter();
}

MapPainterNav::~MapPainterNav()
{
  delete symbolPainter;
}

void MapPainterNav::paint(const MapLayer *mapLayer, Marble::GeoPainter *painter,
                          Marble::ViewportParams *viewport)
{
  if(mapLayer == nullptr)
    return;

  bool drawFast = widget->viewContext() == Marble::Animation;

  const GeoDataLatLonAltBox& curBox = viewport->viewLatLonAltBox();
  QElapsedTimer t;
  t.start();

  setRenderHints(painter);

  if(mapLayer->isWaypoint())
  {
    const QList<MapWaypoint> *waypoints = query->getWaypoints(curBox, mapLayer, drawFast);
    if(waypoints != nullptr)
    {
      if(widget->viewContext() == Marble::Still)
      {
        qDebug() << "Number of waypoints" << waypoints->size();
        qDebug() << "Time for query" << t.elapsed() << " ms";
        qDebug() << curBox.toString();
        qDebug() << *mapLayer;
        t.restart();
      }

      for(const MapWaypoint& waypoint : *waypoints)
      {
        int x, y;
        bool visible = wToS(waypoint.pos, x, y);

        if(visible)
          symbolPainter->drawWaypointSymbol(painter, waypoint, x, y,
                                            mapLayer->getWaypointSymbolSize(), drawFast);
        QStringList texts;

        if(mapLayer->isWaypointName())
          texts.append(waypoint.ident);

        x -= mapLayer->getNdbSymbolSize() / 2 + 2;
        textBox(painter, texts, QColor(Qt::magenta), x, y, false, false, true, 0);
      }
    }
  }

  if(mapLayer->isVor())
  {
    const QList<MapVor> *vors = query->getVors(curBox, mapLayer, drawFast);
    if(vors != nullptr)
    {
      if(widget->viewContext() == Marble::Still)
      {
        qDebug() << "Number of vors" << vors->size();
        qDebug() << "Time for query" << t.elapsed() << " ms";
        qDebug() << curBox.toString();
        qDebug() << *mapLayer;
        t.restart();
      }

      for(const MapVor& vor : *vors)
      {
        int x, y;
        bool visible = wToS(vor.pos, x, y);

        if(visible)
        {
          symbolPainter->drawVorSymbol(painter, vor, x, y, mapLayer->getVorSymbolSize(), drawFast);

          QStringList texts;

          if(mapLayer->isVorInfo())
          {
            QString range;
            if(vor.range < 40)
              range = "T";
            else if(vor.range < 62)
              range = "L";
            else if(vor.range < 200)
              range = "H";
            texts.append(vor.ident + " (" + range + ")");
            texts.append(QString::number(vor.frequency / 1000., 'f', 2));
          }
          else if(mapLayer->isVorIdent())
            texts.append(vor.ident);

          x -= mapLayer->getVorSymbolSize() / 2 + 2;
          textBox(painter, texts, QColor(Qt::darkBlue), x, y, false, false, true, 0);
        }
      }
    }
  }

  if(mapLayer->isNdb())
  {
    const QList<MapNdb> *ndbs = query->getNdbs(curBox, mapLayer, drawFast);
    if(ndbs != nullptr)
    {
      if(widget->viewContext() == Marble::Still)
      {
        qDebug() << "Number of ndbs" << ndbs->size();
        qDebug() << "Time for query" << t.elapsed() << " ms";
        qDebug() << curBox.toString();
        qDebug() << *mapLayer;
        t.restart();
      }

      for(const MapNdb& ndb : *ndbs)
      {
        int x, y;
        bool visible = wToS(ndb.pos, x, y);

        if(visible)
        {
          symbolPainter->drawNdbSymbol(painter, ndb, x, y, mapLayer->getNdbSymbolSize(), drawFast);

          QStringList texts;

          if(mapLayer->isNdbInfo())
          {
            QString type = ndb.type == "COMPASS_POINT" ? "CP" : ndb.type;
            texts.append(ndb.ident + " (" + type + ")");
            texts.append(QString::number(ndb.frequency / 100., 'f', 1));
          }
          else if(mapLayer->isNdbIdent())
            texts.append(ndb.ident);

          x -= mapLayer->getNdbSymbolSize() / 2 + 2;
          textBox(painter, texts, QColor(Qt::darkRed), x, y, false, false, true, 0);
        }
      }
    }
  }

  if(widget->viewContext() == Marble::Still)
    qDebug() << "Time for paint" << t.elapsed() << " ms";
}
