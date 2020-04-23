// SPDX-License-Identifier: BSD-2-Clause
#ifndef _TINSEL_H_
#define _TINSEL_H_

#include <stdint.h>
#include <config.h>
#include <io.h>
#include <tinsel-interface.h>

// Control/status registers
#define CSR_INSTR_ADDR  "0x800"
#define CSR_INSTR       "0x801"
#define CSR_FREE        "0x802"
#define CSR_CAN_SEND    "0x803"
#define CSR_HART_ID     "0xf14"
#define CSR_CAN_RECV    "0x805"
#define CSR_SEND_LEN    "0x806"
#define CSR_SEND_PTR    "0x807"
#define CSR_SEND_DEST   "0x808"
#define CSR_RECV        "0x809"
#define CSR_WAIT_UNTIL  "0x80a"
#define CSR_FROM_UART   "0x80b"
#define CSR_TO_UART     "0x80c"
#define CSR_NEW_THREAD  "0x80d"
#define CSR_KILL_THREAD "0x80e"
#define CSR_EMIT        "0x80f"
#define CSR_CYCLE       "0xc00"
#define CSR_FLUSH       "0xc01"

// Performance counter CSRs
#define CSR_PERFCOUNT     "0xc07"
#define CSR_MISSCOUNT     "0xc08"
#define CSR_HITCOUNT      "0xc09"
#define CSR_WBCOUNT       "0xc0a"
#define CSR_CPUIDLECOUNT  "0xc0b"
#define CSR_CPUIDLECOUNTU "0xc0c"
#define CSR_CYCLEU        "0xc0d"

// Get globally unique thread id of caller
INLINE uint32_t tinselId()
{
  int id;
  asm ("csrrw %0, " CSR_HART_ID ", zero" : "=r"(id));
  return id;
}

// Read cycle counter
INLINE uint32_t tinselCycleCount()
{
  uint32_t n;
  asm volatile ("csrrw %0, " CSR_CYCLE ", zero" : "=r"(n));
  return n;
}

// Flush cache line (non-blocking)
INLINE void tinselFlushLine(uint32_t lineNum, uint32_t way)
{
  uint32_t arg = (lineNum << TinselDCacheLogNumWays) | way;
  asm volatile("csrrw zero, " CSR_FLUSH ", %0" : : "r"(arg));
}

// Cache flush (non-blocking)
INLINE void tinselCacheFlush()
{
  for (uint32_t i = 0; i < (1<<TinselDCacheLogSetsPerThread); i++)
    for (uint32_t j = 0; j < (1<<TinselDCacheLogNumWays); j++)
      tinselFlushLine(i, j);
}

// Write a word to instruction memory
INLINE void tinselWriteInstr(uint32_t addr, uint32_t word)
{
  asm volatile("csrrw zero, " CSR_INSTR_ADDR ", %0\n"
               "csrrw zero, " CSR_INSTR ", %1\n"
               : : "r"(addr >> 2), "r"(word));
}

// Emit word to console (simulation only)
INLINE void tinselEmit(uint32_t x)
{
  asm volatile("csrrw zero, " CSR_EMIT ", %0\n" : : "r"(x));
}

// Send byte to host (over DebugLink UART)
// (Returns non-zero on success)
INLINE uint32_t tinselUartTryPut(uint8_t x)
{
  uint32_t ret;
  asm volatile("csrrw %0, " CSR_TO_UART ", %1\n" : "=r"(ret) : "r"(x));
  return ret;
}

// Receive byte from host (over DebugLink UART)
// (Byte present in bits [7:0]; bit 8 indicates validity)
INLINE uint32_t tinselUartTryGet()
{
  uint32_t x;
  asm volatile("csrrw %0, " CSR_FROM_UART ", zero\n" : "=r"(x));
  return x;
}

// Insert new thread (with given id) into run queue
INLINE void tinselCreateThread(uint32_t id)
{
  asm volatile("csrrw zero, " CSR_NEW_THREAD ", %0\n" : : "r"(id));
}

// Do not insert currently running thread back in to run queue
INLINE void tinselKillThread()
{
  asm volatile("csrrw zero, " CSR_KILL_THREAD ", zero\n");
}

// Tell mailbox that given message slot is no longer needed
INLINE void tinselFree(volatile void* addr)
{
  asm volatile("csrrw zero, " CSR_FREE ", %0" : : "r"(addr));
}

// Determine if calling thread can send a message
INLINE int tinselCanSend()
{
  int ok;
  asm volatile("csrrw %0, " CSR_CAN_SEND ", zero" : "=r"(ok));
  return ok;
}

// Determine if calling thread can receive a message
INLINE int tinselCanRecv()
{
  int ok;
  asm volatile("csrrw %0, " CSR_CAN_RECV ", zero" : "=r"(ok));
  return ok;
}

// Set message length for send operation
// (A message of length N is comprised of N+1 flits)
INLINE void tinselSetLen(int n)
{
  asm volatile("csrrw zero, " CSR_SEND_LEN ", %0" : : "r"(n));
}

// Send message to multiple threads on the given mailbox
INLINE void tinselMulticast(
  uint32_t mboxDest,      // Destination mailbox
  uint32_t destMaskHigh,  // Destination bit mask (high bits)
  uint32_t destMaskLow,   // Destination bit mask (low bits)
  volatile void* addr)    // Message pointer
{
  asm volatile("csrrw zero, " CSR_SEND_PTR ", %0" : : "r"(addr) : "memory");
  asm volatile("csrrw zero, " CSR_SEND_DEST ", %0" : : "r"(mboxDest));
  // Opcode: 0000000 rs2 rs1 000 rd 0001000, with rd=0, rs1=x10, rs2=x11
  asm volatile(
    "mv x10, %0\n"
    "mv x11, %1\n"
    ".word 0x00b50008\n" : : "r"(destMaskHigh), "r"(destMaskLow)
                           : "x10", "x11");
}

// Send message at addr to dest
INLINE void tinselSend(int dest, volatile void* addr)
{
  uint32_t threadId = dest & 0x3f;
  uint32_t high = threadId >= 32 ? (1 << (threadId-32)) : 0;
  uint32_t low = threadId < 32 ? (1 << threadId) : 0;
  tinselMulticast(dest >> 6, high, low, addr);
}

// Send message at addr using given routing key
INLINE void tinselKeySend(int key, volatile void* addr)
{
  // Special address to signify use of routing key
  uint32_t useRoutingKey = 1 <<
    (TinselMailboxMeshYBits + TinselMailboxMeshXBits +
     TinselMeshXBits + TinselMeshYBits + 2);
  tinselMulticast(useRoutingKey, 0, key, addr);
}

// Receive message
INLINE volatile void* tinselRecv()
{
  volatile void* ok;
  asm volatile("csrrw %0, " CSR_RECV ", zero" : "=r"(ok));
  return ok;
}

// Suspend thread until wakeup condition satisfied
INLINE void tinselWaitUntil(TinselWakeupCond cond)
{
  asm volatile("csrrw zero, " CSR_WAIT_UNTIL ", %0" : : "r"(cond));
}

#ifdef __cplusplus
INLINE TinselWakeupCond operator|(TinselWakeupCond a, TinselWakeupCond b)
{
  return (TinselWakeupCond) (((uint32_t) a) | ((uint32_t) b));
}
#endif

// Suspend thread until message arrives or all threads globally are idle
INLINE int tinselIdle(int vote)
{
  int result;
  int cond = vote ? 0b1110 : 0b0110;
  asm volatile("csrrw %0, " CSR_WAIT_UNTIL ", %1" : "=r"(result) : "r"(cond));
  return (result >> 2);
}

// Return pointer to base of thread's DRAM partition
INLINE void* tinselHeapBase()
{
  uint32_t me = tinselId();
  uint32_t partId = me & (TinselThreadsPerDRAM-1);
  uint32_t addr = TinselBytesPerDRAM -
                    ((partId+1) << TinselLogBytesPerDRAMPartition);
  // Use the partition-interleaved translation
  addr |= 0x80000000;
  return (void*) addr;
}

// Return pointer to base of thread's SRAM partition
INLINE void* tinselHeapBaseSRAM()
{
  uint32_t me = tinselId();
  uint32_t partId = me & (TinselThreadsPerDRAM-1);
  uint32_t addr = (1 << TinselLogBytesPerSRAM)
                + (partId << TinselLogBytesPerSRAMPartition);
  return (void*) addr;
}

// Reset performance counters
INLINE void tinselPerfCountReset()
{
  asm volatile("csrrw zero, " CSR_PERFCOUNT ", %0" : : "r"(0));
}

// Start performance counters
INLINE void tinselPerfCountStart()
{
  asm volatile("csrrw zero, " CSR_PERFCOUNT ", %0" : : "r"(1));
}

// Stop performance counters
INLINE void tinselPerfCountStop()
{
  asm volatile("csrrw zero, " CSR_PERFCOUNT ", %0" : : "r"(2));
}

// Performance counter: get the cache miss count
INLINE uint32_t tinselMissCount()
{
  uint32_t n;
  asm volatile ("csrrw %0, " CSR_MISSCOUNT ", zero" : "=r"(n));
  return n;
}

// Performance counter: get the cache hit count
INLINE uint32_t tinselHitCount()
{
  uint32_t n;
  asm volatile ("csrrw %0, " CSR_HITCOUNT ", zero" : "=r"(n));
  return n;
}

// Performance counter: get the cache writeback count
INLINE uint32_t tinselWritebackCount()
{
  uint32_t n;
  asm volatile ("csrrw %0, " CSR_WBCOUNT ", zero" : "=r"(n));
  return n;
}

// Performance counter:: get the CPU-idle count
INLINE uint32_t tinselCPUIdleCount()
{
  uint32_t n;
  asm volatile ("csrrw %0, " CSR_CPUIDLECOUNT ", zero" : "=r"(n));
  return n;
}

// Performance counter: get the CPU-idle count (upper 8 bits)
INLINE uint32_t tinselCPUIdleCountU()
{
  uint32_t n;
  asm volatile ("csrrw %0, " CSR_CPUIDLECOUNTU ", zero" : "=r"(n));
  return n;
}

// Read cycle counter (upper 8 bits)
INLINE uint32_t tinselCycleCountU()
{
  uint32_t n;
  asm volatile ("csrrw %0, " CSR_CYCLEU ", zero" : "=r"(n));
  return n;
}

// Get address of any specified host
// (This Y coordinate specifies the row of the FPGA mesh that the
// host is connected to, and the X coordinate specifies whether it is
// the host on the left or the right of that row.)
// (Note that the return value is a relative address: it may differ
// depending on which thread it is called)
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
// (Note that the return value is a relative address: it may differ
// depending on which thread it is called)
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

#endif
