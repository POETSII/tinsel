// SPDX-License-Identifier: BSD-2-Clause
#include "ASP.h"

#include <HostLink.h>
#include <POLite.h>
#include <EdgeList.h>
#include <assert.h>
#include <sys/time.h>
#include <config.h>

int main(int argc, char**argv)
{
  if (argc != 2) {
    printf("Specify edges file\n");
    exit(EXIT_FAILURE);
  }

  struct timeval startAll, finishAll;
  gettimeofday(&startAll, NULL);

  // Read network
  printf("Loading...\n");
  EdgeList net;
  net.read(argv[1]);
  printf("Max fanout: %d\n", net.maxFanOut());

  // Check that parameters make sense
  uint32_t numTiles = (net.numNodes+NROOTS-1)/NROOTS;
  uint32_t totalNodes = numTiles * net.numNodes;
  printf("Sources per tile: %d\nTiles: %d\nTotal nodes: %d\n", NROOTS, numTiles, totalNodes);
  assert(net.numNodes%NROOTS == 0);

  // Connection to tinsel machine
  printf("Creating host link...\n");
  HostLink hostLink;

  // Create POETS graph
  printf("Creating graph...\n");
  PGraph<ASPDevice, ASPState, None, ASPMessage> graph;

  // Create nodes in POETS graph
  for (uint32_t t = 0; t < numTiles; t++) {
    for (uint32_t i = 0; i < net.numNodes; i++) {
      PDeviceId id = graph.newDevice();
      assert(t*net.numNodes+i == id);
    }
  }

  // Create connections in POETS graph
  for (uint32_t t = 0; t < numTiles; t++) {
    uint32_t tileOffs = t*net.numNodes;
    for (uint32_t i = 0; i < net.numNodes; i++) {
      uint32_t numNeighbours = net.neighbours[i][0];
      for (uint32_t j = 0; j < numNeighbours; j++)
        graph.addEdge(i+tileOffs, 0, net.neighbours[i][j+1]+tileOffs);
    }
  }

  // Prepare mapping from graph to hardware
  printf("Mapping...\n");
  graph.mapVerticesToDRAM = true;
  graph.mapEdgesToDRAM = true;
  graph.map();

  // Create nodes in POETS graph
  for (uint32_t t = 0; t < numTiles; t++) {
    for (uint32_t i = 0; i < net.numNodes; i++) {
      uint32_t id = t*net.numNodes+i;
      ASPState* dev = &graph.devices[id]->state;
      dev->rootIdx = -1;
    }
  }

  if (net.numNodes%NROOTS > 0) {
    // TODO: Fill remainder tile
  }
  
  // Initialise sources
  uint32_t totalSources = 0;
  for (uint32_t t = 0; t < numTiles; t++) {
    uint32_t tileOffs = t*net.numNodes;
    uint32_t baseSrc = t*NROOTS + tileOffs;
    for (PDeviceId i = 0; (i<NROOTS) && (baseSrc+i < totalNodes); i++) {
      ASPState* dev = &graph.devices[baseSrc+i]->state;
      dev->rootIdx = i;
	  totalSources++;
    }
  }
  assert(totalSources == net.numNodes);
 
  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();

  // Timer
  printf("\nStarted\n");
  struct timeval startCompute, finishCompute;
  gettimeofday(&startCompute, NULL);

  // Consume performance stats
  politeSaveStats(&hostLink, "stats.txt");

  // Sum of all shortest paths
  uint64_t sum = 0;

  // Accumulate sum at each device
  for (uint32_t i = 0; i < graph.numDevices; i++) {
    PMessage<None, ASPMessage> msg;
    hostLink.recvMsg(&msg, sizeof(msg));
    if (i == 0) {
      // Stop timer
      gettimeofday(&finishCompute, NULL);
    }
    sum += msg.payload.sum;
  }

  // Emit sum
  printf("Sum = %lu\n", sum);
  printf("ASP = %.4lf\n", (double)sum/(double)net.numNodes/(double)(net.numNodes-1));

  // Display time
  struct timeval diff;
  double duration;
  timersub(&finishCompute, &startCompute, &diff);
  duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time (compute) = %lf\n", duration);
  gettimeofday(&finishAll, NULL);
  timersub(&finishAll, &startAll, &diff);
  duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time (all) = %lf\n", duration);

  return 0;
}
