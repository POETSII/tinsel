// SPDX-License-Identifier: BSD-2-Clause
// (Based on code by David Thomas)
#ifndef _Izhikevich_H_
#define _Izhikevich_H_

#define POLITE_DUMP_STATS
#define POLITE_COUNT_MSGS
#include <POLite.h>
#include "RNG.h"

// Number of time steps to run for
#define NUM_STEPS 100

// Vertex state
struct IzhikevichState {
  // Random-number-generator state
  uint32_t rng;
  // Neuron state
  float u, v, I, acc, accNext;
  uint32_t spikeCount;
  // Protocol
  bool sent;
  uint16_t received, receivedNext, fanIn, time;
  // Neuron properties
  float a, b, c, d, Ir;
};

// Edge weight type
typedef float Weight;

// Message type
struct IzhikevichMsg {
  // Did the sender spike or not?
  bool spike;
  // Time step of sender
  uint16_t time;
  // Number of times sender has spiked
  uint32_t spikeCount;
};

// Vertex behaviour
struct IzhikevichDevice : PDevice<IzhikevichState,Weight,IzhikevichMsg> {
  inline void init() {
    s->v = -65.0f;
    s->u = s->b * s->v;
    s->I = s->Ir * grng(s->rng);
    *readyToSend = Pin(0);
  }

  // We call this on every state change
  inline void change() {
    // Execution complete?
    if (s->time == NUM_STEPS) return;

    // Proceed to next time step?
    if (s->sent && s->received == s->fanIn) {
      s->time++;
      s->I += s->acc;
      s->acc = s->accNext;
      s->accNext = 0;
      s->received = s->receivedNext;
      s->receivedNext = 0;
      s->sent = false;
      *readyToSend = s->time == (NUM_STEPS+1) ? No : Pin(0);
    }
  }

  // Send handler
  inline void send(volatile IzhikevichMsg* msg) {
    bool spike = false;
    float &v = s->v;
    float &u = s->u;
    float &I = s->I;
    v = v+0.5*(0.04*v*v+5*v+140-u+I); // Step 0.5 ms
    v = v+0.5*(0.04*v*v+5*v+140-u+I); // for numerical
    u = u + s->a*(s->b*v-u);          // stability
    if (v >= 30.0) {
      v = s->c;
      u += s->d;
      s->spikeCount++;
      spike = true;
    }
    s->I = s->Ir * grng(s->rng);
    msg->time = s->time;
    msg->spike = spike;
    msg->spikeCount = s->spikeCount;
    s->sent = true;
    *readyToSend = No;
    change();
  }

  // Receive handler
  inline void recv(IzhikevichMsg* msg, Weight* weight) {
    if (msg->time == s->time) {
      if (msg->spike) s->acc += *weight;
      s->received++;
      change();
    }
    else {
      if (msg->spike) s->accNext += *weight;
      s->receivedNext++;
    }
  }

  inline bool step() {
    return false;
  }

  inline bool finish(IzhikevichMsg* msg) {
    msg->spikeCount = s->spikeCount;
    return true;
  }
};

#endif
