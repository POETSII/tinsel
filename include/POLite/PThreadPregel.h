#pragma once

#include "PDevice.h"

#include <array>
#include <initializer_list>

template <typename M, int N> class DistributedCache {
public:
  DistributedCache(std::array<M *, N> ptrs, std::array<uint16_t, N> size)
      : data{ptrs}, cache_sizes{size} {}
  void push_back(M *msg) {
    if (current_idx[i] == cache_sizes[i]) {
      i++;
    }
  }

private:
  std::array<M *, N> data;
  std::array<uint16_t, N> cache_sizes;
  uint8_t i;
  std::array<uint8_t, N> current_idx;
};

/*
template <typename VertexValue, typename EdgeValue, typename MessageValue>
class Vertex {
public:
  virtual void Compute(MessageIterator *msgs) = 0;
  const string &vertex_id() const;
  int64 superstep() const;
  const VertexValue &GetValue();
  VertexValue *MutableValue();
  OutEdgeIterator GetOutEdgeIterator();
  void SendMessageTo(const string &dest_vertex, const MessageValue &message);
  void VoteToHalt();
};
*/

constexpr bool EVENT_LOOP_DEBUG = false;

template <typename DeviceType> struct PregelPThread;

template <typename S, typename E, typename M, typename D>
struct PregelVertex : public PDevice<None, S, E, M> {
  using Accumulator = None;
  using State = S;
  using Edge = E;
  using Message = M;

public: // Extra device state (has to be constructed when required)
  int32_t superstep_;
  bool send_message = false;
  bool send_message_host = false;
  PregelPThread<D> *thread;

public:
  const S &GetValue() { return *this->s; };
  S *MutableValue() { return this->s; };
  int32_t superstep() const { return superstep_; };
  void SendMessageToAllNeighbors(const M &msg) {
#ifdef TINSEL
    thread->SendMessageTo(msg, this->deviceAddr.devId, Pin(0));
#endif
  };
  void SendMessageToHost(const M &msg) {
#ifdef TINSEL
    thread->SendMessageTo(msg, this->deviceAddr.devId, HostPin);
#endif
  };
  void VoteToHalt() {
    if constexpr (EVENT_LOOP_DEBUG) {
      printf("Voting to halt\n");
    }
    this->s->halted = true;
  };

  // overridable methods
  bool PreCompute(const M *msg) { return false; };
  void Compute(){};

public: // TODO
  int32_t NumVertices() const { return 100; }
  int32_t GetOutEdgeIterator();
};

template <typename MessageType> struct PregelState {
  bool halted;
  
  // this should become some reference to DRAM ideally rather than the current
  // Will need to modify the mapper to do that
  using MessageBuffer = std::array<MessageType, 0>;
  using IncomingIterator = typename MessageBuffer::const_iterator;

#ifdef INCOMING_STORAGE
  void insert_incoming(const MessageType &msg) {
    incoming[incoming_idx] = msg;
    incoming_idx++;
  }
  void clear_incoming() { 
    incoming_idx = 0; 
  }
  uint8_t num_incoming() const { return incoming_idx; };
  IncomingIterator incoming_begin() const { return incoming.cbegin(); }
  IncomingIterator incoming_end() const {
    return incoming.cbegin() + incoming_idx;
  }
  MessageBuffer incoming;
  uint8_t incoming_idx = 0; 
#else
  void insert_incoming(const MessageType &msg){}
  void clear_incoming() {}
#endif
};

template <typename DeviceType>
struct PregelPThread : public PThread<DeviceType> {
  using A = typename DeviceType::Accumulator;
  using S = typename DeviceType::State;
  using E = typename DeviceType::Edge;
  using M = typename DeviceType::Message;

  using MessageType = PMessage<E, M>;

  enum class PregelState { RECEIVE, PROCESS };

  INLINE DeviceType createDeviceStub(PLocalDeviceId i, uint32_t superstep) {
    DeviceType dev = PThread<DeviceType>::createDeviceStub(i);
    dev.superstep_ = superstep;
    dev.send_message = false;
    dev.thread = this;
    return dev;
  }

  INLINE void SendMessageTo(const M &msg, PLocalDeviceId src, PPin destinationPin);
  INLINE void HandleCanRecv() {
    if constexpr (EVENT_LOOP_DEBUG) {
      printf("Receiving message\n");
    }

    // Receive message
    MessageType *msg = (MessageType *)tinselRecv();

    // Lookup destination device
    DeviceType dev = this->createDeviceStub(msg->devId, superstep);
    allDevicesHalted = false;
    dev.s->halted = false;

    bool precomputed = dev.PreCompute(&(msg->payload));
    if (precomputed == false) {
      dev.s->insert_incoming(msg->payload);
    }
    tinselAlloc(msg);
  }

  uint32_t superstep;
  bool allDevicesHalted;

  void run() {
    // Set number of flits per message
    tinselSetLen((sizeof(MessageType) - 1) >> TinselLogBytesPerFlit);

    // Allocate some slots for incoming messages
    // (Slot 0 is reserved for outgoing messages)
    for (int i = 1; i <= NUM_RECV_SLOTS; i++) {
      tinselAlloc(tinselSlot(i));
    }

    PregelState ps = PregelState::RECEIVE;
    superstep = 0;

    allDevicesHalted = false;
    bool emittedHaltCallbacks = false;

    while (1) {
      if (tinselCanRecv()) {
        HandleCanRecv();
      } else if (ps == PregelState::RECEIVE and tinselIdle()) {
        // this can be fixed with the new tinsel API that Matt has been working
        // on w.r.t. idle detection

        if (emittedHaltCallbacks) {
        } else if (allDevicesHalted) {
          // if everybody is halted and we are idle, notify the devices
          for (int i = 0; i < this->numDevices; i++) {
            DeviceType dev = createDeviceStub(i, superstep);
            dev.Halt();
          }
          emittedHaltCallbacks = true;
        } else {
          // The devices do not want to halt but the system is idle - start
          // processing
          if constexpr (EVENT_LOOP_DEBUG) {
            printf("Moving from RECEIVE to PROCESS because idle - numDev=%x\n",
                   this->numDevices);
          }
          // advance the state from
          ps = PregelState::PROCESS;
        }
      } else if (ps == PregelState::PROCESS) {
        if constexpr (EVENT_LOOP_DEBUG) {
          printf("Processing messages for ss=%x\n", superstep);
        }

        allDevicesHalted = true;

        for (uint32_t i = 0; i < this->numDevices; i++) {
          DeviceType dev = createDeviceStub(i, superstep);
          if constexpr (EVENT_LOOP_DEBUG) {
            printf("Device=%x incoming=%x\n", i, dev.s->num_incoming());
          }
          if (dev.s->halted) {
            continue;
          }

          dev.Compute();
          dev.s->clear_incoming();

          if (!dev.s->halted) {
            allDevicesHalted = false;
          }
        }
        superstep++;
        ps = PregelState::RECEIVE;
      } else {
        tinselWaitUntil(TINSEL_CAN_RECV);
      }
    }
  }
};

#ifdef TINSEL

template <typename DeviceType>
void PregelPThread<DeviceType>::SendMessageTo(const M &msg, PLocalDeviceId src,
                                              PPin destinationPin) {
  auto *nb = static_cast<PNeighbour<E> *>(this->devices[src].neighboursBase) +
             MAX_PIN_FANOUT * (destinationPin - 1);

  // Load the message and the neighbour array
  auto m = static_cast<volatile PMessage<E, M> *>(tinselSlot(0));
  m->payload = msg;

  while (isValidThreadId(nb->destThread)) {
    if (nb->destThread == tinselId()) {
      if constexpr (EVENT_LOOP_DEBUG) {
        printf("Doing actual local send\n");
      }

      // Destination device is on current thread, simply add to buffer
      DeviceType target_dev = createDeviceStub(nb->devId, superstep);

      bool precomputed = target_dev.PreCompute(&msg);
      if (precomputed == false) {
        target_dev.s->insert_incoming(msg);
      }
      nb++;
    } else if (tinselCanSend()) {
      if constexpr (EVENT_LOOP_DEBUG) {
        printf("Doing actual remote send\n");
      }

      // Destination device is on another thread
      m->devId = nb->devId;
      tinselSend(nb->destThread, m);
      nb++;
    } else if (tinselCanRecv()) {
      // In order to prevent deadlocks, we have to handle all messages that we
      // can receive
      HandleCanRecv();
    } else {
      if constexpr (EVENT_LOOP_DEBUG) {
        printf("Waiting for TINSEL_CAN_SEND | TINSEL_CAN_RECV\n");
      }
      tinselWaitUntil(TINSEL_CAN_SEND | TINSEL_CAN_RECV);
    }
  }

  if constexpr (EVENT_LOOP_DEBUG) {
    printf("Finished send\n");
  }
}
#endif