#ifndef _TINSEL_INTERFACE_H_
#define _TINSEL_INTERFACE_H_

#include <stdint.h>
#include <config.h>
#include <io.h>

#define INLINE inline __attribute__((always_inline))

// Get globally unique thread id of caller
int tinselId();

// Read cycle counter
uint32_t tinselCycleCount();

// Cache flush
void tinselCacheFlush();

// Write a word to instruction memory
void tinselWriteInstr(uint32_t addr, uint32_t word);

// Emit word to console (simulation only)
void tinselEmit(uint32_t x);

// Send byte to host (over DebugLink UART)
// (Returns non-zero on success)
uint32_t tinselUartTryPut(uint8_t x);

// Receive byte from host (over DebugLink UART)
// (Byte present in bits [7:0]; bit 8 indicates validity)
uint32_t tinselUartTryGet();

// Insert new thread (with given id) into run queue
void tinselCreateThread(uint32_t id);

// Do not insert currently running thread back in to run queue
void tinselKillThread();

// Get pointer to message-aligned slot in mailbox scratchpad
INLINE volatile void* tinselSlot(int n)
{
  const volatile char* mb_scratchpad_base =
    (char*) (1 << (TinselLogBytesPerMsg + TinselLogMsgsPerThread));
  return (void*) (mb_scratchpad_base + (n << TinselLogBytesPerMsg));
}

// Give mailbox permission to use given address to store an message
void tinselAlloc(volatile void* addr);

// Determine if calling thread can send a message
int tinselCanSend();

// Determine if calling thread can receive a message
int tinselCanRecv();

// Set message length for send operation
// (A message of length N is comprised of N+1 flits)
void tinselSetLen(int n);

// Send message at addr to dest
void tinselSend(int dest, volatile void* addr);

// Receive message
volatile void* tinselRecv();

// Thread can be woken by a logical-OR of these events
typedef enum {TINSEL_CAN_SEND = 1, TINSEL_CAN_RECV = 2} TinselWakeupCond;

// Suspend thread until wakeup condition satisfied
void tinselWaitUntil(TinselWakeupCond cond);

#ifdef __cplusplus
TinselWakeupCond operator|(TinselWakeupCond a, TinselWakeupCond b);
#endif

// Get globally unique thread id of host
// (Host board has X coordinate of 0 and Y coordinate on mesh rim)
uint32_t tinselHostId();

// Return pointer to base of thread's DRAM partition
void* tinselHeapBase();

#endif
