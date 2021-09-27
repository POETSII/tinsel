// SPDX-License-Identifier: BSD-2-Clause
#ifndef _PDEVICE_H_
#define _PDEVICE_H_

#include <stdint.h>
#include <type_traits>

#ifdef TINSEL
  #include <tinsel.h>
  #define PTR(t) t*
#else
  #include <tinsel-interface.h>
  #define PTR(t) uint32_t
#endif

// Use this to align on half-cache-line boundary
#define ALIGNED __attribute__((aligned(1<<(TinselLogBytesPerLine-1))))

// This is a static limit on the number of pins per device
#ifndef POLITE_NUM_PINS
#define POLITE_NUM_PINS 1
#endif

// The local-multicast key points to a list of incoming edges.  Some
// of those edges are stored in a header, the rest in an array at a
// different location.  The number stored in the header is controlled
// by the following parameter.  If it's too low, we risk wasting
// memory bandwidth.  If it's too high, we risk wasting memory.  
// The minimum value is 0.  For large edge state sizes, use 0.
#ifndef POLITE_EDGES_PER_HEADER
#define POLITE_EDGES_PER_HEADER 6
#endif

// Macros for performance stats:
//   POLITE_DUMP_STATS - dump performance stats on termination
//   POLITE_COUNT_MSGS - include message counts in performance stats

// Thread-local device id
typedef uint16_t PLocalDeviceId;

// Thread id
typedef uint32_t PThreadId;

// Device address
// Bits 17->0: thread id
// Bit 18: invalid address
// Bits 31->19: thread-local device id
typedef uint32_t PDeviceAddr;

// Device address constructors
inline PDeviceAddr invalidDeviceAddr() { return 0x40000; }
inline PDeviceAddr makeDeviceAddr(PThreadId t, PLocalDeviceId d) {
  return (d << 19) | t;
}

// Device address deconstructors
inline bool isValidDeviceAddr(PDeviceAddr addr) { return !(addr & 0x40000); }
inline PThreadId getThreadId(PDeviceAddr addr) { return addr & 0x3ffff; }
inline PLocalDeviceId getLocalDeviceId(PDeviceAddr addr) { return addr >> 19; }

// What's the max allowed local device address?
inline uint32_t maxLocalDeviceId() { return 8192; }

// Local multicast key
typedef uint16_t Key;
#define InvalidKey 0xffff

// Pins
//   No      - means 'not ready to send'
//   HostPin - means 'send to host'
//   Pin(n)  - means 'send to application pin number n'
typedef uint8_t PPin;
constexpr PPin No = 0;
constexpr PPin HostPin = 1;
constexpr PPin Pin(uint8_t n){ return ((n)+2); }

// For template arguments that are not used
struct None {};

#include <type_traits>

template <typename T, typename = void>
struct PState_has_device_performance_counters
{
  static constexpr bool value = false;
  static constexpr unsigned count = 0;
};

template <typename T>
struct PState_has_device_performance_counters<T, decltype((void)T::device_performance_counters, void())>
{
  static constexpr unsigned count_helper()
  {
    constexpr int res= sizeof(T::device_performance_counters) / sizeof(uint32_t);
    return res;
  }

  static constexpr unsigned count = count_helper();
  static constexpr bool value = count > 0;
};



// Generic device structure
// Type parameters:
//   S - State
//   E - Edge label
//   M - Message structure
template <typename S, typename E, typename M> struct PDevice {
  static constexpr bool HasDevicePerfCounters = PState_has_device_performance_counters<S>::value;
  static constexpr unsigned NumDevicePerfCounters = PState_has_device_performance_counters<S>::count;

  // State
  S* s;
  PPin* readyToSend;
  uint32_t numVertices;
  uint16_t time;

  // Handlers
  void init();
  void send(volatile M* msg);
  void recv(M* msg, E* edge);
  bool step();
  bool finish(volatile M* msg);
};

// Generic device state structure
/* A subtlety is that a device can turn on and off its ready-to-send in the
  receive handler, and if that happens, we need to either remove it from the
  RTS list, or make sure it doesn't get added twice. This seemed to be a subtle
  bug in the original implementation, probably because this didn't happen in
  the original 
  So we need both readyToSend, which is what the device said at the end of
  the last handler, and something that tracks whether it is currently on
  the RTS list.
*/
template <typename S, int T_NUM_PINS=POLITE_NUM_PINS> struct ALIGNED PState {
  // Pointer to base of neighbours arrays
  uint16_t pinBase[T_NUM_PINS];
  // Ready-to-send status
  PPin readyToSend;
  int8_t isMarkedRTS;
  // Custom state
  S state;
};

// Message structure
template <typename M> struct PMessage {
  // Destination key
  uint16_t destKey;
  // Application message
  M payload;
};

// An outgoing edge from a device
struct POutEdge {
  // Destination mailbox
  uint16_t mbox;
  // Routing key
  uint16_t key;
  // Destination threads
  uint32_t threadMaskLow;
  uint32_t threadMaskHigh;
};

// An incoming edge to a device
template <typename E> struct PInEdge {
  // Destination device
  PLocalDeviceId devId;
  // Edge data
  E edge;
};

// An incoming edge to a device (unlabelled)
template <> struct PInEdge<None> {
  union {
    // Destination device
    PLocalDeviceId devId;
    // Unused
    None edge;
  };
};

// Header for a list of incoming edges (fixed size structure to
// support fast construction/packing of local-multicast tables)
template <typename E> struct PInHeader {
  // Number of receivers
  uint16_t numReceivers;
  // Pointer to remaining edges in inTableRest,
  // if they don't all fit in the header
  uint16_t restIndex;
  // Edges stored in the header, to make good use of cached data
  PInEdge<E> edges[POLITE_EDGES_PER_HEADER];
};

// Generic thread structure
template <typename TDeviceType,
          typename S, typename E, typename M,
          int T_NUM_PINS=POLITE_NUM_PINS,
          bool T_ENABLE_CORE_PERF_COUNTERS=false,
          bool T_ENABLE_THREAD_PERF_COUNTERS=false
          >
struct PThread {
  static constexpr int NUM_PINS = T_NUM_PINS;

  using DeviceType = TDeviceType;

  
  static constexpr bool ENABLE_CORE_PERF_COUNTERS = T_ENABLE_CORE_PERF_COUNTERS;
  static constexpr bool ENABLE_THREAD_PERF_COUNTERS = T_ENABLE_THREAD_PERF_COUNTERS;

  enum ThreadPerfCounters {
    SendHandlerCalls,
    TotalSendHandlerTime,
    BlockedSends,
    MsgsSent,   
    
    MsgsRecv,
    TotalRecvHandlerTime,

    MinBarrierActive,
    SumBarrierActiveDiv256,
    MaxBarrierActive,

    Fake_BeginBarrier, // Not an actual perf counter, just used to keep track of things
    Fake_LastActive, // Not an actual perf counter, just used to keep track of things

    Fake_ThreadPerfCounterSize
  };
  static const int NUM_THREAD_PERF_COUNTERS = ENABLE_THREAD_PERF_COUNTERS ? Fake_BeginBarrier : 0;

  // Number of devices handled by thread
  PLocalDeviceId numDevices;
  // Number of times step handler has been called
  uint16_t time;
  // Number of devices in graph
  uint32_t numVertices;
  // Pointer to array of device states
  using DevState = PState<S,NUM_PINS>; // work around macro expansion
  PTR(DevState) devices;
  // Pointer to base of routing tables
  PTR(POutEdge) outTableBase;
  PTR(PInHeader<E>) inTableHeaderBase;
  PTR(PInEdge<E>) inTableRestBase;
  // Array of local device ids are ready to send
  PTR(PLocalDeviceId) senders;
  // This array is accessed in a LIFO manner
  PTR(PLocalDeviceId) sendersTop;

  // Count number of messages sent
  #ifdef POLITE_COUNT_MSGS
  // Total messages sent
  uint32_t msgsSent;
  // Total messages received
  uint32_t msgsReceived;
  // Number of times we wanted to send but couldn't
  uint32_t blockedSends;
  #endif

  // This will probably take up space even when array is length 0, but it wont be accessed
  // unless thread perf is turned on.
  uint32_t thread_performance_counters[ENABLE_THREAD_PERF_COUNTERS ? Fake_ThreadPerfCounterSize : 0];
  uint32_t paddddd[32];

  #ifdef TINSEL

  INLINE bool senders_queue_empty() const
  { return senders==sendersTop; }

  //! \pre: !senders_queue_empty()
  PLocalDeviceId senders_queue_pop()
  {
    PLocalDeviceId id=*(--senders);
    devices[id].isMarkedRTS=false;
    return id;
  }

  void senders_queue_add(PLocalDeviceId id)
  {
    if(!devices[id].isMarkedRTS){
      *(senders++) = id;
      devices[id].isMarkedRTS=true;
    }
  }

  // Helper function to construct a device
  INLINE DeviceType getDevice(uint32_t id) {
    DeviceType dev;
    dev.s           = &devices[id].state;
    dev.readyToSend = &devices[id].readyToSend;
    dev.numVertices = numVertices;
    dev.time        = time;
    return dev;
  }

  // Dump performance counter stats over UART
  void dumpStats() {
    tinselPerfCountStop();
    uint32_t me = tinselId();
    // Per-cache performance counters
    uint32_t cacheMask = (1 <<
      (TinselLogThreadsPerCore + TinselLogCoresPerDCache)) - 1;
    if ((me & cacheMask) == 0) {
      printf("H:%x,M:%x,W:%x\n",
        tinselHitCount(),
        tinselMissCount(),
        tinselWritebackCount());
    }
    // Per-core performance counters
    uint32_t coreMask = (1 << (TinselLogThreadsPerCore)) - 1;
    if ((me & coreMask) == 0) {
      printf("C:%x %x,I:%x %x\n",
        tinselCycleCountU(), tinselCycleCount(),
        tinselCPUIdleCountU(), tinselCPUIdleCount());
    }
    // Per-thread performance counters
    #ifdef POLITE_COUNT_MSGS
    uint32_t intraBoardId = me & ((1<<TinselLogThreadsPerBoard) - 1);
    uint32_t progRouterSent =
      intraBoardId == 0 ? tinselProgRouterSent() : 0;
    uint32_t progRouterSentInter =
      intraBoardId == 0 ? tinselProgRouterSentInterBoard() : 0;
    printf("MS:%x,MR:%x,PR:%x,PRI:%x,BL:%x\n",
      msgsSent, msgsReceived, progRouterSent,
        progRouterSentInter, blockedSends);
    #endif
  }

  void __attribute__((noinline)) dumpPerformanceCountersHelper(const char *pattern, unsigned group, unsigned n, const uint32_t *v)
  {
    uint32_t me=tinselId();
    for(unsigned i=0; i<n; i++){
      printf(pattern, me, group, i, v[i]);
    }
  }

    void dumpDevicePerformanceCounters()
    {
      if constexpr(DeviceType::HasDevicePerfCounters){
        for (uint32_t i = 0; i < numDevices; i++) {
          DeviceType dev = getDevice(i);
          static_assert(sizeof(dev.s->device_performance_counters[0])==4);
          dumpPerformanceCountersHelper("DPC:%x,%x,%x,%x\n", i, DeviceType::NumDevicePerfCounters, dev.s->device_performance_counters);
        }
      }
    }

    void dumpThreadPerformanceCounters()
    {
      if constexpr(ENABLE_THREAD_PERF_COUNTERS){
        dumpPerformanceCountersHelper("ThPC:%x,%x,%x,%x\n", 0, NUM_THREAD_PERF_COUNTERS, thread_performance_counters);
      }
    }

  void dumpCorePerformanceCounters()
  {
    if constexpr(ENABLE_CORE_PERF_COUNTERS){
      tinselPerfCountStop();

      uint32_t me = tinselId();
    
      // Per-cache performance counters
      uint32_t cacheMask = (1 << (TinselLogThreadsPerCore + TinselLogCoresPerDCache)) - 1;
      if ((me & cacheMask) == 0) {
        uint32_t counters[]={ tinselHitCount(), tinselMissCount(), tinselWritebackCount() };
        dumpPerformanceCountersHelper("CaPC:%x,%x,%x,%x\n", 0, std::size(counters), counters);
      }

      // Per-core performance counters
      uint32_t coreMask = (1 << (TinselLogThreadsPerCore)) - 1;
      if ((me & coreMask) == 0) {
        uint32_t counters[]={ tinselCycleCountU(), tinselCycleCount(), tinselCPUIdleCountU(), tinselCPUIdleCount() };
        dumpPerformanceCountersHelper("CoPC:%x,%x,%x,%x\n", 0, std::size(counters), counters);
      }

      // Per board performance counters
      uint32_t intraBoardId = me & ((1<<TinselLogThreadsPerBoard) - 1);
      if(intraBoardId==0){
        uint32_t counters[]={tinselProgRouterSent(), tinselProgRouterSentInterBoard()};
        dumpPerformanceCountersHelper("BoPC:%x,%x,%x,%x\n", 0, std::size(counters), counters);
      }
    }
  }

  // Invoke device handlers
  void run() {
    // Current out-going edge in multicast
    POutEdge* outEdge;

    // Outgoing edge to host
    POutEdge outHost[2];
    outHost[0].mbox = tinselHostId() >> TinselLogThreadsPerMailbox;
    outHost[0].key = 0;
    outHost[1].key = InvalidKey;
    // Initialise outEdge to null terminator
    outEdge = &outHost[1];

    // Did last call to step handler request a new time step?
    bool active = true;

    // Reset performance counters
    tinselPerfCountReset();
    if constexpr(ENABLE_THREAD_PERF_COUNTERS){
      for(unsigned i=0; i<Fake_ThreadPerfCounterSize; i++){
        thread_performance_counters[i]=0;
      }
    }

    // Initialisation
    sendersTop = senders;
    for (uint32_t i = 0; i < numDevices; i++) {
      DeviceType dev = getDevice(i);
      // Invoke the initialiser for each device
      dev.init();
      devices[i].isMarkedRTS=false;
      // Device ready to send?
      if (*dev.readyToSend != No) {
        senders_queue_add(i);
      }
    }

    // Set number of flits per message
    static_assert(sizeof(PMessage<M>) <= (1<<TinselLogBytesPerMsg));
    tinselSetLen((sizeof(PMessage<M>)-1) >> TinselLogBytesPerFlit);

    // Event loop
    while (1) {
      // Step 1: try to send
      if (outEdge->key != InvalidKey) {
        if (tinselCanSend()) {
          PMessage<M>* m = (PMessage<M>*) tinselSendSlot();
          // Send message
          if(outEdge == outHost){
            tinselSend(tinselHostId(), m);
          }else{
              m->destKey = outEdge->key;
              tinselMulticast(outEdge->mbox, outEdge->threadMaskHigh,
                outEdge->threadMaskLow, m);
          }
          #ifdef POLITE_COUNT_MSGS
          msgsSent++;
          #endif
          if constexpr(ENABLE_THREAD_PERF_COUNTERS){
            thread_performance_counters[MsgsSent] ++; 
          }
          // Move to next neighbour
          outEdge++;
        }
        else {
          #ifdef POLITE_COUNT_MSGS
          blockedSends++;
          #endif
          if constexpr(ENABLE_THREAD_PERF_COUNTERS){
            thread_performance_counters[BlockedSends] ++; 
          }
          tinselWaitUntil(TINSEL_CAN_SEND|TINSEL_CAN_RECV);
        }
      }
      else if (!senders_queue_empty()) {
        if (tinselCanSend()) {
          // Start new multicast
          PLocalDeviceId src = senders_queue_pop();
          // Lookup device
          DeviceType dev = getDevice(src);
          PPin pin = *dev.readyToSend;
          // A pin could have changed it's mind about sending, yet still be on 
          // the list, so check whether it wants to send.
          if(pin == No){
            // We don't need to do anything. outEdge is still invalid.
            // We'll go back round the loop
          }else{
            // Invoke send handler
            PMessage<M>* m = (PMessage<M>*) tinselSendSlot();

            uint32_t start;
            if constexpr(ENABLE_THREAD_PERF_COUNTERS){
              start=tinselCycleCount();
            }
            dev.send(&m->payload);
            if constexpr(ENABLE_THREAD_PERF_COUNTERS){
              thread_performance_counters[SendHandlerCalls]++;
              thread_performance_counters[TotalSendHandlerTime] += tinselCycleCount() - start;
            }
            // Reinsert sender, if it still wants to send
            if (*dev.readyToSend != No) {
              senders_queue_add(src);
            }
            // Determine out-edge array for sender
            if (pin == HostPin){
              outEdge = outHost;
            }else{
              outEdge = (POutEdge*) &outTableBase[
                devices[src].pinBase[pin-2]
              ];
            }
          }
        }
        else {
          #ifdef POLITE_COUNT_MSGS
          blockedSends++;
          #endif
          if constexpr(ENABLE_THREAD_PERF_COUNTERS){
            thread_performance_counters[BlockedSends] ++; 
          }
          tinselWaitUntil(TINSEL_CAN_SEND|TINSEL_CAN_RECV);
        }
      }
      else {
        // Idle detection
        if constexpr(ENABLE_THREAD_PERF_COUNTERS){
          thread_performance_counters[Fake_LastActive] = tinselCycleCount(); 
        }
        int idle = tinselIdle(!active);
        if (idle > 1)
          break;
        else if (idle) {
          if constexpr(ENABLE_THREAD_PERF_COUNTERS){
            uint32_t delta=thread_performance_counters[Fake_LastActive] - thread_performance_counters[Fake_BeginBarrier];
            thread_performance_counters[Fake_BeginBarrier] = tinselCycleCount();
            thread_performance_counters[MinBarrierActive] = std::min(thread_performance_counters[MinBarrierActive], delta);
            thread_performance_counters[MaxBarrierActive] = std::max(thread_performance_counters[MaxBarrierActive], delta);
            thread_performance_counters[SumBarrierActiveDiv256] += delta / 256;
          }

          active = false;
          for (uint32_t i = 0; i < numDevices; i++) {
            DeviceType dev = getDevice(i);
            // Invoke the step handler for each device
            active = dev.step() || active;
            // Device ready to send?
            if (*dev.readyToSend != No) {
              senders_queue_add(i);
            }
          }
          time++;
        }
      }

      // Step 2: try to receive
      while (tinselCanRecv()) {
        PMessage<M>* inMsg = (PMessage<M>*) tinselRecv();
        PInHeader<E>* inHeader = &inTableHeaderBase[inMsg->destKey];
        // Determine number and location of edges/receivers
        uint32_t numReceivers = inHeader->numReceivers;
        PInEdge<E>* inEdge = inHeader->edges;
        // For each receiver
        for (uint32_t i = 0; i < numReceivers; i++) {
          if (i == POLITE_EDGES_PER_HEADER)
            inEdge = &inTableRestBase[inHeader->restIndex];
          // Lookup destination device
          PLocalDeviceId id = inEdge->devId;
          DeviceType dev = getDevice(id);
          // Invoke receive handler
          uint32_t start;
          if constexpr(ENABLE_THREAD_PERF_COUNTERS){
            start=tinselCycleCount();
          }
          dev.recv(&inMsg->payload, &inEdge->edge);
          if constexpr(ENABLE_THREAD_PERF_COUNTERS){
            thread_performance_counters[TotalRecvHandlerTime] += tinselCycleCount() - start;
            thread_performance_counters[MsgsRecv] ++;
          }
          // Insert device into a senders array, if not already there
          if (*dev.readyToSend != No) {
            senders_queue_add(id);
          }
          inEdge++;
          #ifdef POLITE_COUNT_MSGS
          msgsReceived++;
          #endif
          
        }
        tinselFree(inMsg);
      }
    }

    // Termination
    #ifdef POLITE_DUMP_STATS
      dumpStats();
    #endif

    // Invoke finish handler for each device
    for (uint32_t i = 0; i < numDevices; i++) {
      DeviceType dev = getDevice(i);
      tinselWaitUntil(TINSEL_CAN_SEND);
      PMessage<M>* m = (PMessage<M>*) tinselSendSlot();
      if (dev.finish(&m->payload)) tinselSend(tinselHostId(), m);
    }

    dumpDevicePerformanceCounters();
    dumpThreadPerformanceCounters();
    dumpCorePerformanceCounters();

    // Sleep. Next message in will free us up, and then we
    // will return back into the bootloader.
    tinselWaitUntil(TINSEL_CAN_RECV);
  }

  #endif

};

#endif
