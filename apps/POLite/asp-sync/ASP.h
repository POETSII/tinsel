// SPDX-License-Identifier: BSD-2-Clause
// Multi-source shortest paths on an unweighted graph
// Can be used to compute an estimate of the average shortest path

#ifndef _ASP_H_
#define _ASP_H_

#define POLITE_DUMP_STATS
#define POLITE_COUNT_MSGS

// Synch frontend
#include <POLite.h>

// Number of sources = N * 32
#define N 12

struct ASPMessage {
  // Sum of path lengths reaching sender
  uint32_t sum;
  // Bit vector of nodes reaching sender
  uint32_t reaching[N];
};

struct ASPState {
  // Sum of lengths of all paths reaching this device
  uint32_t sum;
  // Bit vector of nodes reaching this device
  uint32_t reaching[N];
  // Bit vector of new nodes reaching this device
  uint32_t newReaching[N];
};

struct ASPDevice : PDevice<ASPState, None, ASPMessage> {
  // Called once by POLite at start of execution
  inline void init() {
    *readyToSend = Pin(0);
  }

  // Send handler
  inline void send(volatile ASPMessage* msg) {
    for (uint32_t i = 0; i < N; i++)
       msg->reaching[i] = s->reaching[i];
    *readyToSend = No;
  }

  // Receive handler
  inline void recv(ASPMessage* msg, None* edge) {
    for (uint32_t i = 0; i < N; i++)
      s->newReaching[i] |= msg->reaching[i];
  }

  // Called by POLite on idle event
  inline bool step() {
    // Fold in new reaching nodes
    bool changed = false;
    for (uint32_t i = 0; i < N; i++) {
      uint32_t rs = s->reaching[i];
      uint32_t nrs = s->newReaching[i];
      s->reaching[i] = rs | nrs;
      uint32_t bits = nrs & ~rs;
      changed = changed || bits != 0;
      while (bits != 0) {
        s->sum += time+1;
        bits = bits & (bits-1);
      }
      s->newReaching[i] = 0;
    }
    // Start new time step?
    if (changed) {
      *readyToSend = Pin(0);
      return true;
    }
    else {
      *readyToSend = No;
      return false;
    }
  }

  // Optionally send message to host on termination
  inline bool finish(volatile ASPMessage* msg) {
    msg->sum = s->sum;
    return true;
  }

};

#endif
