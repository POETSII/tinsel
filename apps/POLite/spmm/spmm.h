#pragma once

#include <POLite.h>
#include <array>

using RING_TYPE = float;
//#define DEBUG_INFO

struct SPMMMessage : PMessage {
  uint32_t src;
  RING_TYPE value;
};

struct ALIGNED SPMMDevice : PDevice {
  enum Type : uint8_t {
    INPUT, MIDDLE, OUTPUT
  };
  Type type;
  RING_TYPE init_weight;

  RING_TYPE output;
  struct Entry {
    uint32_t src;
    RING_TYPE value;
    RING_TYPE weight;
  };
  std::array<Entry, 16> entries;

  // Called once by POLite at start of execution
  void init() {
    if(type == Type::OUTPUT) {
      printf("Init thread=0x%x type=0x%x\n", thisDeviceId(), (unsigned int)type);
    }
    readyToSend = NONE;
  
    for(auto& e : entries) {
      e.src = -1;
      e.value = 0;
      e.weight = 0;
    }
  }
  
  inline void send(SPMMMessage* msg) {
    msg->value = output;
    msg->src = thisDeviceId();

    #ifdef DEBUG_INFO
    printf("Receiving msg thread=0x%x type=0x%x o=0x%x src=0x%x val=0x%x\n", thisDeviceId(), (unsigned int)type, output, msg->src, msg->value);
    #endif

    readyToSend = NONE;
  }

  // Receive handler
  inline void recv(SPMMMessage* msg) {
    #ifdef DEBUG_INFO
    printf("Receiving msg thread=0x%x type=0x%x o=0x%x src=0x%x val=0x%x\n", thisDeviceId(), (unsigned int)type, output, msg->src, msg->value);
    #endif

    switch(type) {
      case Type::INPUT: {

        output = msg->value;
        
        #ifdef DEBUG_INFO
        printf("INPUT thread=0x%x output=0x%x\n", thisDeviceId(), output);
        #endif

        readyToSend = PIN(0);
        break;
      }
      case Type::MIDDLE:
      case Type::OUTPUT: {
        for(auto& e : entries) { 
          if(e.src == msg->src) {
            output += (msg->value - e.value)*e.weight;
            e.value = msg->value;
            break;  
          }
          
          if(e.weight == 0) {
            e.src = msg->src;
            e.value = msg->value;
            e.weight = init_weight;
            output += e.value * e.weight;
            break;
          }
        }

        #ifdef DEBUG_INFO
        if(type == Type::MIDDLE) {
          printf("MIDDLE thread=0x%x output=0x%x\n", thisDeviceId(), output);
        } else {
          printf("OUTPUT thread=0x%x output=0x%x\n", thisDeviceId(), output);          
        }
        #endif

        readyToSend = (type == Type::MIDDLE ? PIN(0) : HOST_PIN);
        break;
      }
    }
  }
};
