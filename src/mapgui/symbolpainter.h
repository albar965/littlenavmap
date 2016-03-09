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

  QColor& colorForAirport(const MapAirport& ap);

private:
  QColor airportSymbolFillColor, toweredAirportColor, unToweredAirportColor, airportEmptyColor;

};

#endif // SYMBOLPAINTER_H
