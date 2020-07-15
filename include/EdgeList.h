// SPDX-License-Identifier: BSD-2-Clause
#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <vector>

struct EdgeList {
  // Number of nodes and edges
  uint32_t numNodes;
  uint32_t numEdges;

  // Mapping from node id to array of neighbouring node ids
  // First element of each array holds the number of neighbours
  uint32_t** neighbours;

  // Read network from file
  void read(const char* filename)
  {
    std::fstream file(filename, std::ios_base::in);
    std::vector<uint32_t> vec;

    // Count number of nodes and edges
    numEdges = 0;
    numNodes = 0;
    uint32_t numInts = 0;
    uint32_t val;
    while (file >> val) {
      vec.push_back(val);
      numNodes = val >= numNodes ? val+1 : numNodes;
      numEdges++;
    }
    assert((numEdges&1) == 0);
    numEdges >>= 1;

    uint32_t* count = (uint32_t*) calloc(numNodes, sizeof(uint32_t));
    for (int i = 0; i < vec.size(); i+=2) {
      count[vec[i]]++;
    }

    // Create mapping from node id to neighbours
    neighbours = (uint32_t**) calloc(numNodes, sizeof(uint32_t*));
    for (int i = 0; i < numNodes; i++) {
      neighbours[i] = (uint32_t*) calloc(count[i]+1, sizeof(uint32_t));
      neighbours[i][0] = count[i];
    }
    for (int i = 0; i < vec.size(); i+=2) {
      uint32_t src = vec[i];
      uint32_t dst = vec[i+1];
      neighbours[src][count[src]--] = dst;
    }
 
    // Release
    free(count);
    file.close();
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

  // Determine min fan-out
  uint32_t minFanOut() {
    uint32_t min = ~0;
    for (uint32_t i = 0; i < numNodes; i++) {
      uint32_t numNeighbours = neighbours[i][0];
      if (numNeighbours < min) min = numNeighbours;
    }
    return min;
  }

};

#endif
