#ifndef _GRAPH_H_
#define _GRAPH_H_

#include <stdint.h>
#include <assert.h>
#include <POLite/Seq.h>

typedef uint32_t NodeId;
typedef uint32_t NodeLabel;

struct Graph {
  // Incoming and outgoing edges
  // Invariant: these two sequences always have equal length
  Seq<Seq<NodeId>*>* incoming;
  Seq<Seq<NodeId>*>* outgoing;

  // Each node has a label
  Seq<NodeLabel>* labels;

  // Constructor
  Graph() {
    const uint32_t initialCapacity = 4096;
    incoming = new Seq<Seq<NodeId>*> (initialCapacity);
    outgoing = new Seq<Seq<NodeId>*> (initialCapacity);
    labels = new Seq<NodeLabel> (initialCapacity);
  }

  // Deconstructor
  ~Graph() {
    for (uint32_t i = 0; i < incoming->numElems; i++) {
      delete incoming->elems[i];
      delete outgoing->elems[i];
    }
    delete incoming;
    delete outgoing;
    delete labels;
  }

  // Add new node
  NodeId newNode() {
    const uint32_t initialCapacity = 8;
    incoming->append(new Seq<NodeId> (initialCapacity));
    outgoing->append(new Seq<NodeId> (initialCapacity));
    labels->append(incoming->numElems - 1);
    return incoming->numElems - 1;
  }

  // Set node label
  void setLabel(NodeId id, NodeLabel lab) {
    assert(id < labels->numElems);
    labels->elems[id] = lab;
  }

  // Add edge
  void addEdge(NodeId x, NodeId y) {
    outgoing->elems[x]->append(y);
    incoming->elems[y]->append(x);
  }

  // Add bidirectional edge
  void addBidirectionalEdge(NodeId x, NodeId y) {
    addEdge(x, y);
    addEdge(y, x);
  }
};

#endif
