// SPDX-License-Identifier: BSD-2-Clause
// Regression test: on each step, every device sends to its 26 3D neighbours

#include "NHood.h"
#include <HostLink.h>
#include <POLite.h>
#include <sys/time.h>

#define T 1000
#define D 10

int main()
{
  HostLink hostLink;
  PGraph<NHoodDevice, NHoodState, None, NHoodMessage> graph;
  //graph.mapVerticesToDRAM = true;

  int devs[D][D][D];
  for (int x = 0; x < D; x++)
    for (int y = 0; y < D; y++)
      for (int z = 0; z < D; z++)
        devs[x][y][z] = graph.newDevice();

  for (int x = 0; x < D; x++)
    for (int y = 0; y < D; y++)
      for (int z = 0; z < D; z++)
        for (int i = -1; i < 2; i++)
          for (int j = -1; j < 2; j++)
            for (int k = -1; k < 2; k++) {
              if (! (i == 0 && j == 0 && k == 0)) {
                int xd = (x+i) < 0 ? (D-1) : ((x+i) >= D ? 0 : (x+i));
                int yd = (y+j) < 0 ? (D-1) : ((y+j) >= D ? 0 : (y+j));
                int zd = (z+k) < 0 ? (D-1) : ((z+k) >= D ? 0 : (z+k));
                graph.addEdge(devs[x][y][z], 0, devs[xd][yd][zd]);
              }
            }

  // Prepare mapping from graph to hardware
  graph.map();

  for (int i = 0; i < D*D*D; i++)
    graph.devices[i]->state.test_length = T;

  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();
  printf("Starting\n");

  uint32_t msg[1 << TinselLogWordsPerMsg];
  hostLink.recv(msg);
  printf("Finished\n");

  return 0;
}
