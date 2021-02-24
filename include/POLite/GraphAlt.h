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

  // Creates n new nodes with contiguous ids, and returns the id of the first one
  NodeId newNodes(uint32_t n) {
    NodeId base=nodes.numElems;
    nodes.extendBy(n);
    for(NodeId i=base; i<base+n; i++){
      nodes[i].label=i;
    }
    return base;
  }

  // Set node label
  void setLabel(NodeId id, NodeLabel lab) {
    assert(id < nodes.numElems);
    nodes[id].label=lab;
  }

  // Set node label
  NodeLabel getLabel(NodeId id) {
    assert(id < nodes.numElems);
    return nodes[id].label;
  }

  // Add edge using given output pin
  void addEdge(NodeId x, PinId p, NodeId y)
  {
    assert(p>=0); // Not completely sure why PinId is signed; assume it is positive
    nodes[x].outgoing.append({y,unsigned(p)});
    nodes[x].max_pin=std::max(nodes[x].max_pin, p);
    nodes[y].incoming.append(x);
  }

  // Add edge using output pin 0
  void addEdge(NodeId x, NodeId y) {
    addEdge(x, 0, y);
  }

  void reserveOutgoingEdgeSpace(NodeId from, PinId pin, size_t n)
  {
    nodes[from].outgoing.ensureSpaceFor(n);
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

  unsigned nodeCount() const
  {
    return nodes.numElems;
  }

  // Determine fan-in of given node
  uint32_t fanIn(NodeId id) const {
    return nodes[id].incoming.numElems;
  }

  // Determine fan-out of given node
  uint32_t fanOut(NodeId id) const {
    return nodes[id].outgoing.numElems;
  }

  template<class TId>
  void exportOutgoingNodeIds(NodeId id, uint32_t n, TId *dst) const
  {
    const Node &node = nodes[id];
    assert(n==node.outgoing.size());
    for(unsigned i=0; i<n; i++){
      dst[i] = node.outgoing[i].dst;
    }
  }

  template<class TId>
  void exportIncomingNodeIds(NodeId id, uint32_t n, TId *dst) const
  {
    const Node &node = nodes[id];
    assert(n==node.incoming.size());
    std::copy(node.incoming.begin(), node.incoming.end(), dst);
  }

  template<class TCb>
  void walkOutgoingNodeIds(NodeId id, const TCb &cb) const
  {
    const Node &node = nodes[id];
    for(unsigned i=0; i<node.outgoing.size(); i++){
      cb(node.outgoing[i].dst);
    }
  }

  template<class TCb>
  void walkOutgoingNodeIdsAndPins(NodeId id, const TCb &cb) const
  {
    const Node &node = nodes[id];
    for(unsigned i=0; i<node.outgoing.size(); i++){
      cb(node.outgoing[i].dst, node.outgoing[i].pin);
    }
  }

  template<class TCb>
  void walkOutgoingNodeIdsForPin(NodeId id, PinId pin, const TCb &cb) const
  {
    const Node &node = nodes[id];
    for(unsigned i=0; i<node.outgoing.size(); i++){
      if(node.outgoing[i].pin==pin){
        cb(node.outgoing[i].dst);
      }
    }
  }

  template<class TCb>
  void walkIncomingNodeIds(NodeId id, const TCb &cb) const
  {
    const Node &node = nodes[id];
    for(unsigned i=0; i<node.incoming.size(); i++){
      cb(node.incoming[i]);
    }
  }

};

#endif
