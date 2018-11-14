#pragma once

#include "PDevice.h"

template <typename DeviceType>
struct DefaultPThread : public PThread<DeviceType> {
  using A = typename DeviceType::Accumulator;
  using S = typename DeviceType::State;
  using E = typename DeviceType::Edge;
  using M = typename DeviceType::Message;
  
  // Helper function to construct a device
  INLINE DeviceType getDevice(uint32_t id) {
    DeviceType dev;
    dev.s           = &this->devices[id].state;
    dev.readyToSend = &this->devices[id].readyToSend;
    dev.numVertices = this->numVertices;
    return dev;
  }

  // Invoke device handlers
  void run() {
    // Empty neighbour array
    PNeighbour<E> empty;
    empty.destThread = invalidThreadId();
    this->neighbour = &empty;

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
    this->sendersTop = this->senders;
    for (uint32_t i = 0; i < this->numDevices; i++) {
      DeviceType dev = getDevice(i);
      // Invoke the initialiser for each device
      dev.init();
      // Device ready to send?
      if (*dev.readyToSend != No) {
        *(this->sendersTop++) = i;
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
      if (isValidThreadId(this->neighbour->destThread)) {
        if (this->neighbour->destThread == tinselId()) {
          // Lookup destination device
          PLocalDeviceId id = this->neighbour->devId;
          DeviceType dev = getDevice(id);
          PPin oldReadyToSend = *dev.readyToSend;
          // Invoke receive handler
          PMessage<E,M>* m = (PMessage<E,M>*) tinselSlot(0);
          dev.recv(&m->payload, &m->edge);
          // Insert device into a senders array, if not already there
          if (oldReadyToSend == No && *dev.readyToSend != No)
             *(this->sendersTop++) = id;
          // Move to next neighbour
          this->neighbour++;
          #ifdef POLITE_COUNT_MSGS
          this->intraThreadSendCount++;
          #endif
        }
        else if (tinselCanSend()) {
          // Destination device is on another thread
          PMessage<E,M>* m = (PMessage<E,M>*) tinselSlot(0);
          // Copy neighbour edge info into message
          m->devId = this->neighbour->devId;
          if (typeid(E) != typeid(None))
            m->edge = this->neighbour->edge;
          // Send message
          tinselSend(this->neighbour->destThread, m);
          // Move to next neighbour
          this->neighbour++;
          #ifdef POLITE_COUNT_MSGS
          this->interThreadSendCount++;
          this->interBoardSendCount +=
            hopsBetween(this->neighbour->destThread, tinselId());
          #endif
        }
        else {
          // Go to sleep
          tinselWaitUntil(TINSEL_CAN_RECV | TINSEL_CAN_SEND);
        }
      }
      else if (this->sendersTop != this->senders) {
        if (tinselCanSend()) {
          // Start new multicast
          PLocalDeviceId src = *(--this->sendersTop);
          // Lookup device
          DeviceType dev = getDevice(src);
          PPin pin = *dev.readyToSend - 1;
          // Invoke send handler
          PMessage<E,M>* m = (PMessage<E,M>*) tinselSlot(0);
          dev.send(&m->payload);
          // Reinsert sender, if it still wants to send
          if (*dev.readyToSend != No) this->sendersTop++;
          // Determine neighbours array for sender
          this->neighbour = (PNeighbour<E>*) &neighboursBase[
            (this->devices[src].neighboursOffset + pin) * POLITE_MAX_FANOUT];
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
          for (uint32_t i = 0; i < this->numDevices; i++) {
            DeviceType dev = getDevice(i);
            // Invoke the idle handler for each device
            dev.idle(idle > 1);
            // Device ready to send?
            if (*dev.readyToSend != No) {
              active = true;
              *(this->sendersTop++) = i;
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
          *(this->sendersTop++) = id;
      }
    }
  }
};

