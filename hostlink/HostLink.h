#ifndef _HOSTLINK_H_
#define _HOSTLINK_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <config.h>
#include "DebugLink.h"

class HostLink {
  // JTAG UART connections
  DebugLink* debugLinks;

  // PCIeStream file descriptors
  int toPCIe, fromPCIe, pcieCtrl;

 public:

  // DebugLink to the host board
  DebugLink* hostBoard;

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
};

#endif
