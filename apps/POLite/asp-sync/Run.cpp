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

  // Read network
  EdgeList net;
  net.read(argv[1]);

  // Print max fan-out
  printf("Max fan-out = %d\n", net.maxFanOut());

  // Check that parameters make sense
  assert(32*N <= net.numNodes);

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

  // Initialise devices
  for (PDeviceId i = 0; i < graph.numDevices; i++) {
    ASPState* dev = &graph.devices[i]->state;
    if (i < 32*N) {
      // This is a source node
      // By definition, a source node reaches itself
      dev->reaching[i >> 5] = 1 << (i & 0x1f);
    }
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

  #if DUMP_STATS > 0
    // Open file for performance counters
    FILE* statsFile = fopen("stats.txt", "wt");
    if (statsFile == NULL) {
      printf("Error creating stats file\n");
      exit(EXIT_FAILURE);
    }
    uint32_t numCaches = TinselMeshXLen * TinselMeshYLen *
                           TinselDCachesPerDRAM * TinselDRAMsPerBoard;
    hostLink.dumpStdOut(statsFile, numCaches);
    fclose(statsFile);
  #endif

  // Sum of all shortest paths
  uint32_t sum = 0;

  // Accumulate sum at each device
  for (uint32_t i = 0; i < graph.numDevices; i++) {
    PMessage<None, ASPMessage> msg;
    hostLink.recvMsg(&msg, sizeof(msg));
    if (i == 0) {
      // Stop timer
      gettimeofday(&finish, NULL);
    }
    sum += msg.payload.sum;
  }

  // Emit sum
  printf("Sum of subset of shortest paths = %u\n", sum);

  // Display time
  timersub(&finish, &start, &diff);
  double duration = (double) diff.tv_sec + (double) diff.tv_usec / 1000000.0;
  printf("Time = %lf\n", duration);

  return 0;
}
