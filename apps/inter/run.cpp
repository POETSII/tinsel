// SPDX-License-Identifier: BSD-2-Clause
#include <HostLink.h>

int main()
{
  HostLink hostLink;

  printf("Booting\n");
  hostLink.boot("code.v", "data.v");

  printf("Starting\n");
  hostLink.go();

  uint32_t resp[4];
  hostLink.recv(resp);
  printf("Done cycles=%u\n", resp[0]);

  return 0;
}
