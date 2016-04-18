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

#include "routefinder.h"
#include "routenetwork.h"

using nw::Node;
using atools::geo::Pos;

RouteFinder::RouteFinder(RouteNetwork *routeNetwork)
  : network(routeNetwork)
{

}

RouteFinder::~RouteFinder()
{

}

void RouteFinder::calculateRoute(const atools::geo::Pos& from, const atools::geo::Pos& to,
                                 QVector<maptypes::MapIdType>& route)
{
  Node startNode = network->addArtificialNode(from);
  Node destNode = network->addArtificialNode(to);

  openlistHeap.push(startNode, cost(startNode, destNode));
  Node curNode;

  while(!openlistHeap.isEmpty())
  {
    // Contains known nodes
    openlistHeap.pop(curNode);

    if(Pos(curNode.lonx, curNode.laty) == to)
      break;

    // Contains nodes with known shortest path
    closedlist.insert(curNode.id);

    expand(curNode, destNode);
  }

  int predId = curNode.id;
  while(predId != -1)
  {
    route.prepend({network->getNavIdForNode(curNode), toMapObjectType(curNode.type)});
    predId = pred.value(predId, -1);
  }
}

void RouteFinder::expand(const nw::Node& currentNode, const nw::Node& destNode)
{
  QList<Node> successors;

  network->getNeighbours(currentNode, successors);

  for(const Node& successor : successors)
  {
    if(closedlist.contains(successor.id))
      continue;

    // g = all costs from start to current node
    float tentativeG = g.value(currentNode.id) + cost(currentNode, successor);

    if(tentativeG >= g.value(successor.id) && openlistHeap.contains(successor))
      continue;

    pred[successor.id] = currentNode.id;
    g[successor.id] = tentativeG;

    // h = estimate to destination
    // f = g+h
    float h = cost(successor, destNode);
    float f = tentativeG + h;

    if(openlistHeap.contains(successor))
      openlistHeap.change(successor, f);
    else
      openlistHeap.push(successor, f);
  }
}

float RouteFinder::cost(const nw::Node& currentNode, const nw::Node& successor)
{
  Pos from(currentNode.lonx, currentNode.laty);
  Pos to(successor.lonx, successor.laty);
  return from.distanceMeterTo(to);
}

maptypes::MapObjectTypes RouteFinder::toMapObjectType(nw::NodeType type)
{
  switch(type)
  {
    case nw::VOR:
    case nw::VORDME:
    case nw::DME:
      return maptypes::VOR;

    case nw::NDB:
      return maptypes::NDB;

    case nw::WAYPOINT:
      return maptypes::WAYPOINT;

    case nw::ARTIFICIAL:
      return maptypes::USER;

    case nw::NONE:
      break;
  }
  return maptypes::NONE;
}

/* *INDENT-OFF* */

//declare openlist as PriorityQueue with Nodes // Prioritätenwarteschlange
//declare closedlist as Set with Nodes

//program a-star
//    // Initialisierung der Open List, die Closed List ist noch leer
//    // (die Priorität bzw. der f Wert des Startknotens ist unerheblich)
//    openlist.enqueue(startknoten, 0)
//    // diese Schleife wird durchlaufen bis entweder
//    // - die optimale Lösung gefunden wurde oder
//    // - feststeht, dass keine Lösung existiert
//    repeat
//        // Knoten mit dem geringsten f Wert aus der Open List entfernen
//        currentNode := openlist.removeMin()
//        // Wurde das Ziel gefunden?
//        if currentNode == zielknoten then
//            return PathFound
//        // Der aktuelle Knoten soll durch nachfolgende Funktionen
//        // nicht weiter untersucht werden damit keine Zyklen entstehen
//        closedlist.add(currentNode)
//        // Wenn das Ziel noch nicht gefunden wurde: Nachfolgeknoten
//        // des aktuellen Knotens auf die Open List setzen
//        expandNode(currentNode)
//    until openlist.isEmpty()
//    // die Open List ist leer, es existiert kein Pfad zum Ziel
//    return NoPathFound
//end

// überprüft alle Nachfolgeknoten und fügt sie der Open List hinzu, wenn entweder
// - der Nachfolgeknoten zum ersten Mal gefunden wird oder
// - ein besserer Weg zu diesem Knoten gefunden wird
//function expandNode(currentNode)
//    foreach successor of currentNode
//        // wenn der Nachfolgeknoten bereits auf der Closed List ist – tue nichts
//        if closedlist.contains(successor) then
//            continue
//        // g Wert für den neuen Weg berechnen: g Wert des Vorgängers plus
//        // die Kosten der gerade benutzten Kante
//        tentative_g = g(currentNode) + c(currentNode, successor)
//        // wenn der Nachfolgeknoten bereits auf der Open List ist,
//        // aber der neue Weg nicht besser ist als der alte – tue nichts
//        if openlist.contains(successor) and tentative_g >= g(successor) then
//            continue
//        // Vorgängerzeiger setzen und g Wert merken
//        successor.predecessor := currentNode
//        g(successor) = tentative_g
//        // f Wert des Knotens in der Open List aktualisieren
//        // bzw. Knoten mit f Wert in die Open List einfügen
//        f := tentative_g + h(successor)
//        if openlist.contains(successor) then
//            openlist.decreaseKey(successor, f)
//        else
//            openlist.enqueue(successor, f)
//    end
//end
/* *INDENT-ON* */
