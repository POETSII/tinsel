#ifndef _PAGERANK_H_
#define _PAGERANK_H_

#include <POLite.h>

#define NUM_ITERATIONS 6

struct PageRankMessage {
  // Page rank score to pass on 
  float val;
};

struct PageRankState {
   // Final score for this vertex on this iteration
   float score;
   // Accumulator used to sum in scores from incoming messages
   float sum;
   // Total number of vertices in the graph
   uint32_t numVertices;
   // Fan-out for this vertex
   uint16_t fanOut; 
   // Current iteration
   uint8_t iter;
};

struct PageRankDevice : PDevice<None, PageRankState, None, PageRankMessage> {

  // Called once by POLite at start of execution
  inline void init() { 
    s->sum = 0.0;
    s->score = 1.0 / s->numVertices;
    s->iter = 0;
    *readyToSend = Pin(0);
  }

  // Called by POLite when system becomes idle
  inline void idle() {
    // calculate the score for this iter
    s->score = 0.15/s->numVertices + 0.85*s->sum;
    // clear the accumulator
    s->sum = 0.0;
    if (s->iter < NUM_ITERATIONS) {
      s->iter++;
      *readyToSend = Pin(0);
    }
    else if (s->iter == NUM_ITERATIONS) {
      s->iter++;
      *readyToSend = HostPin;
    }
    else {
      *readyToSend = No;
    }
  }

  // Send handler
  inline void send(PageRankMessage* msg) {
    if (s->iter == (NUM_ITERATIONS+1))
      msg->val = s->score;
    else
      msg->val = s->score/s->fanOut;
    *readyToSend=No;
  }

  // Receive handler
  inline void recv(PageRankMessage* msg, None* edge) {
    s->sum += msg->val;
  }
};

#endif /* _PAGERANK_H */
