// SPDX-License-Identifier: BSD-2-Clause
#include <tinsel.h>

int main()
{
  unsigned int me = tinselId();
  printf("Hello from thread 0x%x\n", me);
  return 0;
}
