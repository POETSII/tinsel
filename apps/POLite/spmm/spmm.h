#pragma once

#include <POLite.h>
#include <array>

using RING_TYPE = int;
//#define DEBUG_INFO

constexpr int DEBUG_VERBOSITY = 1;

struct SPMMMessage : PMessage
{
  uint32_t src;
  RING_TYPE value;
  uint32_t update_ts;
};

struct ALIGNED SPMMDevice : PDevice
{
  enum Type : uint8_t
  {
    MIDDLE,
    OUTPUT
  };
  uint32_t last_update_cause;

  Type type;
  RING_TYPE init_weight;
  RING_TYPE output;
  int num_inputs;
  bool copy_host;
  RING_TYPE highest_output;

  struct Entry
  {

    uint32_t src = -1;
    RING_TYPE value = 0;
    RING_TYPE weight = 0;
    uint32_t update_ts = 0;
  };
  std::array<Entry, 16> entries;

  // Called once by POLite at start of execution
  void init()
  {
    if (DEBUG_VERBOSITY > 2)
    {
      printf("Init thread=0x%x type=0x%x\n", thisDeviceId(), (unsigned int)type);
    }

    readyToSend = NONE;
    last_update_cause = 0;
    output = 0;
    highest_output = 0;

    
    for (auto &e : entries)
    {
      e.src = -1;
      e.value = 0;
      e.weight = 0;
      e.update_ts = 0;
    }
  }

  inline void send(SPMMMessage *msg)
  {
    if (DEBUG_VERBOSITY > 5)
    {
      if(output < highest_output) {
        printf("Emitting lower output, hi=%x cu=%x\n", highest_output, output);
      } else {
        highest_output = output;
      }
    }

    msg->value = output;
    msg->src = thisDeviceId();
    msg->update_ts = last_update_cause++; //tinselCycleCount();
    //msg->dest = ();

    if (DEBUG_VERBOSITY > 3)
    {
      printf("op=send thread=0x%x o=0x%x src=0x%x val=0x%x ts=0x%x luc=0x%x\n",
             thisDeviceId(), output, msg->src, msg->value, msg->update_ts,
             last_update_cause);
    }
    const bool enable_host_copy = false;

    if(enable_host_copy) 
    {
      if(copy_host) {
        readyToSend = HOST_PIN;
        copy_host = false;
      } else {
        readyToSend = NONE;
      }
    } 
    else 
    {
      readyToSend = NONE;
    }
  }

  uint16_t dest() const { return this->type == OUTPUT ? HOST_PIN : PIN(0); }

  bool updateOutput(SPMMMessage *msg, uint32_t now) {
    auto get_output_diff = [this, msg]() -> RING_TYPE {
      for (auto &e : entries)
      {
        // the source has already been registered
        if (e.src == msg->src)
        {
          if(msg->update_ts <= e.update_ts) {
            if (DEBUG_VERBOSITY > 2)
            {
              printf("Found out of order update, src=%x existing=%x v=%x new=%x v=%x\n", e.src, e.update_ts, e.value, msg->update_ts, msg->value);
            }
            return 0;
          }

          auto d = (msg->value - e.value) * e.weight;
          
          // if (d < 0)
          // {
          //   printf("Found existing contrib, d=%x src=%x v=%x ov=%x ts=%x ots=%x\n", d, e.src, msg->value, e.value, msg->update_ts, e.update_ts);
          // }

          e.value = msg->value;
          e.update_ts = msg->update_ts;
          return d;
        }

        // the source has not yet been registered, pick an empty slot
        if (e.weight == 0)
        {
          auto d = msg->value * init_weight;
          e.src = msg->src;
          e.weight = init_weight;

          // if (d < 0)
          // {
          //   printf("Found new contrib, d=%x src=%x val=%x w=%x\n", d, msg->src, msg->value, e.weight);
          // }
          
          e.value = msg->value;
          e.update_ts = msg->update_ts;
          
          
          return d;
        }
      }
      return 0;
    };

    auto output_diff = get_output_diff();

    if (output_diff == 0)
    {
      if (DEBUG_VERBOSITY > 4)
      {
        printf("fast path output_diff is 0\n");
      }
      return false;
    }

    // update the output
    output += output_diff;

    return true;

    /*
    uint32_t last_sent_diff = now - lastSendTime;
    uint32_t last_sent_diff_us = (last_sent_diff >> 8);
    RING_TYPE mult_threshold = 0; // this will need to be tweaked based on the network busyness

    // if either the difference or the time since the last message is small
    // resulting in the value being below the threshold
    // assume that we haven't changed too much yet
    bool res = (output_diff * last_sent_diff_us) >= mult_threshold;
    if (DEBUG_VERBOSITY > 4)
    {
      printf("slow path res=0x%x od=0x%x lsdu=%x\n", res, output_diff, last_sent_diff_us);
    }
    return res;
    */
  }

  // Receive handler
  inline void recv(SPMMMessage *msg)
  {
    if (DEBUG_VERBOSITY > 3)
    {
      printf("op=recv thread=0x%x o=0x%x src=0x%x val=0x%x ts=0x%x\n",
             thisDeviceId(), output, msg->src, msg->value, msg->update_ts);
    }

    if (msg->update_ts == -1)
    {
      // handle the trigger messages
      readyToSend = dest();
      return;
    }

    RING_TYPE pre_out = output;
    auto now = tinselCycleCount();

    if (updateOutput(msg, now))
    {
      readyToSend = dest();
      copy_host = true;
    }
    else
    {
      readyToSend = NONE;
    }

    if (DEBUG_VERBOSITY > 2)
    {
      printf("UPDATE thread=0x%x before=0x%x after=0x%x\n", thisDeviceId(), pre_out, output);
    }
  }
};
