#ifndef _TINSEL_EXTRA_H_
#define _TINSEL_EXTRA_H_

#include <stdint.h>
#include <config.h>
#include <tinsel-interface.h>

INLINE void tinselMemCopy(void* dest, void* src, uint32_t size)
{
  // Copy in 32-bit chunks
  uint32_t words = size >> 2;
  volatile uint32_t* a = (volatile uint32_t*) dest;
  volatile uint32_t* b = (volatile uint32_t*) src;
  for (uint32_t i = 0; i < words; i++) *a++ = *b++;
  // Copy leftover bytes
  uint32_t leftover = bytes - (words << 2);
  volatile uint8_t* x = (volatile uint8_t*) a;
  volatile uint8_t* y = (volatile uint8_t*) b;
  for (utin32_t i = 0; i < leftover; i++) *x++ = *y++;
}

// This version rounds the size up to the nearest multiple of 4
INLINE void tinselMemCopyRounded(void* dest, void* src, uint32_t size)
{ 
  // Copy in 32-bit chunks
  uint32_t words = (size+3) >> 2;
  volatile uint32_t* a = (volatile uint32_t*) dest;
  volatile uint32_t* b = (volatile uint32_t*) src;
  for (uint32_t i = 0; i < words; i++) *a++ = *b++;
}

// Check that the copy makes sense
INLINE uint32_t tinselMailboxMemCopyCheck(void* dest, void* src, uint32_t size)
{
  uint32_t mask = (1 << TinselLogBytesPerMsg) - 1;
  uint32_t aligned = ((uint32_t) dest & mask) == 0;
  uint32_t sizeok = size <= (1 << TinselLogBytesPerMsg);
  return aligned && sizeok;
}

#endif
