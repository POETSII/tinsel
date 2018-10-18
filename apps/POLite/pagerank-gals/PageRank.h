#ifndef _PAGERANK_H_
#define _PAGERANK_H_

#include <POLite.h>

#define ITER 6

struct PageRankMessage : PMessage {
  // Time step
  uint32_t t;
  // the page rank score for "from" at time step "t" 
  float val;
  // Sender address
  PDeviceAddr from;
};

struct ALIGNED PageRankDevice : PDevice {
  // Current time step of device
  uint32_t t;
  // Current temperature of device
  float acc, accNext;
  // the score for the current timestep
  float score;
  // Count messages sent and received
  uint8_t sent;
  uint16_t received, receivedNext;
  // the fanOut for this vertex
  uint16_t fanOut;
  // the total number of vertices in the graph
  uint32_t num_vertices;

  // Called once by POLite at start of execution
  void init() {
    score = 1.0/num_vertices;
    acc = 0.0;
    accNext = 0.0;
    t = 0;
    readyToSend = PIN(0);
  }

  // Called by POLite when system becomes idle -- this is unused in the GALS version
  inline void idle() { return; }

  // We call this on every state change
  inline void step() {
    // Proceed to next time step?
    if (sent && received == fanIn) {
      score = 0.15/num_vertices + 0.85*acc;
      if(t < ITER-1) {
      	      acc = accNext;
      	      received = receivedNext;
      	      accNext = receivedNext = 0;
      	      sent = 0;
	      t++;
	      readyToSend = PIN(0);
      } else if (t == ITER-1) {
	      readyToSend = HOST_PIN;
	      sent = 0;
              t++;
      } else {
              readyToSend = NONE;
      }
    }
  }

  // Send handler
  inline void send(PageRankMessage* msg) {
   if(t==ITER){
     msg->val = score;
   } else {
     msg->val = score/fanOut;
   }
   msg->t = t;
   msg->from = thisDeviceId();
   sent = 1;
   if(t <= ITER) {
       readyToSend = NONE;
       step();
   }
  }

  // Receive handler
  inline void recv(PageRankMessage* msg) {
    if (msg->t == t) {
      // Receive temperature for this time step
      acc += msg->val;
      received++;
      step();
    }
    else {
      // Receive temperature for next time step
      accNext += msg->val;
      receivedNext++;
    }
  }
};

#endif /* PageRank */
