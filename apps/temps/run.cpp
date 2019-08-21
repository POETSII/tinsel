// SPDX-License-Identifier: BSD-2-Clause
#include <HostLink.h>

int main()
{
  HostLink hostLink;

  printf("Asking for temp\n");
  int32_t temp = hostLink.debugLink->getTemp(0, 0);
  printf("Temp = %i\n", temp);

  return 0;
}
