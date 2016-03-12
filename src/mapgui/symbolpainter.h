#ifndef SYMBOLPAINTER_H
#define SYMBOLPAINTER_H

#include <QColor>

class QPainter;
struct MapAirport;

struct MapWaypoint;

struct MapVor;

struct MapNdb;

class SymbolPainter
{
public:
  SymbolPainter();

  void drawAirportSymbol(QPainter *painter, const MapAirport& ap, int x, int y, int size,
                         bool isAirportDiagram, bool fast);

  void drawWaypointSymbol(QPainter *painter, const MapWaypoint& wp, int x, int y, int size, bool fast);
  void drawVorSymbol(QPainter *painter, const MapVor& vor, int x, int y, int size, bool fast, int largeSize);
  void drawNdbSymbol(QPainter *painter, const MapNdb& ndb, int x, int y, int size, bool fast);
  void drawMarkerSymbol(QPainter *painter, const MapMarker& marker, int x, int y, int size, bool fast);

private:
};

#endif // SYMBOLPAINTER_H
