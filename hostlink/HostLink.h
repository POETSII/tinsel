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
#define PCIESTREAM_LOCK     "/tmp/pciestream-lock"
#define PCIESTREAM_IN       "/tmp/pciestream-in"
#define PCIESTREAM_OUT      "/tmp/pciestream-out"
#define PCIESTREAM_CTRL_IN  "/tmp/pciestream-ctrl-in"

class HostLink {
  // JTAG UART connections
  DebugLink* debugLinks;

  // Lock file for acquring exclusive access to PCIeStream
  int lockFile;

  // PCIeStream file descriptors
  int toPCIe, fromPCIe, toPCIeCtrl, fromPCIeCtrl;

  // Line buffers for JTAG UART StdOut
  char lineBuffer[TinselMeshXLen][TinselMeshYLen]
                 [TinselCoresPerBoard][TinselThreadsPerCore]
                 [MaxLineLen];
  int lineBufferLen[TinselMeshXLen][TinselMeshYLen]
                   [TinselCoresPerBoard][TinselThreadsPerCore];
 public:

  // DebugLink to the bridge board
  DebugLink* bridgeBoard;

  // DebugLinks to the cluster boards
  DebugLink* mesh[TinselMeshXLen][TinselMeshYLen];

  // Constructor
  HostLink();

  // Destructor
  ~HostLink();
 
  // Address construction
  uint32_t toAddr(uint32_t meshX, uint32_t meshY,
             uint32_t coreId, uint32_t threadId);

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

  // Interface to boot loader
  // ------------------------

  // Load application code and data onto the mesh
  void boot(const char* codeFilename, const char* dataFilename);

  // Trigger to start application execution
  void go();

  // Load application code and data onto a single thread
  void bootOne(const char* codeFilename, const char* dataFilename);

  // Trigger to start application execution on a single thread
  void goOne();

  // UART console
  // ------------

  // Display StdOut character stream
  bool pollStdOut(FILE* outFile);
  bool pollStdOut();
  void dumpStdOut(FILE* outFile);
  void dumpStdOut();
};

#endif
