// SPDX-License-Identifier: BSD-2-Clause
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
  // Number of incoming connections
  uint16_t fanIn;
  // Device id
  uint32_t id;
  // Current time step of device
  uint32_t time;
  // Current temperature of device
  uint32_t val;
  // Accumulator for temperatures received at times t and t+1
  uint32_t acc, accNext;
  // Count messages sent and received
  uint8_t sent, received, receivedNext;
  // Is the temperature of this device constant?
  bool isConstant;
};

struct HeatDevice : PDevice<HeatState, None, HeatMessage> {

  // Called once by POLite at start of execution
  void init() {
    *readyToSend = Pin(0);
  }

  // We call this on every state change
  inline void change() {
    // Execution complete?
    if (s->time == 0) return;

    // Proceed to next time step?
    if (s->sent && s->received == s->fanIn) {
      s->time--;
      if (!s->isConstant) s->val = s->acc >> 2;
      s->acc = s->accNext;
      s->received = s->receivedNext;
      s->accNext = s->receivedNext = 0;
      s->sent = 0;
      *readyToSend = s->time == 0 ? No : Pin(0);
    }
  }

  // Send handler
  inline void send(volatile HeatMessage* msg) {
    msg->time = s->time;
    msg->val = s->val;
    msg->from = s->id;
    s->sent = 1;
    *readyToSend = No;
    change();
  }

  // Receive handler
  inline void recv(HeatMessage* msg, None* edge) {
    if (msg->time == s->time) {
      // Receive temperature for this time step
      s->acc += msg->val;
      s->received++;
      change();
    }
    else {
      // Receive temperature for next time step
      s->accNext += msg->val;
      s->receivedNext++;
    }
  }

  // Called by POLite when system becomes idle
  inline bool step() { *readyToSend = No; return false; }

  // Optionally send message to host on termination
  inline bool finish(volatile HeatMessage* msg) {
    msg->val = s->val;
    msg->from = s->id;
    return true;
  }
};

#endif
