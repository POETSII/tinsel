#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <config.h>
#include "DebugLink.h"

// Open UART with given instance id
void DebugLink::open(int instId)
{
  uart.open(instId);
}

// Send query request
void DebugLink::putQuery()
{
  uint8_t cmd[2];
  cmd[0] = DEBUGLINK_QUERY_IN;
  uart.put(cmd, 1);
  uart.flush();
}

// Get query response
bool DebugLink::getQuery(uint32_t* boardId)
{
  uint8_t cmd[2];
  uart.get(cmd, 2);
  if (cmd[0] != DEBUGLINK_QUERY_OUT) {
    fprintf(stderr, "Unexpected response to CoreLink query command\n");
    exit(EXIT_FAILURE);
  }
  if (cmd[1] != 0) {
    *boardId = (uint32_t) (cmd[1] - 1);
    return true;
  }
  return false;
}

// Set destination to thread id
void DebugLink::setDest(uint32_t coreId, uint32_t threadId)
{
  uint8_t cmd[3];
  // SetDest command
  cmd[0] = DEBUGLINK_SET_DEST;
  // Core-local thread id
  cmd[1] = threadId;
  // Board-local core id
  cmd[2] = coreId;
  // Send command
  uart.put(cmd, 3);
}

// Set destinations to given local thread id on every core
void DebugLink::setBroadcastDest(uint32_t threadId)
{
  uint8_t cmd[3];
  // SetDest command
  cmd[0] = DEBUGLINK_SET_DEST;
  // Core-local thread id
  cmd[1] = (uint8_t) threadId;
  // Broadcast address
  cmd[2] = 0x80;
  // Send command
  uart.put(cmd, 3);
}

// Send byte to destination thread (StdIn)
void DebugLink::put(uint8_t byte)
{
  uint8_t cmd[2];
  cmd[0] = DEBUGLINK_STD_IN;
  cmd[1] = byte;
  uart.put(cmd, 2);
}

// Receive byte (StdOut)
void DebugLink::get(uint32_t* coreId, uint32_t* threadId, uint8_t* byte)
{
  uint8_t cmd[4];
  uart.get(cmd, 4);
  if (cmd[0] != DEBUGLINK_STD_OUT) {
    fprintf(stderr, "Got unexpected response.  Expected StdOut.");
    exit(EXIT_FAILURE);
  }
  *coreId = (uint32_t) cmd[2];
  *threadId = (uint32_t) cmd[1];
  *byte = cmd[3];
}

// Is a data available for reading?
bool DebugLink::canGet()
{
  return uart.canGet();
}

// Flush writes
void DebugLink::flush()
{
  uart.flush();
}

// Close UART
void DebugLink::close()
{
  uart.close();
}
