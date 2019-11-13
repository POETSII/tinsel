// SPDX-License-Identifier: BSD-2-Clause
// Regression test: on each step, every device sends to its 26 3D neighbours

#include "Pressure.h"
#include <HostLink.h>
#include <POLite.h>
#include <sys/time.h>

// Number of time steps
#define T 1000

// Volume dimensions
#define D 40

int main()
{
  HostLink hostLink;
  PGraph<PressureDevice, PressureState, Dir, PressureMessage> graph;
  //graph.mapVerticesToDRAM = true;

  int devs[D][D][D];
  for (int x = 0; x < D; x++)
    for (int y = 0; y < D; y++)
      for (int z = 0; z < D; z++)
        devs[x][y][z] = graph.newDevice();

  for (int x = 0; x < D; x++)
    for (int y = 0; y < D; y++)
      for (int z = 0; z < D; z++) {
        int label = 0;
        for (int i = -1; i < 2; i++)
          for (int j = -1; j < 2; j++)
            for (int k = -1; k < 2; k++) {
              if (! (i == 0 && j == 0 && k == 0)) {
                int xd = (x+i) < 0 ? (D-1) : ((x+i) >= D ? 0 : (x+i));
                int yd = (y+j) < 0 ? (D-1) : ((y+j) >= D ? 0 : (y+j));
                int zd = (z+k) < 0 ? (D-1) : ((z+k) >= D ? 0 : (z+k));
                graph.addLabelledEdge(label, devs[x][y][z], 0,
                                             devs[xd][yd][zd]);
                label++;
              }
            }
      }

  // Prepare mapping from graph to hardware
  graph.map();

  // Initialise devices
  srand(0);
  for (int i = 0; i < D*D*D; i++) {
    graph.devices[i]->state.numSteps = T;
    graph.devices[i]->state.pressure = rand() % 100;
  }

  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();
  printf("Starting\n");

  // Start timer
  struct timeval start, finish, diff;
  gettimeofday(&start, NULL);

  // Consume performance stats
  politeSaveStats(&hostLink, "stats.txt");

  int64_t* pressures = new int64_t [D*D*D];
  PMessage<Dir, PressureMessage> msg;
  int64_t total = 0;
  for (int i = 0; i < D*D*D; i++) {
    hostLink.recvMsg(&msg, sizeof(PMessage<Dir, PressureMessage>));
    if (i == 0) gettimeofday(&finish, NULL);
    pressures[i] = msg.payload.pressure;
    total += msg.payload.pressure;
  }
  int64_t average = total/(D*D*D);
  printf("Average: %ld\n", average);

  total = 0;
  for (int i = 0; i < D*D*D; i++) {
    int64_t diff = pressures[i] - average;
    total += diff*diff;
  }
  printf("Standard deviation: %lf\n", sqrt((double) total / (double) (D*D*D)));

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  return 0;
}
