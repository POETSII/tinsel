// SPDX-License-Identifier: BSD-2-Clause
#include "RandomSet.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>

// Number of nodes and edges
uint32_t numNodes;
uint32_t numEdges;

// Mapping from node id to array of neighbouring node ids
// First element of each array holds the number of neighbours
uint32_t** neighbours;

// Mapping from node id to bit vector of reaching nodes
uint64_t** reaching;
uint64_t** reachingNext;

// Number of 64-bit words in reaching vector
const uint64_t vectorSize = 6;

void readGraph(const char* filename, bool undirected)
{
  // Read edges
  FILE* fp = fopen(filename, "rt");
  if (fp == NULL) {
    fprintf(stderr, "Can't open '%s'\n", filename);
    exit(EXIT_FAILURE);
  }

  // Count number of nodes and edges
  numEdges = 0;
  numNodes = 0;
  int ret;
  while (1) {
    uint32_t src, dst;
    ret = fscanf(fp, "%d %d", &src, &dst);
    if (ret == EOF) break;
    numEdges++;
    numNodes = src >= numNodes ? src+1 : numNodes;
    numNodes = dst >= numNodes ? dst+1 : numNodes;
  }
  rewind(fp);

  // Create mapping from node id to number of neighbours
  uint32_t* count = (uint32_t*) calloc(numNodes, sizeof(uint32_t));
  for (int i = 0; i < numEdges; i++) {
    uint32_t src, dst;
    ret = fscanf(fp, "%d %d", &src, &dst);
    count[src]++;
    if (undirected) count[dst]++;
  }

  // Create mapping from node id to neighbours
  neighbours = (uint32_t**) calloc(numNodes, sizeof(uint32_t*));
  rewind(fp);
  for (int i = 0; i < numNodes; i++) {
    neighbours[i] = (uint32_t*) calloc(count[i]+1, sizeof(uint32_t));
    neighbours[i][0] = count[i];
  }
  for (int i = 0; i < numEdges; i++) {
    uint32_t src, dst;
    ret = fscanf(fp, "%d %d", &src, &dst);
    neighbours[src][count[src]--] = dst;
    if (undirected) neighbours[dst][count[dst]--] = src;
  }

  // Create mapping from node id to bit vector of reaching nodes
  reaching = (uint64_t**) calloc(numNodes, sizeof(uint64_t*));
  reachingNext = (uint64_t**) calloc(numNodes, sizeof(uint64_t*));
  for (int i = 0; i < numNodes; i++) {
    reaching[i] = (uint64_t*) calloc(vectorSize, sizeof(uint64_t));
    reachingNext[i] = (uint64_t*) calloc(vectorSize, sizeof(uint64_t));
  }

  // Release
  free(count);
  fclose(fp);
}

// Compute sum of all shortest paths from given sources
uint64_t ssp(uint32_t numSources, uint32_t* sources)
{
  // Sum of distances
  uint64_t sum = 0;

  // Initialise reaching vector for each node
  for (int i = 0; i < numNodes; i++) {
    for (int j = 0; j < vectorSize; j++) {
      reaching[i][j] = 0;
      reachingNext[i][j] = 0;
    }
  }
  for (int i = 0; i < numSources; i++) {
    uint32_t src = sources[i];
    reaching[src][i/64] |= 1ul << (i%64);
  }

  int* queue = new int [numNodes];
  int queueSize = 0;
  for (int i = 0; i < numNodes; i++) queue[queueSize++] = i;

  // Distance increases on each iteration
  uint32_t dist = 1;

  while (queueSize > 0) {
    // For each node
    for (int i = 0; i < queueSize; i++) {
      int me = queue[i];
      // For each neighbour
      uint32_t numNeighbours = neighbours[me][0];
      for (int j = 1; j <= numNeighbours; j++) {
        uint32_t n = neighbours[me][j];
        // For each chunk
        for (int k = 0; k < vectorSize; k++) {
          if (reaching[me][k] & ~reachingNext[n][k])
            reachingNext[n][k] |= reaching[me][k];
        }
      }
    }

    // For each node, update reaching vector
    queueSize = 0;
    for (int i = 0; i < numNodes; i++) {
      bool addToQueue = false;
      for (int k = 0; k < vectorSize; k++) {
        uint64_t diff = reachingNext[i][k] & ~reaching[i][k];
        if (diff) {
          addToQueue = true;
          uint32_t n = __builtin_popcountll(diff);
          sum += n * dist;
          reaching[i][k] |= reachingNext[i][k];
        }
      }
      if (addToQueue) queue[queueSize++] = i;
    }
    dist++;
  }

  printf("steps: %d\n", dist-1);

  return sum;
}

int main(int argc, char**argv)
{
  if (argc != 2) {
    printf("Specify edges file\n");
    exit(EXIT_FAILURE);
  }
  bool undirected = false;
  readGraph(argv[1], undirected);
  printf("Nodes: %u.  Edges: %u\n", numNodes, numEdges);

  uint32_t numSources = 64*vectorSize;
  assert(numSources < numNodes);
  uint32_t sources[numSources];
  for (int i = 0; i < numSources; i++) sources[i] = i;
  //randomSet(numSources, sources, numNodes);

  struct timeval start, finish, diff;

  uint64_t sum = 0;
  const int nodesPerVector = 64 * vectorSize;
  gettimeofday(&start, NULL);
  sum = ssp(numSources, sources);
  gettimeofday(&finish, NULL);

  printf("Sum of subset of shortest paths = %lu\n", sum);
 
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  return 0;
}
