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

// Get pointer to message-aligned slot in mailbox scratchpad
INLINE volatile void* tinselSlot(int n)
{
  const volatile char* mb_scratchpad_base =
    (char*) (1 << (TinselLogBytesPerMsg + TinselLogMsgsPerThread));
  return (void*) (mb_scratchpad_base + (n << TinselLogBytesPerMsg));
}

// Give mailbox permission to use given address to store an message
INLINE void tinselAlloc(volatile void* addr);

// Determine if calling thread can send a message
INLINE int tinselCanSend();

// Determine if calling thread can receive a message
INLINE int tinselCanRecv();

// Set message length for send operation
// (A message of length N is comprised of N+1 flits)
INLINE void tinselSetLen(int n);

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
// (Master host is accessible via mesh origin)
INLINE uint32_t tinselHostId()
{
  return 1 << (1 + TinselMeshYBits + TinselMeshXBits +
                     TinselLogCoresPerBoard + TinselLogThreadsPerCore);
}

// Get address of any specified host
// (This Y coordinate specifies the row of the FPGA mesh that the
// host is connected to, and the X coordinate specifies whether it is
// the host on the left or the right of that row.)
INLINE uint32_t tinselBridgeId(uint32_t x, uint32_t y)
{
  uint32_t me = tinselId();
  uint32_t yAddr = y << (TinselMeshXBits + TinselLogCoresPerBoard +
                           TinselLogThreadsPerCore);
  uint32_t xAddr = (me >> (TinselLogCoresPerBoard + TinselLogThreadsPerCore))
                 & ((1 << TinselMeshXBits)-1);
  uint32_t bridge = (x == 0 ? 2 : 3) <<
    (TinselMeshYBits + TinselMeshXBits + TinselLogCoresPerBoard +
       TinselLogThreadsPerCore);
  return xAddr | yAddr | bridge;
}

// Get address of host in same box as calling thread
INLINE uint32_t tinselMyBridgeId()
{
  uint32_t me = tinselId();
  uint32_t xAddr = (me >> (TinselLogCoresPerBoard + TinselLogThreadsPerCore))
                 & ((1 << TinselMeshXBits)-1);
  uint32_t bridge = (xAddr < TinselMeshXLenWithinBox ? 2 : 3) <<
    (TinselMeshYBits + TinselMeshXBits + TinselLogCoresPerBoard +
       TinselLogThreadsPerCore);
  return (me | bridge);
}

// Return pointer to base of thread's DRAM partition
INLINE void* tinselHeapBase();

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

#endif
