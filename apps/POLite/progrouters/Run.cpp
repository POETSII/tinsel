// SPDX-License-Identifier: BSD-2-Clause
#include <HostLink.h>
#include <POLite.h>

int main(int argc, char **argv)
{
  // Connection to tinsel machine
  HostLink hostLink;

  // Create routing tables
  ProgRouterMesh mesh(TinselMeshXLenWithinBox, TinselMeshYLenWithinBox);

  // Board (1, 0)
  for (int i = 0; i < 60; i++) {
    uint64_t mask = 1ul << i;
    mesh.table[0][1].addMRM(1, 0, mask >> 32, mask, 0xf0f0);
  }
  uint32_t key01 = mesh.table[0][0].genKey();

  // Board (0, 0)
  for (int i = 0; i < 40; i++) {
    uint64_t mask = 1ul << i;
    mesh.table[0][0].addMRM(1, 0, mask >> 32, mask, 0xf0f0);
  }
  for (int i = 0; i < 30; i++) {
    uint64_t mask = 1ul << i;
    mesh.table[0][0].addMRM(1, 1, mask >> 32, mask, 0xf0f0);
  }
  mesh.table[0][0].addRR(2, key01); // East
  uint32_t key00 = mesh.table[0][0].genKey();

  // Transfer routing tables to FPGAs
  mesh.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();

  // Send key
  printf("Sending key %x\n", key00);
  uint32_t msg[1 << TinselLogWordsPerMsg];
  msg[0] = key00;
  hostLink.send(0, 1, msg);

  hostLink.dumpStdOut();
  return 0;
}
