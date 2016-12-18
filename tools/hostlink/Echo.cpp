#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "HostLink.h"

int main()
{
  HostLink link;

  link.setDest(0x80000000);
  link.put(123);

  for (;;) {
    uint32_t src, data;
    link.get(&src, &data);
    printf("Got %d from core %d\n", data, src);
  }

  return 0;
}
