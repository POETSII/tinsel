#ifndef _PDEVICE_H_
#define _PDEVICE_H_

#include <stdint.h>

#ifdef TINSEL
  #include <tinsel.h>
  #define PTR(t) t*
#else
  #define PTR(t) uint32_t
#endif

// Physical device identifier
typedef uint16_t PLocalDeviceAddr;
typedef uint16_t PThreadId;
struct PDeviceAddr {
  PThreadId threadId;
  PLocalDeviceAddr localAddr;
};

// Generic device structure
struct PDevice {
  // Thread local device id
  PLocalDeviceAddr localAddr;
  // Is the device ready to send?
  uint8_t readyToSend;
  // If ready to send, what is the destination?
  PDeviceAddr dest;
  // Number of incoming and outgoing edges
  uint16_t fanIn, fanOut;

  #ifdef TINSEL
    // Obtain device id
    inline PDeviceAddr thisDeviceId() {
      PDeviceAddr devId;
      devId.threadId = tinselId();
      devId.localAddr = localAddr;
      return devId;
    }
    // Obtain device id of host
    inline PDeviceAddr hostDeviceId() {
      PDeviceAddr devId;
      devId.threadId = tinselHostId();
      devId.localAddr = 0;
      return devId;
    }
  #else
    inline PDeviceAddr undef() { 
      PDeviceAddr devId;
      devId.threadId = 0xffff;
      devId.localAddr = 0xffff;
      return devId;
    }
    // Obtain device id
    inline PDeviceAddr thisDeviceId() { return undef(); }
    // Obtain device id of host
    inline PDeviceAddr hostDeviceId() { return undef(); }
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
  // Device size (number of bytes)
  uint16_t deviceSize;
  // Pointer to array of devices
  PTR(uint8_t) devices;
  // Array of pointers to devices that are ready to send internally
  // (to device on same thread) or externally (to device on different thread)
  PTR(PTR(DeviceType)) intArray;
  PTR(PTR(DeviceType)) extArray;
  // These arrays are accessed in a LIFO manner
  PTR(PTR(DeviceType)) intTop;
  PTR(PTR(DeviceType)) extTop;

  // Get pointer to device at given index
  inline DeviceType* getDevicePtr(uint32_t i) {
    return (DeviceType*) (devices + (i*deviceSize));
  }

  #ifdef TINSEL
  // Insert device into internal senders or external senders
  inline void insert(DeviceType* dev) {
    if (dev->dest.threadId == tinselId())
      *(intTop++) = dev;
    else
      *(extTop++) = dev;
  }

  // Invoke device handlers
  void run() {
    // Initialisation
    intTop = intArray;
    extTop = extArray;
    for (uint32_t i = 0; i < numDevices; i++) {
      DeviceType* dev = getDevicePtr(i);
      // Invoke the initialiser for each device
      dev->init();
      // Device ready to send?
      if (dev->readyToSend) insert(dev);
    }

    // Set number of flits per message
    tinselSetLen((sizeof(MessageType)-1) >> TinselLogBytesPerFlit);

    // Allocate some slots for incoming messages
    // (Slot 0 is reserved for outgoing messages)
    for (int i = 1; i <= 4; i++) tinselAlloc(tinselSlot(i));

    // Event loop
    while (1) {
      if (tinselCanSend() && extTop != extArray) { // External senders
        // Lookup the next external sender
        DeviceType* dev = *(--extTop);
        // Destination device is on another thread
        MessageType* msg = (MessageType*) tinselSlot(0);
        msg->dest = dev->dest.localAddr;
        PThreadId destThread = dev->dest.threadId;
        // Invoke send handler & send resulting message
        sendHandler(dev, msg);
        tinselSend(destThread, msg);
        // Reinsert device into a senders array
        if (dev->readyToSend) insert(dev);
      }
      else if (tinselCanRecv()) {
        // Receive message
        MessageType *msg = (MessageType *) tinselRecv();
        // Lookup destination device
        DeviceType* dev = getDevicePtr(msg->dest);
        // Was it ready to send?
        uint32_t wasReadyToSend = dev->readyToSend;
        // Invoke receive handler
        dev->recv(msg);
        // Reallocate mailbox slot
        tinselAlloc(msg);
        // Insert device into a senders array, if not already there
        if (dev->readyToSend && !wasReadyToSend) insert(dev);
      }
      else if (intTop != intArray) { // Internal senders
        MessageType msg;
        // Lookup the next internal sender
        DeviceType* dev = *(--intTop);
        // Lookup destination device
        DeviceType* destDev = getDevicePtr(dev->dest.localAddr);
        uint32_t wasReadyToSend = destDev->readyToSend;
        // Invoke both send and recv handlers
        dev->send(&msg);
        destDev->recv(&msg);
        // Insert devices into a senders array, if not already there
        if (dev->readyToSend) insert(dev);
        if (!wasReadyToSend && destDev->readyToSend) insert(destDev);
      }
      else {
        // Go to sleep
        TinselWakeupCond cond = TINSEL_CAN_RECV |
          (extTop == extArray ? (TinselWakeupCond) 0 : TINSEL_CAN_SEND);
        tinselWaitUntil(cond);
      }
    }
  }
  #endif

};

// Obtain edge with given index
template <typename T> inline PDeviceAddr edge(T* dev, uint32_t i)
{ 
  uint32_t* ptr = (uint32_t*) dev + ((sizeof(T)+3)>>2) + i;
  return *((PDeviceAddr*) ptr);
}

// Obtain outgoing edge with given index
template <typename T> inline PDeviceAddr outEdge(T* dev, uint32_t i)
{ 
  return edge(dev, i);
}

#endif
