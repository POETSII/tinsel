#ifndef _PDEVICE_H_
#define _PDEVICE_H_

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
#define Pin(n) ((n) + 2)

// For template arguments that are not used
struct None {};

// Generic device structure
<<<<<<< HEAD
// Type parameters:
//   A - Accumulator (small, on-chip memory)
//   S - State (larger, off-chip memory)
//   E - Edge label
//   M - Message structure
template <typename A, typename S, typename E, typename M> struct PDevice {
  // State
  S *s;
  A *acc;
  PPin *readyToSend;
  uint32_t numVertices;

  // Handlers
  void init();
  void send(volatile M *msg);
  void recv(M *msg, E *edge);
  void idle();

  // Temp extra
  enum class State { IDLE, WAITING_TO_SEND, SENDING };

  // Thread local device id
  PLocalDeviceAddr localAddr;
  // Is the device ready to send?
  // (If so, to which pin? See the pin macros)
  uint16_t readyToSend;
  // Number of incoming edges
  uint16_t fanIn;
  // what is the device currently doing
  State state;
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
template <typename DeviceType, typename A, typename S, typename E, typename M>
struct PThread {
  enum class State { IDLE, SENDING };

public:
  State state;

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
}

template <typename D, typename A, typename S, typename E, typename M>
struct DefaultPThread : public PThread<D, A, S, E, M> {
  inline PPin *readyToSend(PLocalDeviceId id) {
    PPin *p = (PPin *)tinselSlot(NUM_RECV_SLOTS + 1);
    return &p[id];
  }

  // Get accumulator state for given device id
  inline A *accum(PLocalDeviceId id) {
    uint32_t offset = (numDevices * sizeof(PPin) + 3) / 4;
    A *p = (A *)tinselSlot(NUM_RECV_SLOTS + 1) + offset;
    return &p[id];
  }

  void run() {
    // Empty neighbour array
    PNeighbour<E> empty;
    empty.destThread = invalidThreadId();
    neighbour = &empty;

    // Initialisation
    sendersTop = senders;
    for (uint32_t i = 0; i < numDevices; i++) {
      DeviceType dev;
      dev.s = &devices[i].state;
      dev.acc = accum(i);
      dev.readyToSend = readyToSend(i);
      dev.numVertices = numVertices;
      // Invoke the initialiser for each device
      dev.init();
      // Device ready to send?
      if (*dev.readyToSend != No)
        *(sendersTop++) = i;
    }

    // Set number of flits per message
    tinselSetLen((sizeof(PMessage<E, M>) - 1) >> TinselLogBytesPerFlit);

    // Allocate some slots for incoming messages
    // (Slot 0 is reserved for outgoing messages)
    for (int i = 1; i <= NUM_RECV_SLOTS; i++)
      tinselAlloc(tinselSlot(i));

    // Event loop
    while (1) {
      // Step 1: try to send
      if (isValidThreadId(neighbour->destThread)) {
        if (neighbour->destThread == tinselId()) {
          // Lookup destination device
          PLocalDeviceId id = neighbour->devId;
          DeviceType dev;
          dev.s = &devices[id].state;
          dev.acc = accum(id);
          dev.readyToSend = readyToSend(id);
          dev.numVertices = numVertices;
          PPin oldReadyToSend = *dev.readyToSend;
          // Invoke receive handler
          PMessage<E, M> *m = (PMessage<E, M> *)tinselSlot(0);
          dev.recv(&m->payload, &m->edge);
          // Insert device into a senders array, if not already there
          if (oldReadyToSend == No && *dev.readyToSend != No)
            *(sendersTop++) = id;
          // Move to next neighbour
          neighbour++;
        } else if (tinselCanSend()) {
          // Destination device is on another thread
          PMessage<E, M> *m = (PMessage<E, M> *)tinselSlot(0);
          // Copy neighbour edge info into message
          m->devId = neighbour->devId;
          if (typeid(E) != typeid(None))
            m->edge = neighbour->edge;
          // Send message
          tinselSend(neighbour->destThread, m);
          // Move to next neighbour
          neighbour++;
        } else {
          // Go to sleep
          tinselWaitUntil(TINSEL_CAN_RECV | TINSEL_CAN_SEND);
        }
      } else if (sendersTop != senders) {
        if (tinselCanSend()) {
          // Start new multicast
          PLocalDeviceId src = *(--sendersTop);
          // Lookup device
          DeviceType dev;
          dev.s = &devices[src].state;
          dev.acc = accum(src);
          dev.readyToSend = readyToSend(src);
          dev.numVertices = numVertices;
          PPin pin = *dev.readyToSend - 1;
          // Invoke send handler
          PMessage<E, M> *m = (PMessage<E, M> *)tinselSlot(0);
          dev.send(&m->payload);
          // Reinsert sender, if it still wants to send
          if (*dev.readyToSend != No)
            sendersTop++;
          // Determine neighbours array for sender
          neighbour = (PNeighbour<E> *)devices[src].neighboursBase +
                      MAX_PIN_FANOUT * pin;
        } else {
          // Go to sleep
          tinselWaitUntil(TINSEL_CAN_RECV | TINSEL_CAN_SEND);
        }
      } else {
        // Idle detection
        if (tinselIdle()) {
          for (uint32_t i = 0; i < numDevices; i++) {
            DeviceType dev;
            dev.s = &devices[i].state;
            dev.acc = accum(i);
            dev.readyToSend = readyToSend(i);
            dev.numVertices = numVertices;
            // Invoke the idle handler for each device
            dev.idle();
            // Device ready to send?
            if (*dev.readyToSend != No)
              *(sendersTop++) = i;
          }
        }
      }

      // Step 2: try to receive
      while (tinselCanRecv()) {
        // Receive message
        PMessage<E, M> *m = (PMessage<E, M> *)tinselRecv();
        // Lookup destination device
        PLocalDeviceId id = m->devId;
        DeviceType dev;
        dev.s = &devices[id].state;
        dev.acc = accum(id);
        dev.readyToSend = readyToSend(id);
        dev.numVertices = numVertices;
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
}

template <typename D, typename A, typename S, typename E, typename M>
struct InterruptiblePThread : public PThread<D, A, S, E, M> {
  volatile MessageType *get_send_buffer(PLocalDeviceAddr addr) {
    volatile MessageType *sending_slot = nullptr;
    DeviceType &device = devices[addr];
    if (device.state == PDevice::State::SENDING or
        this->state == PThread::State::IDLE) {
      sending_slot = static_cast<volatile MessageType *>(tinselSlot(0));
    }
    return sending_slot;
  }

  void run() {
    // Initialisation
    dest = 0; // destination 0 is invalid
    state = PThread::State::IDLE;

    for (uint32_t i = 0; i < numDevices; i++) {
      DeviceType *dev = &devices[i];
      // Invoke the initialiser for each device
      dev->state = PDevice::State::IDLE;
      dev->init();
    }

    // Set number of flits per message
    tinselSetLen((sizeof(MessageType) - 1) >> TinselLogBytesPerFlit);

    // Allocate some slots for incoming messages
    // (Slot 0 is reserved for outgoing messages)
    for (int i = 1; i <= 7; i++)
      tinselAlloc(tinselSlot(i));

    uint8_t c = 0;
    // Event loop
    while (1) {

      auto handle_message = [this](DeviceType *dev, MessageType *msg) {
        // get the sending slot - assume that this always works
        // this will introduce data corruption if you start writing to a mailbox
        // that is currently sending already

        // if there is you would have to pass a pointer to a SRAM buffer you can
        // write to and this buffer then gets drained into the mailbox when the
        // system is otherwise idle

        // several cases:
        // 1. this thread is already sending    -> overwrite the mailbox and
        // restart
        // 2. another thread is already sending -> write message to SRAM and add
        // to queue
        // 3. nobody is sending                 -> write to mailbox and start
        // sending

        // for now assume (2) does not happen (just pass nullptr and handle in
        // client)

        // IDEA:
        // now highly optimized for sending a message on every update - might be
        // better to only get the send buffer when it's actually required from
        // the client side

        // volatile MessageType * sending_slot =
        // get_send_buffer(getPLocalDeviceAddr(msg->dest));
        bool should_send = dev->process(msg, this);

        if (should_send) {
          // Start new multicast
          multicastSource = dev;

          // Determine neighbours array for sender
          uint16_t pin = multicastSource->readyToSend - 1;
          neighbours = multicastSource->neighboursBase + MAX_PIN_FANOUT * pin;
          // Lookup first destination
          dest = *neighbours;

          // Update the state of sending
          state = PThread::State::SENDING;

          if (multicastSource->state == PDevice::State::SENDING) {
            multicastSource->onSendRestart();
          } else {
            multicastSource->onSendStart();
          }
          multicastSource->state = PDevice::State::SENDING;
        }
      };

      // Step 1: try to receive
      if (tinselCanRecv()) {
        // Receive message
        MessageType *msg = (MessageType *)tinselRecv();
        // Lookup destination device
        DeviceType *dev = &devices[msg->dest];

        // Was it ready to send?
        handle_message(dev, msg);

        // Reallocate mailbox slot
        tinselAlloc(msg);
      }

      // Step 2: try to send the currently active MC batch
      // check in the current MC batch, can we start sending
      if (isPDeviceAddrValid(dest)) {

        if (getPThreadId(dest) == tinselId()) {
          // message is directed at this thread

          // Lookup destination device
          DeviceType *destDev = &devices[getPLocalDeviceAddr(dest)];
          handle_message(destDev, (MessageType *)tinselSlot(0));
          dest = *(++neighbours);
        } else if (tinselCanSend()) {
          // Destination device is on another thread
          volatile MessageType *msg = (MessageType *)tinselSlot(0);
          msg->dest = getPLocalDeviceAddr(dest);
          // Send message
          tinselSend(getPThreadId(dest), msg);
          // Lookup next destination
          dest = *(++neighbours);
        }
      } else if (state != PThread::State::IDLE) {
        // invalid address to send to
        state = PThread::State::IDLE;
        multicastSource->state = PDevice::State::IDLE;
        multicastSource->onSendFinished();

        // clear for good measure, though not necessary
        // multicastSource = nullptr;
      }

      // add in a trigger if this is implemented by the app
      if (++c == 0) {
        for (uint32_t i = 0; i < numDevices; i++) {
          devices[i].onTrigger();
        }
      }

      tinselWaitUntil(TINSEL_CAN_RECV | TINSEL_CAN_SEND);
    }
  }
};

#endif
