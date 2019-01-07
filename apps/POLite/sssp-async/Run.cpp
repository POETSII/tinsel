#include "SSSP.h"

#include <HostLink.h>
#include <POLite.h>

int main()
{
  // Parameters
  const uint32_t width  = 256;
  const uint32_t height = 256;
  const uint32_t time   = 100;

  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<SSSPDevice, SSSPState, int32_t, int32_t> graph;

  // Create 2D mesh of devices
  PDeviceId mesh[height][width];
  for (uint32_t y = 0; y < height; y++)
    for (uint32_t x = 0; x < width; x++)
      mesh[y][x] = graph.newDevice();

  // Add edges
  for (uint32_t y = 0; y < height; y++)
    for (uint32_t x = 0; x < width; x++) {
      if (x < width-1) {
        graph.addLabelledEdge(2, mesh[y][x], 0, mesh[y][x+1]);
      }
      if (y < height-1) {
        graph.addLabelledEdge(4, mesh[y][x], 0, mesh[y+1][x]);
      }
    }

  // Prepare mapping from graph to hardware
  graph.map();

  // Set initial distances
  for (uint32_t y = 0; y < height; y++)
    for (uint32_t x = 0; x < width; x++)
      graph.devices[mesh[y][x]]->state.dist = 0x7fffffff;

  // Set source vertex
  graph.devices[mesh[0][0]]->state.isSource = true;
  graph.devices[mesh[0][0]]->state.dist = 0;

  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();
  printf("Starting\n");

  // Receive final distance to each vertex
  int32_t sum = 0;
  for (uint32_t i = 0; i < graph.numDevices; i++) {
    // Receive message
    PMessage<int32_t, int32_t> msg;
    hostLink.recvMsg(&msg, sizeof(msg));
    // Accumulate
    sum += msg.payload;
  }

  // Emit result
  printf("Sum of distances = %d\n", sum);

  return 0;
}
