// SPDX-License-Identifier: BSD-2-Clause
// (Based on code by David Thomas)
// (Then based on code by Mathew Naylor :)
#ifndef _Izhikevich_H_
#define _Izhikevich_H_

//#define POLITE_DUMP_STATS
//#define POLITE_COUNT_MSGS

#define POLITE_NUM_PINS 3

#include <POLite.h>
#include "RNG.h"
#include <cassert>
#include <cstdio>
#include <algorithm>
#include <cmath>

#include <cstdint>

/* 
  We want to keep things as small as possible, so we optimise
  for T < 2^14 and m (fan-in) <= 2^14

  This means the worst gap possible is 2^14, and the most messages
  received, the smaller the largest gap must be.

  Similarly, the worst gap squared is 2^28, and very quickly drops.
  If at least 4 roughly equally spaced events happen, then the
  max gap squared drops to (2^12)^2 = 2^24.

  Overflow is very possible, but assuming a reasonable spiking
  regularity we wont see massive gaps. This is particularly
  true if most neurons have a high fan in.
*/

struct SpikeStatsState
{
  uint16_t sent=0;
  uint16_t send_gap_max=0;
  uint32_t send_gap_sum_squared=0;

  uint32_t received=0;

  uint16_t recv_gap_max=0;
  // uint16_t _pad_;
  uint32_t recv_gap_sum_squared=0;

  int16_t recv_delta_min=INT16_MAX;
  int16_t recv_delta_max=INT16_MIN;
  int32_t recv_delta_sum=0;
  uint32_t recv_delta_sum_abs=0;
  uint64_t recv_delta_sum_squared=0;

  int16_t last_send_time =-1;
  int16_t last_recv_time =-1;

  SpikeStatsState()
  {
  }

  void on_send(int32_t time)
  {
    if(last_send_time>=0){
      assert(time > last_send_time);
      uint32_t gap=time-last_send_time;
      send_gap_max=(uint16_t)std::max<uint32_t>(send_gap_max, gap);
      send_gap_sum_squared+=gap*gap;
    }
    sent++;
    last_send_time=time;
  }

  void on_recv(int32_t local_time, int32_t spike_time)
  {
    if(last_recv_time>=0){
      assert(local_time >= last_recv_time);
      uint32_t gap=last_recv_time-local_time;
      recv_gap_max=(uint16_t)std::max<uint32_t>(recv_gap_max, gap);
      recv_gap_sum_squared+=gap*gap;
    }
    received++;
    last_recv_time=local_time;
  
    int32_t delta=spike_time-local_time;
    recv_delta_sum+=delta;
    recv_delta_min=(uint16_t)std::min<int32_t>(recv_delta_min, delta);
    recv_delta_max=(uint16_t)std::max<int32_t>(recv_delta_max, delta);
    recv_delta_sum_abs += std::abs(delta);
    recv_delta_sum_squared += delta*delta;
  }

  double mean(unsigned count, double sum)
  { return sum/count; }

  double stddev(unsigned count, double sum, double sum_sqr)
  {
    return sqrt( sum_sqr / count - mean(count,sum)*mean(count,sum) );
  }

  void dump(uint32_t id)
  {
    fprintf(stderr, "%u : Sent=%u, gap[max=%d;mu=%g;std=%g]\n", id, sent, send_gap_max, mean(sent-1, last_send_time), stddev(sent-1, last_send_time, send_gap_sum_squared));
    fprintf(stderr, "%u : Recv=%u, gap[max=%d;mu=%g;std=%g]\n", id, received, recv_gap_max, mean(received-1, last_recv_time), stddev(received-1, last_recv_time, recv_gap_sum_squared));
    fprintf(stderr, "%u : Recv=%u, delta[min=%d;max=%d;mu=%g;std=%g]\n", id, received, recv_delta_min, recv_delta_max, mean(received-1, recv_delta_sum), stddev(received-1, recv_delta_sum, recv_delta_sum_squared));
  }
};

inline void memcpy_dwords(void *dst, const void *src, size_t n)
{
  for(unsigned i=0; i<n; i++){
    ((uint32_t*)dst)[i]=((uint32_t*)src)[i];
  }
}

template<class TMsg, size_t FragmentBytes>
struct MessageFragmenter
{
  static_assert((FragmentBytes%4)==0, "MaxBytes must be multiple of 4.");
  static_assert((sizeof(TMsg)%4)==0, "sizeof TMsg must be a multiple of 4.");

  uint32_t progress=0;

  bool complete() const
  { return progress == sizeof(TMsg); }

  bool import_msg(TMsg &msg, const void *bytes)
  {
    size_t todo=std::min<size_t>(FragmentBytes, sizeof(TMsg)-progress);
    memcpy_dwords( progress+(char*)&msg, bytes, todo/4 );
    progress += todo;
    return progress < sizeof(TMsg);
  }

  bool export_msg(const TMsg &msg, void *bytes)
  {
    size_t todo=std::min<size_t>(FragmentBytes, sizeof(TMsg)-progress);
    memcpy_dwords( bytes, progress+(char*)&msg, todo/4);
    progress += todo;
    return progress < sizeof(TMsg);
  }
};

template<class TMsg, size_t FragmentBytes>
struct MessageCollector
{
  MessageFragmenter<TMsg,FragmentBytes> fragmenter;
  TMsg msg;

  bool import_msg(const void *bytes)
  { return fragmenter.import_msg(msg, bytes);  }

  bool complete() const
  { return fragmenter.complete(); }
};


// Edge weight type
typedef float Weight;

enum MessageType {
  Tick = 0,
  Spike = 1,
  Tock = 2,
  StatsExport = 3
};

constexpr PPin TICK_OUT = Pin(MessageType::Tick);
constexpr PPin SPIKE_OUT = Pin(MessageType::Spike);
constexpr PPin TOCK_OUT = Pin(MessageType::Tock);

const int LogLevel=0;

// Message type. We want this to fit in a single flit.
struct IzhikevichMsg{
  // One word goes for PMessage header (only 16-bits, but it needs to be aligned)
  MessageType type : 4;
  uint32_t src : 20;
  uint32_t progress : 8;
  union{
    int32_t time;
    char bytes[(1<<TinselLogBytesPerFlit)-2*4];
  };
};

using SpikeStatsFragmenter = MessageFragmenter<SpikeStatsState,sizeof(IzhikevichMsg::bytes)>;
using SpikeStatsCollector = MessageCollector<SpikeStatsState,sizeof(IzhikevichMsg::bytes)>;

// Vertex state
struct IzhikevichState {
  uint32_t id;

  // Random-number-generator state
  uint32_t rng;
  // Neuron state
  float u, v, Inow, Inext;
  uint32_t spikeCount;
  // Neuron properties
  float a, b, c, d, Ir;

  int32_t time =0;
  uint32_t clock_tocks_received =0;

  uint32_t num_steps =0;

  // flags

  int8_t clock_tick_pending =0;
  int8_t clock_tock_pending =0;
  int8_t spike_pending =0;
  int8_t export_pending =0;

  int8_t is_root =0;
  int8_t clock_child_count =0;
  int8_t is_excitatory =0;
  
  SpikeStatsState spike_stats;
  SpikeStatsFragmenter spike_stats_fragmenter;
};

// Vertex behaviour
struct IzhikevichDevice : PDevice<IzhikevichState,Weight,IzhikevichMsg> {
  void advance_time()
  {
    assert(s->spike_pending==0);
    float &v = s->v;
    float &u = s->u;
    float &I = s->Inow;
    I += s->Ir * grng(s->rng);
    v = v+0.5*(0.04*v*v+5*v+140-u+I); // Step 0.5 ms
    v = v+0.5*(0.04*v*v+5*v+140-u+I); // for numerical
    u = u + s->a*(s->b*v-u);          // stability
    if (v >= 30.0) {
      v = s->c;
      u += s->d;
      s->spike_pending=1;
      if(LogLevel>0){
        //fprintf(stderr, "%u : Spike, t=%u\n", s->id, s->time);
      }
    }
    s->Inext = s->Inow;
    s->Inext = 0;
    s->time++;

    if(s->spike_pending){
      s->spike_stats.on_send(s->time);
    }

    if(s->time==s->num_steps){
      s->export_pending=1;
    }
  }

  void RTS()
  {
    *readyToSend=No;
    /*
    if(!s->spike_stats_fragmenter.complete()){
      *readyToSend=HostPin;
    }
    */
    
    if(s->export_pending && !s->spike_stats_fragmenter.complete()){
      if(LogLevel>2){
        printf("%x:RTS, HostPin\n", s->id);
      }
       *readyToSend=HostPin;
    }else if(s->clock_tick_pending){
      if(LogLevel>2){
        printf("%x:RTS, Tick\n", s->id);
      }
      *readyToSend=TICK_OUT;
    }else if(s->clock_tock_pending){
      if(LogLevel>2){
        printf("%x:RTS, Tock\n", s->id);
      }
      *readyToSend=TOCK_OUT;
    }else if(s->spike_pending){
      if(LogLevel>2){
        printf("%x:RTS, Spike\n", s->id);
      }
      *readyToSend=SPIKE_OUT;
    }else{
      if(LogLevel>2){
        printf("%x:RTS, None\n", s->id);
      }
    }
    
  }

  inline void init() {
    if(LogLevel>1){
     // fprintf(stderr, "%04u : Init, child_count=%u, is_root=%u\n", s->id, s->clock_child_count, s->is_root);
    }
    s->v = -65.0f;
    s->u = s->b * s->v;
    s->Inow = 0;
    s->Inext = 0;
    if(s->is_root){
        advance_time();
       s->clock_tick_pending=1;
    }
    RTS();
    if(LogLevel>1){
     printf("Init %x\n", s->id);
    }
  }

  inline void send(IzhikevichMsg* msg) {
    msg->src=s->id;
    msg->time=s->time;
    switch(readyToSend->index){
    default:
      assert(0);
    case HostPin.index:
      if(LogLevel>1){
        printf("%x:Send/HostPin\n", s->id);
      }
      assert(s->export_pending==1);
      assert(!s->spike_stats_fragmenter.complete());
      assert(s->time==s->num_steps);
      msg->type=MessageType::StatsExport;
      msg->progress=s->spike_stats_fragmenter.progress;
      if(!s->spike_stats_fragmenter.export_msg(s->spike_stats, msg->bytes)){
        s->export_pending=0;
      }
      break;
    case SPIKE_OUT.index:
      if(LogLevel>1){
        printf("%x:Send/Spike\n", s->id);
      }
      msg->type=MessageType::Spike;
      s->spike_pending=0;
      break;
    case TICK_OUT.index:
      if(LogLevel>1){
         printf("%x:Send/Tick, child_count=%x\n", s->id, s->clock_child_count);
      }
      assert(s->clock_tick_pending);
      assert(!s->clock_tock_pending);
      assert(s->clock_tocks_received==0);
      msg->type=MessageType::Tick;
      s->clock_tick_pending=0;
      break;
    case TOCK_OUT.index:
      if(LogLevel>1){
        printf("%x:Send/Tock\n", s->id);
      }
      assert(s->clock_tock_pending);
      assert(!s->clock_tick_pending);
      msg->type=MessageType::Tock;
      s->clock_tocks_received=0;
      s->clock_tock_pending=0;
      break;
    }    
    RTS();
  }

  inline void recv(IzhikevichMsg* msg, Weight* weight) {
    if(LogLevel>2){
     printf("%x:Recv\n", s->id);
    }
    switch(msg->type){
    default:
      assert(0);
    case Spike:
      if(LogLevel>1){
        printf("%x:Recv/Spike\n", s->id);
      }
      if(s->time < msg->time){
        assert(s->time+1 == msg->time);
        s->Inext += *weight;
      }else{
        s->Inow += *weight;
      }
      s->spike_stats.on_recv(s->time, msg->time);
      break;
    case Tick:
      if(LogLevel>1){
        printf("%x:Recv/Tick\n", s->id);
        //fprintf(stderr, "%04u : recv/Tick, s->time=%u, msg->time=%u\n", s->id, s->time, msg->time);
      }
      assert(!s->clock_tock_pending);
      assert(!s->clock_tick_pending);
      assert(msg->time==s->time+1);
      advance_time();
      assert(msg->time==s->time);
      if(s->clock_child_count==0){
        s->clock_tock_pending=1;
      }else{
        s->clock_tick_pending=1;
      }
      break;
    case Tock:
      if(LogLevel>1){
        printf("%x:Recv/Tock\n", s->id);
      }
      assert(!s->clock_tock_pending);
      assert(!s->clock_tick_pending);
      assert(s->clock_tocks_received < s->clock_child_count);
      assert(msg->time==s->time);
      s->clock_tocks_received++;
      if(s->clock_tocks_received==s->clock_child_count){
        s->clock_tocks_received=0;
        if(s->is_root){
          if(s->time<s->num_steps){
            advance_time();
            s->clock_tick_pending=1;
          }
        }else{
          s->clock_tock_pending=1;
        }
      }
      break;
    }
    RTS();
  }

  inline bool step()
  {
    printf("%x:Step", s->id);
    return false;
  }

  inline bool finish( volatile IzhikevichMsg * msg)
  {
    printf("%x:Finish", s->id);
    return false;
  }
};

#endif
