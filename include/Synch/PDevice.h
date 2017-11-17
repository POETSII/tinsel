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

// Structure holding information about a device
template <typename DeviceType> struct PDeviceInfo {
  // Time step
  uint32_t time;
  // Number of incoming & outgoing messages per time step
  uint16_t numIn, numOut;
  // Count of messages received & sent
  uint16_t countIn, countOut;
  // Pointers to device states for previous, current, and next cycles
  PTR(DeviceType) prev;
  PTR(DeviceType) current;
  PTR(DeviceType) next;
  // Number of outgoing pins
  uint16_t numOutPins;
  // Number of messages to send for each pin
  PTR(uint16_t) numMsgs;
  // Fan-out of each pin
  PTR(uint16_t) fanOut;
  // Outgoing edges
  PTR(PDeviceAddr) outEdges;
};

// Generic device structure
struct PDevice {
  void init();
  void begin();
  void send(PinId pin, uint32_t msgNum, volatile MessageType* msg);
  void recv(volatile MessageType* msg);
  void end();
};

// Generic message structure
struct PMessage {
  // Destination device
  PLocalDeviceAddr dest;
  // Input pin that message is arriving on
  PinId pin;
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
    dev->next->countIn = dev->next->countOut = 0;
    // Invoke "start of time step" handler for next step
    dev->begin();
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
    for (uint32_t i = 0; i < numDevices; i++)
      queue[i] = &devices[i];

    // Initialise devices
    for (uint32_t i = 0; i < numDevices; i++) {
      PDeviceInfo<DeviceType>* dev = &dev[i];
      dev->prev->init();
      dev->current->begin();
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
        PDeviceInfo<DeviceType>* dev = &devices[msg->dest];
        // Determine if message is for this time step or next
        if (dev->time == msg->time) {
          // Invoke receive handler for current time step
          if (msg->pin != 0) dev->current->recv(msg);
          // Update message count
          dev->current->countIn++;
          // End of time step?
          if (dev->countIn == dev->numIn &&
              dev->countOut == dev->numOut) {
            // Invoke "end of time step" handler
            dev->current->end();
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
        volatile MessageType *msg = (MessageType *) tinselSlot(0);
        if (senderPin == sender->numOutPins) {
          // Move on to next sender
          sender->countOut = sender->numOut;
          sender = queue[front];
          if (front == numDevices) front = 0 else front++;
          senderPin = 0;
          senderBase = 0;
        }
        else if (senderChunk == numMsgs[senderPin]) {
          // Move on to next pin
          senderBase += fanOut[senderPin];
          senderPin++;
          senderChunk = 0;
        }
        else if (senderIndex == fanOut[senderPin]) {
          // Move on to next chunk
          senderChunk++;
          senderIndex = 0;
        }
        else {
          if (senderIndex == 0) {
            // Invoke send handler, except on sync-only pin 0
            if (senderPin != 0) {
              sender->send(senderPin, senderChunk, msg);
              msg->sync = 1;
            }
          }
          // Prepare message
          PDeviceAddr* addr = &outEdges[senderBase + senderIndex];
          msg->dest = addr->localAddr;
          msg->pin = addr->pin;
          // Send chunk to next destination
          tinselSend(addr->threadId, msg);
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
