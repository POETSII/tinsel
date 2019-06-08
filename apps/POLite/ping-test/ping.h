// SPDX-License-Identifier: BSD-2-Clause
// Test messaging between host and threads.

#ifndef _ping_H_
#define _ping_H_

//#define POLITE_DUMP_STATS
//#define POLITE_COUNT_MSGS

// Lightweight POETS frontend
#include <POLite.h>

struct PingMessage {
  // Number to confirm message being transferred is not corrupted
  uint32_t test;
};

struct PingState {
  // Number received to be sent back to host
  uint32_t test;
};

struct PingDevice : PDevice<PingState, None, PingMessage> {
  // Called once by POLite at start of execution
  void init() {
    // Do nothing until a message is received from the host
    *readyToSend = No;
  }

  // Receive handler
  inline void recv(PingMessage* msg, None* edge) {
    // Store number from host to send back to host
    s->test = msg->test;
    *readyToSend = HostPin;
  }

  // Send handler
  inline void send(volatile PingMessage* msg) {
    // Put received value back in message for host to check
    msg->test = s->test;
    *readyToSend = No;
  }

  // Called by POLite when system becomes idle
  inline bool step() {
    return true; // Never terminate
  }

  // Optionally send message to host on termination
  inline bool finish(volatile PingMessage* msg) {
    return false;
  }
};

#endif
