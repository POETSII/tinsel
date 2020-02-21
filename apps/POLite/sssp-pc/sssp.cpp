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

  // Create weights
  srand(1);
  uint32_t** weights = new uint32_t* [net.numNodes];
  for (uint32_t i = 0; i < net.numNodes; i++) {
    uint32_t numNeighbours = net.neighbours[i][0];
    weights[i] = new uint32_t [numNeighbours];
    for (uint32_t j = 0; j < numNeighbours; j++) {
      weights[i][j] = rand() % 100;
    }
  }

  // Create states
  uint32_t* dist = new uint32_t [net.numNodes];
  int* queue = new int [net.numNodes];
  int queueSize = 0;
  int* queueNext = new int [net.numNodes];
  int queueSizeNext = 0;
  bool* inQueue = new bool [net.numNodes];
  for (int i = 0; i < net.numNodes; i++) {
    inQueue[i] = false;
    dist[i] = 0x7fffffff;
  }

  // Set source vertex
  dist[2] = 0;
  queue[queueSize++] = 2;
 
  // Start timer
  printf("Started\n");
  struct timeval start, finish, diff;
  gettimeofday(&start, NULL);

  int iters = 0;
  while (queueSize > 0) {
    for (int i = 0; i < queueSize; i++) {
      uint32_t me = queue[i];
      uint32_t numNeighbours = net.neighbours[me][0];
      for (uint32_t j = 0; j < numNeighbours; j++) {
        uint32_t neighbour = net.neighbours[me][j+1];
        uint32_t newDist = dist[me] + weights[me][j];
        if (newDist < dist[neighbour]) {
          dist[neighbour] = newDist;
          if (!inQueue[neighbour]) {
            queueNext[queueSizeNext++] = neighbour;
            inQueue[neighbour] = true;
          }
        }
      }
    }
    queueSize = queueSizeNext;
    queueSizeNext = 0;
    int32_t* tmp = queue; queue = queueNext; queueNext = tmp;
    for (int i = 0; i < queueSize; i++) inQueue[queue[i]] = false;
    iters++;
  }

  // Stop timer
  gettimeofday(&finish, NULL);

  uint64_t sum = 0;
  for (int i = 0; i < net.numNodes; i++)
    sum += dist[i];
  printf("Sum of distances = %ld\n", sum);
  printf("Iterations = %d\n", iters);

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  return 0;
}
