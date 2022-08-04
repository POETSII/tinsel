// SPDX-License-Identifier: BSD-2-Clause
#include <tinsel.h>
#include <stdarg.h>

int main()
{
  // tinselCacheFlush();
  // volatile uint32_t* ptr = (uint32_t*)4; ptr[0];

  unsigned int me = tinselId();
  printf("hello from printf thread 0x%x\n", me);
  while (1) {};
  return 0;
}
