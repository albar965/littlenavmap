#ifndef SYMBOLPAINTER_H
#define SYMBOLPAINTER_H

#include <QColor>

class QPainter;
struct MapAirport;

class SymbolPainter
{
public:
  SymbolPainter();

  void drawAirportSymbol(QPainter *painter, const MapAirport& ap, int x, int y, int size,
                         bool isAirportDiagram, bool fast);

private:
};

#endif // SYMBOLPAINTER_H
