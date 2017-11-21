#ifndef _HEAT_H_
#define _HEAT_H_

#include <Synch.h>

// For 2D Heat transfer.  There are two kinds of device:
//   Kind 0: temperature may change on each time step
//   Kind 1: temperature remains constant

struct HeatMessage : PMessage {
  // Temperature at sender
  uint32_t val;
};

struct HeatDevice {
  // Current temperature of device
  uint32_t val;

  // Called at beginning of each time step
  inline void begin(PDeviceKind kind) {
    if (kind == 0) val = 0;
  }

  // Called at end of each time step
  inline void end(PDeviceKind kind, HeatDevice* prev) {
    if (kind == 0)
      val = val >> 2;
    else
      val = prev->val;
  }

  // Called for each message to be sent
  inline void send(PDeviceKind kind, PinId pin,
                   uint16_t chunk, volatile HeatMessage* msg) {
    msg->val = val;
  }

  // Called for each message received
  inline void recv(PDeviceKind kind, volatile HeatMessage* msg) {
    val += msg->val;
  }

  // Called on when execution finished
  // (A single message is sent to the host)
  inline void output(PDeviceKind kind, volatile HeatMessage* msg) {
    msg->val = val;
  }
};

#endif
