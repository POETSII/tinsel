#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

struct Network {
  // Number of nodes and edges
  uint32_t numNodes;
  uint32_t numEdges;

  // Mapping from node id to array of neighbouring node ids
  // First element of each array holds the number of neighbours
  uint32_t** neighbours;

  // Read network from file
  void read(const char* filename)
  {
    // Read edges
    FILE* fp = fopen(filename, "rt");
    if (fp == NULL) {
      fprintf(stderr, "Can't open '%s'\n", filename);
      exit(EXIT_FAILURE);
    }

    // Create mapping from node id to number of neighbours
    int ret = fscanf(fp, "%d %d", &numNodes, &numEdges);
    uint32_t* count = (uint32_t*) calloc(numNodes, sizeof(uint32_t));
    for (int i = 0; i < numEdges; i++) {
      uint32_t src, dst;
      ret = fscanf(fp, "%d %d", &src, &dst);
      count[src]++;
      count[dst]++;
    }

    // Create mapping from node id to neighbours
    neighbours = (uint32_t**) calloc(numNodes, sizeof(uint32_t*));
    rewind(fp);
    ret = fscanf(fp, "%d %d", &numNodes, &numEdges);
    for (int i = 0; i < numNodes; i++) {
      neighbours[i] = (uint32_t*) calloc(count[i]+1, sizeof(uint32_t));
      neighbours[i][0] = count[i];
    }
    for (int i = 0; i < numEdges; i++) {
      uint32_t src, dst;
      ret = fscanf(fp, "%d %d", &src, &dst);
      neighbours[src][count[src]--] = dst;
      neighbours[dst][count[dst]--] = src;
    }
 
    // Release
    free(count);
    fclose(fp);
  }

  // Determine max fan-out
  uint32_t maxFanOut() {
    uint32_t max = 0;
    for (uint32_t i = 0; i < numNodes; i++) {
      uint32_t numNeighbours = neighbours[i][0];
      if (numNeighbours > max) max = numNeighbours;
    }
    return max;
  }
};

#endif
