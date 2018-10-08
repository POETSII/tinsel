// Compute the average shortest path in a POETS style:
//   * Every node maintains bit vector of nodes that reach it
//   * On each time step, the bit vector is transferred to the neighbours
//   * Any newly-reaching nodes are detected with the current time
//     step representing the distances to those nodes
//   * Time is synchronised in the standard GALS fashion

#ifndef _ASP_H_
#define _ASP_H_

// Lightweight POETS frontend
#include <POLite.h>

// NUM_SOURCES*32 is the number of sources to compute ASP for
#define NUM_SOURCES 14

struct ASPMessage : PMessage {
  // Time step of sender
  uint16_t time;
  // Bit vector of nodes reaching sender
  uint32_t reaching[NUM_SOURCES];
};

struct ALIGNED ASPDevice : PDevice {
  // Current time step
  uint16_t time;
  // Completion status
  uint8_t done;
  // Has state been sent?
  uint8_t sent;
  // Number of messages received
  uint16_t received, receivedNext;
  // Number of nodes still to reach this device
  uint32_t toReach;
  // Sum of lengths of all paths reaching this device
  uint32_t sum;
  // Bit vector of nodes reaching this device at times t, t+1, and t+2
  uint32_t reaching[NUM_SOURCES];
  uint32_t reaching1[NUM_SOURCES];
  uint32_t reaching2[NUM_SOURCES];

  // Called once by POLite at start of execution
  void init() {
    // Setup first round of sends
    readyToSend = PIN(0);
  }

  // Called by POLite when system becomes idle
  inline void idle() { return; }

  // We call this on every state change
  void step() {
    // Finished execution?
    if (done) { readyToSend = done == 2 ? NONE : HOST_PIN; return; }
    // Ready to send?
    if (sent == 0)
      readyToSend = PIN(0);
    else {
      readyToSend = NONE;
      // Check for completion
      if (toReach == 0) {
        done = 1;
        readyToSend = HOST_PIN;
      }
      else if (received == fanIn) {
        // Proceed to next time step
        time++;
        // Update reaching vectors
        for (uint32_t i = 0; i < NUM_SOURCES; i++) {
          uint32_t s = reaching[i];
          uint32_t s1 = reaching1[i];
          reaching[i] = s | s1;
          uint32_t bits = s1 & ~s;
          while (bits != 0) {
            sum += time;
            toReach--;
            bits = bits & (bits-1);
          }
          reaching1[i] = reaching2[i];
          reaching2[i] = 0;
        }
        // Update counters
        received = receivedNext;
        receivedNext = 0;
        // Fresh round of sends
        sent = 0;
        readyToSend = PIN(0);
      }
    }
  }

  // Send handler
  inline void send(ASPMessage* msg) {
    if (done) {
      msg->reaching[0] = sum;
      done = 2;
    } else {
      msg->time = time;
      for (uint32_t i = 0; i < NUM_SOURCES; i++)
        msg->reaching[i] = reaching[i];
      sent = 1;
    }
    step();
  }

  // Receive handler
  inline void recv(ASPMessage* msg) {
    if (msg->time == time) {
      for (uint32_t i = 0; i < NUM_SOURCES; i++)
        reaching1[i] |= msg->reaching[i];
      received++;
      step();
    }
    else {
      for (uint32_t i = 0; i < NUM_SOURCES; i++)
        reaching2[i] |= msg->reaching[i];
      receivedNext++;
    }
  }

};

#endif
