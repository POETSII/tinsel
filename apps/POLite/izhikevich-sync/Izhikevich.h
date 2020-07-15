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
  float u, v, I;
  uint32_t spikeCount;
  // Neuron properties
  float a, b, c, d, Ir;
};

// Edge weight type
typedef float Weight;

// Message type
struct IzhikevichMsg {
  // Number of times sender has spiked
  uint32_t spikeCount;
};

// Vertex behaviour
struct IzhikevichDevice : PDevice<IzhikevichState,Weight,IzhikevichMsg> {
  inline void init() {
    s->v = -65.0f;
    s->u = s->b * s->v;
    s->I = s->Ir * grng(s->rng);
    *readyToSend = No;
  }
  inline void send(IzhikevichMsg* msg) {
    s->spikeCount++;
    msg->spikeCount = s->spikeCount;
    *readyToSend = No;
  }
  inline void recv(IzhikevichMsg* msg, Weight* weight) {
    s->I += *weight;
  }
  inline bool step() {
    float &v = s->v;
    float &u = s->u;
    float &I = s->I;
    v = v+0.5*(0.04*v*v+5*v+140-u+I); // Step 0.5 ms
    v = v+0.5*(0.04*v*v+5*v+140-u+I); // for numerical
    u = u + s->a*(s->b*v-u);          // stability
    if (v >= 30.0) {
      v = s->c;
      u += s->d;
      *readyToSend = Pin(0);
    }
    s->I = s->Ir * grng(s->rng);
    return (time < NUM_STEPS);
  }
  inline bool finish(IzhikevichMsg* msg) {
    msg->spikeCount = s->spikeCount;
    return true;
  }
};

#endif
