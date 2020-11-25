// SPDX-License-Identifier: BSD-2-Clause
#include <HostLink.h>

int main()
{
  HostLink hostLink;

  printf("Booting\n");

  // Load program 0 onto core 0
  hostLink.loadInstrsOntoCore("code_0.v", 0, 0, 0);
  hostLink.loadDataViaCore("data_0.v", 0, 0, 0);

  // Load program 1 onto core 2
  hostLink.loadInstrsOntoCore("code_1.v", 0, 0, 2);
  hostLink.loadDataViaCore("data_1.v", 0, 0, 2);

  // Start cores
  hostLink.startOne(0, 0, 0, 1);
  hostLink.startOne(0, 0, 2, 1);

  // Trigger cores 0 and 2 into execution
  printf("Starting\n");
  hostLink.goOne(0, 0, 0);
  hostLink.goOne(0, 0, 2);

  printf("Sending ping to thread 0\n");
  uint32_t ping[1 << TinselLogWordsPerMsg];
  ping[0] = 100;
  hostLink.send(0, 1, ping);

  printf("Sending ping to thread 32\n");
  hostLink.send(32, 1, ping);

  printf("Waiting for responses\n");
  hostLink.recv(ping);
  printf("Got response %x\n", ping[0]);
  hostLink.recv(ping);
  printf("Got response %x\n", ping[0]);

  return 0;
}
