#ifndef _TINSEL_H_
#define _TINSEL_H_

#include <stdint.h>
#include <config.h>
#include <io.h>

// Control/status registers
#define CSR_INSTR_ADDR "0x800"
#define CSR_INSTR      "0x801"
#define CSR_ALLOC      "0x802"
#define CSR_CAN_SEND   "0x803"
#define CSR_HART_ID    "0xf14"
#define CSR_CAN_RECV   "0x805"
#define CSR_SEND_LEN   "0x806"
#define CSR_SEND_PTR   "0x807"
#define CSR_SEND       "0x808"
#define CSR_RECV       "0x809"
#define CSR_WAIT_UNTIL "0x80a"
#define CSR_FROM_UART  "0x80b"
#define CSR_TO_UART    "0x80c"
#define CSR_NEW_THREAD "0x80d"
#define CSR_EMIT       "0x80f"

// Get globally unique thread id of caller
inline int tinselId()
{
  int id;
  asm ("csrr %0, " CSR_HART_ID : "=r"(id));
  return id;
}

// Cache flush
inline void tinselCacheFlush()
{
  asm volatile("fence\n");
}

// Write a word to instruction memory
inline void tinselWriteInstr(uint32_t addr, uint32_t word)
{
  asm volatile("csrw " CSR_INSTR_ADDR ", %0\n"
               "csrw " CSR_INSTR ", %1\n"
               : : "r"(addr >> 2), "r"(word));
}

// Emit word to console (simulation only)
inline void tinselEmit(uint32_t x)
{
  asm volatile("csrw " CSR_EMIT ", %0\n" : : "r"(x));
}

// Send byte to host (over CoreLink UART)
inline void tinselUartPut(uint8_t x)
{
  asm volatile("csrw " CSR_TO_UART ", %0\n" : : "r"(x));
}

// Receive byte from host (over CoreLink UART)
inline uint8_t tinselUartGet()
{
  uint32_t x;
  asm volatile("csrr %0, " CSR_FROM_UART "\n" : "=r"(x));
  return x;
}

// Insert new thread into run queue
inline void tinselCreateThread(uint32_t id)
{
  asm volatile("csrw " CSR_NEW_THREAD ", %0\n" : : "r"(id));
}

// Get pointer to message-aligned slot in mailbox scratchpad
inline volatile void* tinselSlot(int n)
{
  const volatile char* mb_scratchpad_base =
    (char*) (1 << (TinselLogBytesPerMsg + TinselLogMsgsPerThread));
  return (void*) (mb_scratchpad_base + (n << TinselLogBytesPerMsg));
}

// Give mailbox permission to use given address to store an message
inline void tinselAlloc(volatile void* addr)
{
  asm volatile("csrw " CSR_ALLOC ", %0" : : "r"(addr));
}

// Determine if calling thread can send a message
inline int tinselCanSend()
{
  int ok;
  asm volatile("csrr %0, " CSR_CAN_SEND : "=r"(ok));
  return ok;
}

// Determine if calling thread can receive a message
inline int tinselCanRecv()
{
  int ok;
  asm volatile("csrr %0, " CSR_CAN_RECV : "=r"(ok));
  return ok;
}

// Set message length for send operation
// (A message of length N is comprised of N+1 flits)
inline void tinselSetLen(int n)
{
  asm volatile("csrw " CSR_SEND_LEN ", %0" : : "r"(n));
}

// Send message at addr to dest
inline void tinselSend(int dest, volatile void* addr)
{
  asm volatile("csrw " CSR_SEND_PTR ", %0" : : "r"(addr));
  asm volatile("csrw " CSR_SEND ", %0" : : "r"(dest));
}

// Receive message
inline volatile void* tinselRecv()
{
  volatile void* ok;
  asm volatile("csrr %0, " CSR_RECV : "=r"(ok));
  return ok;
}

// Thread can be woken by a logical-OR of these events
typedef enum {TINSEL_CAN_SEND = 1, TINSEL_CAN_RECV = 2} TinselWakeupCond;

// Suspend thread until wakeup condition satisfied
inline void tinselWaitUntil(TinselWakeupCond cond)
{
  asm volatile("csrw " CSR_WAIT_UNTIL ", %0" : : "r"(cond));
}

// Get globally unique thread id of host
// (Host board has X coordinate of 0 and Y coordinate on mesh rim)
inline int tinselHostId()
{
  return TinselMeshYLen << (TinselMeshXBits +
                              TinselLogCoresPerBoard +
                                TinselLogThreadsPerCore);
}

#endif
