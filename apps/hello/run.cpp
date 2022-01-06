// SPDX-License-Identifier: BSD-2-Clause
#include <HostLink.h>

int main()
{
  HostLink hostLink;

  hostLink.boot("code.v", "data.v");
  printf("run.cpp: boot done, sending goAll.\n");
  hostLink.go();
  printf("run.cpp: dumping stdout.\n");
  hostLink.dumpStdOut();

  return 0;
}
