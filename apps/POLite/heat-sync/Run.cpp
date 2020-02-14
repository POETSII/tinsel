// SPDX-License-Identifier: BSD-2-Clause
#include "Heat.h"

#include <HostLink.h>
#include <POLite.h>
#include <EdgeList.h>
#include <sys/time.h>

int main(int argc, char **argv)
{
  const uint32_t time = 1000;

  // Read in the example edge list and create data structure
  if (argc != 2) {
    printf("Specify edge file\n");
    exit(EXIT_FAILURE);
  }

  // Load in the edge list file
  printf("Loading in the graph..."); fflush(stdout);
  EdgeList net;
  net.read(argv[1]);
  printf(" done\n");

  // Print max fan-out
  printf("Min fan-out = %d\n", net.minFanOut());
  printf("Max fan-out = %d\n", net.maxFanOut());

  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<HeatDevice, HeatState, None, HeatMessage> graph;

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

  // Specify number of time steps to run on each device
  srand(1);
  for (PDeviceId i = 0; i < graph.numDevices; i++) {
    int r = rand() % 255;
    graph.devices[i]->state.id = i;
    graph.devices[i]->state.time = time;
    graph.devices[i]->state.val = (float) r;
    graph.devices[i]->state.isConstant = false;
    //graph.devices[i]->state.fanOut = graph.fanOut(i);
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

  // Allocate array to contain final value of each device
  float* pixels = new float [graph.numDevices];

  // Receive final value of each device
  for (uint32_t i = 0; i < graph.numDevices; i++) {
    // Receive message
    PMessage<None, HeatMessage> msg;
    hostLink.recvMsg(&msg, sizeof(msg));
    if (i == 0) gettimeofday(&finish, NULL);
    // Save final value
    pixels[msg.payload.from] = msg.payload.val;
  }

  // Display final values of first ten devices
  for (uint32_t i = 0; i < 10; i++) {
    if (i < graph.numDevices) {
      printf("%d: %f\n", i, pixels[i]);
    }
  }

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  return 0;
}
