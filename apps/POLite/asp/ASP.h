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

// There are a max of N*32 nodes in the graph
#define N 2

// M*32 nodes are transferred in each message
#define M 1

struct ASPMessage : PMessage {
  // Offset of reaching vector (see below)
  uint16_t offset;
  // Time step of sender
  uint32_t time;
  // Bit vector of nodes reaching sender
  uint32_t reaching[M];
};

struct ASPDevice : PDevice {
  // Current time step
  uint16_t time;
  // Node id
  uint16_t id;
  // Amount of reaching vector sent so far on current time step
  uint16_t offset;
  // Number of messages sent and received
  uint16_t sent, received, receivedNext;
  // Index of next destination in edge list
  uint8_t nextDest;
  // Completion status
  uint8_t done;
  // Number of messages sent/received on each time step
  uint16_t numMsgs;
  // Number of nodes still to reach this device
  uint32_t toReach;
  // Sum of lengths of all paths reaching this device
  uint32_t sum;
  // Bit vector of nodes reaching this device at times t, t+1, and t+2
  uint32_t reaching[N];
  uint32_t reaching1[N];
  uint32_t reaching2[N];

  // Called once by POLite at start of execution
  void init() {
    // Set bit corresponding to node id handled by this device
    // (By definition, a node reaches itself)
    reaching[id >> 5] = 1 << (id & 0x1f);
    // Calculate number of messages to be sent/received in each time step
    uint32_t chunks = (N+(M-1)) / M;
    numMsgs = fanIn * chunks;
    // Setup first round of sends
    readyToSend = 1;
    dest = outEdge(this, 0);
  }

  // We call this on every state change
  void step() {
    // Finished execution?
    if (done) { readyToSend = 0; return; }
    // Ready to send?
    if (sent < numMsgs) {
      readyToSend = 1;
      dest = outEdge(this, nextDest);
    }
    else {
      readyToSend = 0;
      // Check for completion
      if (toReach == 0) {
        done = 1;
        dest = hostDeviceId();
        readyToSend = 1;
      }
      else if (received == numMsgs) {
        // Proceed to next time step
        time++;
        // Update reaching vectors
        for (uint32_t i = 0; i < N; i++) {
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
        offset = 0;
        // Fresh round of sends
        sent = 0;
        readyToSend = 1;
        dest = outEdge(this, 0);
      }
    }
  }

  // Send handler
  inline void send(ASPMessage* msg) {
    msg->time = done ? sum : time;
    msg->offset = offset;
    for (uint32_t i = 0; i < M; i++)
      msg->reaching[i] = reaching[offset+i];
    nextDest++;
    if (nextDest == fanOut) {
      nextDest = 0;
      offset += M;
    }
    sent++;
    step();
  }

  // Receive handler
  inline void recv(ASPMessage* msg) {
    if (msg->time == time) {
      for (uint32_t i = 0; i < M; i++)
        reaching1[msg->offset+i] |= msg->reaching[i];
      received++;
      step();
    }
    else {
      for (uint32_t i = 0; i < M; i++)
        reaching2[msg->offset+i] |= msg->reaching[i];
      receivedNext++;
    }
  }

};

#endif
