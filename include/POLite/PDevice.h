#ifndef _PDEVICE_H_
#define _PDEVICE_H_

#include <stdint.h>
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
#ifndef MAX_PIN_FANOUT
#define MAX_PIN_FANOUT 64
#endif

// Number of mailbox slots to use for receive buffer
#ifndef NUM_RECV_SLOTS
#define NUM_RECV_SLOTS 12
#endif

// Dump performance stats first time we are idle and stable?
#ifndef DUMP_STATS
#define DUMP_STATS 0
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
    // Only one thread on each cache reports performance counters
    uint32_t me = tinselId();
    uint32_t mask = (1 <<
      (TinselLogThreadsPerCore + TinselLogCoresPerDCache)) - 1;
    if ((me & mask) == 0) {
      tinselPerfCountStop();
      printf("C:%x,H:%x,M:%x,W:%x,I:%x\n",
        tinselCycleCount(),
        tinselHitCount(),
        tinselMissCount(),
        tinselWritebackCount(),
        tinselCPUIdleCount());
    }
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

    // Have performance stats been dumped?
    bool doStatDump = DUMP_STATS;

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
    for (int i = 1; i <= NUM_RECV_SLOTS; i++) tinselAlloc(tinselSlot(i));

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
            (devices[src].neighboursOffset + pin) * MAX_PIN_FANOUT];
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
          if (doStatDump && idle > 1) {
            dumpStats();
            doStatDump = false;
          }
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
