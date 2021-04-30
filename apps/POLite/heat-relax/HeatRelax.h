// SPDX-License-Identifier: BSD-2-Clause
#ifndef _HEAT_RELAX_H_
#define _HEAT_RELAX_H_

#define POLITE_DUMP_STATS
#define POLITE_COUNT_MSGS
#include <POLite.h>

struct HeatMessage {
  uint16_t generation;   // Generation at sender
  uint16_t colour;
  int16_t val;
};

struct HeatState {
  uint16_t tolerance;
  bool is_fixed = false;
  uint16_t colour;

  uint16_t generation = 0;
  int16_t curr_heat = 0;
  int16_t sent_heat = 0;

  uint32_t msgs_sent=0;
  uint32_t msgs_recv=0;

  struct
  {
    uint16_t generation;
    int16_t heat;
  } ghosts[4] = {{0,0}};
};

struct HeatDevice : PDevice<HeatState, None, HeatMessage> {

  bool is_dirty() const
  {
    return abs(curr_heat - sent_heat) > s->tolerance;
  }

  bool is_after(uint16_t a, uint16_t b) const
  {
    return a < b;
  }

  // Called once by POLite at start of execution
  void init() {
    inv_degree = (int)(65535.0f/degree);

    if( is_dirty() ){
      *readyToSend = Pin(0);
    }else{
      *readyToSend = No;
    }
  }

  // Send handler
  inline void send(volatile HeatMessage* msg) {
    assert( is_dirty() );

    s->generation++;
    s->sent_heat=s->curr_heat;

    msg->colour=s->colour;
    msg->generation=s->generation;
    msg->val=s->sent_heat;

    *readyToSend = No;
  }

  // Receive handler
  inline void recv(HeatMessage* msg, None* edge) {
    auto &ghost=s->ghosts[msg->colour];
    if(is_after(ghost.generation, msg->generation)){
      ghost.generation=msg->generation;
      ghost.heat=msg->val;

      if(!s->is_fixed){
        int32_t sum=0;
        for(unsigned i=0; i<4; i++){
          sum += s->ghosts[i].heat;
        }
        s->curr_heat = sum>>2;
      }
    }

    if( is_dirty() ){
      *readyToSend = Pin(0);
    }else{
      *readyToSend = No;
    }
  }

  // Called by POLite when system becomes idle
  inline bool step() { *readyToSend = No; return false; }

  // Optionally send message to host on termination
  inline bool finish(volatile HeatMessage* msg) {
    msg->generation=s->generation;
    msg->val=s->curr_heat;
    return true;
  }
};

#endif
