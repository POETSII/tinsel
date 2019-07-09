// SPDX-License-Identifier: BSD-2-Clause
// Test messaging between host and threads.

#ifndef _ping_H_
#define _ping_H_

#define POLITE_DUMP_STATS
#define POLITE_COUNT_MSGS
#define POLITE_MAX_FANOUT 27
#include <POLite.h>

// Lightweight POETS frontend
#include <POLite.h>

struct PingMessage {
  // Number to confirm message being transferred is not corrupted
  uint32_t srcThread;
  uint32_t intraThreadSent;
  uint32_t intraThreadRecv;
  uint32_t interThreadSent;
  uint32_t interThreadRecv;
};

struct PingState {
  uint32_t intraThreadNeighbours;
  uint32_t intraThreadSent;
  uint32_t intraThreadRecv;
  uint32_t interThreadSent;
  uint32_t interThreadRecv;
};

struct PingDevice : PDevice<PingState, None, PingMessage> {
  // Called once by POLite at start of execution
  void init() {
    // Do nothing until a message is received from the host
    *readyToSend = Pin(0);
  }

  // Receive handler
  inline void recv(PingMessage* msg, None* edge) {
    // Store number from host to send back to host
    if (msg->srcThread == tinselId()) {
      s->intraThreadRecv++;
    } else {
      s->interThreadRecv++;
    }
  }

  // Send handler
  inline void send(volatile PingMessage* msg) {
    // Put received value back in message for host to check
    msg->srcThread = tinselId();
    s->intraThreadSent += s->intraThreadNeighbours;
    s->interThreadSent += (26 - s->intraThreadNeighbours);
    *readyToSend = No;
  }

  // Called by POLite when system becomes idle
  inline bool step() {
    return false; // Terminate
    // return true; // Do not terminate
  }

  // Send message to host on termination
  inline bool finish(volatile PingMessage* msg) {
    msg->interThreadSent = s->interThreadSent;
    msg->interThreadRecv = s->interThreadRecv;
    msg->intraThreadSent = s->intraThreadSent;
    msg->intraThreadRecv = s->intraThreadRecv;
    return true;
  }
};

#endif
