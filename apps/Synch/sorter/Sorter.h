#ifndef _SORTER_H_
#define _SORTER_H_

// Synch POETS frontend
#include <Synch.h>

// There are three kinds of device
#define TWO_SORTER 0
#define SOURCE     1
#define SINK       2

struct TwoSorterMsg : PMessage {
  // Id of sender
  uint32_t id;
  // Value being sent
  uint32_t val;
};

struct TwoSorterDevice {
  // Values being sent for current time step
  uint32_t vals[2];

  // Called before simulation starts
  inline void init(PDeviceInfo<TwoSorterDevice>* info) {
  }

  // Called at beginning of each time step
  inline void begin(PDeviceInfo<TwoSorterDevice>* info) {
  }

  // Called at end of each time step
  inline void end(PDeviceInfo<TwoSorterDevice>* info, TwoSorterDevice* prev) {
    if (info->kind == TWO_SORTER) {
      if (vals[0] > vals[1]) {
        uint32_t tmp = vals[0];
        vals[0] = vals[1];
        vals[1] = tmp;
      }
    }
    else if (info->kind == SOURCE)
      vals[0] = prev->vals[0];
    else
      vals[1] = prev->vals[1];
  }

  // Called for each message to be sent
  inline void send(PDeviceInfo<TwoSorterDevice>* info, PinId pin,
                   uint16_t chunk, volatile TwoSorterMsg* msg) {
    msg->val = pin == 1 ? vals[0] : vals[1];
  }

  // Called for each message received
  inline void recv(PDeviceInfo<TwoSorterDevice>* info,
                   volatile TwoSorterMsg* msg) {
    if (msg->addr.pin == 1)
      vals[0] = msg->val;
    else
      vals[1] = msg->val;
  }

  // Signal completion
  inline bool done(PDeviceInfo<TwoSorterDevice>* info) {
    return info->time == 12 && info->kind == SINK;
  }

  // Called when execution finished
  // (A single message is sent to the host)
  inline void output(PDeviceInfo<TwoSorterDevice>* info,
                     volatile TwoSorterMsg* msg) {
    msg->val = vals[0];
    msg->id = vals[1];
  }
};

#endif
