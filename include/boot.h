#ifndef _BOOT_H_
#define _BOOT_H_

#include <stdint.h>

// This file defines the format of messages sent between host and boot
// loader.  

// =============================================================================
// Boot message format
// =============================================================================

// Structure of a boot message
typedef struct {
  uint32_t src;  // Id of the sender
  uint32_t cmd;  // Boot command (see list below)
  uint32_t addr; // Aaddress parameter for the boot command
  uint32_t data; // Data parameter for the boot command
} BootMsg;

// Various boot commands supported
// (A boot command is either a request or a response)
typedef enum {
  // Requests
  // --------

  // The boot loader maintains a count of the number of commands it
  // has received. GetCountReq requests the value of this count
  // and also resets the count back to 0. GetCountReq is itself
  // included in the count.
  GetCountReq = 0,

  // The boot loader maintains a checksum of all flits it has
  // received.  The checksum can be obtained using GetChecksumReq.
  // GetChecksumReq is itself included in the checksum.
  GetChecksumReq = 1,

  // WriteInstrReq writes an instruction to instruction memory.
  // The address and data fields contain the write parameters.
  WriteInstrReq = 2,

  // StoreReq performs a store instruction.
  // The address and data fields contain the store parameters.
  StoreReq = 3,

  // LoadReq performs a load instruction.
  // The address field contains the load parameter.
  LoadReq = 4,

  // CacheFlushReq performs a cache flush.
  CacheFlushReq = 5,

  // StartReq ends the boot loader and jumps to the value specified
  // in the address field.
  StartReq = 6,

  // Responses
  // ---------

  // Responding to GetCountReq, the boot loader sends a
  // CountResp.  The data field contains the count.
  CountResp = 0,

  // Responding to LoadReq, the boot loader sends a LoadResp.
  // The data field contains the value loaded.
  LoadResp = 1,

  // Responding to GetChecksumReq, the loader sends a
  // ChecksumResp.  The data field contains the checksum.
  ChecksumResp = 2,

  // One-hot bits
  // ------------

  // The broadcast bit can be set in addition to any request.  It
  // indicates that the receiving thread should broadcast the request
  // to all threads on the same board.  To avoid deadlock, only one 
  // can be broadcasting at a time.
  BroadcastReq = 128
} BootCmd;

#define BootCmdMask (BroadcastReq-1)

#endif
