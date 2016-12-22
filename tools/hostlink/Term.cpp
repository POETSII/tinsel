#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <boot.h>
#include <config.h>
#include "HostLink.h"

int main()
{
  HostLink link;

  // Dump all messages to terminal
  for (;;) {
    uint32_t src, val;
    link.get(&src, &val);
    printf("%d: %x\n", src, val);
  }
 
  return 0;
}
