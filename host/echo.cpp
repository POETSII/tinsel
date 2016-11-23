#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "link.h"

int main()
{
  Link link;

  // Create flit
  uint8_t out[21];
  for (int i = 0; i < sizeof(out); i++) out[i] = 0;
  out[0] = 0;  // Length of flit minus 1
  out[4] = 0;  // Destination
  out[5] = 10; // Value to send

  // Send flit
  link.put(&out, sizeof(out));
  printf("Sent flit\n");

  // Receive flit
  uint8_t in[17];
  link.get(&in, sizeof(in));
  printf("Got flit of size %x and value %x\n", in[0], in[1]);

  return 0;
}
