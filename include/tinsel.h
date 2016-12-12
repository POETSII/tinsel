#ifndef _TINSEL_H_
#define _TINSEL_H_

#include <stdint.h>
#include "config.h"

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

// Get globally unique thread id of caller
inline int me()
{
  int id;
  asm ("csrr %0, " CSR_HART_ID : "=r"(id));
  return id;
}

// Get id for communicating with host
inline int get_host_id()
{
  int threadsPerBoard = 1 << (LogThreadsPerMailbox + LogMailboxesPerBoard);
  return threadsPerBoard+1;
}

// Cache flush
inline int flush()
{
  asm volatile("fence\n");
}

// Write a word to instruction memory
inline void write_instr(uint32_t index, uint32_t word)
{
  asm volatile("csrw " CSR_INSTR_ADDR ", %0\n"
               "csrw " CSR_INSTR ", %1\n"
               : : "r"(index), "r"(word));
}

// Get pointer to message-aligned slot in mailbox scratchpad
inline volatile void* mailbox(int n)
{
  const volatile char* mb_scratchpad_base =
    (char*) (1 << (LogBytesPerMsg + LogMsgsPerThread));
  return (void*) (mb_scratchpad_base + (n << LogBytesPerMsg));
}

// Give mailbox permission to use given address to store an message
inline void mb_alloc(volatile void* addr)
{
  asm volatile("csrw " CSR_ALLOC ", %0" : : "r"(addr));
}

// Determine if calling thread can send a message
inline int mb_can_send()
{
  int ok;
  asm volatile("csrr %0, " CSR_CAN_SEND : "=r"(ok));
  return ok;
}

// Determine if calling thread can receive a message
inline int mb_can_recv()
{
  int ok;
  asm volatile("csrr %0, " CSR_CAN_RECV : "=r"(ok));
  return ok;
}

// Set message length for send operation
// (A message of length N is comprised of N+1 flits)
inline void mb_set_len(int n)
{
  asm volatile("csrw " CSR_SEND_LEN ", %0" : : "r"(n));
}

// Send message at addr to dest
inline void mb_send(int dest, volatile void* addr)
{
  asm volatile("csrw " CSR_SEND_PTR ", %0" : : "r"(addr));
  asm volatile("csrw " CSR_SEND ", %0" : : "r"(dest));
}

// Receive message
inline void* mb_recv()
{
  void* ok;
  asm volatile("csrr %0, " CSR_RECV : "=r"(ok));
  return ok;
}

// Thread can be woken by a logical-OR of these events
typedef enum {CAN_SEND = 1, CAN_RECV = 2} WakeupCond;

// Suspend thread until wakeup condition satisfied
inline void mb_wait_until(WakeupCond cond)
{
  asm volatile("csrw " CSR_WAIT_UNTIL ", %0" : : "r"(cond));
}

#endif
