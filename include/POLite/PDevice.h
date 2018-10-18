#ifndef _PDEVICE_H_
#define _PDEVICE_H_

#include <stdint.h>

#ifdef TINSEL
  #include <tinsel.h>
  #define PTR(t) t*
#else
  #include <tinsel-interface.h>
  #define PTR(t) uint32_t
#endif

// Use this to align on cache-line boundary
#define ALIGNED __attribute__((aligned(1<<TinselLogBytesPerLine)))

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
typedef uint32_t PThreadId;

// Device address
struct PDeviceAddr {
  PThreadId threadId;
  PLocalDeviceId devId;
};

// In some cases we use the MSB of this to mean "invalid thread"
inline bool isValidThreadId(PThreadId id) { return !(id >> 31); }
inline PThreadId invalidThreadId() { return 0x80000000; }

// Pins
//   No      - means 'not ready to send'
//   HostPin - means 'send to host'
//   Pin(n)  - means 'send to applicaiton pin number n'
typedef uint8_t PPin;
#define No 0
#define HostPin 1
#define Pin(n) ((n)+2)

// Empty structure
struct PEmpty {};

// Generic device structure
// Type parameters:
//   A - Accumulator (small, on-chip memory)
//   S - State (larger, off-chip memory)
//   E - Edge label
//   M - Message structure
template <typename A, typename S, typename E, typename M> struct PDevice {
  // State
  S* s;
  A* acc;
  PPin* readyToSend;

  // Handlers
  void init();
  void send(volatile M* msg);
  void recv(M* msg, E* edge);
  void idle();
};

// Generic edge structure
// (Only used internally)
template <typename E> struct PEdge {
  // Target device on receiving thread
  PLocalDeviceId devId;
  // Appliation edge label/weight
  E label;
};

// Generic message structure
template <typename E, typename M> struct PMessage {
  // Edge info
  PEdge<E> edge;
  // Application message
  M msg;
};

// Generic device state structure
// (Only used internally)
template <typename S> struct ALIGNED PState {
  // Pointer to base of neighbours arrays
  PTR(void) neighboursBase;
  // Custom state
  S state;
};

// Component type of neighbours array
// (Only used internally)
template <typename E> struct PNeighbour {
  // Destination thread
  PThreadId destThread;
  // Edge info
  PEdge<E> edge;
};

// Generic thread structure
template <typename DeviceType,
          typename A, typename S,
          typename E, typename M> struct PThread {

  // Number of devices handled by thread
  PLocalDeviceId numDevices;
  // Current neighbour in multicast
  PTR(PNeighbour<E>) neighbour;
  // Pointer to array of device states
  PTR(PState<S>) devices;
  // Array of local device ids are ready to send
  PTR(PLocalDeviceId) senders;
  // This array is accessed in a LIFO manner
  PTR(PLocalDeviceId) sendersTop;

  #ifdef TINSEL

  // Get ready-to-send bit for given device id
  inline PPin* readyToSend(PLocalDeviceId id) {
    PPin* p = (PPin*) tinselSlot(NUM_RECV_SLOTS+1);
    return &p[id];
  }

  // Get accumulator state for given device id
  inline A* accum(PLocalDeviceId id) {
    A* p = (A*) tinselSlot(NUM_RECV_SLOTS+1) + numDevices;
    return &p[id];
  }

  // Invoke device handlers
  void run() {
    // Empty neighbour array
    PNeighbour<E> empty;
    empty.destThread = invalidThreadId();
    neighbour = &empty;

    // Initialisation
    sendersTop = senders;
    for (uint32_t i = 0; i < numDevices; i++) {
      DeviceType dev;
      dev.s           = &devices[i].state;
      dev.acc         = accum(i);
      dev.readyToSend = readyToSend(i);
      // Invoke the initialiser for each device
      dev.init();
      // Device ready to send?
      if (*dev.readyToSend != No) *(sendersTop++) = i;
    }

    // Set number of flits per message
    tinselSetLen((sizeof(PMessage<M,E>)-1) >> TinselLogBytesPerFlit);

    // Allocate some slots for incoming messages
    // (Slot 0 is reserved for outgoing messages)
    for (int i = 1; i <= NUM_RECV_SLOTS; i++) tinselAlloc(tinselSlot(i));

    // Event loop
    while (1) {
      // Step 1: try to send
      if (isValidThreadId(neighbour->destThread)) {
        if (neighbour->destThread == tinselId()) {
          // Lookup destination device
          PLocalDeviceId id = neighbour->edge.devId;
          DeviceType dev;
          dev.s           = &devices[id].state;
          dev.acc         = accum(id);
          dev.readyToSend = readyToSend(id);
          PPin oldReadyToSend = *dev.readyToSend;
          // Invoke receive handler
          PMessage<E,M>* m = (PMessage<E,M>*) tinselSlot(0);
          dev.recv(&m->msg, &m->edge.label);
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
          DeviceType dev;
          dev.s           = &devices[src].state;
          dev.acc         = accum(src);
          dev.readyToSend = readyToSend(src);
          PPin pin        = *dev.readyToSend - 1;
          // Invoke send handler
          PMessage<E,M>* m = (PMessage<E,M>*) tinselSlot(0);
          dev.send(&m->msg);
          // Reinsert sender, if it still wants to send
          if (*dev.readyToSend != No) sendersTop++;
          // Determine neighbours array for sender
          neighbour = (PNeighbour<E>*) devices[src].neighboursBase
                    + MAX_PIN_FANOUT * pin;
        }
        else {
          // Go to sleep
          tinselWaitUntil(TINSEL_CAN_RECV | TINSEL_CAN_SEND);
        }
      }
      else {
        // Idle detection
        if (tinselIdle()) {
          for (uint32_t i = 0; i < numDevices; i++) {
            DeviceType dev;
            dev.s           = &devices[i].state;
            dev.acc         = accum(i);
            dev.readyToSend = readyToSend(i);
            // Invoke the idle handler for each device
            dev.idle();
            // Device ready to send?
            if (*dev.readyToSend != No) *(sendersTop++) = i;
          }
        }
      }

      // Step 2: try to receive
      while (tinselCanRecv()) {
        // Receive message
        PMessage<E,M> *m = (PMessage<E,M>*) tinselRecv();
        // Lookup destination device
        PLocalDeviceId id = m->edge.devId;
        DeviceType dev;
        dev.s           = &devices[id].state;
        dev.acc         = accum(id);
        dev.readyToSend = readyToSend(id);
        // Was it ready to send?
        PPin oldReadyToSend = *dev.readyToSend;
        // Invoke receive handler
        dev.recv(&m->msg, &m->edge.label);
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
