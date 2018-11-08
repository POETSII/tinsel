#ifndef _PDEVICE_H_
#define _PDEVICE_H_

#include <stdint.h>
#include <stdlib.h>
#include <typeinfo>

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

// Device address
struct PDeviceAddr {
  PThreadId threadId;
  PLocalDeviceId devId;
};

// In some cases we use the MSB of this to mean "invalid thread"
inline bool isValidThreadId(PThreadId id) { return !(id >> 15); }
inline PThreadId invalidThreadId() { return 0x8000; }

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
template <typename DeviceType,
          typename S, typename E, typename M> struct PThread {

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

  #ifdef TINSEL

  // Helper function to construct a device
  INLINE DeviceType getDevice(uint32_t id) {
    DeviceType dev;
    dev.s           = &devices[id].state;
    dev.readyToSend = &devices[id].readyToSend;
    dev.numVertices = numVertices;
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
    empty.destThread = invalidThreadId();
    neighbour = &empty;

    // Base of all neighbours arrays on this thread
    PNeighbour<E>* neighboursBase = (PNeighbour<E>*) tinselHeapBase();

    // Did last call to init handler or idle handler trigger any send?
    bool active = false;

    #if POLITE_DUMP_STATS > 0
      // Have performance stats been dumped?
      bool doStatDump = POLITE_DUMP_STATS;
    #endif

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
        active = true;
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
      if (isValidThreadId(neighbour->destThread)) {
        if (neighbour->destThread == tinselId()) {
          // Lookup destination device
          PLocalDeviceId id = neighbour->devId;
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
          m->devId = neighbour->devId;
          if (typeid(E) != typeid(None))
            m->edge = neighbour->edge;
          // Send message
          tinselSend(neighbour->destThread, m);
          // Move to next neighbour
          neighbour++;
          #ifdef POLITE_COUNT_MSGS
          interThreadSendCount++;
          interBoardSendCount +=
            hopsBetween(neighbour->destThread, tinselId());
          #endif
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
        if (idle) {
          active = false;
          #if POLITE_DUMP_STATS == 1
          if (doStatDump) {
            dumpStats();
            doStatDump = false;
          }
          #elif POLITE_DUMP_STATS == 2
          if (doStatDump && idle > 1) {
            dumpStats();
            doStatDump = false;
          }
          #endif
          for (uint32_t i = 0; i < numDevices; i++) {
            DeviceType dev = getDevice(i);
            // Invoke the idle handler for each device
            dev.idle(idle > 1);
            // Device ready to send?
            if (*dev.readyToSend != No) {
              active = true;
              *(sendersTop++) = i;
            }
          }
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
  }

  #endif

};

#endif
