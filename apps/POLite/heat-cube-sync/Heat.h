// SPDX-License-Identifier: BSD-2-Clause
#ifndef _HEAT_H_
#define _HEAT_H_

#define POLITE_DUMP_STATS
#define POLITE_COUNT_MSGS
#include <POLite.h>

struct HeatMessage {
  // Id of sender
  uint32_t from;
  // Temperature at sender
  float val;
};

struct HeatState {
  // Device id
  uint32_t id;
  // Current time step of device
  uint32_t time;
  // Current temperature of device
  float val, acc;
  // Number of neighbours
  float numNeighbours;
  // Is the temperature of this device constant?
  bool isConstant;
};

struct HeatDevice : PDevice<HeatState, None, HeatMessage> {

  // Called once by POLite at start of execution
  inline void init() {
    *readyToSend = Pin(0);
  }

  // Send handler
  inline void send(volatile HeatMessage* msg) {
    msg->from = s->id;
    msg->val = s->val;
    *readyToSend = No;
  }

  // Receive handler
  inline void recv(HeatMessage* msg, None* edge) {
    s->acc += msg->val;
  }

  // Called by POLite when system becomes idle
  inline bool step() {
    // Execution complete?
    if (s->time == 0) {
      *readyToSend = No;
      return false;
    }
    else {
      s->time--;
      if (!s->isConstant) s->val = s->acc / s->numNeighbours;
      s->acc = 0.0;
      *readyToSend = Pin(0);
      return true;
    }
  }

  // Optionally send message to host on termination
  inline bool finish(volatile HeatMessage* msg) {
    msg->from = s->id;
    msg->val = s->val;
    return true;
  }
};

#endif
