#ifndef _UART_H_
#define _UART_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "JtagAtlantic.h"

// Non-blocking connection to JTAG UART.  This class abstracts over
// whether tinsel is running in simulation or on FPGA.
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
  int write(char* data, int numBytes);

  // Receive bytes over UART
  int read(char* data, int numBytes);

  // Flush writes
  void flush();

  // Close UART
  void close();

  // Destructor
  ~UART();
};

#endif
