#ifndef _PMESH2D_H_
#define _PMESH2D_H_

#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <HostLink.h>
#include <config.h>
#include "Polite.h"
#include "Seq.h"

// Rectangular region
struct Region2D { uint32_t top, left, bottom, right; };

// Split a region into numX by numY similarly-sized sub-regions
struct SubRegions2D {
  // There are numX by numY sub-regions
  uint32_t numX, numY;
  Region2D** subRegions;

  // Construcor
  SubRegions2D(Region2D r, uint32_t nX, uint32_t nY) {
    // Save dimensions
    numX = nX;
    numY = nY;

    // Allocate sub-regions
    subRegions = new Region2D* [nY];
    for (uint32_t y = 0; y < nY; y++) subRegions[y] = new Region2D [nX];

    // Compute width and height of region
    uint32_t width = r.right - r.left;
    uint32_t height = r.bottom - r.top;

    // Swap dimensions to match region shape
    bool transpose = false;
    if ((width > height && nX < nY) || (width < height && nX > nY)) {
      uint32_t tmp = nX; nX = nY; nY = tmp;
      transpose = true;
    }

    // Compute sub-regions
    uint32_t accX = r.left;
    for (uint32_t i = 0; i < nX; i++) {
      uint32_t lenX = width/nX + (i < (width%nX) ? 1 : 0);
      uint32_t accY = r.top;
      for (uint32_t j = 0; j < nY; j++) {
        uint32_t lenY = height/nY + (i < (height%nY) ? 1 : 0);
        Region2D sub;
        sub.top = accY; sub.left = accX;
        sub.bottom = accY+lenY; sub.right = accX+lenX;
        if (transpose)
          subRegions[i][j] = sub;
        else
          subRegions[j][i] = sub;
        accY += lenY;
      }
      accX += lenX;
    }
  }

  // Deconstructor
  ~SubRegions2D() {
    // Release sub-regions
    for (uint32_t y = 0; y < numY; y++) delete [] subRegions[y];
    delete [] subRegions;
  }
};

// The virtual device id consists of an X and Y coordinate
struct PMesh2DId {
  uint32_t y, x;
};

// A POETS graph that fits a 2D mesh topology
template <typename DeviceType, typename MessageType> class PMesh2D {
 private:
  // Approximately factor a number into two dimensions.  Try to
  // maximise the area of the resulting rectangle (use as many threads
  // as possible), while minimising the perimiter (keep the comms
  // overhead down).
  void factor2D(uint32_t n, uint32_t &bestX, uint32_t &bestY)
  {
    bestX = (uint32_t) sqrt((double) n);
    bestY = n / bestX;
    if (bestX*bestY == n) return;

    uint32_t x = bestX-1;
    while (x > 0) {
      uint32_t y = n / x;
      double efficiency = double(bestX*2+bestY*2) / double(x*2+y*2);
      if (efficiency < 0.85) return;
      if (x*y > bestX*bestY) { bestX = x; bestY = y; }
      x--;
    }
  }

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
  // Dimensions of the mesh
  uint32_t width, height;

  // Mapping from virtual mesh to physical threads and back
  PDeviceId** toDeviceId;   // Virtual mesh coords -> physical device id
  PMesh2DId** fromThreadId; // Thread id -> array of mesh coords

  // Mapping from thread id to number of devices
  uint32_t* numDevices;

  // 2D mesh of device pointers
  DeviceType*** devices;

  // Each thread's heap (DRAM partition), size, and base
  uint8_t** heap;
  uint32_t* heapSize;
  uint32_t* heapBase;

  // Implement mapping to tinsel threads
  void mapper() {
    // Full 2D region covered by mesh
    Region2D fullRegion;
    fullRegion.top = 0; fullRegion.left = 0;
    fullRegion.bottom = height; fullRegion.right = width;

    // Split into sub-regions, one per board
    SubRegions2D boards(fullRegion, TinselMeshXLen, TinselMeshYLen);
    // For each board
    for (uint32_t boardY = 0; boardY < TinselMeshYLen; boardY++) {
      for (uint32_t boardX = 0; boardX < TinselMeshXLen; boardX++) {
        // Split into sub-regions, one per mailbox
        uint32_t numMailboxesX, numMailboxesY;
        factor2D(TinselMailboxesPerBoard, numMailboxesX, numMailboxesY);
        SubRegions2D boxes(boards.subRegions[boardY][boardX],
                           numMailboxesX, numMailboxesY);
        // For each mailbox
        uint32_t boxNum = 0;
        for (uint32_t boxY = 0; boxY < numMailboxesY; boxY++) {
          for (uint32_t boxX = 0; boxX < numMailboxesX; boxX++, boxNum++) {
            // Split into sub-regions, one per thread
            uint32_t numThreadsX, numThreadsY;
            factor2D(1<<TinselLogThreadsPerMailbox, numThreadsX, numThreadsY);
            SubRegions2D threads(boxes.subRegions[boxY][boxX],
                                 numThreadsX, numThreadsY);
            // For each thread
            uint32_t threadNum = 0;
            for (uint32_t threadY = 0; threadY < numThreadsY; threadY++) {
              for (uint32_t threadX = 0; threadX < numThreadsX;
                                           threadX++, threadNum++) {
                // Determine tinsel thread id
                uint32_t threadId = boardY;
                threadId = (threadId << TinselMeshXBits) | boardX;
                threadId = (threadId << TinselLogMailboxesPerBoard) | boxNum;
                threadId = (threadId << (TinselLogCoresPerMailbox +
                              TinselLogThreadsPerCore)) | threadNum;
                // Populate fromThreadId mapping
                Region2D r = threads.subRegions[threadY][threadX];
                uint32_t numDevs = (r.bottom-r.top) * (r.right-r.left);
                fromThreadId[threadId] = (PMesh2DId*)
                  malloc(sizeof(PMesh2DId) * numDevs);
                uint32_t devNum = 0;
                for (int y = r.top; y < r.bottom; y++) {
                  for (int x = r.left; x < r.right; x++, devNum++) {
                    PMesh2DId coords; coords.y = y; coords.x = x;
                    fromThreadId[threadId][devNum] = coords;
                  }
                }
                numDevices[threadId] = devNum;
                // Populate toDeviceId mapping
                devNum = 0;
                for (int y = r.top; y < r.bottom; y++) {
                  for (int x = r.left; x < r.right; x++, devNum++) {
                    PDeviceId devId;
                    devId.threadId = threadId;
                    devId.localId = devNum;
                    toDeviceId[y][x] = devId;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  // Determine the neighbours of mesh node
  void neighbours(PMesh2DId node, Seq<PDeviceId>* ns) {
    ns->clear();
    // Look up, down, left and right
    if (node.y > 0) ns->append(toDeviceId[node.y-1][node.x]);
    if (node.y < height-1) ns->append(toDeviceId[node.y+1][node.x]);
    if (node.x > 0) ns->append(toDeviceId[node.y][node.x-1]);
    if (node.x < width-1) ns->append(toDeviceId[node.y][node.x+1]);
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
    Seq<PDeviceId> ns;
    for (uint32_t threadId = 0; threadId < TinselMaxThreads; threadId++) {
      // This variable is used to count the size of the *initialised*
      // heap.  The total heap size is larger as it includes
      // uninitialised portions.
      uint32_t size = 0;
      // Add space for thread structure
      size = cacheAlign(size + sizeof(PThread<DeviceType, MessageType>));
      // Add space for device array
      size = cacheAlign(size + numDevices[threadId] * sizeof(DeviceType));
      // Add space for edge lists
      for (uint32_t devNum = 0; devNum < numDevices[threadId]; devNum++) {
        PMesh2DId coords = fromThreadId[threadId][devNum];
        // Compute neighbours of device
        neighbours(coords, &ns);
        // Add space for edge lists
        size = cacheAlign(size + 2 * ns.numElems * sizeof(PDeviceId));
      }
      // The total heap size including unintialised portions
      uint32_t totalSize = size + 8 * numDevices[threadId];
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
    Seq<PDeviceId> ns;
    for (uint32_t threadId = 0; threadId < TinselMaxThreads; threadId++) {
      // Heap pointer
      uint32_t hp = 0;
      // Pointer to thread structure
      PThread<DeviceType, MessageType>* thread =
        (PThread<DeviceType, MessageType>*) &heap[threadId][hp];
      // Add space for thread structure
      hp = cacheAlign(hp + sizeof(PThread<DeviceType, MessageType>));
      // Set number of devices on thread
      thread->numDevices = numDevices[threadId];
      // Set tinsel address of devices array
      thread->devices = heapBase[threadId] + hp;
      // Add space for each device on thread
      for (uint32_t devNum = 0; devNum < numDevices[threadId]; devNum++) {
        DeviceType* dev = (DeviceType*) &heap[threadId][hp];
        PMesh2DId coords = fromThreadId[threadId][devNum];
        devices[coords.y][coords.x] = dev;
        // Add space for device
        hp = hp + sizeof(DeviceType);
      }
      // Re-align hp
      hp = cacheAlign(hp);
      // Initialise each device and associated edge list
      for (uint32_t devNum = 0; devNum < numDevices[threadId]; devNum++) {
        PMesh2DId coords = fromThreadId[threadId][devNum];
        DeviceType* dev = devices[coords.y][coords.x];
        // Set thread-local device id
        dev->localId = devNum;
        // Compute neighbours
        neighbours(coords, &ns);
        // Set fanIn and fanOut
        dev->fanIn = dev->fanOut = ns.numElems;
        // Set tinsel address of edges array
        dev->edges = heapBase[threadId] + hp;
        // Edge array
        PDeviceId* edgeArray = (PDeviceId*) &heap[threadId][hp];
        // Outgoing edges
        for (uint32_t i = 0; i < ns.numElems; i++)
          edgeArray[i] = ns.elems[i];
        // Incoming edges
        for (uint32_t i = 0; i < ns.numElems; i++)
          edgeArray[ns.numElems+i] = ns.elems[i];
        // Add space for edges
        hp = cacheAlign(hp + 2 * ns.numElems * sizeof(PDeviceId));
      }
      // At this point, check that hp lines up with heap size
      if (hp != heapSize[threadId]) {
        printf("Error: heap size does not match pre-computed heap size\n");
        exit(EXIT_FAILURE);
      }
      // Set tinsel address of intArray and extArray arrays
      thread->intArray = heapBase[threadId] + hp;
      thread->extArray = thread->intArray +
        numDevices[threadId] * sizeof(uint32_t);
    }
  }

  // Constructor
  PMesh2D(uint32_t w, uint32_t h) {
    // Save mesh dimensions
    width = w; height = h;
    // Allocate device pointers
    devices = (DeviceType***) malloc(sizeof(DeviceType**) * h);
    for (uint32_t y = 0; y < h; y++)
      devices[y] = (DeviceType**) calloc(w, sizeof(DeviceType*));
    // Allocate toDeviceId mapping
    toDeviceId = (PDeviceId**) malloc(sizeof(PDeviceId*) * h);
    for (uint32_t y = 0; y < h; y++)
      toDeviceId[y] = (PDeviceId*) calloc(w, sizeof(PDeviceId));
    // Allocate numDevices mapping
    numDevices = (uint32_t*) calloc(TinselMaxThreads, sizeof(uint32_t));
    // Allocate fromThreadId mapping
    fromThreadId = (PMesh2DId**) calloc(TinselMaxThreads, sizeof(PMesh2DId*));
    // Invoke the mapper
    mapper();
    // Allocate and initialise heaps
    allocateHeaps();
    initialiseHeaps();
  }

  // Deconstructor
  ~PMesh2D() {
    // Release device pointers
    for (uint32_t y = 0; y < height; y++) free(devices[y]);
    free(devices);
    // Release toDeviceId mapping
    for (uint32_t y = 0; y < height; y++) free(toDeviceId[y]);
    free(toDeviceId);
    // Release numDevices mapping
    free(numDevices);
    // Release fromThreadId mapping
    for (uint32_t t = 0; t < TinselMaxThreads; t++)
      if (fromThreadId[t] != NULL) free(fromThreadId[t]);
    free(fromThreadId);
    // Release heaps
    for (uint32_t t = 0; t < TinselMaxThreads; t++)
      if (heap[t] != NULL) free(heap[t]);
    free(heap);
    free(heapSize);
    free(heapBase);
  }

  // Write devices (including topology) to tinsel machine
  void map(HostLink* hostLink) {
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
