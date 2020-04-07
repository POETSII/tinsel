// SPDX-License-Identifier: BSD-2-Clause
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <EdgeList.h>

int main(int argc, char**argv)
{
  if (argc != 2) {
    printf("Specify edges file\n");
    exit(EXIT_FAILURE);
  }

  // Read network
  EdgeList net;
  net.read(argv[1]);

  // Create states
  float* heat = new float [net.numNodes];
  float* heatNext = new float [net.numNodes];
  srand(1);
  for (int i = 0; i < net.numNodes; i++) {
    int r = rand() % 255;
    heat[i] = (float) r;
  }

  // Start timer
  printf("Started\n");
  struct timeval start, finish, diff;
  gettimeofday(&start, NULL);

  for (int t = 0; t < 100; t++) {
    for (int i = 0; i < net.numNodes; i++) {
      uint32_t numNeighbours = net.neighbours[i][0];
      float acc = 0.0;
      for (uint32_t j = 0; j < numNeighbours; j++) {
        uint32_t neighbour = net.neighbours[i][j+1];
        acc += heat[neighbour];
      }
      heatNext[i] = acc / (float) numNeighbours;
    }
    float* tmp = heat; heat = heatNext; heatNext = tmp;
  }

  // Stop timer
  gettimeofday(&finish, NULL);

  // Display final values of first ten devices
  for (uint32_t i = 0; i < 10; i++) {
    if (i < net.numNodes)
      printf("%d: %f\n", i, heat[i]);
  }

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  return 0;
}
