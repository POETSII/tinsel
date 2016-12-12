#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "link.h"

#define N 1024

int main()
{
  Link link;

  // Create flit
  uint8_t out[21];
  for (int i = 0; i < sizeof(out); i++) out[i] = 0;
  out[0] = 0; // Num flits minus 1
  out[1] = 0; // Destination

  for (int i = 0; i < N; i++) {
    // Send flit
    out[5] = i % 16; // Value to send
    link.put(&out, sizeof(out));
    printf("Sent flit %i\n", i%16);

    // Receive flit
    uint8_t in[17];
    link.get(&in, sizeof(in));
    printf("%i: received flit with value %i\n", i, in[1]);
  }

  return 0;
}
