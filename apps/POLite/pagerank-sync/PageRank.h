#ifndef _PAGERANK_H_
#define _PAGERANK_H_

#include <POLite.h>

#define ITER 6

struct PageRankMessage : PMessage {
  // the page rank score to pass on 
  float val;
};

struct ALIGNED PageRankDevice : PDevice {
   // the final score for this vertex on this iteration
   float score;
   // the accumulator used to sum in scores from incoming messages
   float sum;
   // used ot keep track of the current iteration
   uint8_t iter;
   // the total number of vertices in the graph
   uint32_t num_vertices;
   // the fanOut for this vertex
   uint16_t fanOut; 

  // Called once by POLite at start of execution
  inline void init() { 
    score = 1.0/num_vertices;
    sum = 0.0;
    iter = 0;
    readyToSend = PIN(0);
  }

  // Called by POLite when system becomes idle
  inline void idle() {
    // calculate the score for this iter
    score = 0.15/num_vertices + 0.85*sum;
    // clear the accumulator
    sum = 0.0;
    if(iter < ITER) {
      iter++;
      readyToSend = PIN(0);
    } else if (iter == ITER) {
      iter++;
      readyToSend = HOST_PIN;
    } else {
      readyToSend = NONE;
    }
  }

  // Send handler
  inline void send(PageRankMessage* msg) {
	  if(iter==(ITER+1)) {
             msg->val = score;
	  } else {
	      msg->val = score/fanOut;
	  }
	  readyToSend=NONE;
  }

  // Receive handler
  inline void recv(PageRankMessage* msg) {
    sum += msg->val;
  }
};

#endif /* _PAGERANK_H */
