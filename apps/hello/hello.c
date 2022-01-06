// SPDX-License-Identifier: BSD-2-Clause
#include <tinsel.h>

INLINE void sendb(char c) {
  while (tinselUartTryPut(c) == 0);
}


int main()
{
  sendb('h');
  sendb('e');
  sendb('l');
  sendb('l');
  sendb('o');
  sendb('\n');
  // unsigned int me = tinselId();
  // printf("Hello from thread 0x%x\n", me);
  return 0;
}
