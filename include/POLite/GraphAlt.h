// SPDX-License-Identifier: BSD-2-Clause
#ifndef _GRAPH_ALT_H_
#define _GRAPH_ALT_H_

#include <stdint.h>
#include <assert.h>
#include <POLite/Seq.h>

#include <algorithm>
#include <utility>

typedef uint32_t NodeId;
typedef int32_t PinId;
typedef uint32_t NodeLabel;

struct GraphAlt {
  struct OutEdge{
    uint32_t dst : 27;
    uint32_t pin : 5;
  };

  struct Node
  {
    NodeLabel label;
    int32_t max_pin = 0;
    Seq<NodeId> incoming;
    Seq<OutEdge> outgoing;
  };

  Seq<Node> nodes;

  // Constructor
  GraphAlt()
    : nodes(4096)
  {
  }

  // Deconstructor
  ~GraphAlt() {
  }

  // Add new node
  NodeId newNode() {
    const uint32_t initialCapacity = 8;
    Node n;
    n.label=nodes.numElems;
    nodes.append(std::move(n));
    return nodes.numElems-1;
  }

  // Set node label
  void setLabel(NodeId id, NodeLabel lab) {
    assert(id < nodes.numElems);
    nodes[id].label=lab;
  }

  // Add edge using given output pin
  void addEdge(NodeId x, PinId p, NodeId y)
  {
    nodes[x].outgoing.append({y,p});
    nodes[x].max_pin=std::max(nodes[x].max_pin, p);
    nodes[y].incoming.append(x);
  }

  // Add edge using output pin 0
  void addEdge(NodeId x, NodeId y) {
    addEdge(x, 0, y);
  }

  // Determine max pin used by given node
  // (Returns -1 if node has no outgoing edges)
  PinId maxPin(NodeId x) {
   /* int max = -1;
    for (uint32_t i = 0; i < pins->elems[x]->numElems; i++) {
      if (pins->elems[x]->elems[i] > max)
        max = pins->elems[x]->elems[i];
    }
    return max;
    */
   return nodes[x].max_pin;
  }

  // Determine fan-in of given node
  uint32_t fanIn(NodeId id) {
    return nodes[id].incoming.numElems;
  }

  // Determine fan-out of given node
  uint32_t fanOut(NodeId id) {
    return nodes[id].outgoing.numElems;
  }

};

#endif
