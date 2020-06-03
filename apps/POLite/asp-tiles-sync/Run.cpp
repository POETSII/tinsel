// SPDX-License-Identifier: BSD-2-Clause
#include "ASP.h"
#include "RandomSet.h"

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
  uint32_t numSources = N*32;
  uint32_t numTiles = (net.numNodes+numSources-1)/numSources;
  uint32_t totalNodes = numTiles * net.numNodes;
  printf("Sources per tile: %d\nTiles: %d\nTotal nodes: %d\n", numSources, numTiles, totalNodes);

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
  graph.map();

  if (net.numNodes%numSources > 0) {
    // Fill remainder tile
    for (PDeviceId i = 0; i < net.numNodes; i++) {
      uint32_t id = (numTiles-1)*net.numNodes + i;
      ASPState* dev = &graph.devices[id]->state;
      uint32_t fill = numSources - net.numNodes%numSources;
      uint32_t j = N-1;
      for ( ; fill>=32; j--) {
        dev->reaching[j] = 0xffffffff;
        //printf("Dev[%d].r[%d] = %x\n", id, j, dev->reaching[j]);
        fill -= 32;
      }
      uint32_t mask = 0;
      for ( ; fill>0; fill--) {
        mask = (mask>>1) | 0x80000000;
      }
      dev->reaching[j] = mask;
      //printf("Dev[%d].r[%d] = %x\n", id, j, dev->reaching[j]);
    }
  }
  
  // Initialise sources
  uint32_t totalSources = 0;
  for (uint32_t t = 0; t < numTiles; t++) {
    uint32_t tileOffs = t*net.numNodes;
    uint32_t baseSrc = t*numSources + tileOffs;
    for (PDeviceId i = 0; (i<numSources) && (baseSrc+i < totalNodes); i++) {
      // By definition, a source node reaches itself
      ASPState* dev = &graph.devices[baseSrc+i]->state;
      dev->reaching[i/32] |= 1 << (i%32);
      //printf("Init src %d (%d) at dev[%d]\n", i, t*numSources+i, baseSrc+i);
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
  printf("Time (compute, including stats transfer over UART) = %lf\n", duration);
  gettimeofday(&finishAll, NULL);
  timersub(&finishAll, &startAll, &diff);
  duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time (all, including stats transfer over UART) = %lf\n", duration);

  return 0;
}
