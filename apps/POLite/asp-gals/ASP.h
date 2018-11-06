// Compute the average shortest path in a POETS style:
//   * Every node maintains bit vector of nodes that reach it
//   * On each time step, the bit vector is transferred to the neighbours
//   * Any newly-reaching nodes are detected with the current time
//     step representing the distances to those nodes
//   * Time is synchronised in the standard GALS fashion

#ifndef _ASP_H_
#define _ASP_H_

//#define POLITE_DUMP_STATS 1
//#define POLITE_COUNT_MSGS

// Lightweight POETS frontend
#include <POLite.h>

// NUM_SOURCES*32 is the number of sources to compute ASP for
#define NUM_SOURCES 2

struct ASPMessage {
  // Time step of sender
  uint16_t time;
  // Bit vector of nodes reaching sender
  uint32_t reaching[NUM_SOURCES];
};

struct ASPState {
  // Number of incoming connections
  uint16_t fanIn;
  // Current time step
  uint16_t time;
  // Completion status
  uint8_t done;
  // Has state been sent?
  uint8_t sent;
  // Number of messages received
  uint16_t received, receivedNext;
  // Number of nodes still to reach this device
  uint32_t toReach;
  // Sum of lengths of all paths reaching this device
  uint32_t sum;
  // Bit vector of nodes reaching this device at times t, t+1, and t+2
  uint32_t reaching[NUM_SOURCES];
  uint32_t reaching1[NUM_SOURCES];
  uint32_t reaching2[NUM_SOURCES];
};

struct ASPDevice : PDevice<ASPState, None, ASPMessage> {
  // Called once by POLite at start of execution
  void init() {
    // Setup first round of sends
    *readyToSend = Pin(0);
  }

  // We call this on every state change
  void step() {
    // Finished execution?
    if (s->done) { *readyToSend = No; }
    // Ready to send?
    if (s->sent == 0)
      *readyToSend = Pin(0);
    else {
      *readyToSend = No;
      // Check for completion
      if (s->toReach == 0) {
        s->done = 1;
      }
      else if (s->received == s->fanIn) {
        // Proceed to next time step
        s->time++;
        // Update reaching vectors
        for (uint32_t i = 0; i < NUM_SOURCES; i++) {
          uint32_t bs = s->reaching[i];
          uint32_t bs1 = s->reaching1[i];
          s->reaching[i] = bs | bs1;
          uint32_t bits = bs1 & ~bs;
          while (bits != 0) {
            s->sum += s->time;
            s->toReach--;
            bits = bits & (bits-1);
          }
          s->reaching1[i] = s->reaching2[i];
          s->reaching2[i] = 0;
        }
        // Update counters
        s->received = s->receivedNext;
        s->receivedNext = 0;
        // Fresh round of sends
        s->sent = 0;
        *readyToSend = Pin(0);
      }
    }
  }

  // Send handler
  inline void send(ASPMessage* msg) {
    if (s->done) {
      msg->reaching[0] = s->sum;
    }
    else {
      msg->time = s->time;
      for (uint32_t i = 0; i < NUM_SOURCES; i++)
        msg->reaching[i] = s->reaching[i];
      s->sent = 1;
    }
    step();
  }

  // Receive handler
  inline void recv(ASPMessage* msg, None* edge) {
    if (msg->time == s->time) {
      for (uint32_t i = 0; i < NUM_SOURCES; i++)
        s->reaching1[i] |= msg->reaching[i];
      s->received++;
      step();
    }
    else {
      for (uint32_t i = 0; i < NUM_SOURCES; i++)
        s->reaching2[i] |= msg->reaching[i];
      s->receivedNext++;
    }
  }

  // Called by POLite when system becomes idle
  inline void idle(bool stable) {
    *readyToSend = s->done == 1 ? HostPin : No;
    s->done = 2;
  }
};

#endif
