// SPDX-License-Identifier: BSD-2-Clause
#ifndef _PGRAPH_H_
#define _PGRAPH_H_

#include <algorithm>
#include <numeric>
#include <array>
#include <random>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <HostLink.h>
#include <config.h>
#include <POLite.h>
#include <POLite/Seq.h>
#include <POLite/Graph.h>
#include <POLite/Placer.h>
#include <POLite/Bitmap.h>
#include <POLite/ProgRouters.h>
#include <type_traits>
#include <tinsel-interface.h>
#include <array>
#include <POLite/SpinLock.h>
#include <vsnprintf_to_string.h>
#include "ParallelRunningStats.hpp"

#include <functional>
#include <unordered_set>

#include "ParallelFor.h"

// Nodes of a POETS graph are devices
typedef NodeId PDeviceId;

// This structure holds a group of receiving edges on a thread.
// All of the edges originate from the same output pin.
template <typename E> struct PReceiverGroup {
  // Thread id where all the receivers reside
  uint32_t threadId;
  // A sequence of receiving devices on that thread
  SmallSeq<PInEdge<E>> receivers;
};

// This structure holds info about an edge destination
struct PEdgeDest {
  // Index of edge in outgoing edge list
  uint32_t index = 0;
  // Destination device
  PDeviceId dest = 0;
  // Address where destination is located
  PDeviceAddr addr = 0;
};

// Comparison function for PEdgeDest
// (Useful to sort destinations by thread id of destination)
inline int cmpEdgeDestT(const PEdgeDest &d0, const PEdgeDest &d1) {
  return getThreadId(d0.addr) < getThreadId(d1.addr);
}

inline int cmpEdgeDest(const void* e0, const void* e1) {
  PEdgeDest* d0 = (PEdgeDest*) e0;
  PEdgeDest* d1 = (PEdgeDest*) e1;
  return cmpEdgeDest(&d0, &d1);
}

// POETS graph
template <typename DeviceType,
          typename S, typename E, typename M, int T_NUM_PINS=POLITE_NUM_PINS> class PGraph {
public:
  static constexpr int NUM_PINS = T_NUM_PINS;

 private:
  // Align address to 2^n byte boundary
  inline uint32_t align(uint32_t n, uint32_t addr) {
    if ((addr & ((1u<<n)-1)) == 0) return addr;
    return ((addr >> n) + 1) << n;
  }

  // Align address to 32-bit word boundary
  uint32_t wordAlign(uint32_t addr) { return align(2, addr); }

  // Align address to cache-line boundary
  uint32_t cacheAlign(uint32_t addr) {
    return align(TinselLogBytesPerLine, addr);
  }

  // Helper function
  inline uint32_t min(uint32_t x, uint32_t y) { return x < y ? x : y; }

  // Number of FPGA boards available
  uint32_t meshLenX;
  uint32_t meshLenY;

  // Number of FPGA boards to use
  uint32_t numBoardsX;
  uint32_t numBoardsY;

  // Multicast routing tables:
  // Sequence of outgoing edges for every (device, pin) pair
  Seq<POutEdge>*** outTable;

  struct PerThreadRoutingInfo
  {
    // I (dt10) inlined here rather than keeping as pointers.
    // It wastes space if threads are very sparsely populated, but
    // simplifies things a bit in terms of alloc and de-alloc

    //Sequence of in-edge headers
    Seq<PInHeader<E>> inTableHeaders;
    Seq<PInEdge<E>> inTableRest;
    Bitmap inTableBitmap;
    SpinLock lock;
  };

  struct PerMailboxRoutingInfo
  {
    SpinLock lock;
    unsigned lowestFreeIndexPerThread[TinselThreadsPerMailbox];
    static_assert(TinselThreadsPerMailbox==64, "64-bit masks assumes there are 64 threads per mailbox.");
    Seq<uint64_t> threadBitMaps;

    PerMailboxRoutingInfo()
    {
      std::fill(lowestFreeIndexPerThread, lowestFreeIndexPerThread+TinselThreadsPerMailbox, 0);
    }
  };

  PerThreadRoutingInfo *perThreadRoutingInfo;

  PerMailboxRoutingInfo *perMailboxRoutingInfo;

  // Programmable routing tables
  ProgRouterMesh* progRouterTables;

  // Generic constructor
  void constructor(uint32_t lenX, uint32_t lenY) {
    meshLenX = lenX;
    meshLenY = lenY;
    char* str = getenv("POLITE_BOARDS_X");
    int nx = str ? atoi(str) : meshLenX;
    str = getenv("POLITE_BOARDS_Y");
    int ny = str ? atoi(str) : meshLenY;
    setNumBoards(nx, ny);
    numDevices = 0;
    devices = NULL;
    toDeviceAddr = NULL;
    numDevicesOnThread = NULL;
    fromDeviceAddr = NULL;
    vertexMem = NULL;
    vertexMemSize = NULL;
    vertexMemBase = NULL;
    inEdgeHeaderMem = NULL;
    inEdgeHeaderMemSize = NULL;
    inEdgeHeaderMemBase = NULL;
    inEdgeRestMem = NULL;
    inEdgeRestMemSize = NULL;
    inEdgeRestMemBase = NULL;
    outEdgeMem = NULL;
    outEdgeMemSize = NULL;
    outEdgeMemBase = NULL;
    mapVerticesToDRAM = false;
    mapInEdgeHeadersToDRAM = true;
    mapInEdgeRestToDRAM = true;
    mapOutEdgesToDRAM = true;
    outTable = NULL;
    perThreadRoutingInfo = NULL;
    perMailboxRoutingInfo = NULL;
    progRouterTables = NULL;
    chatty = 0;
    str = getenv("POLITE_CHATTY");
    if (str != NULL) {
      chatty = !strcmp(str, "0") ? 0 : 1;
    }
  }

 public:
  // Number of devices
  uint32_t numDevices;

  // Graph containing device ids and connections
  Graph<E> graph;

  // Edge labels: has same structure as graph.outgoing
  //Seq<Seq<E>*> edgeLabels;

  // Mapping from device id to device state
  // (Not valid until the mapper is called)
  PState<S,NUM_PINS>** devices;

  // Mapping from thread id to number of devices on that thread
  // (Not valid until the mapper is called)
  uint32_t* numDevicesOnThread;

  // Mapping from device id to device address and back
  // (Not valid until the mapper is called)
  PDeviceAddr* toDeviceAddr;  // Device id -> device address
  PDeviceId** fromDeviceAddr; // Device address -> device id

  // Each thread's vertex mem and thread mem regions
  // (Not valid until the mapper is called)
  uint8_t** vertexMem;      uint8_t** threadMem;
  uint32_t* vertexMemSize;  uint32_t* threadMemSize;
  uint32_t* vertexMemBase;  uint32_t* threadMemBase;

  // Each thread's in-edge and out-edge regions
  // (Not valid until the mapper is called)
  uint8_t** inEdgeHeaderMem;      uint8_t** inEdgeRestMem;
  uint32_t* inEdgeHeaderMemSize;  uint32_t* inEdgeRestMemSize;
  uint32_t* inEdgeHeaderMemBase;  uint32_t* inEdgeRestMemBase;
  uint8_t** outEdgeMem;
  uint32_t* outEdgeMemSize;
  uint32_t* outEdgeMemBase;

  

  // Where to map the various regions
  // (If false, map to SRAM instead)
  bool mapVerticesToDRAM;
  bool mapInEdgeHeadersToDRAM;
  bool mapInEdgeRestToDRAM;
  bool mapOutEdgesToDRAM;

  // Allow mapper to print useful information to stdout
  uint32_t chatty;

  ParallelFlag use_parallel=ParallelFlag::Default;

  static constexpr bool is_simulation = false;

  PlacerMethod placer_method=PlacerMethod::Default;

  std::function<void(const char *message)> on_fatal_error;
  std::function<void(int direction, const char *part)> on_phase_hook;
  std::function<void(const char *key, double value)> on_export_value;
  std::function<void(const char *key, const char *value)> on_export_string;

  [[noreturn]] void fatal_error(const char *message, ...)
  {
    va_list va;
    va_start(va, message);
    char *fmsg=vsprintf_to_string(message, va);
    va_end(va);

    if(on_fatal_error){
      on_fatal_error(fmsg);
      assert(0);
      abort(); // Should never get here
    }else{
      puts(fmsg);
      exit(1);
    }
  }

  // Used to record when we move between main sections in the mapping process.
  void mark_phase(const char *name)
  {
    if(on_phase_hook){
      on_phase_hook(0, name);
    }
  }

  void begin_phase(const char *name)
  {
    if(on_phase_hook){
      on_phase_hook(+1, name);
    }
  }

  void end_phase(const char *name)
  {
    if(on_phase_hook){
      on_phase_hook(-1, name);
    }
  }

  // Setter for number of boards to use
  void setNumBoards(uint32_t x, uint32_t y) {
    if (x > meshLenX || y > meshLenY) {
      fatal_error("Mapper: %d x %d boards requested, %d x %d available\n",
        numBoardsX, numBoardsY, meshLenX, meshLenY);
    }
    if(chatty && ((x<meshLenX) || (y<meshLenY))){
      fprintf(stderr, "  Using board mesh size of %d x %d in total mesh size of %d x %d\n", (int)x, (int)y, (int)meshLenX, (int)meshLenY);
    }
    numBoardsX = x;
    numBoardsY = y;
  }

  // Create new device
  inline PDeviceId newDevice() {
    //edgeLabels.append(new SmallSeq<E>);
    numDevices++;
    return graph.newNode();
  }

  // Creates n new devices with contiguous ids, and returns the first id
  inline PDeviceId newDevices(uint32_t n)
  {
    PDeviceId base=graph.newNodes(n);
    numDevices+=n;
    /*edgeLabels.extendBy(n);
    for(unsigned i=base; i<base+n; i++){
      edgeLabels[i]=new SmallSeq<E>;
    }*/
    return base;
  }

    // Add labelled edge using given output pin
  void addLabelledEdge(E edge, PDeviceId from, PinId pin, PDeviceId to) {
    if (pin >= NUM_PINS) {
      fatal_error("addEdge: pin exceeds NUM_PINS\n");
    }
    graph.addEdge(edge, from, pin, to);
    //edgeLabels.elems[from]->append(edge);
  }

  // Add a connection between devices
  inline void addEdge(PDeviceId from, PinId pin, PDeviceId to) {
    E edge;
    graph.addEdge(edge, from, pin, to);
  }

  void reserveOutgoingEdgeSpace(PDeviceId from, PinId pin, size_t n)
  {
    graph.reserveOutgoingEdgeSpace(from, pin, n);
  }

  void setDeviceWeight(NodeId id, unsigned weight)
  {
    assert(weight>0);
    graph.setWeight(id, weight);
  }

  unsigned numThreads() const
  {
    return meshLenX*meshLenY*TinselLogThreadsPerBoard;
  }
  /*! Adds an edge, with the constraint that no other thread
    will try to use the same from edge at the same time. Howver,
    the destination thread can be used by many threads in parallel.
  */
  void addLabelledEdgeLockedDst(E edge, PDeviceId from, PinId pin, PDeviceId to)
  {
    if (pin >= NUM_PINS) {
      fatal_error("addEdge: pin exceeds NUM_PINS\n");
    }
    graph.addEdgeLockedDst(edge, from, pin, to);
  }

  /*! Adds an edge, with the constraint that no other thread
    will try to use the same from edge at the same time. Howver,
    the destination thread can be used by many threads in parallel.
  */
  void addEdgeLockedDst(PDeviceId from, PinId pin, PDeviceId to)
  {
    if (pin >= NUM_PINS) {
      fatal_error("addEdge: pin exceeds NUM_PINS\n");
    }
    graph.addEdgeLockedDst({}, from, pin, to);
  }


  /*
  dt10 : the random POLITE_NOINLINE attributes are to avoid the
  compiler flattening them, so that for profiling and debugging you can
  see where you are. While they can be inlined, the performance improvement
  is minimal.
  */


  // Allocate SRAM and DRAM partitions
  __attribute__((noinline)) void allocatePartitions() {
    mark_phase("allocPartitions");

    // Decide a maximum partition size that is reasonable
    // SRAM: Partition size minus 2048 bytes for the stack
    uint32_t maxSRAMSize = (1<<TinselLogBytesPerSRAMPartition) - 2048;
    // DRAM: Partition size minus 65536 bytes for the stack
    uint32_t maxDRAMSize = (1<<TinselLogBytesPerDRAMPartition) - 65536;
    // Allocate partition sizes and bases
    vertexMem = (uint8_t**) calloc(TinselMaxThreads, sizeof(uint8_t*));
    vertexMemSize = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
    vertexMemBase = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
    threadMem = (uint8_t**) calloc(TinselMaxThreads, sizeof(uint8_t*));
    threadMemSize = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
    threadMemBase = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
    inEdgeHeaderMem = (uint8_t**) calloc(TinselMaxThreads, sizeof(uint8_t*));
    inEdgeHeaderMemSize =
      (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
    inEdgeHeaderMemBase =
      (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
    inEdgeRestMem = (uint8_t**) calloc(TinselMaxThreads, sizeof(uint8_t*));
    inEdgeRestMemSize = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
    inEdgeRestMemBase = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
    outEdgeMem = (uint8_t**) calloc(TinselMaxThreads, sizeof(uint8_t*));
    outEdgeMemSize = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
    outEdgeMemBase = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));

    const bool EnableStats=(bool)on_export_value;

    ParallelRunningStats<8> sizeStatistics;

    // Compute partition sizes for each thread
    parallel_for_blocked<unsigned>(use_parallel, 0, TinselMaxThreads, 256, [&](uint32_t beginThreadId, uint32_t endThreadId){
      ParallelRunningStats<8> sizeStatisticsLocal;
      for(uint32_t threadId=beginThreadId; threadId<endThreadId; threadId++){

        // This variable is used to count the size of the *initialised*
        // partition.  The total partition size is larger as it includes
        // uninitialised portions.
        uint32_t sizeVMem = 0;
        uint32_t sizeEIHeaderMem = 0;
        uint32_t sizeEIRestMem = 0;
        uint32_t sizeEOMem = 0;
        uint32_t sizeTMem = 0;
        // Add space for thread structure (always stored in SRAM)
        sizeTMem = cacheAlign(sizeof(PThread<DeviceType, S, E, M, NUM_PINS>));
        // Add space for devices
        uint32_t numDevs = numDevicesOnThread[threadId];
        for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
          // Add space for device
          sizeVMem = sizeVMem + sizeof(PState<S,NUM_PINS>);
        }
        PerThreadRoutingInfo *tbi=perThreadRoutingInfo+threadId;
        // Add space for incoming edge tables
        if (tbi->inTableHeaders.numElems ) {
          sizeEIHeaderMem = tbi->inTableHeaders.numElems *
                              sizeof(PInHeader<E>);
          sizeEIHeaderMem = wordAlign(sizeEIHeaderMem);
        }
        if (tbi->inTableRest.numElems) {
          sizeEIRestMem = tbi->inTableRest.numElems * sizeof(PInEdge<E>);
          sizeEIRestMem = wordAlign(sizeEIRestMem);
        }
        // Add space for outgoing edge table
        unsigned totalFanIn=0, totalFanOut=0;
        for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
          PDeviceId id = fromDeviceAddr[threadId][devNum];
          for (uint32_t p = 0; p < NUM_PINS; p++) {
            Seq<POutEdge>* edges = outTable[id][p];
            sizeEOMem += sizeof(POutEdge) * edges->numElems;
          }

          // This adds a bit of unpredictable memory traffic, but the stats are quite useful
          if(EnableStats){
            totalFanIn += graph.fanIn(id);
            totalFanOut += graph.fanOut(id);
          }
        }
        sizeEOMem = wordAlign(sizeEOMem);
        // The total partition size including uninitialised portions
        uint32_t totalSizeVMem =
          sizeVMem + wordAlign(sizeof(PLocalDeviceId) * numDevs);
        // Check that total size is reasonable
        uint32_t totalSizeSRAM = sizeTMem;
        uint32_t totalSizeDRAM = 0;
        if (mapVerticesToDRAM) totalSizeDRAM += totalSizeVMem;
                          else totalSizeSRAM += totalSizeVMem;
        if (mapInEdgeHeadersToDRAM) totalSizeDRAM += sizeEIHeaderMem;
                              else totalSizeSRAM += sizeEIHeaderMem;
        if (mapInEdgeRestToDRAM) totalSizeDRAM += sizeEIRestMem;
                            else totalSizeSRAM += sizeEIRestMem;
        if (mapOutEdgesToDRAM) totalSizeDRAM += sizeEOMem;
                          else totalSizeSRAM += sizeEOMem;
        if (totalSizeDRAM > maxDRAMSize) {
          fatal_error("Error: max DRAM partition size exceeded\n");
        }
        if (totalSizeSRAM > maxSRAMSize) {
          fatal_error("Error: max SRAM partition size exceeded\n");
        }
        // Allocate space for the initialised portion of the partition
        assert((sizeVMem%4) == 0);
        assert((sizeTMem%4) == 0);
        assert((sizeEIHeaderMem%4) == 0);
        assert((sizeEIRestMem%4) == 0);
        assert((sizeEOMem%4) == 0);
        vertexMem[threadId] = (uint8_t*) calloc(sizeVMem, 1);
        vertexMemSize[threadId] = sizeVMem;
        threadMem[threadId] = (uint8_t*) calloc(sizeTMem, 1);
        threadMemSize[threadId] = sizeTMem;
        inEdgeHeaderMem[threadId] = (uint8_t*) calloc(sizeEIHeaderMem, 1);
        inEdgeHeaderMemSize[threadId] = sizeEIHeaderMem;
        inEdgeRestMem[threadId] = (uint8_t*) calloc(sizeEIRestMem, 1);
        inEdgeRestMemSize[threadId] = sizeEIRestMem;
        outEdgeMem[threadId] = (uint8_t*) calloc(sizeEOMem, 1);
        outEdgeMemSize[threadId] = sizeEOMem;
        // Tinsel address of base of partition
        uint32_t partId = threadId & (TinselThreadsPerDRAM-1);
        uint32_t sramBase = (1 << TinselLogBytesPerSRAM) +
            (partId << TinselLogBytesPerSRAMPartition);
        uint32_t dramBase = TinselBytesPerDRAM -
            ((partId+1) << TinselLogBytesPerDRAMPartition);
        // Use partition-interleaved region for DRAM
        dramBase |= 0x80000000;
        threadMemBase[threadId] = sramBase;
        sramBase += threadMemSize[threadId];
        // Determine base addresses of each region
        if (mapVerticesToDRAM) {
          vertexMemBase[threadId] = dramBase;
          dramBase += totalSizeVMem;
        }
        else {
          vertexMemBase[threadId] = sramBase;
          sramBase += totalSizeVMem;
        }
        if (mapInEdgeHeadersToDRAM) {
          inEdgeHeaderMemBase[threadId] = dramBase;
          dramBase += sizeEIHeaderMem;
        }
        else {
          inEdgeHeaderMemBase[threadId] = sramBase;
          sramBase += sizeEIHeaderMem;
        }
        if (mapInEdgeRestToDRAM) {
          inEdgeRestMemBase[threadId] = dramBase;
          dramBase += sizeEIRestMem;
        }
        else {
          inEdgeRestMemBase[threadId] = sramBase;
          sramBase += sizeEIRestMem;
        }
        if (mapOutEdgesToDRAM) {
          outEdgeMemBase[threadId] = dramBase;
          dramBase += sizeEOMem;
        }
        else {
          outEdgeMemBase[threadId] = sramBase;
          sramBase += sizeEOMem;
        }

        if(EnableStats){
          if(numDevs>0){
            sizeStatisticsLocal += std::array<double,8>{
                (double)sizeVMem, (double)sizeEIHeaderMem, (double)sizeEIRestMem, (double)sizeEOMem, (double)totalSizeDRAM, (double)totalSizeSRAM,
                totalFanIn ? (sizeEIHeaderMem+sizeEIRestMem)/(double)totalFanIn : 0,
                totalFanOut ? sizeEOMem/(double)totalFanOut : 0
            }; 
          }
        }
      } // threadId loop

      if(EnableStats){
        sizeStatistics += sizeStatisticsLocal;
      }
    }); // Parallel loop

    if(EnableStats){
      sizeStatistics.walk_stats(
        { "threadSizeVMem", "threadSizeEIHeaderMem", "threadSizeEIRestMem", "threadSizeEOMem", "threadTotalSizeDRAM", "threadTotalSizeSRAM",
          "threadBytesPerInEdge", "threadBytesPerOutEdge" },
        [&](const char *name, const char *stat, double val){
          on_export_value((std::string(name)+"_"+stat).c_str(), val);
        }
      );
    }
  }

  // Initialise partitions
  __attribute__((noinline)) void initialisePartitions() {
    mark_phase("initPartitions");
    parallel_for_with_grain<unsigned>(use_parallel, 0, TinselMaxThreads,1024, [&](uint32_t threadId){
        // Next pointers for each partition
        uint32_t nextVMem = 0;
        uint32_t nextOutIndex = 0;
        // Pointer to thread structure
        PThread<DeviceType, S, E, M, NUM_PINS>* thread =
          (PThread<DeviceType, S, E, M, NUM_PINS>*) &threadMem[threadId][0];
        // Set number of devices on thread
        thread->numDevices = numDevicesOnThread[threadId];
        // Set tinsel address of array of device states
        thread->devices = vertexMemBase[threadId];
        // Set tinsel address of base of edge tables
        thread->outTableBase = outEdgeMemBase[threadId];
        thread->inTableHeaderBase = inEdgeHeaderMemBase[threadId];
        thread->inTableRestBase = inEdgeRestMemBase[threadId];
        // Add space for each device on thread
        uint32_t numDevs = numDevicesOnThread[threadId];
        for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
          PState<S,NUM_PINS>* dev = (PState<S,NUM_PINS>*) &vertexMem[threadId][nextVMem];
          PDeviceId id = fromDeviceAddr[threadId][devNum];
          devices[id] = dev;
          // Add space for device
          nextVMem = nextVMem + sizeof(PState<S,NUM_PINS>);
        }
        // Initialise each device and the thread's out edges
        for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
          PDeviceId id = fromDeviceAddr[threadId][devNum];
          PState<S,NUM_PINS>* dev = devices[id];
          // Initialise
          POutEdge* outEdgeArray = (POutEdge*) outEdgeMem[threadId];
          for (uint32_t p = 0; p < NUM_PINS; p++) {
            dev->pinBase[p] = nextOutIndex;
            Seq<POutEdge>* edges = outTable[id][p];
            for (uint32_t i = 0; i < (uint32_t)edges->numElems; i++) {
              outEdgeArray[nextOutIndex] = edges->elems[i];
              nextOutIndex++;
            }
          }
        }
        // Intialise thread's in edges
        PerThreadRoutingInfo *tbi=perThreadRoutingInfo+threadId;
        PInHeader<E>* inEdgeHeaderArray =
          (PInHeader<E>*) inEdgeHeaderMem[threadId];
        Seq<PInHeader<E>>* headers = &tbi->inTableHeaders;
        if (headers)
          for (uint32_t i = 0; i < (unsigned)headers->numElems; i++) {
            inEdgeHeaderArray[i] = headers->elems[i];
          }
        PInEdge<E>* inEdgeRestArray = (PInEdge<E>*) inEdgeRestMem[threadId];
        Seq<PInEdge<E>>* edges = &tbi->inTableRest;
        if (edges)
          for (uint32_t i = 0; i < (uint32_t)edges->numElems; i++) {
            inEdgeRestArray[i] = edges->elems[i];
          }
        // At this point, check that next pointers line up with heap sizes
        if (nextVMem != vertexMemSize[threadId]) {
          fatal_error("Error: vertex mem size does not match pre-computed size\n");
        }
        if ((nextOutIndex * sizeof(POutEdge)) != outEdgeMemSize[threadId]) {
          fatal_error("Error: out edge mem size does not match pre-computed size\n");
        }
        // Set tinsel address of senders array
        thread->senders = vertexMemBase[threadId] + nextVMem;
    });
  }

  // Allocate mapping structures
  __attribute__((noinline)) void allocateMapping() {
    mark_phase("allocMapping");
    devices = (PState<S,NUM_PINS>**) calloc(numDevices, sizeof(PState<S,NUM_PINS>*));
    toDeviceAddr = (PDeviceAddr*) calloc(numDevices, sizeof(PDeviceAddr));
    fromDeviceAddr = (PDeviceId**) calloc(TinselMaxThreads, sizeof(PDeviceId*));
    numDevicesOnThread = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
  }

  // Allocate routing tables
  // (Only valid after mapper is called)
  __attribute__((noinline)) void allocateRoutingTables() {
    mark_phase("allocRoutingTables");
    perThreadRoutingInfo=new PerThreadRoutingInfo[TinselMaxThreads];

    // Sender-side tables
    outTable = (Seq<POutEdge>***) calloc(numDevices, sizeof(Seq<POutEdge>**));
    parallel_for_with_grain<unsigned>(use_parallel, 0, numDevices,1024, [&](unsigned d){
      outTable[d] = (Seq<POutEdge>**) calloc(NUM_PINS, sizeof(Seq<POutEdge>*));
      for (uint32_t p = 0; p < NUM_PINS; p++){
        outTable[d][p] = new SmallSeq<POutEdge>;
      }
    });

    perMailboxRoutingInfo=new PerMailboxRoutingInfo[TinselMaxThreads / TinselThreadsPerMailbox];
  }

  // Determine local-multicast routing key for given set of receivers
  // (The key must be the same for all receivers)
  /* This procedure is a hot-spot, and also has a set of global side-effects.
      It needs to find a bit b such that for all 0<=i<numGroups, bitmaps[groups[i]][bit]==0.
      It then needs to reserve bit index b in each group as a single transaction, so that
      no other group can take it.
  */
 template<bool DoLock>
  __attribute__((noinline)) uint32_t findKeyPerThread(uint32_t numGroups, std::array<PReceiverGroup<E>,TinselThreadsPerMailbox> &groups) { 
    
    /* All threads involved here are in the same mail-box, so we can enforce exclusion
      by locking the mailbox. */

    unsigned mbox_index= groups[0].threadId >> TinselLogThreadsPerMailbox;
    for(unsigned i=1; i<numGroups; i++){
      assert( mbox_index == groups[i].threadId >> TinselLogThreadsPerMailbox);
    }
    SpinLockGuard<DoLock> lk(perMailboxRoutingInfo[mbox_index].lock);

    uint32_t res;

    if (numGroups == 1) {
      // Fast path
      PerThreadRoutingInfo *tbi = &perThreadRoutingInfo[groups[0].threadId];
      Bitmap *bm = &tbi->inTableBitmap;

      res = bm->grabNextBit( );
    }else{
      Bitmap *bms[TinselThreadsPerMailbox];
      assert(numGroups <= TinselThreadsPerMailbox);
      for(unsigned i=0; i<numGroups; i++){
        bms[i]=&perThreadRoutingInfo[groups[i].threadId].inTableBitmap;
      }

      // Determine starting index for key search.
      uint32_t index = 0;
      for (uint32_t i = 0; i < numGroups; i++) {
        if (bms[i]->firstFree > index){
          index = bms[i]->firstFree;
        }
      }

      // Find key that is available for all receivers
      uint64_t mask;
      retry:
        mask = 0ul;
        for (uint32_t i = 0; i < numGroups; i++) {
          mask |= bms[i]->getWord(index);
          if (~mask == 0ul) { index++; goto retry; }
        }

      // Mark key as taken in each bitmap
      uint32_t bit = __builtin_ctzll(~mask);
      for (uint32_t i = 0; i < numGroups; i++) {
        bms[i]->setBit(index, bit);
      }
      res = 64*index + bit;
    }

    return res;
  }

  template<bool DoLock>
  __attribute__((noinline)) uint32_t findKeyPerMailbox(uint32_t numGroups, std::array<PReceiverGroup<E>,TinselThreadsPerMailbox> &groups) { 
    
    /* All threads involved here are in the same mail-box, so we can enforce exclusion
      by locking the mailbox. */

    uint64_t required=0;
    unsigned index=0;

    unsigned mbox_index= groups[0].threadId >> TinselLogThreadsPerMailbox;
    PerMailboxRoutingInfo *mbi=perMailboxRoutingInfo+mbox_index;
    auto &threadBitMaps = mbi->threadBitMaps;

    SpinLockGuard<DoLock> lk(mbi->lock);

    if(numGroups==1){
      unsigned threadOffset=groups[0].threadId % (1<<TinselLogThreadsPerMailbox);
      required=1ull<<threadOffset;

      unsigned lowest_free=mbi->lowestFreeIndexPerThread[threadOffset];
      index=lowest_free;
      assert(index <= mbi->threadBitMaps.size());
      if(index==mbi->threadBitMaps.size()){
        mbi->threadBitMaps.extendByWithZero(64);
        lowest_free++;
        mbi->threadBitMaps[index] |= required;
      
      }else{
        assert( (mbi->threadBitMaps[index]&required)==0 );
        mbi->threadBitMaps[index] |= required;

        if(lowest_free==index){
          while( ( lowest_free < mbi->threadBitMaps.size()) && ( mbi->threadBitMaps[lowest_free] & required) ){
            lowest_free++;
          }
        }
      }
      mbi->lowestFreeIndexPerThread[threadOffset]=lowest_free;

    }else{

      for(unsigned i=0; i<numGroups; i++){
        assert( mbox_index == groups[i].threadId >> TinselLogThreadsPerMailbox);
        unsigned threadOffset=groups[i].threadId % (1<<TinselLogThreadsPerMailbox);

        required |= 1ull<<threadOffset;

        unsigned lowest_here = mbi->lowestFreeIndexPerThread[threadOffset];
        index=std::min(index, lowest_here);
        
        assert(lowest_here==threadBitMaps.size() || ((threadBitMaps[lowest_here]&(1ull<<threadOffset))==0) );
      }

      for(; index<threadBitMaps.size(); index++){
        if((threadBitMaps[index] & required)==0){
          break;
        }
      }

      if(index == threadBitMaps.size()){
        threadBitMaps.extendByWithZero(64);
      }

      assert( (threadBitMaps[index] & required) ==0);
      threadBitMaps[index] |= required;

      while(required){
        // https://lemire.me/blog/2018/02/21/iterating-over-set-bits-quickly/
        uint64_t bit = required & -required;
        int o = __builtin_ctzl(required);

        unsigned lowest=mbi->lowestFreeIndexPerThread[o];
        if(lowest==index){
          for(; lowest<threadBitMaps.size(); lowest++){
            if( 0==(bit&threadBitMaps[lowest])){
              break;
            }
          }
          if(lowest==threadBitMaps.size()){
            threadBitMaps.extendByWithZero(64);
          }
          mbi->lowestFreeIndexPerThread[o]=lowest;
        }
        
        required ^= bit;
      }
    }

    // Ensure post-condition
    for(unsigned i=0; i<64; i++){
      assert(  mbi->lowestFreeIndexPerThread[i] <= mbi->threadBitMaps.size() );
      assert(  ( mbi->lowestFreeIndexPerThread[i] == mbi->threadBitMaps.size() ) || ! ( mbi->threadBitMaps[mbi->lowestFreeIndexPerThread[i]] & (1ull<<i) ) );
    }

    return index;
  }

  template<bool DoLock>
  __attribute__((noinline)) uint32_t findKey(uint32_t numGroups, std::array<PReceiverGroup<E>,TinselThreadsPerMailbox> &groups)
  {
    // Both methods work, and intuitively the per mailbox one should be faster.
    // But in practise... the the per-thread one is a touch faster. This may
    // vary based on the average group size and so on, so I'll leave the code in
    // for now.
    //
    //return findKeyPerMailbox<DoLock>(numGroups, groups);
    return findKeyPerThread<DoLock>(numGroups, groups);
  }

  // Add entries to the input tables for the given receivers
  // (Only valid after mapper is called)
  template<bool DoLock>
  __attribute__((noinline)) uint32_t addInTableEntries(uint32_t numGroups, std::array<PReceiverGroup<E>,TinselThreadsPerMailbox> &groups, ParallelRunningStats<1> *edgesPerHeaderStats) {
    uint32_t key = findKey<DoLock>(numGroups, groups);
    if (key >= 0xffff) {
      fatal_error("Routing key exceeds 16 bits\n");
    }
    // Populate inTableHeaders and inTableRest using the key
    for (uint32_t i = 0; i < numGroups; i++) {
      PReceiverGroup<E>* g = &groups[i];
      uint32_t numEdges = g->receivers.numElems;
      PInEdge<E>* edgePtr = g->receivers.elems;

      if(edgesPerHeaderStats){
        (*edgesPerHeaderStats)+=numEdges;
      }

      assert(numEdges!=0); // Why would it be in the group if there are no edges?
      if (numEdges > 0) {
        // Determine thread id of receiver
        uint32_t t = g->threadId;
        PerThreadRoutingInfo *tbi = perThreadRoutingInfo+t;
        // Everything here is associated with thread t and its data structures
        SpinLockGuard<DoLock> lk(tbi->lock);
        
        // Extend table
        Seq<PInHeader<E>>* headers = &tbi->inTableHeaders;
        // Try to avoid uninitialised bytes - force to zero.
        if (key >= (unsigned)headers->numElems) headers->extendByWithZero(key + 1 - headers->numElems);
        // Fill in header
        PInHeader<E>* header = &headers->elems[key];
        header->numReceivers = numEdges;
        if (tbi->inTableRest.numElems > 0xffff) {          
          fatal_error("In-table index exceeds 16 bits\n");
        }
        header->restIndex = tbi->inTableRest.numElems;
        const int EdgesPerHeader = PInHeader<E>::EdgesPerHeader;
        uint32_t numHeaderEdges = numEdges < EdgesPerHeader ? numEdges : EdgesPerHeader;
        for (uint32_t j = 0; j < numHeaderEdges; j++) {
          header->edges[j] = *edgePtr;
          edgePtr++;
        }
        numEdges -= numHeaderEdges;
        // Overflow into rest memory if header not big enough
        for (uint32_t j = 0; j < numEdges; j++) {
          tbi->inTableRest.append(*edgePtr);
          edgePtr++;
        }
      }
    }
    return key;
  }

  // Split edge list into board-local and non-board-local destinations
  // And sort each list by destination thread id
  // (Only valid after mapper is called)
  /* This has no side-effects into global data structures, it only depends:
     - graph
     - toDeviceAddr
    So we dont need to lock anything
  */
  __attribute__((noinline)) void splitDests(PDeviceId devId, PinId pinId,
                    Seq<PEdgeDest>* local, Seq<PEdgeDest>* nonLocal)
      const                
    {
    local->clear();
    nonLocal->clear();
    PDeviceAddr devAddr = toDeviceAddr[devId];
    uint32_t devBoard = getThreadId(devAddr) >> TinselLogThreadsPerBoard;
    // Split destinations into local/non-local
    uint32_t d=0;
    graph.walkOutgoingNodeIdsAndPins(devId, [&](uint32_t dstDev, PinId dstPin){
      if (dstPin == pinId) {
        PEdgeDest e;
        e.index = d;
        e.dest = dstDev;
        e.addr = toDeviceAddr[dstDev];
        uint32_t destBoard = getThreadId(e.addr) >> TinselLogThreadsPerBoard;
        if (devBoard == destBoard)
          local->append(e);
        else
          nonLocal->append(e);
        ++d;
      }
    });
    // Sort local list
    std::sort(local->elems, local->elems+local->numElems, cmpEdgeDestT);
    // Sort non-local list
    std::sort(nonLocal->elems, nonLocal->elems+nonLocal->numElems, cmpEdgeDestT);
  }

  // Compute table updates for destinations for given device
  // (Only valid after mapper is called)
  /*! 
    \param d Source device index
    \param d Source pin on device
    \pre dests is guaranteed to be in ascended softed order (using cmpEdgeDest)
     \todo TODO (dt10) : the ascending nature of the lists may be biasing run-time behaviour
      for higher fanout nets, and in aggregate might cause load to sweeep from lower ordered to
      higher ordered destinations within the entire board.
  */
  template<bool DoLock>
  __attribute__((noinline)) void computeTables(Seq<PEdgeDest>* dests, uint32_t d, uint32_t p,
         Seq<PRoutingDest>* out, std::array<PReceiverGroup<E>,TinselThreadsPerMailbox> &groups, ParallelRunningStats<1> *edgesPerheaderStats) {
    out->clear();
    PRoutingDest dest;
    memset(&dest, 0, sizeof(PRoutingDest));
    uint32_t index = 0;
    while (index < (uint32_t)dests->numElems) {
      // New set of receiver groups on same mailbox
      uint32_t threadMaskLow = 0;
      uint32_t threadMaskHigh = 0;
      uint32_t nextGroup = 0;
      // Current mailbox & thread being considered
      PDeviceAddr mbox = getThreadId(dests->elems[index].addr) >>
                           TinselLogThreadsPerMailbox;
      uint32_t thread = getThreadId(dests->elems[index].addr) &
                          ((1<<TinselLogThreadsPerMailbox)-1);
      // Determine edges targetting same mailbox
      while (index < (uint32_t)dests->numElems) {
        PEdgeDest* edge = &dests->elems[index];
        // Determine destination mailbox address and mailbox-local thread
        uint32_t destMailbox = getThreadId(edge->addr) >>
                                 TinselLogThreadsPerMailbox;
        uint32_t destThread = getThreadId(edge->addr) &
                                 ((1<<TinselLogThreadsPerMailbox)-1);
        // Does destination match current destination?
        if (destMailbox == mbox) {
          if (destThread == thread) {
            // Add to current receiver group
            PInEdge<E> in;
            in.devId = getLocalDeviceId(edge->addr);
            //Seq<E>* edges = edgeLabels.elems[d];
            const E &weight=graph.getEdgeWeight(d, edge->index);
            if (! std::is_same<E, None>::value){
              //in.edge = edges->elems[edge->index];
              in.edge = weight;
            }
            // Update current receiver group
            groups[nextGroup].receivers.append(in);
            groups[nextGroup].threadId = getThreadId(edge->addr);
            if (thread < 32) threadMaskLow |= 1 << thread;
            if (thread >= 32) threadMaskHigh |= 1 << (thread-32);
            index++;
          }
          else {
            // Start new receiver group
            thread = destThread;
            nextGroup++;
            assert(nextGroup < TinselThreadsPerMailbox);
          }
        }
        else break;
      }
      // Add input table entries
      uint32_t key = addInTableEntries<DoLock>(nextGroup+1, groups, edgesPerheaderStats);
      // Add output entry
      dest.kind = PRDestKindMRM;
      dest.mbox = mbox;
      dest.mrm.key = key;
      dest.mrm.threadMaskLow = threadMaskLow;
      dest.mrm.threadMaskHigh = threadMaskHigh;
      assert(threadMaskLow | threadMaskHigh);
      out->append(dest);
      if(chatty>1){
        fprintf(stderr, "  routing-entry: source(dev/pin)=%u/%u kind=MRM, mbox=0x%x, key=%u, tMaskLo=0x%x, tMaskHi=0x%x\n", d, p, dest.mbox, key, threadMaskLow, threadMaskHigh);
      }
      // Clear receiver groups, for a new iteration
      for (uint32_t i = 0; i <= nextGroup; i++) groups[i].receivers.clear();
    }
  }

  template<bool DoLock>
  void computeRoutingTablesForDevice(
    uint32_t d, // device id
    Seq<PEdgeDest> &local,
    Seq<PEdgeDest> &nonLocal,
    Seq<PRoutingDest> &dests,
    std::array<PReceiverGroup<E>,TinselThreadsPerMailbox> &groups,
    ParallelRunningStats<1> *edgesPerLocalHeaderStats,
    ParallelRunningStats<1> *edgesPerNonLocalHeaderStats
  ){
    // For each pin
    for (uint32_t p = 0; p < NUM_PINS; p++) {
      // Split edge lists into local/non-local and sort by target thread id
      splitDests(d, p, &local, &nonLocal);

      // Deal with non-board-local connections
      computeTables<DoLock>(&nonLocal, d, p, &dests, groups, edgesPerNonLocalHeaderStats);
      if(dests.size()>0){
        // We can choose to embed edges on this side, or to do it on the other side.
        // Threshold of 1 _always_ makes sense, as it cannot be worse. Higher is less clear, and probably bad.
        const unsigned router_threshold = 0;
        if(dests.size() <= router_threshold){
          for(unsigned di=0; di<dests.size(); di++){
            PRoutingDest dest=dests[di];
            assert(dest.kind==PRDestKindMRM);
            POutEdge edge;
            edge.mbox=dest.mbox;
            edge.threadMaskHigh=dest.mrm.threadMaskHigh;
            edge.threadMaskLow=dest.mrm.threadMaskLow;
            edge.key=dest.mrm.key;
            // This is only writeable by this function
            outTable[d][p]->append(edge);
          } 
        }else{
          uint32_t src = getThreadId(toDeviceAddr[d]) >> TinselLogThreadsPerMailbox;
          uint32_t key = progRouterTables->addDestsFromBoard(src, &dests);
          POutEdge edge;
          edge.mbox = tinselUseRoutingKey();
          edge.key = 0;
          edge.threadMaskLow = key;
          edge.threadMaskHigh = 0; 
          // This is only writeable by this function
          outTable[d][p]->append(edge);
        }
      }

      // Deal with board-local connections
      // Board-local is after non-local, to try to get the messages travelling
      // longer distances out the door first.
      computeTables<DoLock>(&local, d, p, &dests, groups, edgesPerLocalHeaderStats);
      for (uint32_t i = 0; i < (uint32_t)dests.numElems; i++) {
        PRoutingDest dest = dests.elems[i];
        POutEdge edge;
        edge.mbox = dest.mbox;
        edge.key = dest.mrm.key;
        edge.threadMaskLow = dest.mrm.threadMaskLow;
        edge.threadMaskHigh = dest.mrm.threadMaskHigh;
        // This is only writeable by this function
        outTable[d][p]->append(edge);
      }


      // Add output list terminator
      POutEdge term;
      term.key = InvalidKey;
      // This is only writeable by this function
      outTable[d][p]->append(term);
    }
  }

  // Compute routing tables
  // (Only valid after mapper is called)
  POLITE_NOINLINE void computeRoutingTables() {
    mark_phase("compRouteTables");

    // Allocate per-board programmable routing tables
    progRouterTables = new ProgRouterMesh(numBoardsX, numBoardsY);

    const bool DoLock=true;

    ParallelRunningStats<1> edgesPerHeaderLocalStats;
    ParallelRunningStats<1> edgesPerHeaderNonLocalStats;

    ParallelRunningStats<1> *pEdgesPerHeaderLocalStats = on_export_value ? &edgesPerHeaderLocalStats : nullptr;
    ParallelRunningStats<1> *pEdgesPerHeaderNonLocalStats = on_export_value ? &edgesPerHeaderNonLocalStats : nullptr;

    // For each device
    if(DoLock){
      parallel_for_blocked<unsigned>(use_parallel, 0, numDevices, 1, [&](uint32_t dBegin, uint32_t dEnd) {
        ParallelRunningStats<1> lEdgesPerHeaderLocalStats;
        ParallelRunningStats<1> lEdgesPerHeaderNonLocalStats;

        // Edge destinations (local to sender board, or not)
        Seq<PEdgeDest> local;
        Seq<PEdgeDest> nonLocal;

        // Routing destinations
        Seq<PRoutingDest> dests;
        std::array<PReceiverGroup<E>,TinselThreadsPerMailbox> groups;

        for(unsigned d=dBegin; d<dEnd; d++){
          // Note: the various tables (local,nonLocal,dests,group) will get cleared within functions
          
          computeRoutingTablesForDevice<true>(d, local, nonLocal, dests, groups,
            pEdgesPerHeaderLocalStats ? &lEdgesPerHeaderLocalStats : nullptr,
            pEdgesPerHeaderNonLocalStats ? &lEdgesPerHeaderNonLocalStats : nullptr
          );
        }

        if(pEdgesPerHeaderNonLocalStats){
          *pEdgesPerHeaderNonLocalStats += lEdgesPerHeaderNonLocalStats;
        }
        if(pEdgesPerHeaderLocalStats){
          *pEdgesPerHeaderLocalStats += lEdgesPerHeaderLocalStats;
        }
        
      });
    }else{
      // Edge destinations (local to sender board, or not)
      Seq<PEdgeDest> local;
      Seq<PEdgeDest> nonLocal;

      // Routing destinations
      Seq<PRoutingDest> dests;
      std::array<PReceiverGroup<E>,TinselThreadsPerMailbox> groups;
      for (uint32_t d = 0; d < numDevices; d++) {
        computeRoutingTablesForDevice<false>(d, local, nonLocal, dests, groups,    pEdgesPerHeaderLocalStats, pEdgesPerHeaderNonLocalStats );
      }
    }

    if(pEdgesPerHeaderNonLocalStats){
      pEdgesPerHeaderNonLocalStats->walk_stats( {"edges_per_header_non_local"}, [&](const char *name, const char *stat, double value) {
        on_export_value((name+std::string("_")+stat).c_str(), value);
      });
    }
    if(pEdgesPerHeaderLocalStats){
      pEdgesPerHeaderLocalStats->walk_stats( {"edges_per_header_local"},[&](const char *name, const char *stat, double value) {
        on_export_value((name+std::string("_")+stat).c_str(), value);
      });
    }
  }

  // Release all structures
  void releaseAll() {
    mark_phase("releaseAll");
    if (devices != NULL) {
      free(devices);
      free(toDeviceAddr);
      free(numDevicesOnThread);
      parallel_for_with_grain<unsigned>(use_parallel, 0,TinselMaxThreads,1024, [&](unsigned t) {
          if (fromDeviceAddr[t] != NULL) free(fromDeviceAddr[t]);
          if (vertexMem[t] != NULL) free(vertexMem[t]);
          if (threadMem[t] != NULL) free(threadMem[t]);
          if (inEdgeHeaderMem[t] != NULL) free(inEdgeHeaderMem[t]);
          if (inEdgeRestMem[t] != NULL) free(inEdgeRestMem[t]);
          if (outEdgeMem[t] != NULL) free(outEdgeMem[t]);
      });
      free(fromDeviceAddr);
      free(vertexMem);
      free(vertexMemSize);
      free(vertexMemBase);
      free(threadMem);
      free(threadMemSize);
      free(threadMemBase);
      free(inEdgeHeaderMem);
      free(inEdgeHeaderMemSize);
      free(inEdgeHeaderMemBase);
      free(inEdgeRestMem);
      free(inEdgeRestMemSize);
      free(inEdgeRestMemBase);
      free(outEdgeMem);
      free(outEdgeMemSize);
      free(outEdgeMemBase);
    }
    if(perThreadRoutingInfo!=NULL){
      delete []perThreadRoutingInfo;
    }
    if(perMailboxRoutingInfo!=NULL){
      delete []perMailboxRoutingInfo;
    }
    if (outTable != NULL) {
      for (uint32_t d = 0; d < numDevices; d++) {
        if (outTable[d] == NULL) continue;
        for (uint32_t p = 0; p < NUM_PINS; p++)
          delete outTable[d][p];
        free(outTable[d]);
      }
      free(outTable);
      outTable = NULL;
    }
    if (progRouterTables != NULL) delete progRouterTables;
  }

  // Implement mapping to tinsel threads
  /*! \param mapOnly - Only do the mapping. Don't actually allocated any tables.
                        If mapOnly is specified you can't load it into hardware...
  */
  void map(bool mapOnly=false) {

    begin_phase("map");

    // Let's measure some times
    struct timeval placementStart, placementFinish;
    struct timeval routingStart, routingFinish;
    struct timeval initStart, initFinish;

    // Release all mapping and heap structures
    releaseAll();

    // Reallocate mapping structures
    allocateMapping();

    // Start placement timer
    gettimeofday(&placementStart, NULL);


  mark_phase("partTopLevel");
  if(on_export_value){
    on_export_value("numBoardsX", numBoardsX);
    on_export_value("numBoardsY", numBoardsY);
    on_export_value("numBoardsTotal", numBoardsX*numBoardsY);
    on_export_value("numThreadsTotal", (numBoardsX*numBoardsY)<<TinselLogThreadsPerBoard);
  }
  #ifdef SCOTCH
    Placer scotch(&graph, numBoardsX*32, numBoardsY*32);
    int32_t threadOfScotch = 0;
    for (uint32_t boardY = 0; boardY < numBoardsY; boardY++) {
      for (uint32_t boardX = 0; boardX < numBoardsX; boardX++) {
        for (uint32_t boxX = 0; boxX < TinselMailboxMeshXLen; boxX++) {
          for (uint32_t boxY = 0; boxY < TinselMailboxMeshYLen; boxY++) {
            // Partition into subgraphs, one per thread
            uint32_t numThreads = 1<<TinselLogThreadsPerMailbox;
            // PartitionId t = boxes.mapping[boxY][boxX];
            // Placer threads(&boxes.subgraphs[t], numThreads, 1);

            // For each thread
            for (uint32_t threadNum = 0; threadNum < numThreads; threadNum++) {
              // Determine tinsel thread id
              uint32_t threadId = boardY;
              threadId = (threadId << TinselMeshXBits) | boardX;
              threadId = (threadId << TinselMailboxMeshYBits) | boxY;
              threadId = (threadId << TinselMailboxMeshXBits) | boxX;
              threadId = (threadId << (TinselLogCoresPerMailbox +
                            TinselLogThreadsPerCore)) | threadNum;
              // Get subgraph
              Graph* g = &scotch.subgraphs[threadOfScotch];
              threadOfScotch++;

              // Populate fromDeviceAddr mapping
              uint32_t numDevs = g->incoming->numElems;
              numDevicesOnThread[threadId] = numDevs;
              fromDeviceAddr[threadId] = (PDeviceId*)
                malloc(sizeof(PDeviceId) * numDevs);
              for (uint32_t devNum = 0; devNum < numDevs; devNum++)
                fromDeviceAddr[threadId][devNum] = g->labels->elems[devNum];
              // Populate toDeviceAddr mapping
              assert(numDevs < maxLocalDeviceId());
              for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
                PDeviceAddr devAddr =
                  makeDeviceAddr(threadId, devNum);
                toDeviceAddr[g->labels->elems[devNum]] = devAddr;
              }
            }
          }
        }
      }
    }
  #else
    fprintf(stderr, "Top-level placer\n");
    // Partition into subgraphs, one per board
    Placer<E> boards(&graph, numBoardsX, numBoardsY, 0, placer_method);

    mark_phase("placeTopLevel");
    // Place subgraphs onto 2D mesh
    uint32_t placerEffort = 8;
    if(getenv("POLITE_PLACER_EFFORT")){
      placerEffort=std::stoi(getenv("POLITE_PLACER_EFFORT"));
      fprintf(stderr, "   Setting placer effort = %u\n", placerEffort);
    }
    
    fprintf(stderr, "Board place\n");
    boards.place(placerEffort);
    if(on_export_string){
      on_export_string("board_placer_method", placer_method_to_string(boards.method_chosen));
    }
    if(on_export_value){
      on_export_value("board_placer_effort", (int64_t) placerEffort);
    }

    mark_phase("placeBoards");
    // 0==DevicesPerThread, 1== fanIn, 2==fanOut
    ParallelRunningStats<3> systemThreadStats;

    PlacerMethod boardPlacerMethod=Default;
    PlacerMethod mailboxPlacerMethod=Default;

    // HACK: has side-effects for printing
    // boards.computeInterLinkCounts();

    // For each board
    parallel_for_2d_with_grain<unsigned>(use_parallel, 0,numBoardsY,1, 0,numBoardsX,1, [&](uint32_t boardY, uint32_t boardX) {
      ParallelRunningStats<3> boardThreadStats;


      // Partition into subgraphs, one per mailbox
      PartitionId b = boards.mapping[boardY][boardX];
      Placer<None> boxes(&boards.subgraphs[b], 
              TinselMailboxMeshXLen, TinselMailboxMeshYLen, 1, placer_method);
      // DT10 : TODO - is this call to place adding much?
      boxes.place(placerEffort);
      if(boardX==0 && boardY==0){
        if(on_export_string){
          on_export_string("mailbox_placer_method", placer_method_to_string(boxes.method_chosen));
        }
        if(on_export_value){
         on_export_value("mailbox_placer_effort", placerEffort);
        }
      }

      // For each mailbox
      parallel_for_2d_with_grain<unsigned>(0,TinselMailboxMeshXLen,1,  0,TinselMailboxMeshYLen,1, [&](uint32_t boxX, uint32_t boxY) {
        ParallelRunningStats<3> mailboxThreadStats;

        // Partition into subgraphs, one per thread
        uint32_t numThreads = 1<<TinselLogThreadsPerMailbox;
        PartitionId t = boxes.mapping[boxY][boxX];
        Placer<None> threads(&boxes.subgraphs[t], numThreads, 1, 2, placer_method);
        if(boardX==0 && boardY==0 && boxX==0 && boxY==0 && on_export_string){
          on_export_string("thread_placer_method", placer_method_to_string(threads.method_chosen));
        }

        // For each thread
        for (uint32_t threadNum = 0; threadNum < numThreads; threadNum++) {
          // Determine tinsel thread id
          uint32_t threadId = boardY;
          threadId = (threadId << TinselMeshXBits) | boardX;
          threadId = (threadId << TinselMailboxMeshYBits) | boxY;
          threadId = (threadId << TinselMailboxMeshXBits) | boxX;
          threadId = (threadId << (TinselLogCoresPerMailbox +
                        TinselLogThreadsPerCore)) | threadNum;

          // Get subgraph
          Graph<None>* g = &threads.subgraphs[threadNum];

          // Populate fromDeviceAddr mapping
          uint32_t numDevs = g->nodeCount();
          numDevicesOnThread[threadId] = numDevs;
          if(numDevs>0){
            assert(numDevs < maxLocalDeviceId());

            unsigned totalFanIn=0, totalFanOut=0;
            fromDeviceAddr[threadId] = (PDeviceId*) malloc(sizeof(PDeviceId) * numDevs);
            for (uint32_t devNum = 0; devNum < numDevs; devNum++){
              unsigned tid=g->getLabel(devNum);
              fromDeviceAddr[threadId][devNum] = tid;
              totalFanIn += graph.fanIn(tid);
              totalFanOut += graph.fanOut(tid);

              // Populate toDeviceAddr mapping
              PDeviceAddr devAddr = makeDeviceAddr(threadId, devNum);
              // Safe in parallel context, as each thread is written exactly once
              toDeviceAddr[g->getLabel(devNum)] = devAddr;
            }

            mailboxThreadStats += std::array<double,3>{(double)numDevs, (double)totalFanIn, (double)totalFanOut};
          }
        }

        boardThreadStats += mailboxThreadStats; // This is safe for parallelism
      });
      systemThreadStats += boardThreadStats; // Safe for parallelism
    });

    if(on_export_value){
      systemThreadStats.walk_stats(
        {"devices_per_thread", "fanin_per_thread", "fanout_per_thread"},
        [&](const char *name, const char *stat, double value){
          on_export_value((std::string(name)+"_"+stat).c_str(), value);
        }
      );
    }
    #endif
    // Stop placement timer and start routing timer
    gettimeofday(&placementFinish, NULL);
    gettimeofday(&routingStart, NULL);

    if(mapOnly){
      return;
    }

    // Compute send and receive side routing tables
    allocateRoutingTables();
    computeRoutingTables();

    // Stop routing timer and start init timer
    gettimeofday(&routingFinish, NULL);
    gettimeofday(&initStart, NULL);

    // Reallocate and initialise heap structures
    allocatePartitions();
    initialisePartitions();

    // Display times, if chatty
    gettimeofday(&initFinish, NULL);
    if (chatty > 0) {
      struct timeval diff;

      timersub(&placementFinish, &placementStart, &diff);
      double duration = (double) diff.tv_sec +
        (double) diff.tv_usec / 1000000.0;
      fprintf(stderr, "POLite mapper profile:\n");
      fprintf(stderr, "  Partitioning and placement: %lfs\n", duration);

      timersub(&routingFinish, &routingStart, &diff);
      duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
      fprintf(stderr, "  Routing table construction: %lfs\n", duration);

      timersub(&initFinish, &initStart, &diff);
      duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
      fprintf(stderr, "  Thread state initialisation: %lfs\n", duration);
    }

    end_phase("map");
  }

  // Constructor
  PGraph() {
    char* str = getenv("HOSTLINK_BOXES_X");
    int x = str ? atoi(str) : 1;
    x = x * TinselMeshXLenWithinBox;
    str = getenv("HOSTLINK_BOXES_Y");
    int y = str ? atoi(str) : 1;
    y = y * TinselMeshYLenWithinBox;
    constructor(x, y);
  }
  PGraph(uint32_t numBoxesX, uint32_t numBoxesY) {
    int x = numBoxesX * TinselMeshXLenWithinBox;
    int y = numBoxesY * TinselMeshYLenWithinBox;
    constructor(x, y);
  }

  // Deconstructor
  ~PGraph() {
    releaseAll();
    //for (uint32_t i = 0; i < edgeLabels.numElems; i++)
    //  delete edgeLabels.elems[i];
  }

  // Write partition to tinsel machine
  void writeRAM(HostLink* hostLink,
         uint8_t** heap, uint32_t* heapSize, uint32_t* heapBase) {

    // Number of bytes written by each thread
    uint32_t* writeCount = (uint32_t*)
      calloc(TinselMaxThreads, sizeof(uint32_t));

    // Number of threads completed by each core
    uint32_t*** threadCount = (uint32_t***)
      calloc(meshLenX, sizeof(uint32_t**));
    for (uint32_t x = 0; x < meshLenX; x++) {
      threadCount[x] = (uint32_t**)
        calloc(meshLenY, sizeof(uint32_t*));
      for (uint32_t y = 0; y < meshLenY; y++)
        threadCount[x][y] = (uint32_t*)
          calloc(TinselCoresPerBoard, sizeof(uint32_t));
    }

    // Initialise write addresses
    for (unsigned x = 0; x < meshLenX; x++)
      for (unsigned y = 0; y < meshLenY; y++)
        for (unsigned c = 0; c < TinselCoresPerBoard; c++)
          hostLink->setAddr(x, y, c, heapBase[hostLink->toAddr(x, y, c, 0)]);

    // Write heaps
    uint32_t done = false;
    while (! done) {
      done = true;
      for (unsigned x = 0; x < meshLenX; x++) {
        for (unsigned y = 0; y < meshLenY; y++) {
          for (int c = 0; c < TinselCoresPerBoard; c++) {
            uint32_t t = threadCount[x][y][c];
            if (t < TinselThreadsPerCore) {
              done = false;
              uint32_t threadId = hostLink->toAddr(x, y, c, t);
              uint32_t written = writeCount[threadId];
              if (written == heapSize[threadId]) {
                threadCount[x][y][c] = t+1;
                if ((t+1) < TinselThreadsPerCore)
                  hostLink->setAddr(x, y, c,
                    heapBase[hostLink->toAddr(x, y, c, t+1)]);
              } else {
                uint32_t send = min((heapSize[threadId] - written)>>2, 15);
                hostLink->store(x, y, c, send,
                  (uint32_t*) &heap[threadId][written]);
                writeCount[threadId] = written + send * sizeof(uint32_t);
              }
            }
          }
        }
      }
    }

    // Release memory
    free(writeCount);
    for (uint32_t x = 0; x < meshLenX; x++) {
      for (uint32_t y = 0; y < meshLenY; y++)
        free(threadCount[x][y]);
      free(threadCount[x]);
    }
    free(threadCount);
  }

  // Write graph to tinsel machine
  void write(HostLink* hostLink) { 
    begin_phase("upload");

    // Start timer
    struct timeval start, finish;
    gettimeofday(&start, NULL);

    bool useSendBufferOld = hostLink->useSendBuffer;
    hostLink->useSendBuffer = true;
    mark_phase("writeVertexMemToRAM");
    writeRAM(hostLink, vertexMem, vertexMemSize, vertexMemBase);
    mark_phase("writeThreadMemToRAM");
    writeRAM(hostLink, threadMem, threadMemSize, threadMemBase);
    mark_phase("writeInEdgeHeaderMemToRAM");
    writeRAM(hostLink, inEdgeHeaderMem,
               inEdgeHeaderMemSize, inEdgeHeaderMemBase);
    mark_phase("writeInEdgeRestMemToRAM");
    writeRAM(hostLink, inEdgeRestMem, inEdgeRestMemSize, inEdgeRestMemBase);
    mark_phase("writeOutEdgeMemToRAM");
    writeRAM(hostLink, outEdgeMem, outEdgeMemSize, outEdgeMemBase);
    mark_phase("writeProgRouterTables");
    progRouterTables->write(hostLink);
    mark_phase("flushRAM");
    hostLink->flush();
    hostLink->useSendBuffer = useSendBufferOld;

    // Display time if chatty
    gettimeofday(&finish, NULL);
    if (chatty > 0) {
      struct timeval diff;
      timersub(&finish, &start, &diff);
      double duration = (double) diff.tv_sec +
        (double) diff.tv_usec / 1000000.0;
      fprintf(stderr, "POLite graph upload time: %lfs\n", duration);
    }
    end_phase("upload");
  }

  // Determine fan-in of given device
  uint32_t fanIn(PDeviceId id) {
    return graph.fanIn(id);
  }

  // Determine fan-out of given device
  uint32_t fanOut(PDeviceId id) {
    return graph.fanOut(id);
  }

  uint32_t getMeshLenX() const { return meshLenX; }
  uint32_t getMeshLenY() const { return meshLenY; }

  uint32_t getDeviceId(uint32_t threadId, unsigned deviceOffset)
  {
    assert(fromDeviceAddr);
    assert(fromDeviceAddr[threadId]);
    return fromDeviceAddr[threadId][deviceOffset];
  }

  uint32_t getThreadIdFromDeviceId(uint32_t deviceId)
  {
    assert(toDeviceAddr);
    assert(deviceId < numDevices);
    return getThreadId(toDeviceAddr[deviceId]);
  }

  uint32_t getMaxFanOut() const
  { return graph.getMaxFanOut(); }

  uint32_t getMaxFanIn() const
  { return graph.getMaxFanIn(); }

  uint64_t getEdgeCount() const
  { return graph.getEdgeCount(); }
};


// Read performance stats and store in file
inline void politeSaveStats(HostLink* hostLink, const char* filename) {
  (void)hostLink;
  (void)filename;
  #ifdef POLITE_DUMP_STATS
  // Open file for performance counters
  FILE* statsFile = fopen(filename, "wt");
  if (statsFile == NULL) {
    printf("Error creating stats file\n");
    exit(EXIT_FAILURE);
  }
  uint32_t meshLenX = hostLink->meshXLen;
  uint32_t meshLenY = hostLink->meshYLen;
  // Number of caches
  uint32_t numLines = meshLenX * meshLenY *
                        TinselDCachesPerDRAM * TinselDRAMsPerBoard;
  // Add on number of cores
  numLines += meshLenX * meshLenY * TinselCoresPerBoard;
  // Add on number of threads
  #ifdef POLITE_COUNT_MSGS
  numLines += meshLenX * meshLenY * TinselThreadsPerBoard;
  #endif
  hostLink->dumpStdOut(statsFile, numLines);
  fclose(statsFile);
  #endif
}

#endif
