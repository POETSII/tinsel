#ifndef _ASP_H_
#define _ASP_H_

// Lightweight POETS frontend
#include <POLite.h>
#include <array>

// NUM_SOURCES is the number of sources to compute ASP for
//#define NUM_SOURCES 32
#define NUM_UPDATE_SLOTS NUM_SOURCES
#define NUM_UPDATES 4

#define WITH_PERF
#define TINSEL_IDLE_SUPPORT

using DistanceType = uint8_t;
using UpdateCountType = uint16_t;

const DistanceType INIT_DISTANCE = -2;

struct UPDATE_TYPE {
  UpdateCountType idx;
  DistanceType distance;
} __attribute__((packed));


struct PerfCounters {
  using PerfCounterType = int32_t;
  PerfCounterType send_host_success;
  PerfCounterType send_dest_success;
  
  PerfCounterType recv;
  PerfCounterType update_slot_overflow;
};

struct ASPMessage {
  int32_t result;
  uint32_t send_time;
  PDeviceAddrNum src; 
  UpdateCountType num_updates;


  UPDATE_TYPE updates[NUM_UPDATES];

#ifdef WITH_PERF
  PerfCounters perf;
#endif

};

static_assert(sizeof(PMessage<None, ASPMessage>) <= 48 ); // 4 flits

struct ASPState { // : public InterruptibleState
  uint32_t id;
  uint32_t hostMsg;
  uint32_t result;
  bool sent_result;

  std::array<UpdateCountType, NUM_UPDATE_SLOTS> toUpdate;
  UpdateCountType toUpdateIdx;

  std::array<DistanceType, NUM_SOURCES> distances;
#ifdef WITH_PERF
  PerfCounters perf;
#endif

};

struct ASPDevice : PDevice<None, ASPState, None, ASPMessage> {
  using ThreadType = DefaultPThread<ASPDevice>;

  inline void idle();
  inline void init();
  inline void recv(volatile ASPMessage *msg, None *);
  inline void send(volatile ASPMessage *msg);
};

#ifdef TINSEL

inline void ASPDevice::idle() {
#ifdef TINSEL_IDLE_SUPPORT 

  if(s->sent_result) {
    *readyToSend = No;
    return;
  }

  s->result = 0;
  for(auto& d : s->distances) {
    if(d == INIT_DISTANCE) {
      s->result = -1;
      break;
    }
    s->result += d;
  }
  *readyToSend = HostPin;
#endif
}
inline void ASPDevice::init() {
  s->hostMsg = 1;
  *readyToSend = Pin(0);
}
inline void ASPDevice::recv(volatile ASPMessage *msg, None *) {
#ifdef WITH_PERF
  s->perf.recv += msg->num_updates;
#endif

  bool has_changed = false;
  // on update send the new shortest path to all 
  for(int i = 0; i < msg->num_updates; i++) {
    const volatile UPDATE_TYPE& update = msg->updates[i];

    DistanceType& current_distance = s->distances[update.idx];
    const DistanceType new_distance = update.distance + 1;

    // and patch the currently known distance if necessary
    if(new_distance < current_distance) {
      // have got a new update, so ready to send
      has_changed = true;

      current_distance = new_distance;

      bool found = false;
      for(int j = 0; j < s->toUpdateIdx; j++) {
        if(s->toUpdate[j] == update.idx) {
          found = true;
          break;
        }
      }

      if(!found) {
        if(s->toUpdateIdx == NUM_UPDATE_SLOTS) {
#ifdef WITH_PERF          
          s->perf.update_slot_overflow++;
#endif
        } else {
          s->toUpdate[s->toUpdateIdx] = update.idx;
          s->toUpdateIdx++;  
        }
      }
    }
  }

  // ISSUE - when recv gets called twice in a row and the readyToSend value updates in the mean time
  // this means that on the second call, the readyToSend value will no longer be correct, and the slot
  // meant for Pin(0) is instead used to send to  
  // that were meant for Pin(0) are instead 
  if(has_changed) {
    *readyToSend = Pin(0);
  }
}
inline void ASPDevice::send(volatile ASPMessage *msg) {
  msg->src = s->id;
  msg->send_time = s->hostMsg++;
  
  if(*readyToSend == Pin(0)) {
    int inserted_updates = 0;
    while((s->toUpdateIdx > 0) && (inserted_updates < NUM_UPDATES)) {
      s->toUpdateIdx--;

      UpdateCountType source_to_update = s->toUpdate[s->toUpdateIdx];
      volatile UPDATE_TYPE& u = msg->updates[inserted_updates];
      u.idx = source_to_update;
      u.distance = s->distances[source_to_update];
      inserted_updates++;
    }

#ifdef WITH_PERF
    s->perf.send_dest_success += inserted_updates;
#endif
    msg->num_updates = inserted_updates;
  
    if(s->toUpdateIdx == 0) {
      // when there are no more updates to send, check whether to send to host
      
#ifdef TINSEL_IDLE_SUPPORT
      *readyToSend = No;
#else
      bool found_unupdated = false;
      s->result = 0;

      // ewww linear scan
      for(auto& d : s->distances) {
        
        if(d == INIT_DISTANCE) {
          found_unupdated = true;
          s->result = 0;
          break;
        }

        s->result += d;
      }

      if(!found_unupdated) {
        *readyToSend = HostPin;
      } else {
        *readyToSend = No;
      }
#endif
    }
    return;
  }

  if(*readyToSend == HostPin) {
    msg->result = s->result;

#ifdef WITH_PERF
    s->perf.send_host_success++;
    const_cast<PerfCounters&>(msg->perf) = s->perf;
#endif
    
    s->sent_result = true;
    *readyToSend = No;
    return;
  }
}

/*
inline void ASPDevice::init(ThreadType *thread) {
  thread->triggerTime = 3;
}

inline void ASPDevice::idle(ThreadType *thread) {}

inline void ASPDevice::onTrigger(ThreadType *thread) {
  if(s->toUpdateIdx > 0) {
    volatile ASPMessage *output_msg = thread->get_send_buffer(this->deviceAddr.devId, false);
    if(output_msg == nullptr) {
      s->perf.send_dest_unable++;
      return false;
    }

    int inserted_updates = 0;
    while((s->toUpdateIdx > 0) && (inserted_updates < NUM_UPDATES)) {
      s->toUpdateIdx--;

      auto i = s->toUpdate[s->toUpdateIdx];
      auto& u = output_msg->updates[inserted_updates];
      u.idx = i;
      u.distance = s->distances[i];
      inserted_updates++;
    }
    
    output_msg->num_updates = inserted_updates;
    output_msg->src = s->id;
    *readyToSend = Pin(0);
    
    s->perf.send_dest_success++;
    
    return;
  }

  if(s->do_done) {
    volatile ASPMessage *output_msg = thread->get_send_buffer(this->deviceAddr.devId, false);
    if(output_msg == nullptr) {
      s->perf.send_host_unable++;
      return false;
    }

    uint32_t total = 0;
    for(auto& d : s->distances) {
      total += d;
    }
    output_msg->result = total;
    output_msg->src = s->id;
    s->perf.send_host_success++;
    
    const_cast<PerfCounters&>(output_msg->perf) = s->perf;
    *readyToSend = HostPin;
    s->do_done = false;
    thread->send_message(output_)
    return;
  }
}

inline void ASPDevice::process(ASPMessage *msg, ThreadType *thread) {
  
  // on update send the new shortest path to all 
  for(int i = 0; i < msg->num_updates; i++) {
    const UPDATE_TYPE& update = msg->updates[i];

    DistanceType& current_distance = s->distances[update.idx];
    const DistanceType new_distance = update.distance + 1;

    // and patch the currently known distance if necessary
    if(new_distance < current_distance) {
      s->do_done = true;

      current_distance = new_distance;
      s->toUpdate[s->toUpdateIdx] = update.idx;
      s->toUpdateIdx++;
    }
  }

  return false;
}
*/

#endif

#endif
