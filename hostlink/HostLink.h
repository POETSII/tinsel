#ifndef _HOSTLINK_H_
#define _HOSTLINK_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <config.h>
#include "DebugLink.h"

// Max line length for line-buffered UART StdOut capture
#define MaxLineLen 128

// Connections to PCIeStream
#define PCIESTREAM      "pciestream"
#define PCIESTREAM_SIM  "tinsel.b-1.5"

// Power down the mesh boards
void powerdown();

// Power up the mesh boards
void powerup();

class HostLink {
  // JTAG UART connections
  DebugLink* debugLinks;

  // Lock file for acquring exclusive access to PCIeStream
  int lockFile;

  // File descriptor for link to PCIeStream
  int pcieLink;

  // Line buffers for JTAG UART StdOut
  char lineBuffer[TinselMeshXLen][TinselMeshYLen]
                 [TinselCoresPerBoard][TinselThreadsPerCore]
                 [MaxLineLen];
  int lineBufferLen[TinselMeshXLen][TinselMeshYLen]
                   [TinselCoresPerBoard][TinselThreadsPerCore];
 public:

  // Constructor
  HostLink();

  // Destructor
  ~HostLink();
 
  // Debug links
  // -----------

  // Link to the bridge board (opened by constructor)
  DebugLink* bridgeBoard;

  // Links to the mesh boards (opened by constructor)
  DebugLink* mesh[TinselMeshXLen][TinselMeshYLen];

  // Send and receive messages over PCIe
  // -----------------------------------

  // Send a message (blocking)
  void send(uint32_t dest, uint32_t numFlits, void* msg);

  // Can send a message without blocking?
  bool canSend();

  // Receive a flit (blocking)
  void recv(void* flit);

  // Can receive a flit without blocking?
  bool canRecv();

  // Receive a message (blocking), given size of message in bytes
  void recvMsg(void* msg, uint32_t numBytes);

  // Address construction/deconstruction
  // -----------------------------------

  // Address construction
  uint32_t toAddr(uint32_t meshX, uint32_t meshY,
             uint32_t coreId, uint32_t threadId);

  // Address deconstruction
  void fromAddr(uint32_t addr, uint32_t* meshX, uint32_t* meshY,
         uint32_t* coreId, uint32_t* threadId);

  // Assuming the boot loader is running on the cores
  // ------------------------------------------------
  //
  // (Only thread 0 on each core is active when the boot loader is running)

  // Load application code and data onto the mesh
  void boot(const char* codeFilename, const char* dataFilename);

  // Trigger to start application execution
  void go();

  // Set address for remote memory access to given board via given core
  // (This address is auto-incremented on loads and stores)
  void setAddr(uint32_t meshX, uint32_t meshY,
               uint32_t coreId, uint32_t addr);

  // Store words to remote memory on given board via given core
  void store(uint32_t meshX, uint32_t meshY,
             uint32_t coreId, uint32_t numWords, uint32_t* data);

  // Finer-grained control over application loading and execution
  // ------------------------------------------------------------

  // Load instructions into given core's instruction memory
  void loadInstrsOntoCore(const char* codeFilename,
         uint32_t meshX, uint32_t meshY, uint32_t coreId);

  // Load data via given core on given board
  void loadDataViaCore(const char* dataFilename,
        uint32_t meshX, uint32_t meshY, uint32_t coreId);

  // Start given number of threads on given core
  void startOne(uint32_t meshX, uint32_t meshY,
         uint32_t coreId, uint32_t numThreads);

  // Trigger application execution on all started threads on given core
  void goOne(uint32_t meshX, uint32_t meshY, uint32_t coreId);

  // Line-buffered StdOut console
  // ----------------------------

  // Receive StdOut byte streams and append to file (non-blocking)
  bool pollStdOut(FILE* outFile);

  // Receive StdOut byte streams and display on stdout (non-blocking)
  bool pollStdOut();

  // Receive StdOut byte streams and append to file (non-terminating)
  void dumpStdOut(FILE* outFile);

  // Receive StdOut byte streams and display on stdout (non-terminating)
  void dumpStdOut();
};

#endif
