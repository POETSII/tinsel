// SPDX-License-Identifier: BSD-2-Clause
#ifndef _TINSEL_INTERFACE_H_
#define _TINSEL_INTERFACE_H_

#include <stdint.h>
#include <config.h>
#include <io.h>

#define INLINE inline __attribute__((always_inline))

// Get globally unique thread id of caller
INLINE uint32_t tinselId();

// Read cycle counter
INLINE uint32_t tinselCycleCount();

// Cache flush
INLINE void tinselCacheFlush();

// Flush cache line
INLINE void tinselFlushLine(uint32_t lineNum, uint32_t way);

// Write a word to instruction memory
INLINE void tinselWriteInstr(uint32_t addr, uint32_t word);

// Emit word to console (simulation only)
INLINE void tinselEmit(uint32_t x);

// Send byte to host (over DebugLink UART)
// (Returns non-zero on success)
INLINE uint32_t tinselUartTryPut(uint8_t x);

// Receive byte from host (over DebugLink UART)
// (Byte present in bits [7:0]; bit 8 indicates validity)
INLINE uint32_t tinselUartTryGet();

// Insert new thread (with given id) into run queue
INLINE void tinselCreateThread(uint32_t id);

// Do not insert currently running thread back in to run queue
INLINE void tinselKillThread();

// Tell mailbox that given message slot is no longer needed
INLINE void tinselFree(volatile void* addr);

// Determine if calling thread can send a message
INLINE int tinselCanSend();

// Determine if calling thread can receive a message
INLINE int tinselCanRecv();

// Get pointer to thread's message slot reserved for sending
INLINE volatile void* tinselSendSlot();

// Set message length for send operation
// (A message of length N is comprised of N+1 flits)
INLINE void tinselSetLen(int n);

// Send message to multiple threads on the given mailbox
INLINE void tinselMulticast(
  uint32_t mboxDest,      // Destination mailbox
  uint32_t destMaskHigh,  // Destination bit mask (high bits)
  uint32_t destMaskLow,   // Destination bit mask (low bits)
  volatile void* addr);   // Message pointer

// Send message at addr to dest
INLINE void tinselSend(int dest, volatile void* addr);

// Receive message
INLINE volatile void* tinselRecv();

// Thread can be woken by a logical-OR of these events
typedef enum {TINSEL_CAN_SEND = 1, TINSEL_CAN_RECV = 2} TinselWakeupCond;

// Suspend thread until wakeup condition satisfied
INLINE void tinselWaitUntil(TinselWakeupCond cond);

// Suspend thread until message arrives or all threads globally are idle
INLINE int tinselIdle();

#ifdef __cplusplus
INLINE TinselWakeupCond operator|(TinselWakeupCond a, TinselWakeupCond b);
#endif

// Get address of master host
// DE5: Master host is accessible via mesh origin
// DE10: host is available via board-local ID MaxX, maxY+1
#ifdef TinselStratixV
INLINE uint32_t tinselHostId()
{
  return 1 << (1 + TinselMeshYBits + TinselMeshXBits +
                     TinselLogCoresPerBoard + TinselLogThreadsPerCore);

}
#endif

#ifdef TinselStratix10
INLINE uint32_t tinselHostId()
{
  uint32_t addr;
  addr = 0; // accelerator bit
  addr = (addr << 1) | 0; //iskey
  addr = (addr << 2) | 3; //maybehost, ishost
  addr = (addr << TinselMeshYBits) | 0;
  addr = (addr << TinselMeshXBits) | 0;
  addr = (addr << TinselMailboxMeshYBits) | ((1 << TinselMailboxMeshYBits) - 1);
  addr = (addr << TinselMailboxMeshXBits) | ((1 << TinselMailboxMeshXBits) - 1);
  addr = (addr << TinselLogCoresPerMailbox) | 0;
  addr = (addr << TinselLogThreadsPerCore) | 0;
  return addr;
}
#endif

// Given thread id, return base address of thread's partition in DRAM
INLINE uint32_t tinselHeapBaseGeneric(uint32_t id)
{
  uint32_t partId = id & (TinselThreadsPerDRAM-1);
  uint32_t addr = TinselBytesPerDRAM -
                    ((partId+1) << TinselLogBytesPerDRAMPartition);
  // Use the partition-interleaved translation
  addr |= 0x80000000;
  return addr;
}

#ifdef TinselStratixV
// Given thread id, return base address of thread's partition in SRAM
INLINE uint32_t tinselHeapBaseSRAMGeneric(uint32_t id)
{
  uint32_t partId = id & (TinselThreadsPerDRAM-1);
  uint32_t addr = (1 << TinselLogBytesPerSRAM)
                + (partId << TinselLogBytesPerSRAMPartition);
  return addr;
}
#endif

// Return pointer to base of calling thread's DRAM partition
INLINE void* tinselHeapBase();

#ifdef TinselStratixV
// Return pointer to base of calling thread's SRAM partition
INLINE void* tinselHeapBaseSRAM();
#endif

// Reset performance counters
INLINE void tinselPerfCountReset();

// Start performance counters
INLINE void tinselPerfCountStart();

// Stop performance counters
INLINE void tinselPerfCountStop();

// Performance counter: get the cache miss count
INLINE uint32_t tinselMissCount();

// Performance counter: get the cache hit count
INLINE uint32_t tinselHitCount();

// Performance counter: get the cache writeback count
INLINE uint32_t tinselWritebackCount();

// Performance counter: get the CPU-idle count
INLINE uint32_t tinselCPUIdleCount();

// Performance counter: get the CPU-idle count (upper 8 bits)
INLINE uint32_t tinselCPUIdleCountU();

// Read cycle counter (upper 8 bits)
INLINE uint32_t tinselCycleCountU();

// Address construction
INLINE uint32_t tinselToAddr(
         uint32_t boardX, uint32_t boardY,
           uint32_t tileX, uint32_t tileY,
             uint32_t coreId, uint32_t threadId)
{
  uint32_t addr;
  addr = boardY;
  addr = (addr << TinselMeshXBits) | boardX;
  addr = (addr << TinselMailboxMeshYBits) | tileY;
  addr = (addr << TinselMailboxMeshXBits) | tileX;
  addr = (addr << TinselLogCoresPerMailbox) | coreId;
  addr = (addr << TinselLogThreadsPerCore) | threadId;
  return addr;
}

// Address deconstruction
INLINE void tinselFromAddr(uint32_t addr,
         uint32_t* boardX, uint32_t* boardY,
           uint32_t* tileX, uint32_t* tileY,
             uint32_t* coreId, uint32_t* threadId)
{
  *threadId = addr & ((1 << TinselLogThreadsPerCore) - 1);
  addr >>= TinselLogThreadsPerCore;

  *coreId = addr & ((1 << TinselLogCoresPerMailbox) - 1);
  addr >>= TinselLogCoresPerMailbox;

  *tileX = addr & ((1 << TinselMailboxMeshXBits) - 1);
  addr >>= TinselMailboxMeshXBits;

  *tileY = addr & ((1 << TinselMailboxMeshYBits) - 1);
  addr >>= TinselMailboxMeshYBits;

  *boardX = addr & ((1 << TinselMeshXBits) - 1);
  addr >>= TinselMeshXBits;

  *boardY = addr & ((1 << TinselMeshYBits) - 1);
}

// Get address of specified custom accelerator
INLINE uint32_t tinselAccId(
         uint32_t boardX, uint32_t boardY,
           uint32_t tileX, uint32_t tileY)
{
  uint32_t addr;
  addr = 0x8;
  addr = (addr << TinselMeshYBits) | boardY;
  addr = (addr << TinselMeshXBits) | boardX;
  addr = (addr << TinselMailboxMeshYBits) | tileY;
  addr = (addr << TinselMailboxMeshXBits) | tileX;
  addr = addr << (TinselLogCoresPerMailbox+TinselLogThreadsPerCore);
  return addr;
}

// Special address to signify use of routing key
INLINE uint32_t tinselUseRoutingKey()
{
  // Special address to signify use of routing key
  return 1 <<
    (TinselMailboxMeshYBits + TinselMailboxMeshXBits +
     TinselMeshXBits + TinselMeshYBits + 2);
}

#endif
