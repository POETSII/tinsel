#pragma once

#include "PDevice.h"


struct InterruptibleState {
  enum class DeviceState : uint8_t { IDLE, WAITING_TO_SEND, SENDING };
  DeviceState state;
};

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