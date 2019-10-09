// SPDX-License-Identifier: BSD-2-Clause
#include <HostLink.h>

int main()
{
  HostLink hostLink;

  printf("Booting\n");
  hostLink.boot("code.v", "data.v");

  printf("Starting\n");
  hostLink.go();

  uint32_t totalThreads =
    TinselMeshXLenWithinBox * TinselMeshYLenWithinBox * TinselThreadsPerBoard;

  printf("Waiting for responses from %d threads\n", totalThreads);
  uint32_t resp[1 << TinseLogWordsPerMsg];
  for (int i = 0; i < totalThreads; i++) {
    hostLink.recv(resp);
    printf("Received response from %d\n", resp[0]);
  }

  printf("Test passed, all threads finished\n");

  return 0;
}
