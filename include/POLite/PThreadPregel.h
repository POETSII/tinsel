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

template <typename S, typename E, typename M> 
struct PregelVertex : public PDevice<None, S, E, M> {
  using Accumulator = None;
  using State = S;
  using Edge = E;
  using Message = M;

public: // Extra device state (has to be constructed when required)
  int32_t superstep_;
  bool send_message = false;

public:
  const S& GetValue() { return *this->s; };
  S * MutableValue() { return this->s; };
  int32_t superstep() const { return superstep_; };
  void SendMessageToAllNeighbors(const M& msg) {
    this->s->outgoing = msg;
    this->send_message = true;
  };
  void VoteToHalt() {
    this->s->halted = true;
  };

  // overridable methods
  bool PreCompute(M * msg) { return false; };
  void Compute() {};

public: //TODO
  int32_t NumVertices() const { return 100; }
  int32_t GetOutEdgeIterator();
};

template <typename MessageType> 
struct PregelState {
  using MessageBuffer = std::array<MessageType, 16>;
  using IncomingIterator = typename MessageBuffer::const_iterator;

  void insert_incoming(const MessageType &msg) {
    incoming[incoming_idx] = msg;
    incoming_idx++;
  }
  void clear_incoming() { incoming_idx = 0; }
  uint8_t num_incoming() const { return incoming_idx; };

  IncomingIterator incoming_begin() const {
    return incoming.cbegin();
  }

  IncomingIterator incoming_end() const {
    return incoming.cbegin() + incoming_idx;
  }

  // this should become some reference to DRAM ideally rather than the current
  // Will need to modify the mapper to do that

  bool halted;
  MessageType outgoing;
  MessageBuffer incoming;
  uint8_t incoming_idx = 0;
};

template <typename DeviceType>
struct PregelPThread : public PThread<DeviceType> {
  using A = typename DeviceType::Accumulator;
  using S = typename DeviceType::State;
  using E = typename DeviceType::Edge;
  using M = typename DeviceType::Message;

  using MessageType = PMessage<E, M>;

  enum class PregelState { RECEIVE, PROCESS, SEND };

  INLINE DeviceType createDeviceStub(PLocalDeviceId i, uint32_t superstep) {
    DeviceType dev = PThread<DeviceType>::createDeviceStub(i);
    dev.superstep_ = superstep;
    dev.send_message = false;
    return dev;
  }

  void run() {
    // Init
    this->sendersTop = this->senders;

    // Empty neighbour array
    PNeighbour<E> empty;
    empty.destThread = invalidThreadId();
    this->neighbour = &empty;

    // Set number of flits per message
    tinselSetLen((sizeof(MessageType) - 1) >> TinselLogBytesPerFlit);

    // Allocate some slots for incoming messages
    // (Slot 0 is reserved for outgoing messages)
    for (int i = 1; i <= NUM_RECV_SLOTS; i++) {
      tinselAlloc(tinselSlot(i));
    }

    PregelState ps = PregelState::RECEIVE;
    uint32_t superstep = 0;
    uint32_t haltCount = 0;
    bool halted = false;

    while (1) {
      if (ps == PregelState::RECEIVE and tinselCanRecv()) {
        printf("Receiving message\n");

        // Receive message
        MessageType *msg = (MessageType *)tinselRecv();

        // Lookup destination device
        DeviceType dev = this->createDeviceStub(msg->devId, superstep);

        if(dev.s->halted) {
          haltCount--;
          dev.s->halted = false;
        }
        bool precomputed = dev.PreCompute(&(msg->payload));
        if(precomputed == false) {
          dev.s->insert_incoming(msg->payload);
        }
        tinselAlloc(msg);
      } else if (ps == PregelState::PROCESS) {
        printf("Processing messages\n");

        for (uint32_t i = 0; i < this->numDevices; i++) {
          DeviceType dev = this->createDeviceStub(i, superstep);
          printf("Device=%x incoming=%x\n", i, dev.s->num_incoming());
          if(dev.s->halted) {
            continue;
          }
          dev.Compute();
          dev.s->clear_incoming();

          if(dev.send_message) {
            *(this->sendersTop++) = i;
          }

          if(dev.s->halted) {
            haltCount++;
          }
        }
        superstep++;
        ps = PregelState::SEND;
      } else if (ps == PregelState::SEND) {

        if (isValidThreadId(this->neighbour->destThread)) {
          if (this->neighbour->destThread == tinselId()) {
            printf("Doing actual local send\n");


            // Destination device is on current thread, simply add to buffer
            DeviceType target_dev = this->createDeviceStub(this->neighbour->devId, superstep);

            // referring the the device that we are currently doing a multicast for
            DeviceType source_dev = this->createDeviceStub(*(this->sendersTop+1), superstep);
            target_dev.s->insert_incoming(source_dev.s->outgoing);
            this->neighbour++;
          } else if (tinselCanSend()) {
            printf("Doing actual remote send\n");
            
            // Destination device is on another thread
            PMessage<E, M> *m = (PMessage<E, M> *)tinselSlot(0);
            m->devId = this->neighbour->devId;            
            
            tinselSend(this->neighbour->destThread, m);
            this->neighbour++;
          } else {
            // Go to sleep
            tinselWaitUntil(TINSEL_CAN_SEND);
          }
        } else if (this->sendersTop != this->senders) {
          if (tinselCanSend()) {
            printf("Starting a new multicast\n");

            // Start new multicast
            PLocalDeviceId src = *(--this->sendersTop);
            // Lookup device
            DeviceType dev = this->createDeviceStub(src, superstep);
            PPin pin = 1;
            
            // Load the message and the neighbour array
            PMessage<E, M>& m = *((PMessage<E, M> *)tinselSlot(0));
            m.payload = dev.s->outgoing;
            
            this->neighbour = (PNeighbour<E> *)this->devices[src].neighboursBase +
                              MAX_PIN_FANOUT * pin;
          } else {
            // Go to sleep
            tinselWaitUntil(TINSEL_CAN_SEND);
          }
        } else {
          printf("Done sending\n");

          // must be done sending
          // if everything has been sent, go back to receiving
          ps = PregelState::RECEIVE;
        }
      } else if (ps == PregelState::RECEIVE and tinselIdle()) {
        // this can be fixed with the new tinsel API that Matt has been working on w.r.t. idle detection

        if(haltCount == this->numDevices) {
          if(halted == false) {
            //printf("Are idle and haltCount == numDevices, halting\n");

            // if everybody is halted and we are idle, notify the devices
            for(int i = 0; i < this->numDevices; i++) {
              DeviceType dev = this->createDeviceStub(i, superstep);
              dev.Halt();
            }
            halted = true;
          }
        } else {
          printf("Moving from RECEIVE to PROCESS because idle\n");

          // advance the state from
          ps = PregelState::PROCESS;
        }
      } else {
        tinselWaitUntil(TINSEL_CAN_RECV);
      }
    } 
  }
};