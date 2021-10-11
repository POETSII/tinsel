// SPDX-License-Identifier: BSD-2-Clause
#ifndef _GRAPH_H_
#define _GRAPH_H_

#include <stdint.h>
#include <assert.h>
#include <POLite/Seq.h>

typedef uint32_t NodeId;
typedef int32_t PinId;
typedef uint32_t NodeLabel;

struct Graph {
private:
  // Incoming and m_outgoing edges
  // Invariant: these two sequences always have equal length
  Seq<Seq<NodeId>*>* m_incoming;
  Seq<Seq<NodeId>*>* m_outgoing;

  // Each m_outgoing edge has a pin id
  // Invariant: this sequence always has the same structure as 'm_outgoing'
  Seq<Seq<PinId>*>* m_pins;

  // Each node has a label
  Seq<NodeLabel>* m_labels;

  // Optional weight for each node
  Seq<unsigned> *m_weights;

public:
  // Constructor
  Graph() {
    const uint32_t initialCapacity = 4096;
    m_incoming = new Seq<Seq<NodeId>*> (initialCapacity);
    m_outgoing = new Seq<Seq<NodeId>*> (initialCapacity);
    m_pins = new Seq<Seq<PinId>*> (initialCapacity);
    m_labels = new Seq<NodeLabel> (initialCapacity);
    m_weights = nullptr;
  }

  // Deconstructor
  ~Graph() {
    for (uint32_t i = 0; i < m_incoming->numElems; i++) {
      delete m_incoming->elems[i];
      delete m_outgoing->elems[i];
      delete m_pins->elems[i];
    }
    delete m_incoming;
    delete m_outgoing;
    delete m_pins;
    delete m_labels;
    if(m_weights){
      delete m_weights;
    }
  }

  // Add new node
  NodeId newNode() {
    const uint32_t initialCapacity = 8;
    m_incoming->append(new Seq<NodeId> (initialCapacity));
    m_outgoing->append(new Seq<NodeId> (initialCapacity));
    m_pins->append(new Seq<PinId> (initialCapacity));
    m_labels->append(m_incoming->numElems - 1);
    if(m_weights){
      m_weights->append(1.0);
    }
    return m_incoming->numElems - 1;
  }

  // Set node label
  void setLabel(NodeId id, NodeLabel lab) {
    assert(id < m_labels->numElems);
    m_labels->elems[id] = lab;
  }

  NodeLabel getLabel(NodeId id) const {
    assert(id < m_labels->numElems);
    return m_labels->elems[id];
  }

  void setWeight(NodeId id, unsigned weight) {
    assert(id < m_labels->numElems);
    if(m_weights==0){
      m_weights=new Seq<unsigned>(std::max<unsigned>(4096, m_labels->numElems));
      m_weights->numElems=m_labels->numElems;
      std::fill(m_weights->begin(), m_weights->end(), 1);
    }
    m_weights->elems[id]=weight;
  }

  unsigned getWeight(NodeId id) const
  {
    if(m_weights){
      return m_weights->elems[id];
    }else{
      return 1;
    }
  }

  bool hasWeights() const
  { return m_weights!=0; }

  // Add edge using output pin 0
  void addEdge(NodeId x, NodeId y) {
    m_outgoing->elems[x]->append(y);
    m_pins->elems[x]->append(0);
    m_incoming->elems[y]->append(x);
  }

  // Add edge using given output pin
  void addEdge(NodeId x, PinId p, NodeId y) {
    m_outgoing->elems[x]->append(y);
    m_pins->elems[x]->append(p);
    m_incoming->elems[y]->append(x);
  }

  // Determine max pin used by given node
  // (Returns -1 if node has no m_outgoing edges)
  PinId maxPin(NodeId x) {
    int max = -1;
    for (uint32_t i = 0; i < m_pins->elems[x]->numElems; i++) {
      if (m_pins->elems[x]->elems[i] > max)
        max = m_pins->elems[x]->elems[i];
    }
    return max;
  }

  unsigned numVertices() const
  { return m_incoming->numElems; }

  const Seq<Seq<NodeId>*>* incoming() const
  { return m_incoming; }

  const Seq<Seq<NodeId>*>* outgoing() const
  { return m_outgoing; }

  const Seq<Seq<PinId>*>* pins() const
  {return m_pins; }

  // Determine fan-in of given node
  uint32_t fanIn(NodeId id) {
    return m_incoming->elems[id]->numElems;
  }

  // Determine fan-out of given node
  uint32_t fanOut(NodeId id) {
    return m_outgoing->elems[id]->numElems;
  }

};

#endif
