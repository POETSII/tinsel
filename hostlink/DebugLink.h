// SPDX-License-Identifier: BSD-2-Clause
#ifndef _DEBUGLINK_H_
#define _DEBUGLINK_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "BoardCtrl.h"
#include "DebugLinkFormat.h"

#include <functional>

// DebugLinkH parameters
struct DebugLinkParams {
  uint32_t numBoxesX;
  uint32_t numBoxesY;
  bool useExtraSendSlot;

  // Used to indicate when the hostlink moves through different phases, especially in construction
  std::function<void(const char *)> on_phase;

  // Used to allow retries when connecting to the socket. When performing rapid sweeps,
  // it is quite common for the first attempt in the next process to fail.
  int max_connection_attempts = 5;
};

class DebugLink {

  // Location of this box with full box mesh
  int thisBoxX;
  int thisBoxY;

  // Mapping from (box Y, box X) to TCP connection
  int** conn;

  // Mapping from global (board Y, board X) to (box X, box Y, link id)
  int** boxX;
  int** boxY;
  int** linkId;
 
  // Mapping from (box Y, box X, link id) to global (board X, board Y)
  int*** boardX;
  int*** boardY;

  // Mapping from (box Y, box X) to link id of the bridge board
  int** bridge;
 
  // For fairness between boxes in reading bytes from DebugLink
  int get_tryNextX;
  int get_tryNextY;

  // Helper: blocking send/receive of a BoardCtrlPkt
  void getPacket(int x, int y, BoardCtrlPkt* pkt);
  void putPacket(int x, int y, BoardCtrlPkt* pkt);
 public:
  // Length of box mesh in X and Y dimension
  int boxMeshXLen;
  int boxMeshYLen;

  // Length of board mesh in X and Y dimension
  int meshXLen;
  int meshYLen;

  // Constructor
  DebugLink(DebugLinkParams params);

  // On given board, set destination core and thread
  void setDest(uint32_t boardX, uint32_t boardY,
                 uint32_t coreId, uint32_t threadId);

  // On given board, set destinations to core-local thread id on every core
  void setBroadcastDest(uint32_t boardX, uint32_t boardY, uint32_t threadId);

  // On given board, send byte to destination thread (StdIn)
  void put(uint32_t boardX, uint32_t boardY, uint8_t byte);

  // Receive byte (StdOut)
  void get(uint32_t* boardX, uint32_t* boardY,
             uint32_t* coreId, uint32_t* threadId, uint8_t* byte);

  // Read temperature of given board
  int32_t getBoardTemp(uint32_t boardX, uint32_t boardY);

  // Read temperature of given bridge
  int32_t getBridgeTemp(uint32_t boxX, uint32_t boxY);

  // Is a data available for reading?
  bool canGet();

  // Destructor
  ~DebugLink();
};

#endif
