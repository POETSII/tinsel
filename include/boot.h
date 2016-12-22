#ifndef _BOOT_H_
#define _BOOT_H_

#include <stdint.h>

// Various commands supported by the boot loader
typedef enum {

  // Set the "address register" for a subsequent read/write operation.
  // Argument: a 32-bit address.
  SetAddrCmd,

  // Write to instruction memory and increment address register.
  // Argument: the 32-bit instruction to write.
  // The address is taken from the address register.
  WriteInstrCmd,
 
  // Perform a store instruction and increment address register.
  // Argument: the 32-bit word to store.
  // The address is taken from the address register.
  StoreCmd,

  // Perform a load instruction and increment address register.
  // The address is taken from the address register.
  LoadCmd,

  // Perform a cache flush and also request a checksum
  // of all the data the boot loader has received.
  CacheFlushCmd,

  // StartCmd ends the boot loader and starts all threads executing.
  StartCmd,

} BootCmd;


#endif
