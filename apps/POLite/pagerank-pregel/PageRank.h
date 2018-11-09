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


//using PageRankMessage = float;


using ScoreType = int;

struct PageRankMessage {
  // Time step
  uint16_t t;
  // Page rank score for sender at time step t
  ScoreType val;
};

struct PageRankState : public PregelState<PageRankMessage> {
  ScoreType val;
  
  // // Current time step of device
  // uint16_t t;
  // // Count messages sent and received
  // uint16_t sent;
  // uint16_t received, receivedNext;
  // // Current temperature of device
  // float acc, accNext;
  // // Score for the current timestep
  // float score;
  // // Fan-in and fan-out for this vertex
  // uint16_t fanIn, fanOut;
  // // Total number of vertices in the graph
  // uint32_t numVertices;

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

struct PageRankDevice : public PregelVertex<PageRankState, None, PageRankMessage> {
  using ThreadType = PregelPThread<PageRankDevice>;

  void Halt() {
    printf("Halting\n");
  }

  bool PreCompute(PageRankMessage * msg) {
    if (superstep() >= 1) {
      ScoreType pre = GetValue().val;
      MutableValue()->val += msg->val; // 0.15 / NumVertices() + 0.85 *
      printf("PreCompute ss=%x pre=%x val=%x\n", superstep(), pre, GetValue().val);

    }
    return true;
  }

  void Compute() {
    printf("Computing ss=%x val=%x\n", superstep(), GetValue().val);

    // if (superstep() >= 1) {
    //   ScoreType sum = 0;
    //   for(auto it = s->incoming_begin(); it != s->incoming_end(); ++it) {
    //     printf("Receiving message ss=%x val=%x\n", superstep(), it->val);    
    //     sum += it->val;
    //   }
    //   MutableValue()->val += sum; // 0.15 / NumVertices() + 0.85 *
    // }

    if (superstep() < 2) {
      //const int32_t n = 1; // TODO GetOutEdgeIterator().size();
      PageRankMessage m;
      m.val = GetValue().val;
      SendMessageToAllNeighbors(m);
      printf("SendMessageToAllNeighbors ss=%x val=%x\n", superstep(), (int)m.val);


    } else {
      VoteToHalt();
    }
  }
};