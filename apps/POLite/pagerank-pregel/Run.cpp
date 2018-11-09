#include "PageRank.h"

#include <HostLink.h>
#include <POLite.h>
#include <EdgeList.h>
#include <assert.h>
#include <iostream>
#include <sys/time.h>

int main(int argc, char **argv)
{
  // Read in the example edge list and create data structure
  if (argc != 2) {
    printf("Specify edge file\n");
    exit(EXIT_FAILURE);
  }

  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<PageRankDevice::ThreadType> graph;

  // Load in the edge list file
  printf("Loading in the graph..."); fflush(stdout);
  EdgeList net;
  net.read(argv[1]);
  printf(" done\n");

  // Print max fan-out
  printf("Max fan-out = %d\n", net.maxFanOut());
  
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
  printf("Mapping the graph..."); fflush(stdout);
  graph.map();
  printf(" done\n");

  printf("Setting up devices..."); fflush(stdout);
  // Specify number of time steps to run on each device
  for (PDeviceId i = 0; i < graph.numDevices; i++) {
    // graph.devices[i]->state.numVertices = graph.numDevices;
    // graph.devices[i]->state.fanIn = graph.fanIn(i);
    // graph.devices[i]->state.fanOut = graph.fanOut(i);
    graph.devices[i]->state.val = 3; //1 / static_cast<float>(graph.numDevices);
  }
  printf(" done\n");

  // Write graph down to tinsel machine via HostLink
  printf("Loading the graph..."); fflush(stdout);
  graph.write(&hostLink);
  printf(" done\n");

  // Load code and trigger execution
  hostLink.boot("build/code.v", "build/data.v");

  // Global accumulated score
  float gscore = 0.0;

  // Get start time
  printf("Starting\n");
  struct timeval start, finish, diff; 
  gettimeofday(&start, NULL);
  hostLink.go();

  // Wait for response
  PMessage<None, PageRankMessage> msg;
  while(true) {
    // for (uint32_t i = 0; i < graph.numDevices; i++) {
    //if (true or DEBUG_VERBOSITY > 1) {
    hostLink.pollStdOut();
    //}

    if(hostLink.canRecv()) {
      hostLink.recvMsg(&msg, sizeof(msg));
    }
    
    // if(hostlink.)

    // gscore += msg.payload.val;
    // if (i == 0) {
    //   // Get finish time
    //   gettimeofday(&finish, NULL);
    // }
  }
 
  printf("Done\n");
  printf("score=%.8f\n", gscore);

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  return 0;
}
