#pragma once

#include <stdint.h>
#include <stdlib.h>
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
#ifndef POLITE_MAX_FANOUT
#define POLITE_MAX_FANOUT 128
#endif

// Number of mailbox slots to use for receive buffer
#ifndef POLITE_RECV_SLOTS
#define POLITE_RECV_SLOTS 12
#endif

// Dump performance stats?
//   0: don't dump stats
//   1: dump stats first time we are idle
//   2: dump stats first time we are idle and stable
#ifndef POLITE_DUMP_STATS
#define POLITE_DUMP_STATS 0
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
//   S - State
//   E - Edge label
//   M - Message structure
template <typename A, typename S, typename E, typename M> struct PDevice {
  using Accumulator = A;
  using State = S;
  using Edge = E;
  using Message = M;

  // State
  S *s;
  //A *acc;
  PPin *readyToSend;
  uint32_t numVertices;
  PDeviceAddr deviceAddr;

  // Handlers
  void init();
  void send(volatile M* msg);
  void recv(M* msg, E* edge);
  void idle(bool vote);
};

// Generic device state structure
template <typename S> struct ALIGNED PState {
  // Pointer to base of neighbours arrays
  uint16_t neighboursOffset;
  // Ready-to-send status
  PPin readyToSend;
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

// Helper function: Count board hops between two threads
inline uint32_t hopsBetween(uint32_t t0, uint32_t t1) {
  uint32_t xmask = ((1<<TinselMeshXBits)-1);
  int32_t y0 = t0 >> (TinselLogThreadsPerBoard + TinselMeshXBits);
  int32_t x0 = (t0 >> TinselLogThreadsPerBoard) & xmask;
  int32_t y1 = t1 >> (TinselLogThreadsPerBoard + TinselMeshXBits);
  int32_t x1 = (t1 >> TinselLogThreadsPerBoard) & xmask;
  return (abs(x0-x1) + abs(y0-y1));
}

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

  // Count number of messages sent
  #ifdef POLITE_COUNT_MSGS
  // Total messages sent
  uint32_t intraThreadSendCount;
  // Total messages sent between threads
  uint32_t interThreadSendCount;
  // Messages sent between threads on different boards
  uint32_t interBoardSendCount;
  #endif

  // Dump performance counter stats over UART
  void dumpStats() {
    tinselPerfCountStop();
    uint32_t me = tinselId();
    // Per-cache performance counters
    uint32_t cacheMask = (1 <<
      (TinselLogThreadsPerCore + TinselLogCoresPerDCache)) - 1;
    if ((me & cacheMask) == 0) {
      printf("H:%x,M:%x,W:%x\n",
        tinselHitCount(),
        tinselMissCount(),
        tinselWritebackCount());
    }
    // Per-core performance counters
    uint32_t coreMask = (1 << (TinselLogThreadsPerCore)) - 1;
    if ((me & coreMask) == 0) {
      printf("C:%x %x,I:%x %x\n",
        tinselCycleCountU(), tinselCycleCount(),
        tinselCPUIdleCountU(), tinselCPUIdleCount());
    }
    // Per-thread performance counters
    #ifdef POLITE_COUNT_MSGS
    printf("LS:%x,TS:%x,BS:%x\n", intraThreadSendCount,
             interThreadSendCount, interBoardSendCount);
    #endif
  }

#ifdef TINSEL
  INLINE PPin *readyToSend(PLocalDeviceId id) {
    PPin *p = (PPin *)tinselSlot(POLITE_RECV_SLOTS + 1);
    return &p[id];
  }

  // Get accumulator state for given device id
  INLINE A *accum(PLocalDeviceId id) {
    uint32_t offset = (this->numDevices * sizeof(PPin) + 3) / 4;
    A *p = (A *)tinselSlot(POLITE_RECV_SLOTS + 1) + offset;
    return &p[id];
  }

  INLINE DeviceType createDeviceStub(PLocalDeviceId i) {
    DeviceType dev;
    dev.s = &(this->devices[i].state);
    //dev.acc = this->accum(i);
    dev.readyToSend = this->readyToSend(i);
    dev.numVertices = this->numVertices;
    dev.deviceAddr.threadId = this->threadId;
    dev.deviceAddr.devId = i;
    return dev;
  }

  bool addEdge(PLocalDeviceId localsource, uint16_t pin, PDeviceAddr target) {
    PState<S> &dev = this->devices[localsource];
    PNeighbour<E>* pinEdgeArray = (PNeighbour<E>*) dev.neighboursBase + ((pin+1) * POLITE_MAX_FANOUT);

    for(uint32_t i = 0; i < POLITE_MAX_FANOUT - 1; i++) {
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
    PNeighbour<E>* pinEdgeArray = (PNeighbour<E>*) dev.neighboursBase + ((pin+1) * POLITE_MAX_FANOUT);

    bool found = false;
    for(uint32_t i = 0; i < POLITE_MAX_FANOUT - 1; i++) {
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