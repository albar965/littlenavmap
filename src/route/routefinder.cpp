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
                                 QVector<maptypes::MapObjectRef>& route)
{
  network->addStartAndDestinationNodes(from, to);
  Node startNode = network->getStartNode();
  Node destNode = network->getDestinationNode();

  if(startNode.edges.isEmpty())
    return;

  openlistHeap.push(startNode, cost(startNode, destNode));
  Node currentNode;
  bool found = false;
  while(!openlistHeap.isEmpty())
  {
    // Contains known nodes
    openlistHeap.pop(currentNode);

    if(currentNode.id == destNode.id)
    {
      found = true;
      break;
    }

    // Contains nodes with known shortest path
    closedlist.insert(currentNode.id);

    expandNode(currentNode, destNode);
  }

  if(found)
  {
    int predId = currentNode.id;
    while(predId != -1)
    {
      int navId;
      nw::NodeType type;
      network->getNavIdAndTypeForNode(predId, navId, type);

      if(type != nw::START && type != nw::DESTINATION)
        route.prepend({navId, toMapObjectType(type)});

      predId = pred.value(predId, -1);
    }
  }
}

void RouteFinder::expandNode(const nw::Node& currentNode, const nw::Node& destNode)
{
  QList<Node> successors;

  network->getNeighbours(currentNode, successors);

  for(const Node& successor : successors)
  {
    if(closedlist.contains(successor.id))
      continue;

    // g = all costs from start to current node
    float newGValue = g.value(currentNode.id) + cost(currentNode, successor);

    if(newGValue >= g.value(successor.id) && openlistHeap.contains(successor))
      continue;

    pred[successor.id] = currentNode.id;
    g[successor.id] = newGValue;

    // h = estimate to destination
    // f = g+h
    float h = cost(successor, destNode);
    float f = newGValue + h;

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

    case nw::START:
    case nw::DESTINATION:
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
