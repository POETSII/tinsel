// SPDX-License-Identifier: BSD-2-Clause
#ifndef _CLOCKTREE_H_
#define _CLOCKTREE_H_

#include <POLite.h>

// Two pins: one for ticking, one for acking
#define PIN_TICK 0
#define PIN_ACK 1

struct ClockTreeMessage {
  // Report execution time to host
  uint32_t cycleCount;
  // Count number of vertices seen
  // (To check correctness)
  uint32_t vertexCount;
};

struct ClockTreeState {
  // Is this device a root or leaf of the clock tree?
  bool isRoot;
  bool isLeaf;
  // Is this device currently waiting for an ack?
  bool waitingForAck;
  // Count of number of acks received
  uint32_t ackCount;
  // Counter number of vertices
  uint32_t vertexCount;
};

struct ClockTreeDevice : PDevice<ClockTreeState, None, ClockTreeMessage>
{
  // Called once by POLite at start of execution
  inline void init() {
    s->vertexCount = 1;
    *readyToSend = No;
  }

  // Send handler
  inline void send(volatile ClockTreeMessage* msg) {
    s->waitingForAck = !s->waitingForAck;
    msg->vertexCount = s->vertexCount;
    *readyToSend = No;
    #ifdef TINSEL
      msg->cycleCount = tinselCycleCount();
    #endif
  }

  // Receive handler
  inline void recv(ClockTreeMessage* msg, None* edge) {
    if (s->isLeaf) {
      *readyToSend = Pin(PIN_ACK);
    }
    else if (s->waitingForAck) {
      s->vertexCount += msg->vertexCount;
      s->ackCount--;
      if (s->ackCount == 0)
        *readyToSend = s->isRoot ? HostPin : Pin(PIN_ACK);
      else
        *readyToSend = No;
    }
    else
      *readyToSend = Pin(PIN_TICK);
  }

  // Called by POLite when system becomes idle
  inline bool step() {
    if (s->isRoot) {
      #ifdef TINSEL
        tinselPerfCountReset();
      #endif
      *readyToSend = Pin(PIN_TICK);
    }
    return true;
  }

  // Optionally send message to host on termination
  inline bool finish(volatile ClockTreeMessage* msg) {
    return false;
  }
};

#endif
