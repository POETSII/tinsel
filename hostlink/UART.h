#ifndef _UART_H_
#define _UART_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "JtagAtlantic.h"

class UART {
  #ifdef SIMULATE
  int sock;
  #else
  JTAGATLANTIC* jtag;
  #endif
 public:
  int instanceId;

  // Constructor
  UART();

  // Open UART with given instance id
  void open(int instId);

  // Send bytes over UART
  void put(void* buffer, int numBytes);

  // Receive bytes over UART
  void get(void* buffer, int numBytes);

  // Is a byte available for reading?
  bool canGet();
  
  // Flush writes
  void flush();

  // Close UART
  void close();
};

#endif
