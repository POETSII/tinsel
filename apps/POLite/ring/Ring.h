#ifndef _HEAT_H_
#define _HEAT_H_

#include <POLite.h>

struct RingMessage {
  // Empty
};

struct RingState {
  // Is this the root device in the ring?
  uint8_t root;
  // How many messages have we received?
  uint32_t received;
  // How many have we sent?
  uint32_t sent;
  // How many messages should root receive before signaling termination?
  uint32_t stopCount;
};

struct RingDevice : PDevice<RingState, None, RingMessage> {

  // Called once by POLite at start of execution
  void init() {
    *readyToSend = s->received > s->sent ? Pin(0) : No;
  }

  // Send handler
  inline void send(RingMessage* msg) {
    s->sent++;
    *readyToSend = s->received > s->sent ? Pin(0) : No;
  }

  // Receive handler
  inline void recv(RingMessage* msg, None* edge) {
    s->received++;
    // Check termination condition
    if (s->root && s->received == s->stopCount)
      *readyToSend = HostPin;
    else
      *readyToSend = s->received > s->sent ? Pin(0) : No;
  }

  // Called by POLite when system becomes idle
  void idle(bool stable) { return; }
};

#endif
