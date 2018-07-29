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

  // Each thread's heap (DRAM partition), size, and base
  // (Not valid until the mapper is called)
  uint8_t** heap;
  uint32_t* heapSize;
  uint32_t* heapBase;

  // Create new device
  inline PDeviceId newDevice() {
    numDevices++;
    return graph.newNode();
  }

  // Add a connection between devices
  inline void addEdge(PDeviceId from, PDeviceId to) {
    graph.addEdge(from, to);
  }

  // Add a bidirectional connection between devices
  inline void addBidirectionalEdge(PDeviceId a, PDeviceId b) {
    graph.addBidirectionalEdge(a, b);
  }

  // Determine max device edges on given thread
  uint16_t computeMaxEdges(uint32_t threadId) {
    uint16_t max = 0;
    uint32_t numDevs = numDevicesOnThread[threadId];
    for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
      PDeviceId id = fromDeviceAddr[threadId][devNum];
      DeviceType* dev = devices[id];
      Seq<PDeviceId>* in = graph.incoming->elems[id];
      Seq<PDeviceId>* out = graph.outgoing->elems[id];
      uint32_t numEdges = out->numElems+ in->numElems;
      max = numEdges > max ? numEdges : max;
    }
    return max;
  }

  // Allocate heaps
  void allocateHeaps() {
    // Decide a maximum heap size that is reasonable
    // (We choose the partition size minus 65536 bytes for the stack.)
    const uint32_t maxHeapSize = (1<<TinselLogBytesPerDRAMPartition) - 65536;
    // Allocate heap sizes and bases
    heap = (uint8_t**) calloc(TinselMaxThreads, sizeof(uint8_t*));
    heapSize = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
    heapBase = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
    // Compute heap size for each thread
    for (uint32_t threadId = 0; threadId < TinselMaxThreads; threadId++) {
      // This variable is used to count the size of the *initialised*
      // heap.  The total heap size is larger as it includes
      // uninitialised portions.
      uint32_t size = 0;
      // Add space for thread structure
      size = cacheAlign(size + sizeof(PThread<DeviceType, MessageType>));
      // Determine max number of edges on thread
      uint32_t maxEdges = computeMaxEdges(threadId);
      // Add space for devices
      uint32_t numDevs = numDevicesOnThread[threadId];
      for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
        PDeviceId id = fromDeviceAddr[threadId][devNum];
        // Add space for edge lists
        size = wordAlign(size + sizeof(DeviceType));
        size = cacheAlign(size + maxEdges * sizeof(PDeviceAddr));
      }
      // The total heap size including unintialised portions
      // (Add on the size of the internal and external sender arrays)
      uint32_t totalSize = size + 8 * numDevs;
      // Check that total size is reasonable
      if (totalSize > maxHeapSize) {
        printf("Error: max heap size exceeded\n");
        exit(EXIT_FAILURE);
      }
      // Allocate space for the initialised portion of the heap
      heap[threadId] = (uint8_t*) calloc(size, 1);
      heapSize[threadId] = size;
      // Tinsel address of base of heap
      uint32_t partId = threadId & (TinselThreadsPerDRAM-1);
      heapBase[threadId] = TinselBytesPerDRAM -
        ((partId+1) << TinselLogBytesPerDRAMPartition);
      // Use interleaved memory
      heapBase[threadId] |= 0x80000000;
    }
  }

  // Initialise heaps
  void initialiseHeaps() {
    for (uint32_t threadId = 0; threadId < TinselMaxThreads; threadId++) {
      // Heap pointer
      uint32_t hp = 0;
      // Pointer to thread structure
      PThread<DeviceType, MessageType>* thread =
        (PThread<DeviceType, MessageType>*) &heap[threadId][hp];
      // Add space for thread structure
      hp = cacheAlign(hp + sizeof(PThread<DeviceType, MessageType>));
      // Set number of devices on thread
      thread->numDevices = numDevicesOnThread[threadId];
      // Determin max number of edges
      uint32_t maxEdges = computeMaxEdges(threadId);
      // Set device size on thread (number of bytes)
      // Includes device state and connectivity
      thread->deviceSize =
        cacheAlign(wordAlign(sizeof(DeviceType)) +
                     maxEdges * sizeof(PDeviceAddr));
      // Set tinsel address of devices array
      thread->devices = heapBase[threadId] + hp;
      // Add space for each device on thread
      uint32_t numDevs = numDevicesOnThread[threadId];
      for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
        DeviceType* dev = (DeviceType*) &heap[threadId][hp];
        PDeviceId id = fromDeviceAddr[threadId][devNum];
        devices[id] = dev;
        // Set thread-local device address
        dev->localAddr = devNum;
        // Set fanIn and fanOut
        Seq<PDeviceId>* in = graph.incoming->elems[id];
        Seq<PDeviceId>* out = graph.outgoing->elems[id];
        dev->fanIn = in->numElems;
        dev->fanOut = out->numElems;
        // Add space for device
        hp = wordAlign(hp + sizeof(DeviceType));
        // Edge array
        PDeviceAddr* edgeArray = (PDeviceAddr*) &heap[threadId][hp];
        uint32_t e = 0;
        for (uint32_t i = 0; i < out->numElems; i++)
          edgeArray[e++] = toDeviceAddr[out->elems[i]];
        for (uint32_t i = 0; i < in->numElems; i++)
          edgeArray[e++] = toDeviceAddr[in->elems[i]];
        // Add space for edges
        hp = cacheAlign(hp + maxEdges * sizeof(PDeviceAddr));
      }
      // At this point, check that hp lines up with heap size
      if (hp != heapSize[threadId]) {
        printf("Error: heap size does not match pre-computed heap size\n");
        exit(EXIT_FAILURE);
      }
      // Set tinsel address of intArray and extArray arrays
      thread->intArray = heapBase[threadId] + hp;
      thread->extArray = thread->intArray +
        numDevicesOnThread[threadId] * sizeof(uint32_t);
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
        if (heap[t] != NULL) free(heap[t]);
      free(heap);
      free(heapSize);
      free(heapBase);
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
                 TinselMailboxMeshXLen, TinselMailboxMeshXLen);
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
                PDeviceAddr devAddr;
                devAddr.threadId = threadId;
                devAddr.localAddr = devNum;
                toDeviceAddr[g->labels->elems[devNum]] = devAddr;
              }
            }
          }
        }
      }
    }

    // Reallocate and initialise heap structures
    allocateHeaps();
    initialiseHeaps();
  }

  // Constructor
  PGraph() {
    numDevices = 0;
    devices = NULL;
    toDeviceAddr = NULL;
    numDevicesOnThread = NULL;
    fromDeviceAddr = NULL;
    heap = NULL;
    heapSize = NULL;
    heapBase = NULL;
  }

  // Deconstructor
  ~PGraph() {
    releaseAll();
  }

  // Write devices (including topology) to tinsel machine
  void write(HostLink* hostLink) {
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
};

#endif
