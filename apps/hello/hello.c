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

  unsigned int me = tinselId();
  printf("hello from printf thread 0x%x\n", me);


  float five = (float)(5);
  float six = 6.0f;
  float fivebysix = five * six;
  float invfivebysix = 1.0/fivebysix;
  uint32_t *invfivebysix_i = (void *)(&invfivebysix);

  printf("1/(5*6): %x\n",  *invfivebysix_i);

  if (me == 0x00000700 || me == 0x00000710) {

    for (uint32_t* addr=512; addr<768; addr=&addr[4]) {
      printf("%x: %x %x %x %x\n", addr, addr[0], addr[1], addr[2], addr[3]);
    }

  }

  //puthex_me(me);
  //sendb('\n');
  //asm volatile("" : "+r"(me) );

  //



  return 0;
}
