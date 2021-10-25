// SPDX-License-Identifier: BSD-2-Clause
#include <tinsel.h>

int main()
{
  tinselEmit(0x100);
  printf("Hello from thread\n");
  unsigned int me = tinselId();
  printf("Hello from thread 0x%x\n", me);
  return 0;
}
