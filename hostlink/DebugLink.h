#ifndef _DEBUGLINK_H_
#define _DEBUGLINK_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "UART.h"

// CoreLink commands
#define DEBUGLINK_QUERY_IN  0
#define DEBUGLINK_QUERY_OUT 0
#define DEBUGLINK_SET_DEST  1
#define DEBUGLINK_STD_IN    2
#define DEBUGLINK_STD_OUT   2

class DebugLink {
  UART uart;
 public:
  // Open UART with given instance id
  void open(int instId);

  // Put query request
  void putQuery();

  // Get query response, including board id
  bool getQuery(uint32_t* boardId);

  // Set destination core and thread
  void setDest(uint32_t coreId, uint32_t threadId);

  // Set destinations to core-local thread id on every core
  void setBroadcastDest(uint32_t threadId);

  // Send byte to destination thread (StdIn)
  void put(uint8_t byte);

  // Receive byte (StdOut)
  void get(uint32_t* coreId, uint32_t* threadId, uint8_t* byte);

  // Is a data available for reading?
  bool canGet();
  
  // Flush writes
  void flush();

  // Close UART
  void close();
};

#endif
