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

template<class TEdgeWeight>
struct OutEdge{
  uint32_t dst : 27;
  uint32_t pin : 5;
private:
  TEdgeWeight weight;
public:
  OutEdge(uint32_t _dst, uint32_t _pin, const TEdgeWeight &e)
    : dst(_dst)
    , pin(_pin)
    , weight(e)
  {}

  OutEdge()
  {}

  void setWeight(const TEdgeWeight &_weight)
  { weight=_weight; }

  const TEdgeWeight &getWeight() const
  { return weight; }
};

template<>
struct OutEdge<None>{
  uint32_t dst : 27;
  uint32_t pin : 5;

  OutEdge()
  {}

  OutEdge(uint32_t _dst, uint32_t _pin, const None &)
    : dst(_dst)
    , pin(_pin)
  {}

  void setWeight(const None &)
  { }

  const None &getWeight() const
  {
    const static None nothing;
    return nothing;
  }
};


template<class TEdgeWeight>
struct GraphAltNode
{
  NodeLabel label;
  int32_t max_pin = 0;
  uint32_t weight = 1;
  Seq<NodeId> incoming;
  Seq<OutEdge<TEdgeWeight>> outgoing;
};


template<class TEdgeWeight>
struct GraphAlt {

  using Node = GraphAltNode<TEdgeWeight>;
  

  Seq<Node> nodes;

private:
  size_t m_edgeCount=0;
  size_t m_maxFanIn=0;
  size_t m_maxFanOut=0;
  bool m_has_weights=false;
public:

  size_t getEdgeCount() const
  { return m_edgeCount; }

  size_t getMaxFanIn() const
  { return m_maxFanIn; }

  size_t getMaxFanOut() const
  { return m_maxFanOut; }

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
    assert(id < (unsigned)nodes.numElems);
    nodes[id].label=lab;
  }

  // Set node label
  NodeLabel getLabel(NodeId id) {
    assert(id < nodes.numElems);
    return nodes[id].label;
  }

  void setWeight(NodeId id, unsigned weight) {
    assert(id < (unsigned)nodes.numElems);
    m_has_weights=true;
    nodes[id].weight=weight;
  }

  unsigned getWeight(NodeId id) const
  {
    return nodes.elems[id].weight;
  }

  bool hasWeights() const
  { return m_has_weights; }

private:
  template<bool DoLockDst>
  void addEdgeImpl(const TEdgeWeight &e, NodeId x, PinId p, NodeId y)
  {
    assert(p>=0); // Not completely sure why PinId is signed; assume it is positive
    unsigned fanOut = nodes[x].outgoing.append({y,unsigned(p),e});
    nodes[x].max_pin=std::max(nodes[x].max_pin, p);
    unsigned fanIn = nodes[y].incoming.template append_locked<DoLockDst>(x);
    ++m_edgeCount;

    m_maxFanOut=std::max<size_t>(m_maxFanOut, fanOut);
    m_maxFanIn=std::max<size_t>(m_maxFanIn, fanIn);
  }
public:

  // Add edge using given output pin
  // Cannot be called in parallel with any method
  void addEdge(const TEdgeWeight &e, NodeId x, PinId p, NodeId y)
  {
    addEdgeImpl<false>(e, x, p, y);
  }

  // Add edge using output pin 0
  // Cannot be called in parallel with any method
  void addEdge(NodeId x, NodeId y) {
    TEdgeWeight e;
    addEdge(e, x, 0, y);
  }

  // Add edge in parallel, as long as no two threads use the same source. Any
  // number of threads can use the same destination
  void addEdgeLockedDst(const TEdgeWeight &e, NodeId x, PinId p, NodeId y)
  {
    addEdgeImpl<true>(e, x,p,y);
  }

  // Can be called in parallel with other methods using a different source node
  void reserveOutgoingEdgeSpace(NodeId from, PinId pin, size_t n)
  {
    nodes[from].outgoing.ensureSpaceFor(n);
  }

  // Determine max pin used by given node
  // (Returns -1 if node has no outgoing edges)
  PinId maxPin(NodeId x) const {
   return nodes[x].max_pin;
  }

  unsigned nodeCount() const
  {
    return nodes.numElems;
  }

  unsigned numVertices() const
  { return nodeCount(); }

  // Determine fan-in of given node
  uint32_t fanIn(NodeId id) const {
    return nodes[id].incoming.numElems;
  }

  // Determine fan-out of given node
  uint32_t fanOut(NodeId id) const {
    return nodes[id].outgoing.numElems;
  }

  const TEdgeWeight &getEdgeWeight(NodeId id, unsigned index) const
  { return nodes[id].outgoing[index].getWeight(); }

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
