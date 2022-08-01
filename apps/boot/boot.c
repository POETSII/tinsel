// SPDX-License-Identifier: BSD-2-Clause
// Tinsel boot loader

#include <tinsel.h>
#include <boot.h>
#include <stdarg.h>

#include "writeapp.h"

// See "boot.h" for further details of the supported boot commands.

int putchar(int c)
{
  while (tinselUartTryPut(c) == 0);
  return c;
}

INLINE int puts(const char* s)
{
  int count = 0;
  while (*s) { putchar(*s); s++; count++; }
  return count;
}

int puthex(unsigned x)
{
  int count = 0;

  for (count = 0; count < 8; count++) {
    unsigned nibble = x >> 28;
    putchar(nibble > 9 ? ('a'-10)+nibble : '0'+nibble);
    x = x << 4;
  }

  return 8;
}

// Main
int main()
{
  // Global id of this thread
  // Send char to all Threads

  uint32_t me = tinselId();

  // Core-local thread id
  uint32_t threadId = me & ((1 << TinselLogThreadsPerCore) - 1);

  // Host id
  uint32_t hostId = tinselHostId();

  // Use one flit per message
  tinselSetLen(0);

  while ((tinselUartTryGet() & 0x100) == 0);
  puthex(me);
  putchar('s');
  writeapp_data();
  putchar('d');
  writeapp_code();
  putchar('c');

  tinselCacheFlush(); // as we are only running this thread, no need to wait for writeback
  if (threadId == 0) {
    for (int trd=1; trd<TinselThreadsPerCore-1; trd++) {
      tinselCreateThread(trd);
    }
  }

  putchar('f');
  int (*appMain)() = (int (*)()) (TinselMaxBootImageBytes);
  putchar('g');
  appMain();

  while (1);

  // Unreachable
  return 0;
}
