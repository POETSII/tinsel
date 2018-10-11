#define _GLIBCXX_USE_CXX11_ABI 0
#include <HostLink.h>
#include <POLite.h>
#include "PageRank.h"
#include "EdgeListReader.hpp"

int main()
{
  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<PageRankDevice, PageRankMessage> graph;

  std::cout << "Loading in the graph..";

  // read in the example edge list and create data structure
  EdgeListReader elreader("test.el");
  EdgeListGraph *in_elg = elreader.getELGraph();

  std::cout << " done\n";

  // print out the number of vertices and edges
  std::cout << "vertices: " << in_elg->numV() << " edges: " << in_elg->numE() << "\n";  
  fflush(stdout);

  // iterate through all the vertices and make a device for each
  PDeviceId devices[in_elg->numV()];
  for(int i=0; i<in_elg->numV(); i++) {
      devices[i] = graph.newDevice();
  } 

  // iterate through the edges and add them
  for(EdgeListGraph::eiterator i=in_elg->ebegin(), e=in_elg->eend(); i!=e; ++i) {
     edge_t c = *i;
     graph.addEdge(devices[c.src],0,devices[c.dst]);
  }

  std::cout << "Placing the graph..";
  fflush(stdout);
  // Prepare mapping from graph to hardware
  graph.map();
  std::cout << " done\n";
  fflush(stdout);

  std::cout << "Setting up devices..";
  fflush(stdout);
  // Specify number of time steps to run on each device
  for (PDeviceId i = 0; i < graph.numDevices; i++) {
    graph.devices[i]->num_vertices = graph.numDevices;
    graph.devices[i]->fanOut = graph.graph.incoming->elems[i]->numElems;//in_elg->fanOut(i);
  }
  std::cout << " done\n";
  fflush(stdout);

  std::cout << "Loading the graph..";
  fflush(stdout);
  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);
  std::cout << " done\n";
  fflush(stdout);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");

  // the global accumulated score
  float gscore = 0.0;
  unsigned recv_cnt = 0;

  printf("Starting\n");
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
