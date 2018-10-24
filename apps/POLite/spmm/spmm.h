#pragma once

#include <POLite.h>
#include <array>

using RING_TYPE = int;
enum UpdatePropagation { ONLY_TRIGGER, ALWAYS };

// compile time settings
constexpr int DEBUG_VERBOSITY = 0;
constexpr UpdatePropagation prop = ONLY_TRIGGER;

struct SPMMMessage {
  PDeviceAddrNum src;
  RING_TYPE value;
  uint32_t update_ts;
};

struct SPMMInput {
  PDeviceAddrNum src;
  RING_TYPE value;
  RING_TYPE weight;
  uint32_t update_ts;
};

struct SPMMState : public InterruptibleState {
  enum Type : uint8_t { MIDDLE, OUTPUT };
  Type type;
  uint8_t triggerTime;
  bool send_next_trigger;

  RING_TYPE init_weight;
  RING_TYPE output;
  uint32_t last_update_cause;
  std::array<SPMMInput, 16> entries;
};

struct SPMMDevice : PDevice<None, SPMMState, None, SPMMMessage> {
  using ThreadType = InterruptiblePThread<SPMMDevice>;

private:
  INLINE RING_TYPE get_output_diff(SPMMMessage *msg);
  inline bool sendMessage(ThreadType *thread);

public:
  inline void idle() {}

  // Called once by POLite at start of execution
  inline void init(ThreadType *thread);
  inline void onSendStart();
  inline void onSendRestart();
  inline void onSendFinished();
  inline bool onTrigger(ThreadType *thread);
  inline bool process(SPMMMessage *msg, ThreadType *thread);
};

#ifdef TINSEL
void SPMMDevice::init(InterruptiblePThread<SPMMDevice> *thread) {
  if (DEBUG_VERBOSITY > 2) {
    printf("Init thread=0x%x dev=0x%x type=0x%x tt=%x\n", this->deviceAddr.threadId, this->deviceAddr.devId, s->type, s->triggerTime);
  }
  this->readyToSend = No;
  s->last_update_cause = 0;
  s->output = 0;
  s->send_next_trigger = false;

  thread->triggerTime = s->triggerTime;

  for (auto &e : s->entries) {
    e.src = -1;
    e.value = 0;
    e.weight = 0;
    e.update_ts = 0;
  }
}

RING_TYPE SPMMDevice::get_output_diff(SPMMMessage *msg) {
  for (auto &e : s->entries) {
    // the source has already been registered
    if (e.src == msg->src) {
      if (msg->update_ts <= e.update_ts) {
        if (DEBUG_VERBOSITY > 2) {
          printf("Found out of order update, src=%x existing=%x v=%x new=%x "
                 "v=%x\n",
                 e.src, e.update_ts, e.value, msg->update_ts, msg->value);
        }
        return 0;
      }
      auto d = (msg->value - e.value) * e.weight;
      e.value = msg->value;
      e.update_ts = msg->update_ts;
      return d;
    }

    // the source has not yet been registered, pick an empty slot
    if (e.weight == 0) {
      auto d = msg->value * s->init_weight;
      e.src = msg->src;
      e.weight = s->init_weight;
      e.value = msg->value;
      e.update_ts = msg->update_ts;

      if (DEBUG_VERBOSITY > 2) {
        printf("Filled empty slot d=%x src=%x w=%x, v=%x, u=%x\n", d, e.src, e.weight, e.value, e.update_ts);
      }

      return d;
    }
  }
  if (DEBUG_VERBOSITY > 1) {
      printf("Unable to find any slot src=%x\n", msg->src);
  }
  return 0;
}
inline bool SPMMDevice::sendMessage(InterruptiblePThread<SPMMDevice> *thread) {
  volatile SPMMMessage *output_msg = thread->get_send_buffer(this->deviceAddr.devId);

  if (DEBUG_VERBOSITY > 4) {
    printf("sending message to next node val=%x luc=%x\n", s->output, s->last_update_cause);
  }

  if (output_msg == nullptr) {
    // TODO implement the case with multiple devices per thread
    if (DEBUG_VERBOSITY > 1) {
      printf("cannot send, output == nullptr\n");
    }
    return false;
  }

  *readyToSend = s->type == SPMMState::OUTPUT ? HostPin : Pin(0);
  output_msg->value = s->output;
  output_msg->src = this->deviceAddr.num();
  output_msg->update_ts = s->last_update_cause++;
  return true;
}
inline void SPMMDevice::onSendStart() {
  if (DEBUG_VERBOSITY > 1) {
    printf("onSendStart\n");
  }
}
inline void SPMMDevice::onSendRestart() {
  if (DEBUG_VERBOSITY > 1) {
    printf("onSendRestart\n");
  }
}
inline void SPMMDevice::onSendFinished() {
  if (DEBUG_VERBOSITY > 1) {
    printf("onSendFinished\n");
  }
}
inline bool SPMMDevice::onTrigger(InterruptiblePThread<SPMMDevice> *thread) {
  if constexpr (prop == ONLY_TRIGGER) {
    if (s->send_next_trigger and sendMessage(thread)) { // rely on SCE
      if (DEBUG_VERBOSITY > 3) {
        printf("sent update on trigger\n");
      }

      s->send_next_trigger = false;
      return true;
    }
  }
  return false;
}
inline bool SPMMDevice::process(SPMMMessage *msg,
                                InterruptiblePThread<SPMMDevice> *thread) {
  if (DEBUG_VERBOSITY > 3) {
    printf("op=recv tid=0x%x o=0x%x src=0x%x val=0x%x ts=0x%x\n", this->deviceAddr.threadId,
           s->output, msg->src, msg->value, msg->update_ts);
  }

  auto output_diff = get_output_diff(msg);

  if (output_diff == 0) {
    if (DEBUG_VERBOSITY > 4) {
      printf("fast path output_diff is 0\n");
    }
    return false;
  }

  if (DEBUG_VERBOSITY > 2) {
    printf("UPDATE thread=0x%x before=0x%x diff=0x%x\n", this->deviceAddr.threadId, s->output,
           output_diff);
  }

  s->output += output_diff;

  switch (prop) {
  case ALWAYS: {
    return sendMessage(thread);
  }
  case ONLY_TRIGGER: {
    s->send_next_trigger = true;
    if (DEBUG_VERBOSITY > 3) {
      printf("Sending update on next trigger\n");
    }
    return false;
  }
  }

  return false;
}

#endif