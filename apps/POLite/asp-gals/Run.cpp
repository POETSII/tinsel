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

  // Read network
  EdgeList net;
  net.read(argv[1]);

  // Print max fan-out
  printf("Max fan-out = %d\n", net.maxFanOut());

  // Check that parameters make sense
  assert(32*NUM_SOURCES <= net.numNodes);

  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<ASPDevice, ASPState, None, ASPMessage> graph;

  // Create nodes in POETS graph
  for (uint32_t i = 0; i < net.numNodes; i++) {
    PDeviceId id = graph.newDevice();
    assert(i == id);
  }

  // Create connections in POETS graph
  for (uint32_t i = 0; i < net.numNodes; i++) {
    uint32_t numNeighbours = net.neighbours[i][0];
    for (uint32_t j = 0; j < numNeighbours; j++)
      graph.addEdge(i, 0, net.neighbours[i][j+1]);
  }

  // Prepare mapping from graph to hardware
  graph.map();

  // Create random set of source nodes
  uint32_t numSources = NUM_SOURCES*32;
  uint32_t sources[numSources];
  //randomSet(numSources, sources, graph.numDevices);
  for (int i = 0; i < numSources; i++) sources[i] = i;

  // Initialise devices
  for (PDeviceId i = 0; i < graph.numDevices; i++) {
    ASPState* dev = &graph.devices[i]->state;
    dev->toReach = numSources;
    dev->fanIn = graph.fanIn(i);
    assert(dev->fanIn > 0);
  }

  // By definition, a source node reaches itself
  for (PDeviceId i = 0; i < numSources; i++) {
    uint32_t src = sources[i];
    ASPState* dev = &graph.devices[src]->state;
    dev->reaching[i/32] |= 1 << (i%32);
    dev->toReach--;
  }
 
  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();

  // Timer
  printf("Started\n");
  struct timeval start, finish, diff;
  gettimeofday(&start, NULL);

  // Consume performance stats
  politeSaveStats(&hostLink, "stats.txt");

  // Sum of all shortest paths
  uint64_t sum = 0;

  // Accumulate sum at each device
  for (uint32_t i = 0; i < graph.numDevices; i++) {
    PMessage<ASPMessage> msg;
    hostLink.recvMsg(&msg, sizeof(msg));
    // Stop timer
    if (i == 0) gettimeofday(&finish, NULL);
    sum += msg.payload.reaching[0];
  }

  // Emit sum
  printf("Sum of subset of shortest paths = %lu\n", sum);

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  #ifndef POLITE_DUMP_STATS
  printf("Time = %lf\n", duration);
  #endif

  return 0;
}
