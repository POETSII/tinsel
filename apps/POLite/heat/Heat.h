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

struct HeatDevice : PDevice {
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
    readyToSend = 1;
    dest = outEdge(0);
  }

  // We call this on every state change
  inline void step() {
    // Execution complete?
    if (t == 0) {
      readyToSend = 0;
      return;
    }
    // Ready to send?
    if (sent < fanOut) {
      readyToSend = 1;
      dest = outEdge(sent);
    }
    else {
      readyToSend = 0;
      // Proceed to next time step?
      if (received == fanIn) {
        t--;
        if (!isConstant) val = acc >> 2;
        acc = accNext;
        received = receivedNext;
        accNext = receivedNext = 0;
        sent = 0;
        readyToSend = 1;
        dest = outEdge(0);
      }
    }
    // On final time step, send to host
    if (t == 0) dest = hostDeviceId();
  }

  // Send handler
  inline void send(HeatMessage* msg) {
    msg->t = t;
    msg->val = val;
    msg->from = thisDeviceId();
    sent++;
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
