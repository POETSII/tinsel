// SPDX-License-Identifier: BSD-2-Clause
#ifndef _PGRAPH_H_
#define _PGRAPH_H_

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <HostLink.h>
#include <config.h>
#include <POLite.h>
#include <POLite/Seq.h>
#include <POLite/Graph.h>
#include <POLite/Placer.h>
#include <type_traits>

// Nodes of a POETS graph are devices
typedef NodeId PDeviceId;

// POETS graph
template <typename DeviceType,
          typename S, typename E, typename M> class PGraph {
 private:
  // Align address to 2^n byte boundary
  inline uint32_t align(uint32_t n, uint32_t addr) {
    if ((addr & (1<<n)-1) == 0) return addr;
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
    sram = NULL;
    sramSize = NULL;
    sramBase = NULL;
    dram = NULL;
    dramSize = NULL;
    dramBase = NULL;
  }

 public:
  // Number of devices
  uint32_t numDevices;

  // Graph containing device ids and connections
  Graph graph;

  // Mapping from device id to device state
  // (Not valid until the mapper is called)
  PState<S>** devices;

  // Mapping from thread id to number of devices on that thread
  // (Not valid until the mapper is called)
  uint32_t* numDevicesOnThread;

  // Mapping from device id to device address and back
  // (Not valid until the mapper is called)
  PDeviceAddr* toDeviceAddr;  // Device id -> device address
  PDeviceId** fromDeviceAddr; // Device address -> device id

  // Each thread's SRAM and DRAM partitions
  // (Not valid until the mapper is called)
  uint8_t** sram;       uint8_t** dram;
  uint32_t* sramSize;   uint32_t* dramSize;
  uint32_t* sramBase;   uint32_t* dramBase;

  // Setter for number of boards to use
  void setNumBoards(uint32_t x, uint32_t y) {
    if (x > meshLenX || y > meshLenY) {
      printf("Mapper: %d x %d boards requested, %d x %d available\n",
        numBoardsX, numBoardsY, meshLenX, meshLenY);
      exit(EXIT_FAILURE);
    }
    numBoardsX = x;
    numBoardsY = y;
  }

  // Create new device
  inline PDeviceId newDevice() {
    numDevices++;
    return graph.newNode();
  }

  // Add a connection between devices
  inline void addEdge(PDeviceId from, PinId pin, PDeviceId to) {
    graph.addEdge(from, pin, to);
  }

  // Add labelled edge using given output pin
  void addLabelledEdge(EdgeLabel label, PDeviceId x, PinId pin, PDeviceId y) {
    graph.addLabelledEdge(label, x, pin, y);
  }

  // Allocate SRAM and DRAM partitions
  void allocatePartitions() {
    // Decide a maximum partition size that is reasonable
    // SRAM: Partition size minus 2048 bytes for the stack
    uint32_t maxSRAMSize = (1<<TinselLogBytesPerSRAMPartition) - 2048;
    // DRAM: Partition size minus 65536 bytes for the stack
    uint32_t maxDRAMSize = (1<<TinselLogBytesPerDRAMPartition) - 65536;
    // Allocate partition sizes and bases
    sram = (uint8_t**) calloc(TinselMaxThreads, sizeof(uint8_t*));
    sramSize = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
    sramBase = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
    dram = (uint8_t**) calloc(TinselMaxThreads, sizeof(uint8_t*));
    dramSize = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
    dramBase = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
    // Compute partition sizes for each thread
    for (uint32_t threadId = 0; threadId < TinselMaxThreads; threadId++) {
      // This variable is used to count the size of the *initialised*
      // partition.  The total partition size is larger as it includes
      // uninitialised portions.
      uint32_t sizeSRAM = 0;
      uint32_t sizeDRAM = 0;
      // Add space for thread structure (stored in SRAM)
      sizeSRAM = cacheAlign(sizeSRAM +
                              sizeof(PThread<DeviceType, S, E, M>));
      // Add space for edge lists (stored in DRAM)
      uint32_t numDevs = numDevicesOnThread[threadId];
      for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
        // Add space for device
        sizeSRAM = sizeSRAM + sizeof(PState<S>);
        // Add space for neighbour arrays
        PDeviceId id = fromDeviceAddr[threadId][devNum];
        // Determine number of pins
        int32_t numPins = graph.maxPin(id) + 2;
        // Add space for neighbour arrays for each pin
        sizeDRAM = cacheAlign(sizeDRAM + numPins * POLITE_MAX_FANOUT
                                                 * sizeof(PNeighbour<E>));
      }
      // The total partition size including uninitialised portions
      uint32_t totalSizeSRAM = sizeSRAM + sizeof(PLocalDeviceId) * numDevs;
      uint32_t totalSizeDRAM = sizeDRAM;
      // Check that total size is reasonable
      if (totalSizeSRAM > maxSRAMSize) {
        printf("Error: max SRAM partition size exceeded\n");
        exit(EXIT_FAILURE);
      }
      if (totalSizeDRAM > maxDRAMSize) {
        printf("Error: max DRAM partition size exceeded\n");
        exit(EXIT_FAILURE);
      }
      // Allocate space for the initialised portion of the partition
      sram[threadId] = (uint8_t*) calloc(sizeSRAM, 1);
      sramSize[threadId] = sizeSRAM;
      dram[threadId] = (uint8_t*) calloc(sizeDRAM, 1);
      dramSize[threadId] = sizeDRAM;
      // Tinsel address of base of partition
      uint32_t partId = threadId & (TinselThreadsPerDRAM-1);
      sramBase[threadId] = (1 << TinselLogBytesPerSRAM) +
        (partId << TinselLogBytesPerSRAMPartition);
      dramBase[threadId] = TinselBytesPerDRAM -
        ((partId+1) << TinselLogBytesPerDRAMPartition);
      // Use partition-interleaved region for DRAM
      dramBase[threadId] |= 0x80000000;
    }
  }

  // Initialise partitions
  void initialisePartitions() {
    for (uint32_t threadId = 0; threadId < TinselMaxThreads; threadId++) {
      // Next pointers for each partition
      uint32_t nextDRAM = 0;
      uint32_t nextSRAM = 0;
      // Next pointer for neighbours arrays
      uint16_t nextNeighbours = 0;
      // Pointer to thread structure
      PThread<DeviceType, S, E, M>* thread =
        (PThread<DeviceType, S, E, M>*) &sram[threadId][nextSRAM];
      // Add space for thread structure
      nextSRAM = cacheAlign(nextSRAM +
                   sizeof(PThread<DeviceType, S, E, M>));
      // Set number of devices on thread
      thread->numDevices = numDevicesOnThread[threadId];
      // Set number of devices in graph
      thread->numVertices = numDevices;
      // Set tinsel address of array of device states
      thread->devices = sramBase[threadId] + nextSRAM;
      // Add space for each device on thread
      uint32_t numDevs = numDevicesOnThread[threadId];
      for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
        PState<S>* dev = (PState<S>*) &sram[threadId][nextSRAM];
        PDeviceId id = fromDeviceAddr[threadId][devNum];
        devices[id] = dev;
        // Add space for device
        nextSRAM = nextSRAM + sizeof(PState<S>);
      }
      // Initialise each device and associated edge list
      for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
        PDeviceId id = fromDeviceAddr[threadId][devNum];
        PState<S>* dev = devices[id];
        // Set thread-local device address
        // dev->localAddr = devNum;

        // Set tinsel address of neighbours arrays
        dev->neighboursOffset = nextNeighbours;
        // Neighbour array
        PNeighbour<E>* edgeArray = (PNeighbour<E>*) &dram[threadId][nextDRAM];
        // Emit neighbours array for host pin
        edgeArray[0].destAddr = makeDeviceAddr(tinselHostId(), 0);
        edgeArray[1].destAddr = invalidDeviceAddr(); // Terminator
        // Emit neigbours arrays for each application pin
        PinId numPins = graph.maxPin(id) + 1;
        for (uint32_t p = 0; p < numPins; p++) {
          uint32_t base = (p+1) * POLITE_MAX_FANOUT;
          uint32_t offset = 0;
          // Find outgoing edges of current pin
          for (uint32_t i = 0; i < graph.outgoing->elems[id]->numElems; i++) {
            if (graph.pins->elems[id]->elems[i] == p) {
              if (offset+1 >= POLITE_MAX_FANOUT) {
                printf("Error: pin fanout exceeds maximum\n");
                exit(EXIT_FAILURE);
              }
              PDeviceAddr addr = toDeviceAddr[
                graph.outgoing->elems[id]->elems[i]];
              edgeArray[base+offset].destAddr = addr;
              // Edge label
              if (! std::is_same<E, None>::value) {
                if (i >= graph.edgeLabels->elems[id]->numElems) {
                  printf("Edge weight not specified\n");
                  exit(EXIT_FAILURE);
                }
                memcpy(&edgeArray[base+offset].edge,
                  &graph.edgeLabels->elems[id]->elems[i], sizeof(E));
              }
              offset++;
            }
          }
          edgeArray[base+offset].destAddr = invalidDeviceAddr(); // Terminator
        }
        // Add space for edges
        nextDRAM = cacheAlign(nextDRAM + (numPins+1) *
                     POLITE_MAX_FANOUT * sizeof(PNeighbour<E>));
        nextNeighbours += (numPins+1);
      }
      // At this point, check that next pointers line up with heap sizes
      if (nextSRAM != sramSize[threadId]) {
        printf("Error: sram partition size does not match pre-computed size\n");
        exit(EXIT_FAILURE);
      }
      if (nextDRAM != dramSize[threadId]) {
        printf("Error: dram partition size does not match pre-computed size\n");
        exit(EXIT_FAILURE);
      }
      // Set tinsel address of senders array
      thread->senders = sramBase[threadId] + nextSRAM;
    }
  }

  // Allocate mapping structures
  void allocateMapping() {
    devices = (PState<S>**) calloc(numDevices, sizeof(PState<S>*));
    toDeviceAddr = (PDeviceAddr*) calloc(numDevices, sizeof(PDeviceAddr));
    fromDeviceAddr = (PDeviceId**) calloc(TinselMaxThreads, sizeof(PDeviceId*));
    numDevicesOnThread = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
  }

  // Release all structures
  void releaseAll() {
    if (devices != NULL) {
      free(devices);
      free(toDeviceAddr);
      free(numDevicesOnThread);
      for (uint32_t t = 0; t < TinselMaxThreads; t++)
        if (fromDeviceAddr[t] != NULL) free(fromDeviceAddr[t]);
      free(fromDeviceAddr);
      for (uint32_t t = 0; t < TinselMaxThreads; t++)
        if (sram[t] != NULL) free(sram[t]);
      free(sram);
      free(sramSize);
      free(sramBase);
      for (uint32_t t = 0; t < TinselMaxThreads; t++)
        if (dram[t] != NULL) free(dram[t]);
      free(dram);
      free(dramSize);
      free(dramBase);
    }
  }

  // Implement mapping to tinsel threads
  void map() {
    // Release all mapping and heap structures
    releaseAll();

    // Reallocate mapping structures
    allocateMapping();

    // Partition into subgraphs, one per board
    Placer boards(&graph, numBoardsX, numBoardsY);

    // Place subgraphs onto 2D mesh
    const uint32_t placerEffort = 8;
    boards.place(placerEffort);

    // For each board
    for (uint32_t boardY = 0; boardY < numBoardsY; boardY++) {
      for (uint32_t boardX = 0; boardX < numBoardsX; boardX++) {
        // Partition into subgraphs, one per mailbox
        PartitionId b = boards.mapping[boardY][boardX];
        Placer boxes(&boards.subgraphs[b], 
                 TinselMailboxMeshXLen, TinselMailboxMeshYLen);
        boxes.place(placerEffort);

        // For each mailbox
        for (uint32_t boxX = 0; boxX < TinselMailboxMeshXLen; boxX++) {
          for (uint32_t boxY = 0; boxY < TinselMailboxMeshYLen; boxY++) {
            // Partition into subgraphs, one per thread
            uint32_t numThreads = 1<<TinselLogThreadsPerMailbox;
            PartitionId t = boxes.mapping[boxY][boxX];
            Placer threads(&boxes.subgraphs[t], numThreads, 1);

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
              Graph* g = &threads.subgraphs[threadNum];

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

    // Reallocate and initialise heap structures
    allocatePartitions();
    initialisePartitions();
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
  }

  // Write devices (including topology) to tinsel machine
  // The 'ram' parameter is 0 to write SRAM partitions
  // or 1 to write DRAM partitions
  void writeRAM(HostLink* hostLink, uint32_t ram) {
    uint8_t** heap = ram == 0 ? sram : dram;
    uint32_t* heapSize = ram == 0 ? sramSize : dramSize;
    uint32_t* heapBase = ram == 0 ? sramBase : dramBase;

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
    for (int x = 0; x < meshLenX; x++)
      for (int y = 0; y < meshLenY; y++)
        for (int c = 0; c < TinselCoresPerBoard; c++)
          hostLink->setAddr(x, y, c, heapBase[hostLink->toAddr(x, y, c, 0)]);

    // Write heaps
    uint32_t done = false;
    while (! done) {
      done = true;
      for (int x = 0; x < meshLenX; x++) {
        for (int y = 0; y < meshLenY; y++) {
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

  // Write devices (including topology) to tinsel machine
  void write(HostLink* hostLink) {
    // Write SRAM partitions
    writeRAM(hostLink, 0);
    // Write DRAM partitions
    writeRAM(hostLink, 1);
  }

  // Determine fan-in of given device
  uint32_t fanIn(PDeviceId id) {
    return graph.fanIn(id);
  }

  // Determine fan-out of given device
  uint32_t fanOut(PDeviceId id) {
    return graph.fanOut(id);
  }

};

// Read performance stats and store in file
inline void politeSaveStats(HostLink* hostLink, const char* filename) {
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
