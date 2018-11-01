#ifndef _PAGERANK_H_
#define _PAGERANK_H_

#include <POLite.h>

#define NUM_ITERATIONS 30

struct PageRankMessage {
  // Time step
  uint16_t t;
  // Page rank score for sender at time step t
  float val;
};

struct PageRankState {
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

struct PageRankDevice : PDevice<None, PageRankState, None, PageRankMessage> {

  // Called once by POLite at start of execution
  void init() {
    s->score = 1.0/s->numVertices;
    s->acc = 0.0;
    s->accNext = 0.0;
    s->t = 0;
    *readyToSend = Pin(0);
  }

  // We call this on every state change
  inline void step() {
    // Proceed to next time step?
    if (s->sent && s->received == s->fanIn) {
      s->score = 0.15/s->numVertices + 0.85*s->acc;
      if(s->t < NUM_ITERATIONS-1) {
        s->acc = s->accNext;
        s->received = s->receivedNext;
        s->accNext = s->receivedNext = 0;
        s->sent = 0;
        s->t++;
        *readyToSend = Pin(0);
      }
      else if (s->t == NUM_ITERATIONS-1) {
        *readyToSend = No;
        s->sent = 0;
        s->t++;
      }
      else {
        *readyToSend = No;
      }
    }
  }

  // Send handler
  inline void send(PageRankMessage* msg) {
    if (s->t > NUM_ITERATIONS)
      msg->val = s->score;
    else
      msg->val = s->score/s->fanOut;
    msg->t = s->t;
    s->sent = 1;
    *readyToSend = No;
    step();
  }

  // Receive handler
  inline void recv(PageRankMessage* msg, None* edge) {
    if (msg->t == s->t) {
      // Receive temperature for this time step
      s->acc += msg->val;
      s->received++;
      step();
    }
    else {
      // Receive temperature for next time step
      s->accNext += msg->val;
      s->receivedNext++;
    }
  }

  // Called by POLite when system becomes idle
  inline void idle(bool stable) {
    if (s->t == NUM_ITERATIONS) {
      s->t++;
      *readyToSend = HostPin;
    }
    else
      *readyToSend = No;
  }
};

#endif
