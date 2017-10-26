#ifndef _SORTER_H_
#define _SORTER_H_

// Lightweight POETS frontend
#include <POLite.h>

// There are three kinds of device
#define TWO_SORTER 0
#define SOURCE     1
#define SINK       2

struct TwoSorterMsg : PMessage {
  // Is this an synchronisation message;
  bool sync;
  // Time step of sender or id of sender
  union { uint32_t time; uint32_t id; };
  // Value being sent
  uint32_t val;
};

struct TwoSorterDevice : PDevice {
  // Device kind
  uint8_t kind;
  // Number of values sent and received
  uint8_t sent, received, receivedNext;
  // Number of synchronisation messages received
  uint8_t syncsReceived, syncsReceivedNext;
  // Total number of edges (fanIn + fanOut)
  uint8_t numEdges;
  // Current time step
  uint32_t time;
  // Values being sent for current time step
  uint32_t vals[2];
  // Values received for current time step
  uint32_t got[2];
  // Values received for next time step
  uint32_t gotNext[2];
  // Id
  uint32_t id;

  // Called once by POLite at start of execution
  void init() {
    numEdges = fanIn + fanOut;
    readyToSend = 1;
    dest = edge(0);
  }

  // We call this on every state change
  void step() {
    if (time == 0) {
      readyToSend = 0;
    }
    else {
      if (sent < numEdges) {
        readyToSend = 1;
        dest = edge(sent);
      }
      else {
        readyToSend = 0;
        if (received == fanIn && syncsReceived == fanOut) {
          // Proceed to next time step
          time--;
          // Update values
          if (kind == SINK) {
            vals[0] = got[0];
          }
          else if (kind == TWO_SORTER) {
            if (got[0] < got[1]) {
              vals[0] = got[0];
              vals[1] = got[1];
            }
            else {
              vals[0] = got[1];
              vals[1] = got[0];
            }
          }
          got[0] = gotNext[0];
          got[1] = gotNext[1];
          // Update counters
          received = receivedNext;
          receivedNext = 0;
          syncsReceived = syncsReceivedNext;
          syncsReceivedNext = 0;
          // Fresh round of sends
          sent = 0;
          readyToSend = 1;
          if (kind == SINK && time == 0)
            dest = hostDeviceId();
          else
            dest = edge(0);
        }
      }
    }
  }

  // Send handler
  inline void send(TwoSorterMsg* msg) {
    if (time == 0)
      msg->id = id;
    else
      msg->time = time;
    msg->val = vals[sent];
    msg->sync = sent >= fanOut;
    sent++;
    step();
  }

  // Receive handler
  inline void recv(TwoSorterMsg* msg) {
    if (msg->time == time) {
      if (msg->sync)
        syncsReceived++;
      else {        
        got[received] = msg->val;
        received++;
      }
      step();
    }
    else {
      if (msg->sync)
        syncsReceivedNext++;
      else {
        gotNext[receivedNext] = msg->val;
        receivedNext++;
      }
    }
  }
};

#endif
