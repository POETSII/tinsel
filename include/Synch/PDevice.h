#ifndef _PDEVICE_H_
#define _PDEVICE_H_

#include <stdint.h>

#ifdef TINSEL
  #include <tinsel.h>
  #define PTR(t) t*
#else
  #define PTR(t) uint32_t
#endif

// Physical device/pin address
typedef uint8_t PinId;
typedef uint8_t PLocalDeviceAddr;
typedef uint16_t PThreadId;
struct PDeviceAddr {
  PinId pin;
  PLocalDeviceAddr localAddr;
  PThreadId threadId;
}

// Device pin info
struct PPinInfo {
  uint16_t numMsgs;
  uint16_t fanOut;
};

// Device kind
typedef uint8_t PDeviceKind;

// Structure holding information about a device
template <typename DeviceType> struct PDeviceInfo {
  // Time step
  uint32_t time;
  // Number of incoming messages per time step
  uint16_t numIn;
  // Count of messages received
  uint16_t countIn;
  // Have all outgoing messages for current time step been sent?
  uint8_t allSent;
  // Device kind
  PDeviceKind kind;
  // Number of outgoing pins
  uint8_t numOutPins;
  // Thread-local address
  PLocalDeviceAddr localAddr;
  // Info about each pin
  PTR(PPinInfo) pinInfo;
  // Outgoing edges
  PTR(PDeviceAddr) outEdges;
  // Device states for previous, current, and next cycles
  PTR(DeviceType) prev;
  PTR(DeviceType) current;
  PTR(DeviceType) next;
};

// Generic message structure
struct PMessage {
  // Address (source or destination)
  PDeviceAddr addr;
};

// Generic thread structure
struct PThread <typename DeviceType, typename MessageType> {
  // Number of devices handled by thread
  PLocalDeviceAddr numDevices;

  // Array of device info blocks
  PTR(PDeviceInfo<DeviceType>) devices;

  // Queue of senders
  PTR(PTR(PDeviceInfo<DeviceType>)) queue;

  #ifdef TINSEL

  // Advance time step of given device
  inline void advance(PDeviceInfo<DeviceType>* dev) {
    // Increment time step
    dev->time++;
    // Next step becomes current step
    PTR(DeviceType) tmp = dev->current;
    dev->current = dev->next;
    // Current step becomes previous step
    dev->prev = tmp;
    // Next step is reset
    dev->next->countIn = dev->next->allSent = 0;
    dev->next->begin(dev->kind);
  }

  // Interpreter
  void run() {
    // Queue front and back pointer
    uint32_t front, back;

    // Sender state
    PTR(PDeviceInfo<DeviceType>) sender;
    uint32_t senderPin = 0;
    uint32_t senderBase = 0;
    uint32_t senderChunk = 0;
    uint32_t senderIndex = 0;

    // Initialise sender queue
    front = 0;
    back = numDevices;
    sender = queue;
    for (uint32_t i = 0; i < numDevices; i++)
      queue[i] = &devices[i];

    // Initialise devices
    for (uint32_t i = 0; i < numDevices; i++) {
      PDeviceInfo<DeviceType>* dev = &dev[i];
      dev->current->begin(dev->kind);
      dev->next->begin(dev->kind);
    }

    // Allocate some slots for incoming messages
    // (Slots 0 and 1 are reserved for outgoing messages)
    for (int i = 2; i <= 5; i++) tinselAlloc(tinselSlot(i));

    // Event loop
    while (1) {
      if (tinselCanRecv()) {
        // Receive message
        volatile MessageType *msg = (volatile MessageType *) tinselRecv();
        // Lookup destination device info
        PDeviceInfo<DeviceType>* dev = &devices[msg->addr.localAddr];
        // Determine if message is for this time step or next
        if (dev->time == msg->time) {
          // Invoke receive handler for current time step
          if (msg->pin != 0) dev->current->recv(dev->kind, msg);
          // Update message count
          dev->current->countIn++;
          // End of time step?
          if (dev->countIn == dev->numIn && dev->allSent) {
            // Invoke "end of time step" handler
            dev->current->end(dev->kind, dev->prev);
            // Advance time step
            advance(info);
            // Add to queue
            queue[back] = dev;
            if (back == numDevices) back = 0 else back++;
          }
        }
        else {
          // Invoke receive handler for next time step
          if (msg->pin != 0) dev->next->recv(msg);
          // Update message count
          dev->next->countIn++;
        }
      }
      else if (tinselCanSend() && front != back) {
        if (sender->time == 0) {
          volatile MessageType *msg = (MessageType *) tinselSlot(0);
          sender = &queue[front];
          // Invoke output handler
          sender->output(sender->kind, msg);
          msg->addr.threadId = tinselId();
          msg->addr.localAddr = sender->localAddr;
          // Send chunk to host
          tinselSend(tinselHostId(), msg);
          front++;
        }
        else if (senderPin == sender->numOutPins) {
          // Move on to next sender
          sender->allSent = 1;
          if (front == numDevices) front = 0 else front++;
          sender = &queue[front];
          senderPin = 0;
          senderBase = 0;
        }
        else if (senderChunk == pinInfo[senderPin].numMsgs) {
          // Move on to next pin
          senderBase += pinInfo[senderPin].fanOut;
          senderPin++;
          senderChunk = 0;
        }
        else if (senderIndex == pinInfo[senderPin].fanOut) {
          // Move on to next chunk
          senderChunk++;
          senderIndex = 0;
        }
        else {
          volatile MessageType *msg = (MessageType *) tinselSlot(0);
          sender = &queue[front];
          if (senderIndex == 0) {
            // Invoke send handler, except on sync-only pin 0
            if (senderPin != 0) {
              sender->send(sender->kind, senderPin, senderChunk, msg);
              // Set number of flits per message
              tinselSetLen((sizeof(MessageType)-1) >> TinselLogBytesPerFlit);
            }
            else {
              msg->sync = 1;
              // Set number of flits per message
              tinselSetLen(0);
            }
          }
          // Prepare message
          msg->addr = outEdges[senderBase + senderIndex];
          // Send chunk to next destination
          tinselSend(msg->addr.threadId, msg);
          senderIndex++;
        }
      }
      else {
        // Go to sleep
        TinselWakeupCond cond = TINSEL_CAN_RECV |
          (front == back ? (TinselWakeupCond) 0 : TINSEL_CAN_SEND);
        tinselWaitUntil(cond);
      }
    }
  }
  #endif
};

#endif
