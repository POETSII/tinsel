// SPDX-License-Identifier: BSD-2-Clause
#ifndef _HEAT_RELAX_H_
#define _HEAT_RELAX_H_

#include <POLite.h>

#include <algorithm>

#if 1

struct heat_t
{
  int32_t v;

  volatile heat_t& operator = (const heat_t & o) volatile
  {
    v=o.v;
    return *this;
  }

  volatile heat_t& operator = (const volatile heat_t & o) volatile
  {
    v=o.v;
    return *this;
  }

  heat_t& operator = (const heat_t & o)
  {
    v=o.v;
    return *this;
  }

  heat_t& operator = (const volatile heat_t & o)
  {
    v=o.v;
    return *this;
  }

  heat_t operator+(heat_t o)const{
    return {v+o.v};
  }

  heat_t operator-(heat_t o)const{
    return {v-o.v};
  }

  heat_t operator-()const{
    return {-v};
  }

  bool operator<(heat_t o)const{
    return v<o.v;
  }

  heat_t operator*(heat_t b)
  {
    int64_t prod=v*int64_t(b.v);
    prod += (1<<15);
    return {int32_t(prod>>16)};
  }
};

constexpr heat_t to_heat(float x)
{
  return {int32_t(x*65536)};
}

constexpr float from_heat(heat_t x)
{
  return x.v * float(1/65536.0);
}



#else

using heat_t = float;

float from_heat(float x)
{ return x; }

float to_heat(float x)
{ return x; }

#endif

using generation_t = uint32_t;

struct HeatMessage {
  heat_t val;
  generation_t generation;
  // These are only used for export, but POLite doesn't give us a
  // way of changing message size.
  uint16_t x;
  uint16_t y;
  uint16_t colour;
  heat_t sent_heat;
  heat_t ghosts[4];
  uint32_t recv;
  uint32_t sent;
};

struct HeatState {
  heat_t scale;
  heat_t tolerance;
  heat_t min_tolerance;
  heat_t tolerance_scale;
  uint16_t x;
  uint16_t y;
  bool is_fixed = false;
  uint16_t colour;
  
  bool dirty_sticky=false;

  generation_t generation ;
  heat_t curr_heat ;
  heat_t sent_heat ;

  uint32_t msgs_sent=0;
  uint32_t msgs_recv=0;

  struct
  {
    heat_t heat;
    generation_t generation;
  } ghosts[4] = {{0,0}};
};

struct HeatDevice : PDevice<HeatState, None, HeatMessage> {

  void recalc_curr_heat() const
  {
    if(!s->is_fixed){
      heat_t sum{0};
      for(unsigned i=0; i<4; i++){
        sum = sum + s->ghosts[i].heat;
      }
      s->curr_heat = sum * s->scale;
    }
  }

  bool is_dirty() const
  {
    if(s->dirty_sticky){
      return true;
    }

    recalc_curr_heat();

    heat_t delta=s->curr_heat - s->sent_heat;
    if(delta < to_heat(0)){
      delta=-delta;
    }
    bool dirty= s->tolerance < delta;
    //fprintf(stderr, "%d,%d c=%f, s=%f, t=%f, dirty=%d\n", s->x, s->y, s->curr_heat, s->sent_heat, s->tolerance, dirty);
    s->dirty_sticky=dirty;
    return dirty;
  }

  bool is_after(generation_t a, generation_t b) const
  {
    return a < b;
  }

  // Called once by POLite at start of execution
  void init() {
    s->dirty_sticky=true;
    if( is_dirty() ){
      *readyToSend = Pin(0);
    }else{
      *readyToSend = No;
    }
    //printf("init: x=%x, y=%x, is_dirty=%x\n", s->x, s->y, is_dirty());
  }

  // Send handler
  inline void send(volatile HeatMessage* msg) {
    recalc_curr_heat();

    s->generation++;
    s->sent_heat=s->curr_heat;

    msg->colour=s->colour;
    msg->generation=s->generation;
    msg->val=s->sent_heat;

    s->msgs_sent++;

    s->dirty_sticky=false;
    *readyToSend = No;

    if(s->generation==0xFFFFFFFFul){
      printf("Wrap : %x %x %x\n", s->x, s->y, s->generation);
    }

    //printf("send: x=%x, y=%x, gen=%x, dirty=%x\n", s->x, s->y, s->generation, is_dirty());

  }

  // Receive handler
  inline void recv(volatile HeatMessage* msg, None* edge) {
    s->msgs_recv++;

    auto &ghost=s->ghosts[msg->colour];
    if(is_after(ghost.generation, msg->generation)){
      ghost.generation=msg->generation;
      ghost.heat=msg->val;
    }else{
      ///printf("OOO : %x, %x, col=%x, gg=%x, mg=%x\n", s->x, s->y, msg->colour, ghost.generation, msg->generation);
    }

    if( is_dirty() ){
      *readyToSend = Pin(0);
    }else{
      *readyToSend = No;
    }

    //printf("recv: x=%x, y=%x, colour=%x, gen=%x, dirty=%x\n", s->x, s->y, msg->colour, s->generation, is_dirty());
  }

  // Called by POLite when system becomes idle
  inline bool step() {
    if(s->min_tolerance < s->tolerance){
      s->tolerance = s->tolerance * s->tolerance_scale;
      s->tolerance=std::max(s->tolerance, s->min_tolerance);
      if(s->x==0 && s->y==0){
        printf("step tolerance down\n");
      }
    }
    //printf("step: x=%x, y=%x, gen=%x, dirty=%x\n", s->x, s->y, s->generation, is_dirty());
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
    //printf("finish: x=%x, y=%x, gen=%x, dirty=%x\n", s->x, s->y, s->generation, is_dirty());
    
    msg->generation=s->generation;
    msg->val=s->curr_heat;
    msg->x=s->x;
    msg->y=s->y;
    msg->sent_heat=s->curr_heat;
    for(int i=0; i<4; i++){
      msg->ghosts[i]=s->ghosts[i].heat;
    }
    msg->sent+=s->msgs_sent;
    msg->recv+=s->msgs_recv;
    return true;
  }
};

#endif
