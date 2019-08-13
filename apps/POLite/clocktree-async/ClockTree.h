// SPDX-License-Identifier: BSD-2-Clause
#ifndef _CLOCKTREE_H_
#define _CLOCKTREE_H_

#define POLITE_MAX_FANOUT 32
#include <POLite.h>

// Two pins: one for ticking, one for acking
#define PIN_TICK 0
#define PIN_ACK 1

struct ClockTreeMessage {
  // Unused
  uint32_t unused;
};

struct ClockTreeState {
  // Is this device a root or leaf of the clock tree?
  bool isRoot;
  bool isLeaf;
  // Is this device currently waiting for an ack?
  bool waitingForAck;
  // Count of number of acks received
  uint32_t ackCount;
};

struct ClockTreeDevice : PDevice<ClockTreeState, None, ClockTreeMessage>
{
  // Called once by POLite at start of execution
  inline void init() {
    if (s->isRoot)
      *readyToSend = Pin(PIN_TICK);
    else
      *readyToSend = No;
  }

  // Send handler
  inline void send(volatile ClockTreeMessage* msg) {
    s->waitingForAck = !s->waitingForAck;
    *readyToSend = No;
  }

  // Receive handler
  inline void recv(ClockTreeMessage* msg, None* edge) {
    if (s->isRoot)
      *readyToSend = HostPin;
    else if (s->isLeaf)
      *readyToSend = Pin(PIN_ACK);
    else if (s->waitingForAck) {
        s->ackCount--;
        if (s->ackCount == 0)
          *readyToSend = Pin(PIN_ACK);
        else
          *readyToSend = No;
    }
    else
      *readyToSend = Pin(PIN_TICK);
  }

  // Called by POLite when system becomes idle
  inline bool step() {
    return true;
  }

  // Optionally send message to host on termination
  inline bool finish(volatile ClockTreeMessage* msg) {
    return false;
  }
};

#endif
