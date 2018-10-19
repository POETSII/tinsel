#define _GLIBCXX_USE_CXX11_ABI 0
#include <HostLink.h>
#include <POLite.h>
#include <assert.h>
#include <iostream>
#include "PageRank.h"
#include <sys/time.h>
#include "EdgeList.h"

int main(int argc, char **argv)
{
  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<PageRankDevice, PageRankMessage> graph;

  printf("Loading in the graph..");

  // read in the example edge list and create data structure
  if (argc != 2) {
	  printf("Specify edge file\n");
	  exit(EXIT_FAILURE);
  }

  // load in the edge list file
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

  printf("Placing the graph..");
  fflush(stdout);
  // Prepare mapping from graph to hardware
  graph.map();
  printf(" done\n");
  fflush(stdout);

  printf("Setting up devices..");
  fflush(stdout);
  // Specify number of time steps to run on each device
  for (PDeviceId i = 0; i < graph.numDevices; i++) {
    graph.devices[i]->num_vertices = graph.numDevices;
    graph.devices[i]->fanOut = graph.graph.incoming->elems[i]->numElems;//in_elg->fanOut(i);
  }
  printf(" done\n");
  fflush(stdout);

  printf("Loading the graph..");
  fflush(stdout);
  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);
  printf(" done\n");
  fflush(stdout);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");

  // the global accumulated score
  float gscore = 0.0;
  unsigned recv_cnt = 0;

  printf("Starting\n");
  fflush(stdout);
  // Get start time
  struct timeval start, finish, diff; 
  gettimeofday(&start, NULL);

  hostLink.go();

  // Wait for response
  PageRankMessage hostMsg; // score from the current response
  while(recv_cnt < graph.numDevices) {
      hostLink.recvMsg(&hostMsg, sizeof(hostMsg));
      gscore += hostMsg.val;
      if(recv_cnt == 0) {
          // get finish time
          gettimeofday(&finish, NULL);
      }
      recv_cnt++;
  }
 

  printf("Done\n");
  printf("score=%.8f\n", gscore);

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  return 0;
}
