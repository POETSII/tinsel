#pragma once

#include <stdint.h>
#include <typeinfo>

#ifdef TINSEL
#include <tinsel.h>
#define PTR(t) t *
#else
#include <tinsel-interface.h>
#define PTR(t) uint32_t
#endif

// Use this to align on half-cache-line boundary
#define ALIGNED __attribute__((aligned(1 << (TinselLogBytesPerLine - 1))))

// This is a static limit on the fan out of any pin
#ifndef MAX_PIN_FANOUT
#define MAX_PIN_FANOUT 32
#endif

// Number of mailbox slots to use for receive buffer
#ifndef NUM_RECV_SLOTS
#define NUM_RECV_SLOTS 6
#endif

// Thread-local device id
typedef uint16_t PLocalDeviceId;

// Thread id
typedef uint16_t PThreadId;

// In some cases we use the MSB of this to mean "invalid thread"
inline bool isValidThreadId(PThreadId id) { return !(id >> 15); }
inline PThreadId invalidThreadId() { return 0x8000; }

// Device address
typedef uint32_t PDeviceAddrNum;

struct PDeviceAddr {
  PThreadId threadId;
  PLocalDeviceId devId;

  PDeviceAddrNum num() const {
    static_assert(sizeof(PDeviceAddr) == 4);
    PDeviceAddrNum res = 0;
    res |= threadId << 16;
    res |= devId;
    return res;
  }

  static PDeviceAddr fromNum(PDeviceAddrNum n) {
    PDeviceAddr pda;
    pda.threadId = n >> 16;
    pda.devId = n & (-1 >> 16);
    return pda;
  }
};

// Pins
//   No      - means 'not ready to send'
//   HostPin - means 'send to host'
//   Pin(n)  - means 'send to applicaiton pin number n'
typedef uint8_t PPin;
#define No 0
#define HostPin 1
#define Pin(n) ((n) + 2)

// For template arguments that are not used
struct None {};

// Generic device structure
// Type parameters:
//   A - Accumulator (small, on-chip memory)
//   S - State (larger, off-chip memory)
//   E - Edge label
//   M - Message structure
template <typename A, typename S, typename E, typename M> struct PDevice {
  using Accumulator = A;
  using State = S;
  using Edge = E;
  using Message = M;

  // State
  S *s;
  A *acc;
  PPin *readyToSend;
  uint32_t numVertices;
  PDeviceAddr deviceAddr;

  // Handlers
  void init();
  void send(volatile M *msg);
  void recv(M *msg, E *edge);
  void idle();
};

// Generic device state structure
template <typename S> struct ALIGNED PState {
  // Pointer to base of neighbours arrays
  PTR(void) neighboursBase;
  // Custom state
  S state;
};

// Message structure (labelled edges)
template <typename E, typename M> struct PMessage {
  // Target device on receiving thread
  PLocalDeviceId devId;
  // Edge info
  E edge;
  // Application message
  M payload;
};

// Message structure (unlabelled edges)
template <typename M> struct PMessage<None, M> {
  union {
    // Target device on receiving thread
    PLocalDeviceId devId;
    // Unused
    None edge;
  };
  // Application message
  M payload;
};

// Component type of neighbours array
// For labelleled edges
template <typename E> struct PNeighbour {
  // Destination thread
  PThreadId destThread;
  // Target device on receiving thread
  PLocalDeviceId devId;
  // Edge info
  E edge;
};

// Component type of neighbours array
// For unlabelleled
template <> struct PNeighbour<None> {
  // Destination thread
  PThreadId destThread;
  union {
    // Target device on receiving thread
    PLocalDeviceId devId;
    // Unused
    None edge;
  };
};

// Generic thread structure
template <typename DeviceType> struct PThread {
  using A = typename DeviceType::Accumulator;
  using S = typename DeviceType::State;
  using E = typename DeviceType::Edge;
  using M = typename DeviceType::Message;

  // Id of the current thread
  PThreadId threadId;
  // Number of devices handled by thread
  PLocalDeviceId numDevices;
  // Number of devices in graph
  uint32_t numVertices;
  // Current neighbour in multicast
  PTR(PNeighbour<E>) neighbour;
  // Pointer to array of device states
  PTR(PState<S>) devices;
  // Array of local device ids are ready to send
  PTR(PLocalDeviceId) senders;
  // This array is accessed in a LIFO manner
  PTR(PLocalDeviceId) sendersTop;

#ifdef TINSEL
  INLINE PPin *readyToSend(PLocalDeviceId id) {
    PPin *p = (PPin *)tinselSlot(NUM_RECV_SLOTS + 1);
    return &p[id];
  }

  // Get accumulator state for given device id
  INLINE A *accum(PLocalDeviceId id) {
    uint32_t offset = (this->numDevices * sizeof(PPin) + 3) / 4;
    A *p = (A *)tinselSlot(NUM_RECV_SLOTS + 1) + offset;
    return &p[id];
  }

  INLINE DeviceType createDeviceStub(PLocalDeviceId i) {
    DeviceType dev;
    dev.s = &(this->devices[i].state);
    dev.acc = this->accum(i);
    dev.readyToSend = this->readyToSend(i);
    dev.numVertices = this->numVertices;
    dev.deviceAddr.threadId = this->threadId;
    dev.deviceAddr.devId = i;
    return dev;
  }

  bool addEdge(PLocalDeviceId localsource, uint16_t pin, PDeviceAddr target) {
    PState<S> &dev = this->devices[localsource];
    PNeighbour<E>* pinEdgeArray = (PNeighbour<E>*) dev.neighboursBase + ((pin+1) * MAX_PIN_FANOUT);

    for(uint32_t i = 0; i < MAX_PIN_FANOUT - 1; i++) {
      PNeighbour<E>& n = pinEdgeArray[i];
      if(isValidThreadId(n.destThread)) {
        continue;
      }
      
      PNeighbour<E>& next = pinEdgeArray[i+1];
      if(isValidThreadId(next.destThread)) {
        return false;
      }
      n.destThread = target.threadId;
      n.devId = target.devId;
      break;
    }
    return true;
  }

  bool removeEdge(PLocalDeviceId localsource, uint16_t pin, PDeviceAddr target) {
    PState<S> &dev = this->devices[localsource];
    PNeighbour<E>* pinEdgeArray = (PNeighbour<E>*) dev.neighboursBase + ((pin+1) * MAX_PIN_FANOUT);

    bool found = false;
    for(uint32_t i = 0; i < MAX_PIN_FANOUT - 1; i++) {
      PNeighbour<E>& n = pinEdgeArray[i];
      PNeighbour<E>& next = pinEdgeArray[i+1];
      
      if(found) {
        n = next;
        continue;
      }
      if(n.destThread == target.threadId and n.devId == target.devId) {
        found = true;
        n = next;
      }
    }
    return found;
  }
#endif
};

// for the apps that do not include this directly
#include "PThreadDefault.h"