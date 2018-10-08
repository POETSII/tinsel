#ifndef _HEAT_H_
#define _HEAT_H_

#include <POLite.h>

struct HeatMessage : PMessage {
  // Time step
  uint32_t t;
  // Temperature at sender
  uint32_t val;
  // Sender address
  PDeviceAddr from;
};

struct ALIGNED HeatDevice : PDevice {
  // Current time step of device
  uint32_t t;
  // Current temperature of device
  uint32_t val;
  // Accumulator for temperatures received at times t and t+1
  uint32_t acc, accNext;
  // Count messages sent and received
  uint8_t sent, received, receivedNext;
  // Is the temperature of this device constant?
  bool isConstant;

  // Called once by POLite at start of execution
  void init() {
    readyToSend = PIN(0);
  }

  // Called by POLite when system becomes idle
  inline void idle() { return; }

  // We call this on every state change
  inline void step() {
    // Execution complete?
    if (t == 0) return;

    // Proceed to next time step?
    if (sent && received == fanIn) {
      t--;
      if (!isConstant) val = acc >> 2;
      acc = accNext;
      received = receivedNext;
      accNext = receivedNext = 0;
      sent = 0;
      // On final time step, send to host
      readyToSend = t == 0 ? HOST_PIN : PIN(0);
    }
  }

  // Send handler
  inline void send(HeatMessage* msg) {
    msg->t = t;
    msg->val = val;
    msg->from = thisDeviceId();
    sent = 1;
    readyToSend = NONE;
    step();
  }

  // Receive handler
  inline void recv(HeatMessage* msg) {
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

#endif
