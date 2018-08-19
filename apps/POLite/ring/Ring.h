#ifndef _HEAT_H_
#define _HEAT_H_

#include <POLite.h>

struct RingMessage : PMessage {
  // Empty
};

struct RingDevice : PDevice {
  // Is this the root device in the ring?
  uint8_t root;
  // How many messages have we received?
  uint32_t received;
  // How many have we sent?
  uint32_t sent;
  // How many messages should root receive before signaling termination?
  uint32_t stopCount;

  // Called once by POLite at start of execution
  void init() {
    readyToSend = received > sent ? PIN(0) : NONE;
  }

  // Send handler
  inline void send(RingMessage* msg) {
    sent++;
    readyToSend = received > sent ? PIN(0) : NONE;
  }

  // Receive handler
  inline void recv(RingMessage* msg) {
    received++;
    // Check termination condition
    if (root && received == stopCount)
      readyToSend = HOST_PIN;
    else
      readyToSend = received > sent ? PIN(0) : NONE;
  }
};

#endif
