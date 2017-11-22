#ifndef _PGRAPH_H_
#define _PGRAPH_H_

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <HostLink.h>
#include <config.h>
#include <Synch.h>

// Unique id for each device in a POETS graph
typedef uint32_t PDeviceId;

// A global pin id is a (device id, pin id) pair 
struct PGlobalPinId {
  PDeviceId devId;
  PinId pinId;
};

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

  // Pin connections
  // (A list of global pin ids for each device and each device pin)
  Seq<Seq<Seq<PGlobalPinId>*>*> incoming;
  Seq<Seq<Seq<PGlobalPinId>*>*> outgoing;

  // Width of each pin
  // (A list of pin widths for each device and each outgoing pin)
  Seq<Seq<uint16_t>*> pinWidth;

  // Sum of incoming pin widths for each device
  Seq<uint16_t> numIn;

  // Mapping from device id to device pointer
  // (Not valid until the mapper is called)
  PDeviceInfo<DeviceType>** devices;

  // Mapping from thread id to number of devices on that thread
  // (Not valid until the mapper is called)
  uint32_t* numDevicesOnThread;

  // Mapping from device id to device address and back
  // (The "pin" field of the device address is ignored)
  // (Not valid until the mapper is called)
  PDeviceAddr* toDeviceAddr;  // Device id -> device address
  PDeviceId** fromDeviceAddr; // Device address -> device id

  // Each thread's heap (DRAM partition), size, and base
  // (Not valid until the mapper is called)
  uint8_t** heap;
  uint32_t* heapSize;
  uint32_t* heapBase;

  // Create new device
  PDeviceId newDevice() {
    PDeviceId d = numDevices;
    numDevices++;
    const uint32_t initialCapacity = 4;
    incoming.append(new Seq<Seq<PGlobalPinId>*> (initialCapacity));
    outgoing.append(new Seq<Seq<PGlobalPinId>*> (initialCapacity));
    Seq<uint16_t>* initialPinWidths = new Seq<uint16_t> (initialCapacity);
    initialPinWidths->append(1); // Pin 0 has width 1
    pinWidth.append(initialPinWidths);
    numIn.append(0);
    return d;
  }

  // Set pin width (i.e. number of messages sent over pin per tick)
  // The pin width must be set before adding edges from that pin
  inline void setPinWidth(PGlobalPinId pin, uint16_t numMsgs)
  {
    // Pin id 0 is reserved
    assert(pin.pinId != 0);
    // Set pin width
    Seq<uint16_t>* widths = pinWidth.elems[pin.devId];
    for (uint32_t i = widths->numElems; i <= pin.pinId; i++)
      widths->append(0);
    widths->elems[pin.pinId] = numMsgs;
  }

  // Set pin width (i.e. number of messages sent over pin per tick)
  // The pin width must be set before adding edges from that pin
  void setPinWidth(PDeviceId devId, PinId pinId, uint16_t numMsgs)
  {
    PGlobalPinId p; p.devId = devId; p.pinId = pinId;
    setPinWidth(p, numMsgs);
  }

  // Add a connection between device pins
  inline void addEdgeUnchecked(PGlobalPinId from, PGlobalPinId to) {
    const uint32_t initialCapacity = 4;
    // Add outgoing edge
    Seq<Seq<PGlobalPinId>*>* pinList = outgoing.elems[from.devId];
    for (uint32_t i = pinList->numElems; i <= from.pinId; i++)
      pinList->append(new Seq<PGlobalPinId> (initialCapacity));
    pinList->elems[from.pinId]->append(to);
    // Add incoming edge
    pinList = incoming.elems[to.devId];
    for (uint32_t i = pinList->numElems; i <= to.pinId; i++)
      pinList->append(new Seq<PGlobalPinId> (initialCapacity));
    pinList->elems[to.pinId]->append(from);
    // Increment number of incoming messages per tick on target device
    assert(from.pinId < pinWidth.elems[from.devId]->numElems);
    numIn.elems[to.devId] += pinWidth.elems[from.devId]->elems[from.pinId];
  }

  // Add a connection between device pins
  void addEdge(PGlobalPinId from, PGlobalPinId to) {
    // Pin id 0 is reserved
    assert(from.pinId != 0 && to.pinId != 0);
    addEdgeUnchecked(from, to);
  }

  // Add a connection between device pins
  inline void addEdgeUnchecked(PDeviceId from, PinId sourcePin,
                               PDeviceId to, PinId sinkPin) {
    PGlobalPinId src; src.devId = from; src.pinId = sourcePin;
    PGlobalPinId dst; dst.devId = to; dst.pinId = sinkPin;
    addEdgeUnchecked(src, dst);
  }

  // Add a connection between device pins
  void addEdge(PDeviceId from, PinId sourcePin,
                      PDeviceId to, PinId sinkPin) {
    PGlobalPinId src; src.devId = from; src.pinId = sourcePin;
    PGlobalPinId dst; dst.devId = to; dst.pinId = sinkPin;
    addEdge(src, dst);
  }

  // Add a sync edge (i.e. on pin 0) from any device p to any device q
  // if q is connected to p but not viceversa.  This ensures correct
  // GALS synchronisation for any graph.
  void addSyncEdges() {
    for (uint32_t q = 0; q < numDevices; q++) {
      Seq<Seq<PGlobalPinId>*>* qPinList = outgoing.elems[q];
      for (uint32_t i = 0; i < qPinList->numElems; i++) {
        Seq<PGlobalPinId>* qOutList = qPinList->elems[i];
        for (uint32_t j = 0; j < qOutList->numElems; j++) {
          bool found = false;
          PDeviceId p = qOutList->elems[j].devId;
          Seq<Seq<PGlobalPinId>*>* pPinList = outgoing.elems[p];
          for (uint32_t x = 0; x < pPinList->numElems; x++) {
            Seq<PGlobalPinId>* pOutList = pPinList->elems[x];
            for (uint32_t y = 0; y < pOutList->numElems; y++) {
              PDeviceId d = pOutList->elems[y].devId;
              if (d == q) {
                found = true;
                break;
              }
            }
            if (found) break;
          }
          if (! found) addEdgeUnchecked(p, 0, q, 0);
        }
      }
    }
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
      // Number of devices on this thread
      uint32_t numDevs = numDevicesOnThread[threadId];
      // Add space for device array
      size = cacheAlign(size + numDevs * sizeof(PDeviceInfo<DeviceType>));
      // For each device
      for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
        PDeviceId id = fromDeviceAddr[threadId][devNum];
        // Add space for pin info
        // (number of messages per pin and fanout of each pin)
        uint32_t numPins = outgoing.elems[id]->numElems;
        size = cacheAlign(size + numPins * sizeof(PPinInfo));
        // Add space for outgoing edges
        Seq<Seq<PGlobalPinId>*>* pinList = outgoing.elems[id];
        uint32_t numEdges = 0;
        for (uint32_t i = 0; i < pinList->numElems; i++)
          numEdges += pinList->elems[i]->numElems;
        size = cacheAlign(size + numEdges * sizeof(PDeviceAddr));
        // Add space for device states
        size = cacheAlign(size + sizeof(DeviceType));
        size = cacheAlign(size + sizeof(DeviceType));
        size = cacheAlign(size + sizeof(DeviceType));
      }
      // The total heap size including uninitialised portions
      uint32_t totalSize = size + 4 * (numDevs+1);
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
      // Number of decices on thread
      uint32_t numDevs = numDevicesOnThread[threadId];
      // Set number of devices on thread
      thread->numDevices = numDevs;
      // Set tinsel address of devices array
      thread->devices = heapBase[threadId] + hp;
      // Add space for each device on thread
      for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
        PDeviceInfo<DeviceType>* dev =
          (PDeviceInfo<DeviceType>*) &heap[threadId][hp];
        PDeviceId id = fromDeviceAddr[threadId][devNum];
        devices[id] = dev;
        // Add space for device
        hp = hp + sizeof(PDeviceInfo<DeviceType>);
      }
      // Re-align hp
      hp = cacheAlign(hp);
      // Initialise each device and associated info
      for (uint32_t devNum = 0; devNum < numDevs; devNum++) {
        PDeviceId id = fromDeviceAddr[threadId][devNum];
        PDeviceInfo<DeviceType>* dev = devices[id];
        // Set thread-local device address
        dev->localAddr = devNum;
        // Set number of incoming messages per tick
        dev->numIn = numIn.elems[id];
        // Determine number of pins on device
        uint16_t numPins = (uint16_t) pinWidth.elems[id]->numElems;
        dev->numOutPins = numPins;
        // Set location of pin info array
        dev->pinInfo = heapBase[threadId] + hp;
        PPinInfo* pinInfoArray = (PPinInfo*) &heap[threadId][hp];
        Seq<Seq<PGlobalPinId>*>* pinList = outgoing.elems[id];
        for (uint32_t i = 0; i < pinList->numElems; i++) {
          pinInfoArray[i].fanOut = pinList->elems[i]->numElems;
          pinInfoArray[i].numMsgs = pinWidth.elems[id]->elems[i];
        }
        hp = cacheAlign(hp + numPins * sizeof(PPinInfo));
        // Set tinsel address of edges array
        dev->outEdges = heapBase[threadId] + hp;
        // Edge array
        PDeviceAddr* edgeArray = (PDeviceAddr*) &heap[threadId][hp];
        // Outgoing edges
        uint32_t numEdges = 0;
        for (uint32_t i = 0; i < pinList->numElems; i++) {
          Seq<PGlobalPinId>* outList = pinList->elems[i];
          for (uint32_t j = 0; j < outList->numElems; j++) {
            PGlobalPinId p = outList->elems[j];
            PDeviceAddr addr = toDeviceAddr[p.devId];
            addr.pin = p.pinId;
            edgeArray[numEdges++] = addr;
          }
        }
        hp = cacheAlign(hp + numEdges * sizeof(PDeviceAddr));
        // Set location of previous state
        dev->prev = heapBase[threadId] + hp;
        hp = cacheAlign(hp + sizeof(DeviceType));
        // Set location of current state
        dev->current = heapBase[threadId] + hp;
        hp = cacheAlign(hp + sizeof(DeviceType));
        // Set location of next state
        dev->next = heapBase[threadId] + hp;
        hp = cacheAlign(hp + sizeof(DeviceType));
      }
      // At this point, check that hp lines up with heap size
      if (hp != heapSize[threadId]) {
        printf("Error: heap size does not match pre-computed heap size\n");
        exit(EXIT_FAILURE);
      }
      // Set tinsel address of sender queue (queue of ready senders)
      thread->queue = heapBase[threadId] + hp;
    }
  }

  #ifndef TINSEL
  // Get a pointer to the initial state of a given device
  // (Must only be called after the mapper)
  DeviceType* getState(PDeviceId id) {
    PDeviceAddr addr = toDeviceAddr[id];
    uint32_t offset = devices[id]->prev - heapBase[addr.threadId];
    return (DeviceType*) &heap[addr.threadId][offset];
  }
  #endif

  // Allocate mapping structures
  void allocateMapping() {
    devices = (PDeviceInfo<DeviceType>**)
      calloc(numDevices, sizeof(PDeviceInfo<DeviceType>*));
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

    // Andd sync-only edges
    addSyncEdges();

    // Create connectivity graph for placer
    Graph graph;
    for (uint32_t i = 0; i < numDevices; i++) graph.newNode();
    for (uint32_t i = 0; i < numDevices; i++) {
      Seq<Seq<PGlobalPinId>*>* pinList = outgoing.elems[i];
      for (uint32_t j = 0; j < pinList->numElems; j++) {
        Seq<PGlobalPinId>* edgeList = pinList->elems[j];
        for (uint32_t k = 0; k < edgeList->numElems; k++) {
          PGlobalPinId p = edgeList->elems[k];
          graph.addBidirectionalEdge(i, p.devId);
        }
      }
    }

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
        Placer boxes(&boards.subgraphs[b], TinselMailboxesPerBoard, 1);
        boxes.place(placerEffort);

        // For each mailbox
        for (uint32_t boxNum = 0; boxNum < TinselMailboxesPerBoard; boxNum++) {
          // Partition into subgraphs, one per thread
          uint32_t numThreads = 1<<TinselLogThreadsPerMailbox;
          PartitionId t = boxes.mapping[0][boxNum];
          Placer threads(&boxes.subgraphs[t], numThreads, 1);

          // For each thread
          for (uint32_t threadNum = 0; threadNum < numThreads; threadNum++) {
            // Determine tinsel thread id
            uint32_t threadId = boardY;
            threadId = (threadId << TinselMeshXBits) | boardX;
            threadId = (threadId << TinselLogMailboxesPerBoard) | boxNum;
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
    // Release pin widths
    for (uint32_t d = 0; d < numDevices; d++)
      delete pinWidth.elems[d];
    // Release edge lists
    for (uint32_t d = 0; d < numDevices; d++) {
      // Release incoming edges
      Seq<Seq<PGlobalPinId>*>* pinList = incoming.elems[d];
      for (uint32_t i = 0; i < pinList->numElems; i++)
        delete pinList->elems[i];
      delete pinList;
      // Release outgoing edges
      pinList = outgoing.elems[d];
      for (uint32_t i = 0; i < pinList->numElems; i++)
        delete pinList->elems[i];
      delete pinList;
    }
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
