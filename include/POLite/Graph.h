#ifndef _GRAPH_H_
#define _GRAPH_H_

#include <stdint.h>
#include <assert.h>
#include <POLite/Seq.h>

typedef uint32_t NodeId;
typedef int32_t PinId;
typedef uint32_t NodeLabel;

struct Graph {
  // Incoming and outgoing edges
  // Invariant: these two sequences always have equal length
  Seq<Seq<NodeId>*>* incoming;
  Seq<Seq<NodeId>*>* outgoing;

  // Each outgoing edge has a pin id
  // Invariant: this sequence always has the same structure as 'outgoing'
  Seq<Seq<PinId>*>* pins;

  // Each node has a label
  Seq<NodeLabel>* labels;

  // Constructor
  Graph() {
    const uint32_t initialCapacity = 4096;
    incoming = new Seq<Seq<NodeId>*> (initialCapacity);
    outgoing = new Seq<Seq<NodeId>*> (initialCapacity);
    pins = new Seq<Seq<PinId>*> (initialCapacity);
    labels = new Seq<NodeLabel> (initialCapacity);
  }

  // Deconstructor
  ~Graph() {
    for (uint32_t i = 0; i < incoming->numElems; i++) {
      delete incoming->elems[i];
      delete outgoing->elems[i];
      delete pins->elems[i];
    }
    delete incoming;
    delete outgoing;
    delete pins;
    delete labels;
  }

  // Add new node
  NodeId newNode() {
    const uint32_t initialCapacity = 8;
    incoming->append(new Seq<NodeId> (initialCapacity));
    outgoing->append(new Seq<NodeId> (initialCapacity));
    pins->append(new Seq<PinId> (initialCapacity));
    labels->append(incoming->numElems - 1);
    return incoming->numElems - 1;
  }

  // Set node label
  void setLabel(NodeId id, NodeLabel lab) {
    assert(id < labels->numElems);
    labels->elems[id] = lab;
  }

  // Add edge using output pin 0
  void addEdge(NodeId x, NodeId y) {
    outgoing->elems[x]->append(y);
    pins->elems[x]->append(0);
    incoming->elems[y]->append(x);
  }

  // Add bidirectional edge using output pin 0
  void addBidirectionalEdge(NodeId x, NodeId y) {
    addEdge(x, y);
    addEdge(y, x);
  }

  // Add edge using given output pin
  void addEdge(NodeId x, PinId p, NodeId y) {
    outgoing->elems[x]->append(y);
    pins->elems[x]->append(p);
    incoming->elems[y]->append(x);
  }

  // Add bidirectional edge using given output pins
  void addBidirectionalEdge(NodeId x, PinId px, NodeId y, PinId py) {
    addEdge(x, px, y);
    addEdge(y, py, x);
  }

  // Determine max pin used by given node
  // (Returns -1 if node has no outgoing edges)
  PinId maxPin(NodeId x) {
    int max = -1;
    for (uint32_t i = 0; i < pins->elems[x]->numElems; i++) {
      if (pins->elems[x]->elems[i] > max)
        max = pins->elems[x]->elems[i];
    }
    return max;
  }

  // Determine fan-in of given node
  uint32_t fanIn(NodeId id) {
    return incoming->elems[id]->numElems;
  }

  // Determine fan-out of given node
  uint32_t fanOut(NodeId id) {
    return outgoing->elems[id]->numElems;
  }

};

#endif
