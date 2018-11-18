#ifndef _PAGERANK_H_
#define _PAGERANK_H_

#include <POLite.h>

#define NUM_ITERATIONS 30

struct PageRankMessage {
  // Page rank score to pass on 
  float val;
};

struct PageRankState {
   // Final score for this vertex on this iteration
   float score;
   // Accumulator used to sum in scores from incoming messages
   float sum;
   // Fan-out for this vertex
   uint16_t fanOut; 
   // Current iteration
   uint16_t iter;
};

struct PageRankDevice : PDevice<PageRankState, None, PageRankMessage> {

  // Called once by POLite at start of execution
  inline void init() { 
    s->sum = 0.0;
    s->score = 1.0 / numVertices;
    s->iter = 0;
    *readyToSend = Pin(0);
  }

  // Called by POLite when system becomes idle
  inline void idle() {
    // Calculate the score for this iter
    s->score = 0.15/numVertices + 0.85*s->sum;
    // Clear the accumulator
    s->sum = 0.0;
    if (s->iter < NUM_ITERATIONS) {
      s->iter++;
      *readyToSend = Pin(0);
    }
    else {
      *readyToSend = No;
    }
  }

  // Send handler
  inline void send(volatile PageRankMessage* msg) {
    msg->val = s->score/s->fanOut;
    *readyToSend=No;
  }

  // Receive handler
  inline void recv(PageRankMessage* msg, None* edge) {
    s->sum += msg->val;
  }

  // Optionally send message to host on termination
  inline bool sendToHost(volatile PageRankMessage* msg) {
    msg->val = s->score;
    return true;
  }
};

#endif
