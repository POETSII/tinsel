// SPDX-License-Identifier: BSD-2-Clause
// Tinsel boot loader

#include <tinsel.h>
#include <boot.h>
#include <stdarg.h>

// See "boot.h" for further details of the supported boot commands.

INLINE int putchar(int c)
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

INLINE int puthex(unsigned x)
{
  int count = 0;

  for (count = 0; count < 8; count++) {
    unsigned nibble = x >> 28;
    putchar(nibble > 9 ? ('a'-10)+nibble : '0'+nibble);
    x = x << 4;
  }

  return 8;
}

// INLINE int printf(const char* fmt, ...)
// {
//   int count = 0;
//   va_list args;
//
//   va_start(args, fmt);
//
//   while (*fmt) {
//     if (*fmt == '%') {
//       fmt++;
//       if (*fmt == '\0') break;
//       if (*fmt == 's') count += puts(va_arg(args, char*));
//       if (*fmt == 'x') count += puthex(va_arg(args, unsigned));
//     }
//     else { putchar(*fmt); count++; }
//     fmt++;
//   }
//
//   va_end(args);
//
//   return count;
// }
//

// Main
int main()
{
  // Global id of this thread
  uint32_t me = tinselId();

  // Core-local thread id
  uint32_t threadId = me & ((1 << TinselLogThreadsPerCore) - 1);

  // Host id
  uint32_t hostId = tinselHostId();

  // Use one flit per message
  tinselSetLen(0);

  while ((tinselUartTryGet() & 0x100) == 0);

  puts("hello from bootloader 0x");
  puthex(me);
  puts("\n");


  while (1);

  // Unreachable
  return 0;
}
