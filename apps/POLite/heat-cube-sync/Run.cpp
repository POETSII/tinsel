// SPDX-License-Identifier: BSD-2-Clause
#include "Heat.h"

#include <HostLink.h>
#include <POLite.h>
#include <sys/time.h>

int main()
{
  // Dimensions of cube
  const uint32_t D = 40;
  // Number of time steps to simulate
  const uint32_t T = 10000;

  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<HeatDevice, HeatState, None, HeatMessage> graph;

  // 3D volume of devices
  int devs[D][D][D];
  for (int x = 0; x < D; x++)
    for (int y = 0; y < D; y++)
      for (int z = 0; z < D; z++)
        devs[x][y][z] = graph.newDevice();

  // Add edges
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

  for (int i = 0; i < D*D*D; i++) {
    graph.devices[i]->state.time = T;
    graph.devices[i]->state.acc = 40.0;
    graph.devices[i]->state.val = 40.0;
  }

  // Apply constant heat at origin
  graph.devices[0]->state.val = 255.0;

  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();

  // Start timer
  struct timeval start, finish, diff;
  gettimeofday(&start, NULL);

  politeSaveStats(&hostLink, "stats.txt");

  // Receive final value of each device
  for (uint32_t i = 0; i < graph.numDevices; i++) {
    // Receive message
    PMessage<HeatMessage> msg;
    hostLink.recvMsg(&msg, sizeof(msg));
    if (i == 0) gettimeofday(&finish, NULL);
  }

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  return 0;
}
