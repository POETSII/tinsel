#ifndef _PDEVICE_H_
#define _PDEVICE_H_

#include <stdint.h>

#ifdef TINSEL
  #include <tinsel.h>
  #define PTR(t) t*
#else
  #include <tinsel-interface.h>
  #define PTR(t) uint32_t
#endif

// Use this to align on cache-line boundary
#define ALIGNED __attribute__((aligned(1<<TinselLogBytesPerLine)))

// This is a static limit on the fan out of any pin
#define MAX_PIN_FANOUT 32

// Physical device identifier
// Bits [31:16] are the thread id
// Bits [15:1] are the thread-local device id
// Bit 0 is the valid bit
typedef uint16_t PLocalDeviceAddr;
typedef uint16_t PThreadId;
typedef uint32_t PDeviceAddr;

// Constructor
inline PDeviceAddr makePDeviceAddr(
         PThreadId threadId, PLocalDeviceAddr devId, uint16_t valid)
  { return (threadId << 16) | (devId << 1) | valid; }

// Selectors
inline PThreadId getPThreadId(PDeviceAddr addr) { return (addr >> 16); }
inline PLocalDeviceAddr getPLocalDeviceAddr(PDeviceAddr addr)
  { return ((addr&0xffff) >> 1); };
inline bool isPDeviceAddrValid(PDeviceAddr addr) { return (addr & 1); }

// Pin macros
// NONE means 'not ready to send'
// HOST_PIN means 'send to host'
// PIN(n) means 'send to applicaiton pin number n'
#define NONE 0
#define HOST_PIN 1
#define PIN(n) ((n)+2)

// Generic device structure
struct PDevice {
  // Pointer to base of neighbours arrays
  PTR(PDeviceAddr) neighboursBase;

  // Thread local device id
  PLocalDeviceAddr localAddr;

  // Is the device ready to send?
  // (If so, to which pin? See the pin macros)
  uint16_t readyToSend;

  #ifdef TINSEL
    // Obtain device id
    inline PDeviceAddr thisDeviceId() {
      return makePDeviceAddr(tinselId(), localAddr, 1);
    }
  #else
    PDeviceAddr thisDeviceId();
  #endif
};

// Generic message structure
struct PMessage {
  // Destination device
  PLocalDeviceAddr dest;
};

// Generic thread structure
template <typename DeviceType, typename MessageType> class PThread {
 private:

  #ifdef TINSEL
  // Version of send handler that won't be inlined
  // Ensures writes to msg will not be optimised away before tinselSend()
  __attribute__ ((noinline))
    void sendHandler(DeviceType* dev, MessageType* msg) {
      dev->send(msg);
    }
  #endif

 public:

  // Number of devices handled by thread
  PLocalDeviceAddr numDevices;
  // Current destination for multicast
  PDeviceAddr dest;
  // Source of current multicase
  PTR(DeviceType) multicastSource;
  // Pointer to array of devices
  PTR(DeviceType) devices;
  // Pointer to neighbours array of current multicast
  PTR(PDeviceAddr) neighbours;
  // Array of pointers to devices that are ready to send
  PTR(PTR(DeviceType)) senders;
  // This array is accessed in a LIFO manner
  PTR(PTR(DeviceType)) sendersTop;

  #ifdef TINSEL

  // Invoke device handlers
  void run() {
    // Initialisation
    sendersTop = senders;
    dest = 0; // Initial destination is invalid
    for (uint32_t i = 0; i < numDevices; i++) {
      DeviceType* dev = &devices[i];
      // Invoke the initialiser for each device
      dev->init();
      // Device ready to send?
      if (dev->readyToSend != NONE) *(sendersTop++) = dev;
    }

    // Set number of flits per message
    tinselSetLen((sizeof(MessageType)-1) >> TinselLogBytesPerFlit);

    // Allocate some slots for incoming messages
    // (Slot 0 is reserved for outgoing messages)
    for (int i = 1; i <= 7; i++) tinselAlloc(tinselSlot(i));

    // Event loop
    while (1) {
      // Step 1: try to send
      if (isPDeviceAddrValid(dest)) {
        if (getPThreadId(dest) == tinselId()) {
          // Lookup destination device
          DeviceType* destDev = &devices[getPLocalDeviceAddr(dest)];
          uint16_t oldReadyToSend = destDev->readyToSend;
          // Invoke receive handler
          destDev->recv((MessageType*) tinselSlot(0));
          // Insert device into a senders array, if not already there
          if (oldReadyToSend == NONE && destDev->readyToSend != NONE)
             *(sendersTop++) = destDev;
          // Lookup next destination
          dest = *(++neighbours);
        }
        else if (tinselCanSend()) {
          // Destination device is on another thread
          volatile MessageType* msg = (MessageType*) tinselSlot(0);
          msg->dest = getPLocalDeviceAddr(dest);
          // Send message
          tinselSend(getPThreadId(dest), msg);
          // Lookup next destination
          dest = *(++neighbours);
        }
        else {
          // Go to sleep
          tinselWaitUntil(TINSEL_CAN_RECV | TINSEL_CAN_SEND);
        }
      }
      else if (sendersTop != senders) {
        if (tinselCanSend()) {
          // Start new multicast
          multicastSource = *(--sendersTop);
          uint16_t pin = multicastSource->readyToSend-1;
          // Invoke send handler
          sendHandler(multicastSource, (MessageType*) tinselSlot(0));
          // Reinsert sender, if it still wants to send
          if (multicastSource->readyToSend != NONE) sendersTop++;
          // Determine neighbours array for sender
          neighbours = multicastSource->neighboursBase +
                         MAX_PIN_FANOUT * pin;
          // Lookup first destination
          dest = *neighbours;
        }
        else {
          // Go to sleep
          tinselWaitUntil(TINSEL_CAN_RECV | TINSEL_CAN_SEND);
        }
      }
      else {
        // Idle detection
        if (tinselIdle()) {
          for (uint32_t i = 0; i < numDevices; i++) {
            DeviceType* dev = &devices[i];
            // Invoke the idle handler for each device
            dev->idle();
            // Device ready to send?
            if (dev->readyToSend != NONE) *(sendersTop++) = dev;
          }
        }
      }

      // Step 2: try to receive
      if (tinselCanRecv()) {
        // Receive message
        MessageType *msg = (MessageType *) tinselRecv();
        // Lookup destination device
        DeviceType* dev = &devices[msg->dest];
        // Was it ready to send?
        uint16_t oldReadyToSend = dev->readyToSend;
        // Invoke receive handler
        dev->recv(msg);
        // Reallocate mailbox slot
        tinselAlloc(msg);
        // Insert device into a senders array, if not already there
        if (dev->readyToSend != NONE && oldReadyToSend == NONE)
          *(sendersTop++) = dev;
      }
    }
  }
  #endif

};

#endif
