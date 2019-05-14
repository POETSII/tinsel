// SPDX-License-Identifier: BSD-2-Clause
#ifndef _HashMin_H_
#define _HashMin_H_

#define POLITE_DUMP_STATS
#define POLITE_COUNT_MSGS

#include <POLite.h>

// Vertex state
struct HashMinState {
  // Has min seen changed?
  bool changed;
  // The min vertex seen
  int32_t min;
};

// Vertex behaviour
struct HashMinDevice : PDevice<HashMinState,None,int32_t> {
  inline void init() {
    *readyToSend = Pin(0);
  }
  inline void send(int32_t* msg) {
    *msg = s->min;
    *readyToSend = No;
  }
  inline void recv(int32_t* min, None* weight) {
    if (*min < s->min) {
      s->min = *min;
      s->changed = true;
    }
  }
  inline bool step() {
    if (s->changed) {
      s->changed = false;
      *readyToSend = Pin(0);
      return true;
    }
    else
      return false;
  }
  inline bool finish(int32_t* msg) {
    *msg = s->min;
    return true;
  }
};

#endif
