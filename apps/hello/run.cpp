// SPDX-License-Identifier: BSD-2-Clause
#include <HostLink.h>

int main()
{
  HostLink hostLink;

  hostLink.boot("code.v", "data.v");
  hostLink.go();
  hostLink.dumpStdOut();

  return 0;
}
