// SPDX-License-Identifier: BSD-2-Clause
#include <HostLink.h>

int main()
{

  HostLink hostLink;
  printf("[hello/run::main] starting to load cores.\n");

  // hostLink.boot("code.v", "data.v");
  hostLink.loadAll("code.v", "data.v");
  // printf("run.cpp: boot done, reporting stacks.\n");
  // for (int c=0; c<32; c++) {
  //   hostLink.printStack(0, 0, c);
  // }

  // for (uint32_t id=0; id<16*32; id=id+16) {
  //   printf("hostlink.toAddr for threadid %i: %i\n", id, hostLink.toAddr(0, 0, id/16, 0));
  //   hostLink.printStackRawAddr(id);
  // }

  // hostLink.go();
  // // hostLink.goOne(0, 0, 1);
  //
  // // hostLink.startOne(0, 0, 1, 1);
  // hostLink.startAll();

  // hostLink.go();
  // for (int i=0; i<128; i++) {
  //   hostLink.goOne(0, 0, i);
  // }
  // for (int i=0; i<128; i++) {
  //   hostLink.startOne(0, 0, i, 1);
  // }

  // hostLink.startAll();
  for (int i=0; i<64; i++) {
    hostLink.goOne(0, 0, i);
  }
  for (int i=0; i<64; i++) {
    hostLink.startOne(0, 0, i, 1);
  }


  //for (int ci=0; ci<256; ci++) { hostLink.goOne(0, 0, ci); }
  printf("[run.cpp]: dumping stdout.\n");
  hostLink.dumpStdOut();

  return 0;
}
