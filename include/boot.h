#ifndef _BOOT_H_
#define _BOOT_H_

#include <stdint.h>

// 16-byte boot request
typedef struct {
  uint8_t cmd;
  uint8_t numArgs;
  uint16_t unused;
  uint32_t args[3];
} BootReq;

// Various commands supported by the boot loader
typedef enum {

  // Set the "address register" for a subsequent read/write operation.
  // Argument: a 32-bit address.
  SetAddrCmd,

  // Write to instruction memory and increment address register.
  // Argument: up to 3 x 32-bit instructions to write.
  // The address is taken from the address register.
  WriteInstrCmd,
 
  // Perform a store instruction and increment address register.
  // Argument: up to 3 x 32-bit words to store.
  // The address is taken from the address register.
  StoreCmd,

  // Perform a load instruction and increment address register.
  // Argument: the number of 32-bit words to load.
  // The address is taken from the address register.
  LoadCmd,

  // StartCmd performs a cache flush, sends ack to the host, waits for
  // the UART trigger, starts threads, and jumps to the
  // application code.  The first argument is the number of threads
  // to start.
  StartCmd,

  // Ping a thread with an optional thread->thread hop
  // Argument 0: number of hops (zero or one)
  // Argument 1: next hop address (when number of hops is one)
  PingCmd,

} BootCmd;


#endif
