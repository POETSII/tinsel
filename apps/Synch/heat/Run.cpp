#include <HostLink.h>
#include <Synch.h>
#include "Heat.h"
#include "Colours.h"

int main()
{
  // Parameters
  //const uint32_t width  = 64;
  //const uint32_t height = 32;
  //const uint32_t time   = 2;
  const uint32_t width  = 8;
  const uint32_t height = 8;

  // Connection to tinsel machine
  HostLink hostLink;

  // Create POETS graph
  PGraph<HeatDevice, HeatMessage> graph;

  // Create 2D mesh of devices
  PDeviceId mesh[height][width];
  for (uint32_t y = 0; y < height; y++)
    for (uint32_t x = 0; x < width; x++) {
      mesh[y][x] = graph.newDevice();
      // Each device has a single output pin (pin 1), one message wide
      graph.setPinWidth(mesh[y][x], 1, 1);
    }

  // Add edges
  for (uint32_t y = 0; y < height; y++)
    for (uint32_t x = 0; x < width; x++) {
      if (x < width-1) {
        graph.addEdge(mesh[y][x], 1, mesh[y][x+1], 1);
        graph.addEdge(mesh[y][x+1], 1, mesh[y][x], 1);
      }
      if (y < height-1) {
        graph.addEdge(mesh[y][x], 1, mesh[y+1][x], 1);
        graph.addEdge(mesh[y+1][x], 1, mesh[y][x], 1);
      }
    }

  // Prepare mapping from graph to hardware
  graph.map();

  // Apply constant heat at north edge
  // Apply constant cool at south edge
  for (uint32_t x = 0; x < width; x++) {
    graph.devices[mesh[0][x]]->kind = 1;
    graph.getState(mesh[0][x])->val = 255 << 16;
    graph.devices[mesh[height-1][x]]->kind = 1;
    graph.getState(mesh[height-1][x])->val = 40 << 16;
  }

  // Apply constant heat at west edge
  // Apply constant cool at east edge
  for (uint32_t y = 0; y < height; y++) {
    graph.devices[mesh[y][0]]->kind = 1;
    graph.getState(mesh[y][0])->val = 255 << 16;
    graph.devices[mesh[y][width-1]]->kind = 1;
    graph.getState(mesh[y][width-1])->val = 40 << 16;
  }

  // Write graph down to tinsel machine via HostLink
  graph.write(&hostLink);

  // Load code and trigger execution
  hostLink.boot("code.v", "data.v");
  hostLink.go();

  // Allocate array to contain final value of each device
  uint32_t pixels[graph.numDevices];

  // Receive final value of each device
  for (uint32_t i = 0; i < graph.numDevices; i++) {
    // Receive message
    HeatMessage msg;
    hostLink.recvMsg(&msg, sizeof(HeatMessage));
    // Save final value
    PDeviceId id = graph.fromDeviceAddr[msg.addr.threadId][msg.addr.localAddr];
    pixels[id] = msg.val;
  }

  // Emit image
  printf("P3\n%d %d\n255\n", width, height);
  for (uint32_t y = 0; y < height; y++)
    for (uint32_t x = 0; x < width; x++) {
      uint32_t val = (pixels[mesh[y][x]] >> 16) & 0xff;
      printf("%d %d %d\n",
        colours[val*3], colours[val*3+1], colours[val*3+2]);
    }

  return 0;
}
