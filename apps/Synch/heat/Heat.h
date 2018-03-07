#ifndef _HEAT_H_
#define _HEAT_H_

#include <Synch.h>

// 2D Heat transfer, with two kinds of device:
// Kind 0: temperature may change on each time step
// Kind 1: temperature remains constant
#define CONST 1

struct HeatMessage : PMessage {
  // Temperature at sender
  uint32_t val;
};

struct HeatDevice {
  // Current temperature of device
  uint32_t val;

  // Called before simulation starts
  inline void init(PDeviceInfo<HeatDevice>* info) {
  }

  // Called at beginning of each time step
  inline void begin(PDeviceInfo<HeatDevice>* info) {
    if (info->kind != CONST) val = 0;
  }

  // Called at end of each time step
  inline void end(PDeviceInfo<HeatDevice>* info, HeatDevice* prev) {
    if (info->kind== CONST)
      val = prev->val;
    else
      val = val >> 2;
  }

  // Called for each message to be sent
  inline void send(PDeviceInfo<HeatDevice>* info, PinId pin,
                   uint16_t chunk, volatile HeatMessage* msg) {
    msg->val = val;
  }

  // Called for each message received
  inline void recv(PDeviceInfo<HeatDevice>* info,
                   volatile HeatMessage* msg) {
    val += msg->val;
  }

  // Signal completion
  inline bool done(PDeviceInfo<HeatDevice>* info) {
    return info->time == 32;
  }

  // Called on when execution finished
  // (A single message is sent to the host)
  inline void output(PDeviceInfo<HeatDevice>* info,
                     volatile HeatMessage* msg) {
    msg->val = val;
  }
};

#endif
