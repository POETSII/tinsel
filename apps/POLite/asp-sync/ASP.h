// Multi-source shortest paths on an unweighted graph (64 sources)
// Can be used to compute an estimate of the average shortest path

#ifndef _ASP_H_
#define _ASP_H_

//#define POLITE_DUMP_STATS 2
//#define POLITE_COUNT_MSGS

// Lightweight POETS frontend
#include <POLite.h>

// Number of sources = N * 32
#define N 2

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
  // Time step
  uint16_t time;
  // Done flag
  uint8_t done;
};

struct ASPDevice : PDevice<ASPState, None, ASPMessage> {
  // Called once by POLite at start of execution
  void init() {
    // Setup first round of sends
    *readyToSend = Pin(0);
  }

  // Send handler
  inline void send(ASPMessage* msg) {
    msg->sum = s->sum;
    for (uint32_t i = 0; i < N; i++)
       msg->reaching[i] = s->reaching[i];
    *readyToSend = No;
  }

  // Receive handler
  inline void recv(ASPMessage* msg, None* edge) {
    for (uint32_t i = 0; i < N; i++)
      s->newReaching[i] |= msg->reaching[i];
  }

  // Called by POLite when system becomes idle
  inline void idle(bool stable) {
    if (s->done) return;
    if (stable) {
      // Send result to host
      *readyToSend = HostPin;
      s->done = true;
    }
    else {
      s->time++;
      // Fold in new reaching nodes
      bool changed = false;
      for (uint32_t i = 0; i < N; i++) {
        uint32_t rs = s->reaching[i];
        uint32_t nrs = s->newReaching[i];
        s->reaching[i] = rs | nrs;
        uint32_t bits = nrs & ~rs;
        changed = changed || bits != 0;
        while (bits != 0) {
          s->sum += s->time;
          bits = bits & (bits-1);
        }
        s->newReaching[i] = 0;
      }
      // Start new time step
      *readyToSend = changed ? Pin(0) : No;
    }
  }
};

#endif
