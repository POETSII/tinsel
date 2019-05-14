// SPDX-License-Identifier: BSD-2-Clause
#include <HostLink.h>

int main()
{
  HostLink hostLink;

  printf("Booting\n");
  hostLink.boot("code.v", "data.v");

  printf("Starting\n");
  hostLink.go();

  printf("Waiting for message from accelerator\n");
  uint32_t msg[4];
  hostLink.recv(msg);
  printf("Got it: %x\n", msg[0]);

  return 0;
}
