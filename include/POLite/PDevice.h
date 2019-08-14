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

// This is a static limit on the fan out of any pin
#ifndef POLITE_MAX_FANOUT
#define POLITE_MAX_FANOUT 256
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

// Pins
//   No      - means 'not ready to send'
//   HostPin - means 'send to host'
//   Pin(n)  - means 'send to applicaiton pin number n'
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
  // Destination thread and device
  PDeviceAddr destAddr;
  // Edge info
  E edge;
};

// Component type of neighbours array
// For unlabelleled
template <> struct PNeighbour<None> {
  union {
    // Destination thread and device
    PDeviceAddr destAddr;
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
  // Current neighbour in multicast
  PTR(PNeighbour<E>) neighbour;
  // Pointer to array of device states
  PTR(PState<S>) devices;
  // Pointer to base of neighours array
  PTR(PNeighbour<E>) neighboursBase;
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
    printf("LS:%x,TS:%x,BS:%x\n", intraThreadSendCount,
             interThreadSendCount, interBoardSendCount);
    #endif
  }

  // Invoke device handlers
  void run() {
    // Empty neighbour array
    PNeighbour<E> empty;
    empty.destAddr = invalidDeviceAddr();
    neighbour = &empty;

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
    tinselSetLen((sizeof(PMessage<E,M>)-1) >> TinselLogBytesPerFlit);

    // Allocate some slots for incoming messages
    // (Slot 0 is reserved for outgoing messages)
    for (int i = 1; i <= POLITE_RECV_SLOTS; i++) tinselAlloc(tinselSlot(i));

    // Event loop
    while (1) {
      // Step 1: try to send
      if (isValidDeviceAddr(neighbour->destAddr)) {
        PThreadId destThread = getThreadId(neighbour->destAddr);
        if (destThread == tinselId()) {
          // Lookup destination device
          PLocalDeviceId id = getLocalDeviceId(neighbour->destAddr);
          DeviceType dev = getDevice(id);
          PPin oldReadyToSend = *dev.readyToSend;
          // Invoke receive handler
          PMessage<E,M>* m = (PMessage<E,M>*) tinselSlot(0);
          dev.recv(&m->payload, &m->edge);
          // Insert device into a senders array, if not already there
          if (oldReadyToSend == No && *dev.readyToSend != No)
             *(sendersTop++) = id;
          // Move to next neighbour
          neighbour++;
          #ifdef POLITE_COUNT_MSGS
          intraThreadSendCount++;
          #endif
        }
        else if (tinselCanSend()) {
          // Destination device is on another thread
          PMessage<E,M>* m = (PMessage<E,M>*) tinselSlot(0);
          // Copy neighbour edge info into message
          m->devId = getLocalDeviceId(neighbour->destAddr);
          if (! std::is_same<E, None>::value)
            m->edge = neighbour->edge;
          // Send message
          PThreadId destThread = getThreadId(neighbour->destAddr);
          tinselSend(destThread, m);
          #ifdef POLITE_COUNT_MSGS
          interThreadSendCount++;
          interBoardSendCount +=
            hopsBetween(destThread, tinselId());
          #endif
          // Move to next neighbour
          neighbour++;
        }
        else {
          // Go to sleep
          tinselWaitUntil(TINSEL_CAN_RECV | TINSEL_CAN_SEND);
        }
      }
      else if (sendersTop != senders) {
        if (tinselCanSend()) {
          // Start new multicast
          PLocalDeviceId src = *(--sendersTop);
          // Lookup device
          DeviceType dev = getDevice(src);
          PPin pin = *dev.readyToSend - 1;
          // Invoke send handler
          PMessage<E,M>* m = (PMessage<E,M>*) tinselSlot(0);
          dev.send(&m->payload);
          // Reinsert sender, if it still wants to send
          if (*dev.readyToSend != No) sendersTop++;
          // Determine neighbours array for sender
          neighbour = (PNeighbour<E>*) &neighboursBase[
            (devices[src].neighboursOffset + pin) * POLITE_MAX_FANOUT];
        }
        else {
          // Go to sleep
          tinselWaitUntil(TINSEL_CAN_RECV | TINSEL_CAN_SEND);
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
        // Receive message
        PMessage<E,M> *m = (PMessage<E,M>*) tinselRecv();
        // Lookup destination device
        PLocalDeviceId id = m->devId;
        DeviceType dev = getDevice(id);
        // Was it ready to send?
        PPin oldReadyToSend = *dev.readyToSend;
        // Invoke receive handler
        dev.recv(&m->payload, &m->edge);
        // Reallocate mailbox slot
        tinselAlloc(m);
        // Insert device into a senders array, if not already there
        if (*dev.readyToSend != No && oldReadyToSend == No)
          *(sendersTop++) = id;
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
      PMessage<E,M>* m = (PMessage<E,M>*) tinselSlot(0);
      if (dev.finish(&m->payload)) tinselSend(tinselHostId(), m);
    }

    // Sleep
    tinselWaitUntil(TINSEL_CAN_RECV); while (1);
  }

  #endif

};

#endif
