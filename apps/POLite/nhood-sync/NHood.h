// SPDX-License-Identifier: BSD-2-Clause

// Regression test: on each step, every device sends to its 26 3D neighbours

#ifndef _NHOOD_H_
#define _NHOOD_H_

#include <POLite.h>

struct NHoodMessage {
  uint32_t timestep; // the timestep this message is from
};

struct NHoodState {
  uint32_t timestep; // the current timestep that we are on
  uint32_t test_length;
};

struct NHoodDevice : PDevice<NHoodState, None, NHoodMessage> {

	inline void init() {
	  *readyToSend = Pin(0);
    s->timestep = 0;
	}

	inline bool step() {
    *readyToSend = No;
 	  s->timestep++;
    if(s->timestep >= s->test_length) return false;
    *readyToSend = Pin(0);
    return true;
	}

	inline void send(volatile NHoodMessage *msg){
    *readyToSend = No;
	  return;
	}

	inline void recv(NHoodMessage *msg, None* edge) {
	}

	inline bool finish(volatile NHoodMessage* msg) {
    return true;
  }

};

#endif
