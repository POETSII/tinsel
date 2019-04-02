#include "SSSP.h"

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

  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<SSSPDevice, SSSPState, int32_t, int32_t> graph;

  // Create nodes in POETS graph
  for (uint32_t i = 0; i < net.numNodes; i++) {
    PDeviceId id = graph.newDevice();
    assert(i == id);
  }

  // Create connections in POETS graph
  for (uint32_t i = 0; i < net.numNodes; i++) {
    uint32_t numNeighbours = net.neighbours[i][0];
    for (uint32_t j = 0; j < numNeighbours; j++)
      graph.addLabelledEdge(1, i, 0, net.neighbours[i][j+1]);
  }

  // Prepare mapping from graph to hardware
  graph.map();

  // Initialise devices
  for (PDeviceId i = 0; i < graph.numDevices; i++) {
    graph.devices[i]->state.dist = 0x7fffffff;
  }

  // Set source vertex
  graph.devices[2]->state.isSource = true;
  graph.devices[2]->state.dist = 0;
 
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

  int64_t sum = 0;
  // Receive final distance to each vertex
  for (uint32_t i = 0; i < graph.numDevices; i++) {
    // Receive message
    PMessage<int32_t, int32_t> msg;
    hostLink.recvMsg(&msg, sizeof(msg));
    if (i == 0) gettimeofday(&finish, NULL);
    // Accumulate
    sum += msg.payload;
  }

  // Emit result
  printf("Sum of distances = %ld\n", sum);

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  return 0;
}
