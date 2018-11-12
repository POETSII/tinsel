#pragma once

#include <POLite.h>
#include <POLite/PThreadPregel.h>

#include <array>

// compile time settings
constexpr int DEBUG_VERBOSITY = 1;
constexpr bool USE_PRECOMPUTE = true;
constexpr bool DEVICE_DEBUG = false;

using ScoreType = float;

struct PageRankMessage {
  PageRankMessage() {}
  PageRankMessage(ScoreType v) : val(v) {}
  volatile PageRankMessage& operator=(const PageRankMessage& o) volatile {
    val = o.val;
    return *this;
  }

  // Page rank score for sender at time step t
  ScoreType val;
};

struct PageRankState : public PregelState<PageRankMessage> {
  ScoreType val;
  ScoreType sum;
  uint16_t fanOut;
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

struct PageRankDevice : public PregelVertex<PageRankState, None, PageRankMessage, PageRankDevice> {
  using ThreadType = PregelPThread<PageRankDevice>;

  uint32_t NumVertices() const {
    return this->s->numVertices;
  }
  uint16_t GetOutEdgeIteratorSize() const {
    return this->s->fanOut;
  }


  void Halt() {
    PageRankMessage m {GetValue().val};

    if constexpr (DEVICE_DEBUG) {
      printf("SendMessageToHost ss=%x val=%x\n", superstep(), m.val);
    }
    SendMessageToHost(m);
  }

  void PreComputeImpl(const PageRankMessage * msg) {
    if (superstep() >= 1) {
      MutableValue()->sum += msg->val;
      if constexpr (DEVICE_DEBUG) {
        printf("PreCompute ss=%x val=%x\n", superstep(), GetValue().val);
      }
    }
  }

  bool PreCompute(const PageRankMessage * msg) {
    if constexpr (USE_PRECOMPUTE) {
      PreComputeImpl(msg);
      return true;
    } else {
      return false;
    }
  }

  void Compute() {
    if constexpr (DEVICE_DEBUG) { 
      printf("Computing ss=%x val=%x\n", superstep(), GetValue().val);
    }

#ifdef INCOMING_STORAGE
    if constexpr (!USE_PRECOMPUTE) {
      // This could be a coroutine/generator, which would make the PreCompute unnecessary
      // It would make the step of local deliveries significantly more difficult
      for(auto it = s->incoming_begin(); it != s->incoming_end(); ++it) {
        PreComputeImpl(&(*it));
      }
    }
#endif

    if (superstep() >= 1) {
      MutableValue()->val = 0.15 / NumVertices() + 0.85 * GetValue().sum;
      MutableValue()->sum = 0;
    }

    if (superstep() < 1000) {
      PageRankMessage m {GetValue().val / GetOutEdgeIteratorSize()};
      SendMessageToAllNeighbors(m);
      
      if constexpr (DEVICE_DEBUG) {   
        printf("SendMessageToAllNeighbors ss=%x val=%x\n", superstep(), m.val);
      }
    } else {
      VoteToHalt();
    }
  }
};