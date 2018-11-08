#pragma once

#include <POLite.h>
#include <POLite/PThreadPregel.h>

#include <array>

//#define MAX_FAN_IN 16

// using RING_TYPE = float;
// enum UpdatePropagation { ONLY_TRIGGER, ALWAYS };

// compile time settings
constexpr int DEBUG_VERBOSITY = 1;
// constexpr UpdatePropagation prop = ONLY_TRIGGER;

struct PageRankMessage {
  // Time step
  uint16_t t;
  // Page rank score for sender at time step t
  float val;
};

struct PageRankState : public PregelState<PageRankMessage> {
  // Current time step of device
  uint16_t t;
  // Count messages sent and received
  uint16_t sent;
  uint16_t received, receivedNext;
  // Current temperature of device
  float acc, accNext;
  // Score for the current timestep
  float score;
  // Fan-in and fan-out for this vertex
  uint16_t fanIn, fanOut;
  // Total number of vertices in the graph
  uint32_t numVertices;
};

// class PageRankVertex : public Vertex<double, void, double> {
// public:
//   virtual void Compute(MessageIterator *msgs) {
//     if (superstep() >= 1) {
//       double sum = 0;
//       for (; !msgs->Done(); msgs->Next())
//         sum += msgs->Value();
//       *MutableValue() = 0.15 / NumVertices() + 0.85 * sum;
//     }
//     if (superstep() < 30) {
//       const int64 n = GetOutEdgeIterator().size();
//       SendMessageToAllNeighbors(GetValue() / n);
//     } else {
//       VoteToHalt();
//     }
//   }
// };

struct PageRankDevice : PDevice<None, PageRankState, None, PageRankMessage> {
  using ThreadType = PregelPThread<PageRankDevice>;

public:
  //inline void idle() {}

  inline void init() {}

  void compute() {
    for(const auto& msg : s->incoming) {

    }
  }

  
  //inline void onSendStart() {}
  //inline void onSendRestart() {}
  //inline void onSendFinished() {}
  //inline bool onTrigger(ThreadType *thread) {}
  //inline bool process(PageRankMessage *msg, ThreadType *thread) {}
};