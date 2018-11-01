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

template <typename DeviceType>
struct DefaultPThread : public PThread<DeviceType> {
  using A = typename DeviceType::Accumulator;
  using S = typename DeviceType::State;
  using E = typename DeviceType::Edge;
  using M = typename DeviceType::Message;

  void run() {
    // Empty neighbour array
    PNeighbour<E> empty;
    empty.destThread = invalidThreadId();
    this->neighbour = &empty;

    // Initialisation
    this->sendersTop = this->senders;
    for (uint32_t i = 0; i < this->numDevices; i++) {
      DeviceType dev = this->createDeviceStub(i);
      // Invoke the initialiser for each device
      dev.init();
      // Device ready to send?
      if (*dev.readyToSend != No)
        *(this->sendersTop++) = i;
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
      if (isValidThreadId(this->neighbour->destThread)) {
        if (this->neighbour->destThread == tinselId()) {
          // Lookup destination device
          PLocalDeviceId id = this->neighbour->devId;
          DeviceType dev = this->createDeviceStub(id);
          PPin oldReadyToSend = *dev.readyToSend;
          // Invoke receive handler
          PMessage<E, M> *m = (PMessage<E, M> *)tinselSlot(0);
          dev.recv(&m->payload, &m->edge);
          // Insert device into a senders array, if not already there
          if (oldReadyToSend == No && *dev.readyToSend != No)
            *(this->sendersTop++) = id;
          // Move to next neighbour
          this->neighbour++;
        } else if (tinselCanSend()) {
          // Destination device is on another thread
          PMessage<E, M> *m = (PMessage<E, M> *)tinselSlot(0);
          // Copy neighbour edge info into message
          m->devId = this->neighbour->devId;
          if (typeid(E) != typeid(None))
            m->edge = this->neighbour->edge;
          // Send message
          tinselSend(this->neighbour->destThread, m);
          // Move to next neighbour
          this->neighbour++;
        } else {
          // Go to sleep
          tinselWaitUntil(TINSEL_CAN_RECV | TINSEL_CAN_SEND);
        }
      } else if (this->sendersTop != this->senders) {
        if (tinselCanSend()) {
          // Start new multicast
          PLocalDeviceId src = *(--this->sendersTop);
          // Lookup device
          DeviceType dev = this->createDeviceStub(src);
          PPin pin = *dev.readyToSend - 1;
          // Invoke send handler
          PMessage<E, M> *m = (PMessage<E, M> *)tinselSlot(0);
          dev.send(&m->payload);
          // Reinsert sender, if it still wants to send
          if (*dev.readyToSend != No)
            this->sendersTop++;
          // Determine neighbours array for sender
          this->neighbour = (PNeighbour<E> *)this->devices[src].neighboursBase +
                            MAX_PIN_FANOUT * pin;
        } else {
          // Go to sleep
          tinselWaitUntil(TINSEL_CAN_RECV | TINSEL_CAN_SEND);
        }
      } else {
        // Idle detection
        if (tinselIdle()) {
          for (uint32_t i = 0; i < this->numDevices; i++) {
            DeviceType dev = this->createDeviceStub(i);
            // Invoke the idle handler for each device
            dev.idle();
            // Device ready to send?
            if (*dev.readyToSend != No)
              *(this->sendersTop++) = i;
          }
        }
      }

      // Step 2: try to receive
      while (tinselCanRecv()) {
        // Receive message
        PMessage<E, M> *m = (PMessage<E, M> *)tinselRecv();
        // Lookup destination device
        PLocalDeviceId id = m->devId;
        DeviceType dev = this->createDeviceStub(id);
        // Was it ready to send?
        PPin oldReadyToSend = *dev.readyToSend;
        // Invoke receive handler
        dev.recv(&m->payload, &m->edge);
        // Reallocate mailbox slot
        tinselAlloc(m);
        // Insert device into a senders array, if not already there
        if (*dev.readyToSend != No && oldReadyToSend == No)
          *(this->sendersTop++) = id;
      }
    }
  }
};

struct InterruptibleState {
  enum class DeviceState : uint8_t { IDLE, WAITING_TO_SEND, SENDING };
  DeviceState state;
};

const int NUM_SEND_SLOTS = 2;

template <typename DeviceType>
struct InterruptiblePThread : public PThread<DeviceType> {
  using A = typename DeviceType::Accumulator;
  using S = typename DeviceType::State;
  using E = typename DeviceType::Edge;
  using M = typename DeviceType::Message;

  using MessageType = PMessage<E, M>;

  uint32_t triggerTime;
  PLocalDeviceId multicastSource;

  enum class ThreadState { IDLE, SENDING };
  ThreadState state;

  volatile M *get_send_buffer(PLocalDeviceId addr, bool interrupt = false) {
    PState<S> &deviceState = this->devices[addr];
    if ((this->state == ThreadState::IDLE) or (deviceState.state.state == InterruptibleState::DeviceState::SENDING and interrupt)) {
        return static_cast<volatile MessageType *>(tinselSlot(0));
    }
    return nullptr;
  }

  /*
  void send_message(volatile MessageType* send_buffer, PLocalDeviceId addr, uint16_t destination) {
    uint16_t pin = destination - 1;
    this->neighbour = (PNeighbour<E> *)this->devices[id].neighboursBase + MAX_PIN_FANOUT * pin;

    // Update the state of sending
    state = ThreadState::SENDING;
    multicastSource = id;

    if (dev.s->state == S::DeviceState::SENDING) {
      dev.onSendRestart();
    } else {
      dev.onSendStart();
    }
    dev.s->state = S::DeviceState::SENDING;
  };
  */

  void run() {

    // Initialisation
    multicastSource = 0;
    state = ThreadState::IDLE;
    PNeighbour<E> empty;
    empty.destThread = invalidThreadId();
    this->neighbour = &empty;

    for (uint32_t i = 0; i < this->numDevices; i++) {
      DeviceType dev = this->createDeviceStub(i);
      // Invoke the initialiser for each device
      dev.s->state = S::DeviceState::IDLE;
      dev.init(this);
    }

    // Set number of flits per message
    tinselSetLen((sizeof(MessageType) - 1) >> TinselLogBytesPerFlit);

    // Allocate some slots for incoming messages
    // (Slot 0 is reserved for outgoing messages)
    for (int i = 1; i <= NUM_RECV_SLOTS; i++) {
      tinselAlloc(tinselSlot(i));
    }
    
    uint8_t c = 0;
    // Event loop

    while (1) {
      // Step 1: try to receive
      if (tinselCanRecv()) {
        // Receive message
        MessageType *msg = (MessageType *)tinselRecv();
        // Lookup destination device
        DeviceType dev = this->createDeviceStub(msg->devId);

        // Was it ready to send?
        dev.process(&(msg->payload), this);
        
        // Reallocate mailbox slot
        tinselAlloc(msg);
      }

      // Step 2: try to send the currently active MC batch
      // check in the current MC batch, can we start sending
      if (isValidThreadId(this->neighbour->destThread)) {
        if (this->neighbour->destThread == tinselId()) {
          // message is directed at this thread

          PLocalDeviceId id = this->neighbour->devId;
          DeviceType destDev = this->createDeviceStub(id);
          MessageType * msg = (MessageType *)tinselSlot(0);
          destDev.process(&(msg->payload), this);
          this->neighbour++;
        } else if (tinselCanSend()) {
          // Destination device is on another thread
          MessageType *msg = (MessageType *)tinselSlot(0);

          // Copy neighbour edge info into message
          msg->devId = this->neighbour->devId;
          if (typeid(E) != typeid(None))
            msg->edge = this->neighbour->edge;
          // Send message
          tinselSend(this->neighbour->destThread, msg);  
          this->neighbour++;
        }
      } else if (state != ThreadState::IDLE) {
        // invalid address to send to
        DeviceType dev = this->createDeviceStub(multicastSource);
        state = ThreadState::IDLE;
        dev.s->state = S::DeviceState::IDLE;
        dev.onSendFinished();
      }

      // Step 4: handle IDLE + triggers
      if (++c >= triggerTime) {
        c = 0;

        for (uint32_t i = 0; i < this->numDevices; i++) {
          DeviceType dev = this->createDeviceStub(i);
          
          if (tinselIdle()) {
            // Invoke the idle handler for each device
            dev.idle(this);
          }
        
          dev.onTrigger(this);
          // if(dev.onTrigger(this)) {
          //   send_message(dev, i);
          // }
        }
      }
      tinselWaitUntil(TINSEL_CAN_RECV | TINSEL_CAN_SEND);
    }
  }
};

#endif

/*
get the sending slot - assume that this always works
this will introduce data corruption if you start writing to a mailbox
that is currently sending already

if there is you would have to pass a pointer to a SRAM buffer you can
write to and this buffer then gets drained into the mailbox when the
system is otherwise idle

several cases:
1. this thread is already sending    -> overwrite the mailbox and restart
2. another thread is already sending -> write message to SRAM and add to queue
3. nobody is sending                 -> write to mailbox and start sending

for now assume (2) does not happen (just pass nullptr and handle in client)

IDEA:
now highly optimized for sending a message on every update - might be
better to only get the send buffer when it's actually required from
the client side

volatile MessageType * sending_slot =
get_send_buffer(getPLocalDeviceAddr(msg->dest));
*/