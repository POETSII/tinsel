#ifndef _PGRAPH_H_
#define _PGRAPH_H_

#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <HostLink.h>
#include <config.h>
#include <POLite.h>
#include <POLite/Seq.h>
#include <POLite/Graph.h>
#include <POLite/Placer.h>

// This is a static limit on the fan out of any pin
#define MAX_PIN_FANOUT 32

// Nodes of a POETS graph are devices
typedef NodeId PDeviceId;

// POETS graph
template <typename DeviceType, typename MessageType> class PGraph {
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

 public:
  // Number of devices
  uint32_t numDevices;

  // Graph containing device ids and connections
  Graph graph;

  // Mapping from device id to device pointer
  // (Not valid until the mapper is called)
  DeviceType** devices;

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

  // Create new device
  inline PDeviceId newDevice() {
    numDevices++;
    return graph.newNode();
  }

  // Add a connection between devices
  inline void addEdge(PDeviceId from, PinId pin, PDeviceId to) {
    graph.addEdge(from, pin, to);
  }

  // Add a bidirectional connection between devices
  inline void addBidirectionalEdge(PDeviceId x, PinId px,
                                   PDeviceId y, PinId py) {
    graph.addBidirectionalEdge(x, px, y, py);
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
                              sizeof(PThread<DeviceType, MessageType>));
      // Add space for device array (stored in SRAM)
      sizeSRAM = cacheAlign(sizeSRAM + numDevicesOnThread[threadId] *
                                 sizeof(DeviceType));
      // Add space for edge lists (stored in DRAM)
      uint32_t numDevs = numDevicesOnThread[threadId];
      for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
        PDeviceId id = fromDeviceAddr[threadId][devNum];
        // Determine number of pins
        uint32_t numPins = 0;
        PinId max = graph.maxPin(id);
        if (max >= 0) numPins = max+1;
        // Add space for neighbour arrays for each pin
        sizeDRAM = cacheAlign(sizeDRAM + numPins * MAX_PIN_FANOUT
                                                 * sizeof(PDeviceAddr));
      }
      // The total partition size including unintialised portions
      uint32_t totalSizeSRAM = sizeSRAM + 4 * numDevs;
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
      // Pointer to thread structure
      PThread<DeviceType, MessageType>* thread =
        (PThread<DeviceType, MessageType>*) &sram[threadId][nextSRAM];
      // Add space for thread structure
      nextSRAM = cacheAlign(nextSRAM +
                   sizeof(PThread<DeviceType, MessageType>));
      // Set number of devices on thread
      thread->numDevices = numDevicesOnThread[threadId];
      // Set tinsel address of devices array
      thread->devices = sramBase[threadId] + nextSRAM;
      // Add space for each device on thread
      uint32_t numDevs = numDevicesOnThread[threadId];
      for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
        DeviceType* dev = (DeviceType*) &sram[threadId][nextSRAM];
        PDeviceId id = fromDeviceAddr[threadId][devNum];
        devices[id] = dev;
        // Add space for device
        nextSRAM = cacheAlign(nextSRAM + sizeof(DeviceType));
      }
      // Set tinsel address of neighbours arrays
      thread->neighboursBase = dramBase[threadId] + nextDRAM;
      // Initialise each device and associated edge list
      for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
        PDeviceId id = fromDeviceAddr[threadId][devNum];
        DeviceType* dev = devices[id];
        // Set thread-local device address
        dev->localAddr = devNum;
        // Set fanIn
        Seq<PDeviceId>* in = graph.incoming->elems[id];
        dev->fanIn = in->numElems;

        // Edge array
        PDeviceAddr* edgeArray = (PDeviceAddr*) &dram[threadId][nextDRAM];
        // Emit neighbours array for host pin
        edgeArray[0] = makePDeviceAddr(tinselHostId(), 0, 1);
        // Emit neigbours arrays for each application pin
        PinId numPins = graph.maxPin(id) + 1;
        for (uint32_t p = 0; p < numPins; p++) {
          uint32_t base = (p+1) * MAX_PIN_FANOUT;
          uint32_t offset = 0;
          // Find outgoing edges of current pin
          for (uint32_t i = 0; i < graph.outgoing->elems[id]->numElems; i++) {
            if (graph.pins->elems[id]->elems[i] == p) {
              if (offset >= MAX_PIN_FANOUT) {
                printf("Error: pin fanout exceeds maximum\n");
                exit(EXIT_FAILURE);
              }
              edgeArray[base+offset] =
                toDeviceAddr[graph.outgoing->elems[id]->elems[i]];
              offset++;
            }
          }
        }
        // Add space for edges
        nextDRAM = cacheAlign(nextDRAM + (numPins+1) *
                     MAX_PIN_FANOUT * sizeof(PDeviceAddr));
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
    devices = (DeviceType**) calloc(numDevices, sizeof(DeviceType*));
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
    Placer boards(&graph, TinselMeshXLen, TinselMeshYLen);

    // Place subgraphs onto 2D mesh
    const uint32_t placerEffort = 8;
    boards.place(placerEffort);

    // For each board
    for (uint32_t boardY = 0; boardY < TinselMeshYLen; boardY++) {
      for (uint32_t boardX = 0; boardX < TinselMeshXLen; boardX++) {
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
              for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
                PDeviceAddr devAddr = 
                  makePDeviceAddr(threadId, devNum, 1);
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
      calloc(TinselMeshXLen, sizeof(uint32_t**));
    for (uint32_t x = 0; x < TinselMeshXLen; x++) {
      threadCount[x] = (uint32_t**)
        calloc(TinselMeshYLen, sizeof(uint32_t*));
      for (uint32_t y = 0; y < TinselMeshYLen; y++)
        threadCount[x][y] = (uint32_t*)
          calloc(TinselCoresPerBoard, sizeof(uint32_t));
    }

    // Initialise write addresses
    for (int x = 0; x < TinselMeshXLen; x++)
      for (int y = 0; y < TinselMeshYLen; y++)
        for (int c = 0; c < TinselCoresPerBoard; c++)
          hostLink->setAddr(x, y, c, heapBase[hostLink->toAddr(x, y, c, 0)]);

    // Write heaps
    uint32_t done = false;
    while (! done) {
      done = true;
      for (int x = 0; x < TinselMeshXLen; x++) {
        for (int y = 0; y < TinselMeshYLen; y++) {
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
    for (uint32_t x = 0; x < TinselMeshXLen; x++) {
      for (uint32_t y = 0; y < TinselMeshYLen; y++)
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
};

#endif
