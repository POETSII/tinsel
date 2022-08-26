// SPDX-License-Identifier: BSD-2-Clause
#include <HostLink.h>

int main()
{

  HostLink hostLink;

  hostLink.go();
  printf("run.cpp: dumping stdout.\n");
  hostLink.dumpStdOut();

  return 0;
}
