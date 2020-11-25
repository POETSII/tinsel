// SPDX-License-Identifier: BSD-2-Clause
#ifndef _PIPELINE_H_
#define _PIPELINE_H_

#define POLITE_DUMP_STATS
#define POLITE_COUNT_MSGS
#include <POLite.h>

// Message contents doesn't really matter for this benchmark
struct PipelineMessage {
  uint32_t data;
};

// Does the device produce, consume, or both (i.e. forward)
enum DeviceKind { Produce, Consume, Forward };

struct PipelineState {
  // Did device receive on current timestep?
  uint8_t didRecv;
  // Device kind
  DeviceKind kind;
  // Number of waves left to perform
  uint32_t numWaves;
};

struct PipelineDevice : PDevice<PipelineState, None, PipelineMessage> {

  // Called once by POLite at start of execution
  inline void init() {
    *readyToSend = (s->numWaves > 0 && s->kind == Produce) ? Pin(0) : No;
  }

  // Send handler
  inline void send(volatile PipelineMessage* msg) {
    msg->data = 42;
    if (s->kind == Produce) s->numWaves--;
    *readyToSend = No;
  }

  // Receive handler
  inline void recv(PipelineMessage* msg, None* edge) {
    s->didRecv = 1;
  }

  // Called by POLite when system becomes idle
  inline bool step() {
    if (s->didRecv) s->numWaves--;
    if (s->kind == Produce)
      *readyToSend = s->numWaves > 0 ? Pin(0) : No;
    else if (s->kind == Forward) {
      *readyToSend = s->didRecv ? Pin(0) : No;
    }
    s->didRecv = 0;
    return s->numWaves > 0;
  }

  // Optionally send message to host on termination
  inline bool finish(volatile PipelineMessage* msg) {
    return false;
  }
};

#endif
