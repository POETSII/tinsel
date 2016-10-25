#ifndef _TINSEL_H_
#define _TINSEL_H_

#include "config.h"

// =============================================================================
// General
// =============================================================================

// Get globally unique thread id of caller
inline int me()
{
  int id;
  asm ("csrr %0, 0xf14" : "=r"(id));
  return id;
}

// =============================================================================
// Mailbox
// =============================================================================

// Opcodes for mailbox custom instructions
#define OPCODE_MB_ALLOC     "0"
#define OPCODE_MB_CAN_SEND  "1"
#define OPCODE_MB_CAN_RECV  "2"
#define OPCODE_MB_SEND      "3"
#define OPCODE_MB_RECV      "4"

// Get pointer to message-aligned slot in mailbox scratchpad
inline volatile void* mailbox(int n)
{
  const volatile char* mb_scratchpad_base =
    (char*) (1 << (LogBytesPerMsg + LogMsgsPerThread));
  return (void*) (mb_scratchpad_base + (n << LogBytesPerMsg));
}

// Give mailbox permission to use given address to store incoming
// message in scratchpad
inline void mb_alloc(volatile void* addr)
{
  asm volatile("custom0 zero,%0,zero," OPCODE_MB_ALLOC : : "r"(addr));
}

// Determine if calling thread can send a message
inline int mb_can_send()
{
  int ok;
  asm volatile("custom0 %0,zero,zero," OPCODE_MB_CAN_SEND : "=r"(ok));
  return ok;
}

// Determine if calling thread can receive a message
inline int mb_can_recv()
{
  int ok;
  asm volatile("custom0 %0,zero,zero," OPCODE_MB_CAN_RECV : "=r"(ok));
  return ok;
}

// Send message at addr to dest
inline int mb_send(int dest, volatile void* addr)
{
  int ok;
  asm volatile("custom0 %0,%1,%2," OPCODE_MB_SEND :
          "=r"(ok) : "r"(addr), "r"(dest));
  return ok;
}

// Receive message
inline void* recv()
{
  void* ok;
  asm volatile("custom0 %0,zero,zero," OPCODE_MB_RECV : "=r"(ok));
  return ok;
}

#endif
