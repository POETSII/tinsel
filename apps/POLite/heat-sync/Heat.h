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
  uint32_t val, acc;
  // Is the temperature of this device constant?
  bool isConstant;

  // Called once by POLite at start of execution
  inline void init() {
    readyToSend = PIN(0);
  }

  // Called by POLite when system becomes idle
  inline void idle() {
    // Execution complete?
    if (t == 0) return;

    t--;
    if (!isConstant) val = acc >> 2;
    acc = 0;
    readyToSend = t == 0 ? HOST_PIN : PIN(0);
  }

  // Send handler
  inline void send(HeatMessage* msg) {
    msg->t = t;
    msg->val = val;
    msg->from = thisDeviceId();
    readyToSend = NONE;
  }

  // Receive handler
  inline void recv(HeatMessage* msg) {
    acc += msg->val;
  }
};

#endif
