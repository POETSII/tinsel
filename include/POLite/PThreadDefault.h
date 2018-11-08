#pragma once

#include "PDevice.h"

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

