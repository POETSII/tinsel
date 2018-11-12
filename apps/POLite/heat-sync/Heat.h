#ifndef _HEAT_H_
#define _HEAT_H_

#include <POLite.h>

struct HeatMessage {
  // Sender id
  uint32_t from;
  // Time step
  uint32_t time;
  // Temperature at sender
  uint32_t val;
};

struct HeatState {
  // Device id
  uint32_t id;
  // Current time step of device
  uint32_t time;
  // Current temperature of device
  uint32_t val, acc;
  // Is the temperature of this device constant?
  bool isConstant;
};

struct HeatDevice : PDevice<HeatState, None, HeatMessage> {

  // Called once by POLite at start of execution
  inline void init() {
    *readyToSend = Pin(0);
  }

  // Send handler
  inline void send(HeatMessage* msg) {
    msg->from = s->id;
    msg->time = s->time;
    msg->val = s->val;
    *readyToSend = No;
  }

  // Receive handler
  inline void recv(HeatMessage* msg, None* edge) {
    s->acc += msg->val;
  }

  // Called by POLite when system becomes idle
  inline void idle(bool stable) {
    // Execution complete?
    if (s->time == 0) return;

    s->time--;
    if (!s->isConstant) s->val = s->acc >> 2;
    s->acc = 0;
    *readyToSend = s->time == 0 ? HostPin : Pin(0);
  }
};

#endif
