// SPDX-License-Identifier: BSD-2-Clause

// Discrete pressure simulator
// (Assumes at most 32 neighbours per device)

#ifndef _PRESSURE_H_
#define _PRESSURE_H_

#define POLITE_DUMP_STATS
#define POLITE_COUNT_MSGS
#include <POLite.h>

// Number of neighbours per device
// (Assumed to be an even number)
#define NUM_NEIGHBOURS 26

// Modes
#define SHARE 0
#define MOVE 1

struct PressureMessage {
  // Number of beads at sender
  int32_t pressure;
  // Move vector: one bit per neighbour
  uint32_t move;
};

struct PressureState {
  // Mode: sharing data with neighbours, or migrating beads?
  uint8_t mode;
  // Number of time steps left in simulation
  int32_t numSteps;
  // Number of beads at device
  int32_t pressure, newPressure;
  // One bit per neighbour
  uint32_t move;
  // State of random number generator
  uint32_t prng;
};

// Each neighbour has a differen direction
typedef uint16_t Dir;

// Given a direction, return the opposite direction
INLINE Dir opposite(Dir dir) {
  Dir newDir = (dir + NUM_NEIGHBOURS/2);
  if (newDir >= NUM_NEIGHBOURS) newDir -= NUM_NEIGHBOURS;
  return newDir;
}

// Random number generator
INLINE float rand(uint32_t* seed) {
  *seed = (*seed * 1103515245 + 12345) & 0x7fffffff;
  return ((float) *seed) / ((float) 0x7fffffff);
}

struct PressureDevice : PDevice<PressureState, Dir, PressureMessage> {

  inline void init() {
    #ifdef TINSEL
    s->prng = tinselId();
    #endif
    s->mode = SHARE;
    s->newPressure = s->pressure;
    *readyToSend = Pin(0);
  }

  inline void send(volatile PressureMessage *msg){
    msg->pressure = s->pressure;
    msg->move = s->move;
    *readyToSend = No;
  }

  inline void recv(PressureMessage *msg, Dir* dir) {
    if (s->mode == SHARE) {
      int32_t diff = s->newPressure - msg->pressure;
      if (diff > 0) {
        if (rand(&s->prng) < float(diff)/float(NUM_NEIGHBOURS)) {
          s->move |= 1 << opposite(*dir);
          s->newPressure--;
        }
      }
    }
    else if (msg->move & (1 << *dir))
      s->newPressure++;
  }

  inline bool step() {
    if (s->numSteps > 0) {
      s->pressure = s->newPressure;
      s->mode = s->mode == SHARE ? MOVE : SHARE;
      if (s->mode == SHARE) s->move = 0;
      *readyToSend = Pin(0);
      s->numSteps--;
      return true;
    }
    else {
      *readyToSend = No;
      return false;
    }
  }

  inline bool finish(volatile PressureMessage* msg) {
    msg->pressure = s->pressure;
    return true;
  }
};

#endif
