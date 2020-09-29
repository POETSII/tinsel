// SPDX-License-Identifier: BSD-2-Clause
#ifndef _PDEVICE_H_
#define _PDEVICE_H_

#include <stdint.h>
#include <stdlib.h>
#include <type_traits>

#ifdef TINSEL
  #include <tinsel.h>
  #define PTR(t) t*
#else
  #include <tinsel-interface.h>
  #define PTR(t) uint32_t
#endif

// Use this to align on half-cache-line boundary
#define ALIGNED __attribute__((aligned(1<<(TinselLogBytesPerLine-1))))

// This is a static limit on the number of pins per device
#ifndef POLITE_NUM_PINS
#define POLITE_NUM_PINS 1
#endif

// Number of mailbox slots to use for receive buffer
#ifndef POLITE_RECV_SLOTS
#define POLITE_RECV_SLOTS 15
#endif

// Macros for performance stats
//   POLITE_DUMP_STATS - dump performance stats on termination
//   POLITE_COUNT_MSGS - include message counts of performance stats

// Thread-local device id
typedef uint16_t PLocalDeviceId;
#define InvalidLocalDevId 0xffff
#define UnusedLocalDevId 0xfffe

// Thread id
typedef uint32_t PThreadId;

// Device address
// Bits 17->0: thread id
// Bit 18: invalid address
// Bits 31->19: thread-local device id
typedef uint32_t PDeviceAddr;

// Device address constructors
inline PDeviceAddr invalidDeviceAddr() { return 0x40000; }
inline PDeviceAddr makeDeviceAddr(PThreadId t, PLocalDeviceId d) {
  return (d << 19) | t;
}

// Device address deconstructors
inline bool isValidDeviceAddr(PDeviceAddr addr) { return !(addr & 0x40000); }
inline PThreadId getThreadId(PDeviceAddr addr) { return addr & 0x3ffff; }
inline PLocalDeviceId getLocalDeviceId(PDeviceAddr addr) { return addr >> 19; }

// What's the max allowed local device address?
inline uint32_t maxLocalDeviceId() { return 8192; }

// Routing key
typedef uint16_t Key;
#define InvalidKey 0xffff

// Pins
//   No      - means 'not ready to send'
//   HostPin - means 'send to host'
//   Pin(n)  - means 'send to application pin number n'
typedef uint8_t PPin;
#define No 0
#define HostPin 1
#define Pin(n) ((n)+2)

// For template arguments that are not used
struct None {};

// Generic device structure
// Type parameters:
//   S - State
//   E - Edge label
//   M - Message structure
template <typename S, typename E, typename M> struct PDevice {
  // State
  S* s;
  PPin* readyToSend;
  uint32_t numVertices;
  uint16_t time;

  // Handlers
  void init();
  void send(volatile M* msg);
  void recv(M* msg, E* edge);
  bool step();
  bool finish(volatile M* msg);
};

// Generic device state structure
template <typename S> struct ALIGNED PState {
  // Pointer to base of neighbours arrays
  uint16_t pinBase[POLITE_NUM_PINS];
  // Ready-to-send status
  PPin readyToSend;
  // Custom state
  S state;
};

// Message structure
template <typename M> struct PMessage {
  // Source-based routing key
  Key key;
  // Application message
  M payload;
};

// An outgoing edge from a device
struct POutEdge {
  // Destination thread
  uint16_t thread;
  // Routing key
  uint16_t key;
};

// An incoming edge to a device (labelleled)
template <typename E> struct PInEdge {
  // Destination device
  PLocalDeviceId devId;
  // Edge info
  E edge;
};

// An incoming edge to a device (unlabelleled)
template <> struct PInEdge<None> {
  union {
    // Destination device
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
template <typename DeviceType,
          typename S, typename E, typename M> struct PThread {

  // Number of devices handled by thread
  PLocalDeviceId numDevices;
  // Number of times step handler has been called
  uint16_t time;
  // Number of devices in graph
  uint32_t numVertices;
  // Pointer to array of device states
  PTR(PState<S>) devices;
  // Pointer to base of routing tables
  PTR(POutEdge) outTableBase;
  PTR(PInEdge<E>) inTableBase;
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
  // Number of times we wanted to send but couldn't
  uint32_t blockedSends;
  #endif

  #ifdef TINSEL

  // Helper function to construct a device
  INLINE DeviceType getDevice(uint32_t id) {
    DeviceType dev;
    dev.s           = &devices[id].state;
    dev.readyToSend = &devices[id].readyToSend;
    dev.numVertices = numVertices;
    dev.time        = time;
    return dev;
  }

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
    printf("LS:%x,TS:%x,BS:%x,BL:%x\n", intraThreadSendCount,
             interThreadSendCount, interBoardSendCount, blockedSends);
    #endif
  }

  // Invoke device handlers
  void run() {
    // Current out-going edge in multicast
    POutEdge* outEdge;

    // Outgoing edge to host
    POutEdge outHost[2];
    outHost[0].thread = tinselHostId();
    outHost[0].key = 0;
    outHost[1].key = InvalidKey;
    // Initialise outEdge to null terminator
    outEdge = &outHost[1];

    // Did last call to step handler request a new time step?
    bool active = true;

    // Reset performance counters
    tinselPerfCountReset();

    // Initialisation
    sendersTop = senders;
    for (uint32_t i = 0; i < numDevices; i++) {
      DeviceType dev = getDevice(i);
      // Invoke the initialiser for each device
      dev.init();
      // Device ready to send?
      if (*dev.readyToSend != No) {
        *(sendersTop++) = i;
      }
    }

    // Set number of flits per message
    tinselSetLen((sizeof(PMessage<M>)-1) >> TinselLogBytesPerFlit);

    // Allocate some slots for incoming messages
    // (Slot 0 is reserved for outgoing messages)
    for (int i = 1; i <= POLITE_RECV_SLOTS; i++) tinselAlloc(tinselSlot(i));

    // Event loop
    while (1) {
      // Step 1: try to send
      if (outEdge->key != InvalidKey) {
        if (tinselCanSend()) {
          PMessage<M>* m = (PMessage<M>*) tinselSlot(0);
          // Send message
          m->key = outEdge->key;
          tinselSend(outEdge->thread, m);
          #ifdef POLITE_COUNT_MSGS
          interThreadSendCount++;
          interBoardSendCount += hopsBetween(outEdge->thread, tinselId());
          #endif
          // Move to next neighbour
          outEdge++;
        }
        else {
          #ifdef POLITE_COUNT_MSGS
          blockedSends++;
          #endif
          tinselWaitUntil(TINSEL_CAN_SEND|TINSEL_CAN_RECV);
        }
      }
      else if (sendersTop != senders) {
        if (tinselCanSend()) {
          // Start new multicast
          PLocalDeviceId src = *(--sendersTop);
          // Lookup device
          DeviceType dev = getDevice(src);
          PPin pin = *dev.readyToSend;
          // Invoke send handler
          PMessage<M>* m = (PMessage<M>*) tinselSlot(0);
          dev.send(&m->payload);
          // Reinsert sender, if it still wants to send
          if (*dev.readyToSend != No) sendersTop++;
          // Determine out-edge array for sender
          if (pin == HostPin)
            outEdge = outHost;
          else
            outEdge = (POutEdge*) &outTableBase[
              devices[src].pinBase[pin-2]
            ];
        }
        else {
          #ifdef POLITE_COUNT_MSGS
          blockedSends++;
          #endif
          tinselWaitUntil(TINSEL_CAN_SEND|TINSEL_CAN_RECV);
        }
      }
      else {
        // Idle detection
        int idle = tinselIdle(!active);
        if (idle > 1)
          break;
        else if (idle) {
          active = false;
          for (uint32_t i = 0; i < numDevices; i++) {
            DeviceType dev = getDevice(i);
            // Invoke the step handler for each device
            active = dev.step() || active;
            // Device ready to send?
            if (*dev.readyToSend != No) {
              *(sendersTop++) = i;
            }
          }
          time++;
        }
      }

      // Step 2: try to receive
      while (tinselCanRecv()) {
        PMessage<M>* inMsg = (PMessage<M>*) tinselRecv();
        PInEdge<E>* inEdge = &inTableBase[inMsg->key];
        while (inEdge->devId != InvalidLocalDevId) {
          // Lookup destination device
          PLocalDeviceId id = inEdge->devId;
          DeviceType dev = getDevice(id);
          // Was it ready to send?
          PPin oldReadyToSend = *dev.readyToSend;
          // Invoke receive handler
          dev.recv(&inMsg->payload, &inEdge->edge);
          // Insert device into a senders array, if not already there
          if (*dev.readyToSend != No && oldReadyToSend == No)
            *(sendersTop++) = id;
          inEdge++;
          #ifdef POLITE_COUNT_MSGS
          intraThreadSendCount++;
          #endif
        }
        tinselAlloc(inMsg);
      }
    }

    // Termination
    #ifdef POLITE_DUMP_STATS
      dumpStats();
    #endif

    // Invoke finish handler for each device
    for (uint32_t i = 0; i < numDevices; i++) {
      DeviceType dev = getDevice(i);
      tinselWaitUntil(TINSEL_CAN_SEND);
      PMessage<M>* m = (PMessage<M>*) tinselSlot(0);
      if (dev.finish(&m->payload)) tinselSend(tinselHostId(), m);
    }

    // Sleep
    tinselWaitUntil(TINSEL_CAN_RECV); while (1);
  }

  #endif

};

#endif
