#include <HostLink.h>
#include <POLite.h>
#include "ASP.h"

int main()
{
  // Parameters
  const uint32_t width  = 6;
  const uint32_t height = 6;

  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<ASPDevice, ASPMessage> graph;

  // Create 2D mesh of devices for testing purposes
  PDeviceId mesh[height][width];
  for (uint32_t y = 0; y < height; y++)
    for (uint32_t x = 0; x < width; x++)
      mesh[y][x] = graph.newDevice();

  // Add edges
  for (uint32_t y = 0; y < height; y++)
    for (uint32_t x = 0; x < width; x++) {
      if (x < width-1)
        graph.addBidirectionalEdge(mesh[y][x], 0, mesh[y][x+1], 0);
      if (y < height-1)
        graph.addBidirectionalEdge(mesh[y][x], 0, mesh[y+1][x], 0);
    }

  // Prepare mapping from graph to hardware
  graph.map();

  // Initialise devices
  for (PDeviceId i = 0; i < graph.numDevices; i++) {
    graph.devices[i]->id = i;
    graph.devices[i]->toReach = graph.numDevices-1;
  }
 
  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();
  printf("Started\n");

  // Sum of all shortest paths
  uint32_t sum = 0;

  // Accumulate sum at each device
  for (uint32_t i = 0; i < graph.numDevices; i++) {
    ASPMessage msg;
    hostLink.recvMsg(&msg, sizeof(ASPMessage));
    sum += msg.time;
  }

  // Emit sum
  printf("Sum of all shortest paths = %i\n", sum);

  return 0;
}
