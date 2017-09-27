#ifndef _HEAT_H_
#define _HEAT_H_

#include "Polite.h"

struct HeatMessage : PMessage {
  uint32_t t;
  uint32_t val;
  PDeviceId from;
};

struct HeatDevice : PDevice {
  uint32_t val, acc, accNext, t;
  uint8_t sent, received, receivedNext;
  bool isConstant;

  inline void init() {
    readyToSend = 1;
    dest = outEdge(0);
  }

  inline void step() {
    t--;
    if (!isConstant) val = acc >> 2;
    acc = accNext;
    received = receivedNext;
    accNext = receivedNext = 0;
    readyToSend = 1;
  }

  inline void send(HeatMessage* msg) {
    msg->t = t;
    msg->val = val;
    if (t == 0) {
      msg->from = thisDeviceId();
      readyToSend = 0;
    }
    else {
      sent++;
      if (sent == fanOut) {
        sent = 0;
        if (received == fanIn)
          step();
        else
          readyToSend = 0;
      }
      dest = t == 0 ? hostDeviceId() : outEdge(sent);
    }
  }

  inline void recv(HeatMessage* msg) {
    if (msg->t == t) {
      acc += msg->val;
      received++;
      if (received == fanIn && !readyToSend) step();
    }
    else {
      accNext += msg->val;
      receivedNext++;
    }
  }
};

#endif
