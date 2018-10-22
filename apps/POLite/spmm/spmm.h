#pragma once

#include <POLite.h>
#include <array>

using RING_TYPE = float;
enum UpdatePropagation { ONLY_TRIGGER, ALWAYS };

// compile time settings
constexpr int DEBUG_VERBOSITY = 0;
constexpr UpdatePropagation prop = ONLY_TRIGGER;

struct SPMMMessage : PMessage {
  uint32_t src;
  RING_TYPE value;
  uint32_t update_ts;
};

struct ALIGNED SPMMDevice : PDevice {
  enum Type : uint8_t { MIDDLE, OUTPUT };

  Type type;
  bool send_next_trigger;
  RING_TYPE init_weight;
  RING_TYPE output;
  uint32_t last_update_cause;

  struct Entry {
    uint32_t src = -1;
    RING_TYPE value = 0;
    RING_TYPE weight = 0;
    uint32_t update_ts = 0;
  };
  std::array<Entry, 8> entries;

  // Called once by POLite at start of execution
  void init(PThread<SPMMDevice, SPMMMessage> *thread) {
    if (DEBUG_VERBOSITY > 2) {
      printf("Init thread=0x%x type=0x%x\n", thisDeviceId(),
             (unsigned int)type);
    }
    readyToSend = NONE;
    last_update_cause = 0;
    output = 0;
    send_next_trigger = false;

    for (auto &e : entries) {
      e.src = -1;
      e.value = 0;
      e.weight = 0;
      e.update_ts = 0;
    }
  }

#ifdef TINSEL
  RING_TYPE get_output_diff(SPMMMessage *msg) {
    for (auto &e : entries) {
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
        auto d = msg->value * init_weight;
        e.src = msg->src;
        e.weight = init_weight;
        e.value = msg->value;
        e.update_ts = msg->update_ts;
        return d;
      }
    }
    return 0;
  }
  inline bool sendMessage(PThread<SPMMDevice, SPMMMessage> *thread) {
    volatile SPMMMessage *output_msg = thread->get_send_buffer(localAddr);

    if (output_msg == nullptr) {
      // TODO implement the case with multiple devices per thread
      if (DEBUG_VERBOSITY > 1) {
        printf("cannot send, output == nullptr\n");
      }
      return false;
    }

    readyToSend = type == OUTPUT ? HOST_PIN : PIN(0);
    output_msg->value = output;
    output_msg->src = thisDeviceId();
    output_msg->update_ts = last_update_cause++;
    return true;
  }
  inline void onSendStart() {
    if (DEBUG_VERBOSITY > 1) {
      printf("onSendStart\n");
    }
  }
  inline void onSendRestart() {
    if (DEBUG_VERBOSITY > 1) {
      printf("onSendRestart\n");
    }
  }
  inline void onSendFinished() {
    if (DEBUG_VERBOSITY > 1) {
      printf("onSendFinished\n");
    }
  }
  inline bool onTrigger(PThread<SPMMDevice, SPMMMessage> *thread) {
    if constexpr (prop == ONLY_TRIGGER) {
      if (send_next_trigger and sendMessage(thread)) { // rely on SCE
        send_next_trigger = false;
        return true;
      }
    }
    return false;
  }
  inline bool process(SPMMMessage *msg,
                      PThread<SPMMDevice, SPMMMessage> *thread) {
    if (DEBUG_VERBOSITY > 3) {
      printf("op=recv thread=0x%x o=0x%x src=0x%x val=0x%x ts=0x%x\n",
             thisDeviceId(), output, msg->src, msg->value, msg->update_ts);
    }

    auto output_diff = get_output_diff(msg);

    if (output_diff == 0) {
      if (DEBUG_VERBOSITY > 4) {
        printf("fast path output_diff is 0\n");
      }
      return false;
    }

    if (DEBUG_VERBOSITY > 2) {
      printf("UPDATE thread=0x%x before=0x%x diff=0x%x\n", thisDeviceId(),
             output, output_diff);
    }

    output += output_diff;

    switch (prop) {
    case ALWAYS: {
      return sendMessage(thread);
    }
    case ONLY_TRIGGER: {
      send_next_trigger = true;
      return false;
    }
    }

    return false;
  }

#endif
};

// uint32_t last_sent_diff = now - lastSendTime;
// uint32_t last_sent_diff_us = (last_sent_diff >> 8);
// RING_TYPE mult_threshold = 0; // this will need to be tweaked based on the
// network busyness

// // if either the difference or the time since the last message is small
// // resulting in the value being below the threshold
// // assume that we haven't changed too much yet
// bool res = (output_diff * last_sent_diff_us) >= mult_threshold;
// if (DEBUG_VERBOSITY > 4)
// {
//   printf("slow path res=0x%x od=0x%x lsdu=%x\n", res, output_diff,
// last_sent_diff_us);
// }
// return res;