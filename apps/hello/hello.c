// SPDX-License-Identifier: BSD-2-Clause
#include <tinsel.h>
#include <stdarg.h>

INLINE void sendb(char c) {
  while (tinselUartTryPut(c) == 0);
}

INLINE int putchar_me(int c)
{
  while (tinselUartTryPut(c) == 0);
  return c;
}

INLINE int puts_me(const char* s)
{
  int count = 0;
  while (*s) { putchar_me(*s); s++; count++; }
  return count;
}

INLINE int puthex_me(unsigned x)
{
  int count = 0;

  for (count = 0; count < 8; count++) {
    unsigned nibble = x >> 28;
    putchar_me(nibble > 9 ? ('a'-10)+nibble : '0'+nibble);
    x = x << 4;
  }

  return 8;
}
//
int printf_me(const char* fmt, ...)
{
  int count = 0;
  va_list args;

  va_start(args, fmt);

  while (*fmt) {
    if (*fmt == '%') {
      fmt++;
      if (*fmt == '\0') break;
      if (*fmt == 's') count += puts_me(va_arg(args, char*));
      if (*fmt == 'x') count += puthex_me(va_arg(args, unsigned));
    }
    else { putchar(*fmt); count++; }
    fmt++;
  }

  va_end(args);

  return count;
}

char*  __attribute__ ((section ("text"))) msg_text = "hello from text thread\n";
char*  __attribute__ ((section ("bss"))) msg_bss = "hello from bss\n";
char*  __attribute__ ((section ("rodata"))) msg_rodata = "hello from rodata\n";

void called() {
  sendb('c'); sendb('a'); sendb('l'); sendb('l'); sendb('\n');
}

int main()
{
  void* sp = 0;

  sendb('h');
  sendb('e');
  sendb('l');
  sendb('l');
  sendb('o');
  sendb('\n');
  puts_me("stack ptr: \n");
  puthex_me( (uint32_t)(&sp) );
  asm volatile("" : "+r"(sp) );
  sendb('\n');
  // asm volatile("jr zero");
  // puts_me(msg_bss);
  // puts_me(msg_rodata);
  //
  // puts_me(msg_text);
  // puts_me("thread id ");
  unsigned int me = tinselId();
  //puthex_me(me);
  //sendb('\n');
  //asm volatile("" : "+r"(me) );

  //
    for (uint32_t i = me/16 % (1<<TinselDCacheLogSetsPerThread); i < (1<<TinselDCacheLogSetsPerThread); i++) {
      for (uint32_t j = me/(16*(1 << TinselDCacheLogSetsPerThread)); j < (1<<TinselDCacheLogNumWays); j++) {
        // printf_me("flushing set %x way %x \n", i, j);
        puts_me("core ");
        puthex_me(me);
        puts_me(" flushing set ");
        puthex_me(i);
        puts_me(" way ");
        puthex_me(j);
        sendb('\n');
        asm volatile("" : "+r"(i) );
        // seq. point for i
        tinselFlushLine(i, j);
      }
    }




  // tinselCacheFlush();
  // puts_me("flushed cache\n");
  // sendb('\n');
  // sendb('\n');
  // sendb('\n');
  //
  // unsigned int* dram = (void *)0;
  // if (me == 0) {
  //   puts_me("dram dump: \n");
  //   for (int i=512/8; i<768; i++) {
  //     puthex_me(dram[i]);
  //     sendb('\n');
  //   }
  // }
  // uncommenting this prevents any code from producing output.
  // printf("hello from printf thread 0x%x\n", me);

  // printf_me(msg_text, me);
  return 0;
}
