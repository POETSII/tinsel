#pragma once

#include "PDevice.h"

#include <array>
#include <initializer_list>

// template <typename VertexValue, typename EdgeValue, typename MessageValue>
// class Vertex {
// public:
//   virtual void Compute(MessageIterator *msgs) = 0;
//   const string &vertex_id() const;
//   int64 superstep() const;
//   const VertexValue &GetValue();
//   VertexValue *MutableValue();
//   OutEdgeIterator GetOutEdgeIterator();
//   void SendMessageTo(const string &dest_vertex, const MessageValue &message);
//   void VoteToHalt();
// };

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

template <typename MessageType> struct PregelState {
  void insert_incoming(const MessageType &msg) {
    incoming[incoming_idx] = msg;
    incoming_idx++;
  }
  void clear_incoming() { incoming_idx = 0; }
  // this should become some reference to DRAM ideally rather than the current
  // SRAM
  std::array<MessageType, 16> incoming;
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

  void run() {
    // Initialisation
    /*
    multicastSource = 0;
    state = ThreadState::IDLE;
    PNeighbour<E> empty;
    empty.destThread = invalidThreadId();
    this->neighbour = &empty;
    */

    for (uint32_t i = 0; i < this->numDevices; i++) {
      DeviceType dev = this->createDeviceStub(i);
      // Invoke the initialiser for each device
      dev.init();
    }

    // Set number of flits per message
    tinselSetLen((sizeof(MessageType) - 1) >> TinselLogBytesPerFlit);

    // Allocate some slots for incoming messages
    // (Slot 0 is reserved for outgoing messages)
    for (int i = 1; i <= NUM_RECV_SLOTS; i++) {
      tinselAlloc(tinselSlot(i));
    }

    // Keep track of a receiving cache

    PregelState ps = PregelState::RECEIVE;
    uint32_t superstep = 0;

    //MessageType sram_cache[32];
    //MessageType sram_cache2[64];
    //DistributedCache<MessageType, 1> receiveCache({sram_cache}, {32});


    while (1) {
      if (tinselCanRecv()) {
        // Receive message
        MessageType *msg = (MessageType *)tinselRecv();

        // Lookup destination device
        DeviceType dev = this->createDeviceStub(msg->devId);
        dev.s->insert_incoming(msg->payload);
        tinselAlloc(msg);
      } else if (ps == PregelState::PROCESS) {
        for (uint32_t i = 0; i < this->numDevices; i++) {
          DeviceType dev = this->createDeviceStub(i);
          dev.compute();
          dev.s->clear_incoming();
        }
        superstep++;
        ps = PregelState::SEND;
      } else if (ps == PregelState::SEND) {
        if (tinselCanSend()) {
          // do the sending
        }

        // if everything has been sent, go back to receiving
        bool done = false;
        if (done) {
          ps = PregelState::RECEIVE;
        }
      } else if (ps == PregelState::RECEIVE and tinselIdle()) {
        // advance the state from
        ps = PregelState::PROCESS;
      }
    }

    /*
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
        MessageType *msg = (MessageType *)tinselSlot(0);
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
    */
    tinselWaitUntil(TINSEL_CAN_RECV | TINSEL_CAN_SEND);
  }
};