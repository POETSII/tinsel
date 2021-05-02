// SPDX-License-Identifier: BSD-2-Clause
#ifndef _HEAT_RELAX_H_
#define _HEAT_RELAX_H_

#include <POLite.h>

struct HeatMessage {
  float val;
  struct {
    uint32_t colour : 2;
    uint32_t generation : 30;
  };
  // These are only used for export, but POLite doesn't give us a
  // way of changing message size.
  uint16_t x;
  uint16_t y;
};

struct HeatState {
  float scale;
  float omega;
  float tolerance;
  float min_tolerance;
  float tolerance_scale;
  uint16_t x;
  uint16_t y;
  bool is_fixed = false;
  uint16_t colour;
  

  uint16_t generation ;
  float curr_heat ;
  float sent_heat ;

  uint32_t msgs_sent=0;
  uint32_t msgs_recv=0;

  struct
  {
    float heat;
    uint16_t generation;
  } ghosts[4] = {{0,0}};
};

struct HeatDevice : PDevice<HeatState, None, HeatMessage> {

  bool is_dirty() const
  {
    bool dirty= fabsf(s->curr_heat - s->sent_heat) > s->tolerance;
    //fprintf(stderr, "%d,%d c=%f, s=%f, t=%f, dirty=%d\n", s->x, s->y, s->curr_heat, s->sent_heat, s->tolerance, dirty);
    return dirty;
  }

  bool is_after(uint16_t a, uint16_t b) const
  {
    return a < b;
  }

  // Called once by POLite at start of execution
  void init() {
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
        float sum=0;
        for(unsigned i=0; i<4; i++){
          sum += s->ghosts[i].heat;
        }
        float now = (sum*s->scale);
        s->curr_heat = (1-s->omega) * s->sent_heat + s->omega * now;
        //fprintf(stderr, "  omega=%f\n", s->omega);
      }
    }

    if( is_dirty() ){
      *readyToSend = Pin(0);
    }else{
      *readyToSend = No;
    }
  }

  // Called by POLite when system becomes idle
  inline bool step() {
    assert(!is_dirty());
    if(s->tolerance > s->min_tolerance){
      s->tolerance *= s->tolerance_scale;
      s->tolerance=std::max(s->tolerance, s->min_tolerance);
      if(s->x==0 && s->y==0){
        fprintf(stderr, "new tolerance=%g\n", s->tolerance);
      }
    }
    if(is_dirty()){
      *readyToSend = Pin(0);
      return true;
    }else{
      *readyToSend = No;
      return false;
    }
  }

  // Optionally send message to host on termination
  inline bool finish(volatile HeatMessage* msg) {
    assert(!is_dirty());
    msg->generation=s->generation;
    msg->val=s->curr_heat;
    msg->x=s->x;
    msg->y=s->y;
    return true;
  }
};

#endif
